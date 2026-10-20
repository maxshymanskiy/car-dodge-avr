#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>

#include "spi.h"
#include "millis.h"

#define LATCH_PIN PB2 // D10 -> MAX7219 CS/LOAD
#define RIGHT_PIN PD2
#define LEFT_PIN PD3
#define PAUSE_PIN PD4
#define LEFT 1U
#define RIGHT 2U
#define PAUSE 4U
#define POLL_MS 5UL
#define DEBOUNCE_MS 25UL
#define REPEAT_MS 120UL
#define STEP_MS 140UL
#define FRAME_MS 20UL
#define BRIGHTNESS 1
#define CAR_WIDTH 3
#define CAR_HEIGHT 5
#define PLAYER_Y 27             // Five rows: y=27..31, at the very bottom.
#define FLIP_SCREEN 0           // Keep sprite orientation unchanged.

// Top-to-bottom logical blocks -> chain indices (0 = nearest MCU).
static const uint8_t module_order[4] PROGMEM = {3, 2, 1, 0};
// Clockwise quarter turns for each 8x8 block: 0, 1, 2, 3.
static const uint8_t module_rotation[4] PROGMEM = {1, 1, 1, 1};
static const uint8_t module_mirror[4] PROGMEM = {0, 0, 0, 0};

// Bit 0 is the left pixel of each sprite row.
static const uint8_t car[CAR_HEIGHT] PROGMEM = {2, 7, 2, 7, 5};

static uint8_t framebuffer[32];
static uint8_t player_x = 2;
static uint8_t obstacle_x;
static int8_t obstacle_y;
static uint8_t paused;
static uint8_t crashed;
static uint8_t stable_buttons;
static uint16_t random_state = 0xACE1U;
static uint32_t last_step;
static uint32_t last_move;
static uint32_t paused_since;

/* ---------- MAX7219 ---------- */

static void latch(void) {
    PORTB |= (1 << LATCH_PIN);
    _delay_us(1);
    PORTB &= ~(1 << LATCH_PIN);
    _delay_us(1);
}

// Sends the same register/value to all four cascaded MAX7219.
static void send_to_max7219(uint8_t address, uint8_t data) {
    PORTB &= ~(1 << LATCH_PIN);
    for (uint8_t i = 0; i < 4; i++) {
        spi_transfer(address);
        spi_transfer(data);
    }
    latch();
}

static void display_init(void) {
    send_to_max7219(0x0F, 0); // Display test off.
    send_to_max7219(0x0C, 0); // Shutdown while clearing.
    send_to_max7219(0x09, 0); // No BCD decoding.
    send_to_max7219(0x0B, 7); // Scan all eight rows.
    send_to_max7219(0x0A, BRIGHTNESS);
    for (uint8_t row = 1; row <= 8; row++) {
        send_to_max7219(row, 0);
    }
    send_to_max7219(0x0C, 1);
}

// Converts a logical pixel (x, y) to a physical module, row and bit.
static void display_map(uint8_t x, uint8_t y,
                        uint8_t *module, uint8_t *row, uint8_t *bit) {
    if (FLIP_SCREEN) {
        x = 7 - x;
        y = 31 - y;
    }

    uint8_t block = y / 8;
    uint8_t u = x;
    uint8_t v = y % 8;

    if (pgm_read_byte(&module_mirror[block])) {
        u = 7 - u;
    }

    switch (pgm_read_byte(&module_rotation[block]) & 3) {
        case 1: { uint8_t t = u; u = 7 - v; v = t; break; }
        case 2: u = 7 - u; v = 7 - v; break;
        case 3: { uint8_t t = u; u = v; v = 7 - t; break; }
        default: break;
    }

    *module = pgm_read_byte(&module_order[block]);
    *row = v;
    *bit = u;
}

static void display_flush(void) {
    uint8_t physical[4][8] = {{0}};

    for (uint8_t y = 0; y < 32; y++) {
        for (uint8_t x = 0; x < 8; x++) {
            if (!(framebuffer[y] & (1U << x))) continue;

            uint8_t module, row, bit;
            display_map(x, y, &module, &row, &bit);
            physical[module][row] |= (uint8_t)(1U << bit);
        }
    }

    for (uint8_t row = 0; row < 8; row++) {
        PORTB &= ~(1 << LATCH_PIN);
        for (int8_t module = 3; module >= 0; module--) {
            spi_transfer(row + 1);
            spi_transfer(physical[module][row]);
        }
        latch();
    }
}

/* ---------- Drawing ---------- */

static void set_pixel(int8_t x, int8_t y) {
    if (x >= 0 && x < 8 && y >= 0 && y < 32) {
        framebuffer[y] |= (uint8_t)(1U << x);
    }
}

static uint8_t car_row(uint8_t row) {
    return pgm_read_byte(&car[row]);
}

static void draw_car(uint8_t x, int8_t y) {
    for (uint8_t row = 0; row < CAR_HEIGHT; row++) {
        for (uint8_t col = 0; col < CAR_WIDTH; col++) {
            if (car_row(row) & (1U << col)) {
                set_pixel(x + col, y + row);
            }
        }
    }
}

static void render(void) {
    memset(framebuffer, 0, sizeof(framebuffer));
    draw_car(player_x, PLAYER_Y);
    draw_car(obstacle_x, obstacle_y);
    display_flush();
}

/* ---------- Game logic ---------- */

// Spawns one car above the screen at a pseudorandom horizontal position.
static void spawn_obstacle(void) {
    random_state ^= random_state << 7;
    random_state ^= random_state >> 9;
    random_state ^= random_state << 8;
    obstacle_x = random_state % (8 - CAR_WIDTH + 1);
    obstacle_y = -CAR_HEIGHT;
}

static uint8_t collision(void) {
    for (uint8_t row = 0; row < CAR_HEIGHT; row++) {
        int8_t enemy_row = PLAYER_Y + row - obstacle_y;
        if (enemy_row < 0 || enemy_row >= CAR_HEIGHT) continue;

        uint8_t player_bits = car_row(row) << player_x;
        uint8_t enemy_bits = car_row(enemy_row) << obstacle_x;
        if (player_bits & enemy_bits) return 1;
    }
    return 0;
}

static void reset_game(uint32_t now) {
    player_x = 2;
    paused = 0;
    crashed = 0;
    last_step = now;
    last_move = now;
    spawn_obstacle();
}

static void toggle_pause(uint32_t now) {
    if (!paused) {
        paused = 1;
        paused_since = now;
        return;
    }
    // Shift timers so the game continues exactly where it stopped.
    uint32_t pause_length = now - paused_since;
    last_step += pause_length;
    last_move += pause_length;
    paused = 0;
}

static void update_game(uint32_t now) {
    if (paused || crashed || (uint32_t)(now - last_step) < STEP_MS) return;

    last_step = now;
    obstacle_y++;
    if (obstacle_y >= 32) {
        spawn_obstacle();
    }
    crashed = collision();
}

/* ---------- Input ---------- */

static uint8_t read_buttons(void) {
    uint8_t input = PIND;
    uint8_t pressed = 0;

    if (!(input & (1 << LEFT_PIN)))  pressed |= LEFT;
    if (!(input & (1 << RIGHT_PIN))) pressed |= RIGHT;
    if (!(input & (1 << PAUSE_PIN))) pressed |= PAUSE;
    return pressed;
}

// Debounce algorithm reused from Lab_3. Returns newly pressed buttons.
static uint8_t button_events(uint8_t pressed, uint32_t now) {
    static uint8_t candidate;
    static uint32_t changed_at;

    if (pressed != candidate) {
        candidate = pressed;
        changed_at = now;
    }
    if ((uint32_t)(now - changed_at) < DEBOUNCE_MS || stable_buttons == candidate) {
        return 0;
    }

    uint8_t events = candidate & ~stable_buttons;
    stable_buttons = candidate;
    return events;
}

static void handle_input(uint8_t events, uint32_t now) {
    if (crashed) {
        if (events & (LEFT | RIGHT)) reset_game(now);
        return;
    }
    if (events & PAUSE) {
        toggle_pause(now);
        return;
    }
    if (paused) return;

    // Move only while exactly one direction button is held.
    uint8_t direction = stable_buttons & (LEFT | RIGHT);
    if (direction != LEFT && direction != RIGHT) {
        last_move = now;
        return;
    }

    // Move on a fresh press, or repeat while the button is held.
    uint8_t fresh_press = events & (LEFT | RIGHT);
    if (!fresh_press && (uint32_t)(now - last_move) < REPEAT_MS) return;

    if (direction == LEFT && player_x > 0) player_x--;
    if (direction == RIGHT && player_x < 8 - CAR_WIDTH) player_x++;
    last_move = now;
    crashed = collision();
}

/* ---------- Main ---------- */

int main(void) {
    // SS must be output BEFORE enabling SPI master mode.
    PORTB &= ~(1 << LATCH_PIN);
    DDRB |= (1 << LATCH_PIN);
    DDRD &= ~((1 << LEFT_PIN) | (1 << RIGHT_PIN) | (1 << PAUSE_PIN));
    PORTD |= (1 << LEFT_PIN) | (1 << RIGHT_PIN) | (1 << PAUSE_PIN);

    spi_init();
    display_init();
    millis_init();

    uint32_t now = millis();
    uint32_t last_poll = now;
    uint32_t last_frame = now - FRAME_MS; // Render on the first iteration.

    reset_game(now); // Start immediately, without a menu.

    while (1) {
        now = millis();

        if ((uint32_t)(now - last_poll) >= POLL_MS) {
            last_poll = now;
            handle_input(button_events(read_buttons(), now), now);
        }

        update_game(now);

        if ((uint32_t)(now - last_frame) >= FRAME_MS) {
            last_frame = now;
            render();
        }
    }
}