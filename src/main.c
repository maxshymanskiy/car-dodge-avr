#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>

#include "spi.h"
#include "millis.h"

#define LATCH_PIN PB2       // D10 -> MAX7219 CS/LOAD
#define RIGHT_PIN PD2
#define LEFT_PIN PD3
#define PAUSE_PIN PD4
#define LEFT 1U
#define RIGHT 2U
#define PAUSE 4U
#define POLL_MS 5UL
#define DEBOUNCE_MS 25UL
#define REPEAT_MS 120UL
#define FRAME_MS 20UL
#define MENU_BLINK_MS 400UL
#define LIVES_PER_GAME 3
#define HP_BLINK_PERIOD_MS 1000UL
#define HP_BLINK_OFF_MS 100UL
#define CRASH_FRAME_MS 80UL
#define CRASH_FRAMES 12
#define BRIGHTNESS 3
#define CAR_WIDTH 3
#define CAR_HEIGHT 5
#define MAX_OBSTACLES 3
#define LEVEL_COUNT 10
#define CARS_PER_LEVEL 10
#define PLAYFIELD_TOP 8
#define MAX_CAR_X (8 - CAR_WIDTH)
#define CAR_POSITIONS (MAX_CAR_X + 1)
#define SPAWN_Y (PLAYFIELD_TOP - CAR_HEIGHT)
#define REACTION_MS 150UL
// Includes ceil(REACTION_MS / the fastest row interval), plus state zero.
#define PLAN_COOLDOWNS 6
#define PLAYER_Y 27         // Five rows: y=27..31, at the very bottom
#define FLIP_SCREEN 0       // Keep sprite orientation unchanged

// Top-to-bottom logical blocks -> chain indices (0 = nearest MCU)
static const uint8_t module_order[4] PROGMEM = {3, 2, 1, 0}; 
// Clockwise quarter turns for each 8x8 block
static const uint8_t module_rotation[4] PROGMEM = {1, 1, 1, 1};
static const uint8_t module_mirror[4] PROGMEM = {0, 0, 0, 0};

// Each HEX byte is one sprite row; bit 0 is the left pixel
static const uint8_t car[CAR_HEIGHT] PROGMEM = {
    0x02,
    0x07,
    0x02,
    0x07,
    0x05
};

// Fixed traffic density: only speed changes between levels.
static const uint8_t level_speed[LEVEL_COUNT] PROGMEM = {
    100, 90, 80, 70, 60, 55, 50, 45, 40, 35
};
static const uint8_t level_gap[LEVEL_COUNT] PROGMEM = {
    18, 18, 18, 18, 18, 18, 18, 18, 18, 18
};
static const uint8_t level_traffic[LEVEL_COUNT] PROGMEM = {
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3
};

static const uint8_t digits[10][5] PROGMEM = {
    {0x07, 0x05, 0x05, 0x05, 0x07},
    {0x02, 0x06, 0x02, 0x02, 0x07},
    {0x07, 0x01, 0x07, 0x04, 0x07},
    {0x07, 0x01, 0x07, 0x01, 0x07},
    {0x05, 0x05, 0x07, 0x01, 0x01},
    {0x07, 0x04, 0x07, 0x01, 0x07},
    {0x07, 0x04, 0x07, 0x05, 0x07},
    {0x07, 0x01, 0x01, 0x01, 0x01},
    {0x07, 0x05, 0x07, 0x05, 0x07},
    {0x07, 0x05, 0x07, 0x01, 0x07}
};
static const uint8_t explosion[5] PROGMEM = {
    0x15, 0x0E, 0x1F, 0x0E, 0x15
};
static const uint8_t trophy[7] PROGMEM = {
    0x1F, 0x15, 0x15, 0x0E, 0x04, 0x04, 0x0E
};

// Mirrored hollow 4x5 play symbol, centered in the fourth module.
static const uint8_t play_icon[5] PROGMEM = {
    0x30, 0x28, 0x24, 0x28, 0x30
};

// C, A, R with horizontally mirrored glyphs for the physical display.
static const uint8_t logo[32] PROGMEM = {
    // First 8x8 module: C.
    0x00, 0x3C, 0x20, 0x20, 0x20, 0x3C, 0x00, 0x00,

    // Second 8x8 module: A.
    0x00, 0x18, 0x24, 0x3C, 0x24, 0x24, 0x00, 0x00,

    // Third 8x8 module: R.
    0x00, 0x38, 0x24, 0x38, 0x28, 0x24, 0x00, 0x00,

    // Fourth 8x8 module: space for the blinking play symbol.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t framebuffer[32];

typedef enum { MENU, PLAYING, PAUSED, CRASH_ANIMATION, GAME_OVER, VICTORY } game_state_t;

typedef struct {
    uint8_t x;
    int8_t y;
    uint8_t active;
} obstacle_t;

static game_state_t state;
static uint8_t player_x = 2;
static obstacle_t obstacles[MAX_OBSTACLES];
static uint8_t level, passed, spawned, steps_since_spawn;
static uint8_t lives;
static uint8_t stable_buttons;
static uint16_t random_state = 0xACE1U;
static uint32_t last_step;
static uint32_t last_move;
static uint32_t paused_since;
static uint32_t state_since;

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
        framebuffer[y] |= (uint8_t)(1 << x);
    }
}

static uint8_t car_row(uint8_t row) {
    return pgm_read_byte(&car[row]);
}

static void draw_car(uint8_t x, int8_t y) {
    for (uint8_t row = 0; row < CAR_HEIGHT; row++) {
        for (uint8_t col = 0; col < CAR_WIDTH; col++) {
            if (car_row(row) & (1U << col)) {
                if (y + row >= PLAYFIELD_TOP) set_pixel(x + col, y + row);
            }
        }
    }
}

// Returns true only for overlapping lit pixels, not empty sprite corners.
static uint8_t cars_overlap(uint8_t x, uint8_t enemy_x, int8_t enemy_y) {
    for (uint8_t row = 0; row < CAR_HEIGHT; row++) {
        int8_t other_row = PLAYER_Y + row - enemy_y;
        if (other_row >= 0 && other_row < CAR_HEIGHT &&
            ((car_row(row) << x) & (car_row(other_row) << enemy_x))) return 1;
    }
    return 0;
}

static void draw_icon(const uint8_t *icon, uint8_t height, int8_t x, int8_t y) {
    for (uint8_t row = 0; row < height; row++) {
        uint8_t bits = pgm_read_byte(&icon[row]);
        for (uint8_t col = 0; col < 5; col++) {
            if (bits & (1U << col)) set_pixel(x + col, y + row);
        }
    }
}

// Mirror text horizontally, as in the existing CAR logo on this display.
static void draw_digit(uint8_t digit, uint8_t x) {
    for (uint8_t row = 0; row < 5; row++) {
        uint8_t bits = pgm_read_byte(&digits[digit][row]);
        for (uint8_t col = 0; col < 3; col++) {
            if (bits & (1U << (2 - col))) set_pixel(7 - x - col, 1 + row);
        }
    }
}

static void draw_status(uint32_t now) {
    draw_digit(level, 2);
    // Life dots sit beside the digit, briefly blinking off once per second.
    if (now % HP_BLINK_PERIOD_MS < HP_BLINK_PERIOD_MS - HP_BLINK_OFF_MS) {
        for (uint8_t i = 0; i < lives; i++) set_pixel(1, 1 + i * 2);
    }
    // Row 6 is blank; row 7 is the divider. Traffic starts in the next module.
    framebuffer[PLAYFIELD_TOP - 1] = 0xFF;
}

static void draw_play_prompt(uint32_t now) {
    if ((now / MENU_BLINK_MS) % 2 == 0) {
        for (uint8_t row = 0; row < sizeof(play_icon); row++) {
            framebuffer[25 + row] |= pgm_read_byte(&play_icon[row]);
        }
    }
}

static void draw_scene(uint8_t show_player) {
    if (show_player) draw_car(player_x, PLAYER_Y);
    for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) continue;
        if (!show_player && cars_overlap(player_x, obstacles[i].x, obstacles[i].y)) continue;
        draw_car(obstacles[i].x, obstacles[i].y);
    }
}

static void draw_crash(uint32_t now) {
    uint8_t frame = (uint32_t)(now - state_since) / CRASH_FRAME_MS;
    draw_scene(0);
    if (frame < 8 || (frame % 2) == 0) {
        draw_icon(explosion, 5, (int8_t)player_x - 1, PLAYER_Y);
        int8_t cx = player_x + 1, cy = PLAYER_Y + 2;
        uint8_t radius = frame / 2 + 1;
        if (radius > 4) radius = 4;
        for (uint8_t r = 1; r <= radius; r++) {
            set_pixel(cx - r, cy - r); set_pixel(cx + r, cy - r);
            set_pixel(cx - r, cy + r); set_pixel(cx + r, cy + r);
        }
    }
}

static void render(uint32_t now) {
    memset(framebuffer, 0, sizeof(framebuffer));
    if (state == MENU) {
        memcpy_P(framebuffer, logo, sizeof(framebuffer));
        draw_play_prompt(now);
    } else {
        draw_status(now);
        switch (state) {
            case PLAYING: case PAUSED: draw_scene(1); break;
            case CRASH_ANIMATION: draw_crash(now); break;
            case GAME_OVER: draw_play_prompt(now); break;
            case VICTORY:
                draw_icon(trophy, 7, 1, 15);
                for (uint8_t y = 26; y < 32; y++) framebuffer[y] = (y % 2) ? 0xAA : 0x55;
                break;
            default: break;
        }
    }
    display_flush();
}

/* ---------- Game logic ---------- */

static uint8_t step_interval(void) { return pgm_read_byte(&level_speed[level]); }
static uint8_t spawn_gap(void) { return pgm_read_byte(&level_gap[level]); }
static uint8_t traffic_limit(void) { return pgm_read_byte(&level_traffic[level]); }

static uint8_t random_position(void) {
    random_state ^= random_state << 7;
    random_state ^= random_state >> 9;
    random_state ^= random_state << 8;
    return random_state % CAR_POSITIONS;
}

static uint8_t blocked_positions(const int8_t y[MAX_OBSTACLES]) {
    uint8_t blocked = 0;
    for (uint8_t x = 0; x <= MAX_CAR_X; x++) {
        for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
            if (obstacles[i].active && cars_overlap(x, obstacles[i].x, y[i])) {
                blocked |= (1U << x);
                break;
            }
        }
    }
    return blocked;
}

static uint8_t collision(void) {
    for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active && cars_overlap(player_x, obstacles[i].x, obstacles[i].y)) return 1;
    }
    return 0;
}

// Reachability check over road positions and movement/reaction cooldown states.
// It accounts for car pixels, held-button repeat time and a reaction allowance.
// Candidate cars are rejected if no route survives until all known cars pass.
static uint8_t safe_spawn(uint32_t now) {
    int8_t y[MAX_OBSTACLES];
    uint8_t reachable[PLAN_COOLDOWNS] = {0};
    uint8_t step = step_interval();
    uint8_t move_ticks = (REPEAT_MS + POLL_MS + step - 1) / step;
    uint8_t reaction_ticks = (REACTION_MS + step - 1) / step;
    uint32_t elapsed = now - last_move;
    uint8_t cooldown = elapsed >= REPEAT_MS + POLL_MS ? 0 :
        (REPEAT_MS + POLL_MS - elapsed + step - 1) / step;
    if (cooldown < reaction_ticks) cooldown = reaction_ticks;
    if (move_ticks >= PLAN_COOLDOWNS || cooldown >= PLAN_COOLDOWNS) return 0;
    reachable[cooldown] = (1U << player_x);
    for (uint8_t i = 0; i < MAX_OBSTACLES; i++) y[i] = obstacles[i].y;

    for (uint8_t tick = 0; tick < 32; tick++) {
        uint8_t next[PLAN_COOLDOWNS] = {0};
        uint8_t free_before = ~blocked_positions(y);
        for (uint8_t wait = 0; wait < PLAN_COOLDOWNS; wait++) {
            uint8_t safe = reachable[wait] & free_before;
            next[wait ? wait - 1 : 0] |= safe;
            if (!wait) {
                uint8_t moves = ((safe << 1) | (safe >> 1)) &
                    ((1U << CAR_POSITIONS) - 1) & free_before;
                next[move_ticks - 1] |= moves;
            }
        }
        uint8_t pending = 0;
        for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
            if (!obstacles[i].active || y[i] >= 32) continue;
            y[i]++;
            if (y[i] < 32) pending = 1;
        }
        uint8_t free_after = ~blocked_positions(y), any = 0;
        for (uint8_t wait = 0; wait < PLAN_COOLDOWNS; wait++) {
            reachable[wait] = next[wait] & free_after;
            any |= reachable[wait];
        }
        if (!any) return 0;
        if (!pending) return 1;
    }
    return 0;
}

static void try_spawn(uint32_t now) {
    if (spawned >= CARS_PER_LEVEL || steps_since_spawn < spawn_gap()) return;
    uint8_t active = 0, slot = MAX_OBSTACLES;
    for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active) active++;
        else slot = i;
    }
    if (active >= traffic_limit() || slot == MAX_OBSTACLES) return;

    obstacles[slot].active = 1;
    obstacles[slot].y = SPAWN_Y;
    if (!active) {
        // An empty road must never stall on a planner rejection. Put the car
        // opposite the player, with enough horizontal separation to pass safely.
        obstacles[slot].x = player_x <= MAX_CAR_X / 2 ? MAX_CAR_X : 0;
        spawned++;
        steps_since_spawn = 0;
        return;
    }
    uint8_t first = random_position();
    for (uint8_t attempt = 0; attempt < CAR_POSITIONS; attempt++) {
        obstacles[slot].x = (first + attempt) % CAR_POSITIONS;
        if (safe_spawn(now)) {
            spawned++;
            steps_since_spawn = 0;
            return;
        }
    }
    // No safe choice: delay this spawn and try again on the next row step.
    obstacles[slot].active = 0;
}

static void begin_level(uint32_t now) {
    memset(obstacles, 0, sizeof(obstacles));
    player_x = 2;
    spawned = passed = 0;
    steps_since_spawn = spawn_gap();
    last_step = last_move = now;
    state = PLAYING;
    try_spawn(now);
}

static void reset_game(uint32_t now) {
    level = 0;
    lives = LIVES_PER_GAME;
    player_x = 2;
    random_state ^= (uint16_t)now;
    if (!random_state) random_state = 0xACE1U;
    begin_level(now);
}

static void start_crash(uint32_t now) {
    // One life per collision, even if several input/update checks detect it.
    if (state != PLAYING) return;
    if (lives) lives--;
    state = CRASH_ANIMATION;
    state_since = now;
}

static void toggle_pause(uint32_t now) {
    if (state == PLAYING) {
        state = PAUSED;
        paused_since = now;
    } else {
        uint32_t duration = now - paused_since;
        last_step += duration;
        last_move += duration;
        state = PLAYING;
    }
}

static void update_game(uint32_t now) {
    if (state == CRASH_ANIMATION) {
        if ((uint32_t)(now - state_since) >= CRASH_FRAME_MS * CRASH_FRAMES) {
            if (lives) begin_level(now);
            else {
                state = GAME_OVER;
                state_since = now;
            }
        }
        return;
    }
    if (state != PLAYING || (uint32_t)(now - last_step) < step_interval()) return;

    last_step = now;
    if (steps_since_spawn < 255) steps_since_spawn++;
    for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) continue;
        obstacles[i].y++;
        if (obstacles[i].y >= 32) {
            obstacles[i].active = 0;
            passed++;
        }
    }
    if (collision()) { start_crash(now); return; }
    if (passed == CARS_PER_LEVEL) {
        if (level == LEVEL_COUNT - 1) {
            state = VICTORY;
            state_since = now;
        } else {
            level++;
            begin_level(now);
        }
        return;
    }
    try_spawn(now);
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
    if (state == MENU || state == GAME_OVER || state == VICTORY) {
        if (events) reset_game(now);
        return;
    }
    if (state == CRASH_ANIMATION) return;
    if (events & PAUSE) { toggle_pause(now); return; }
    if (state == PAUSED) return;

    uint8_t direction = stable_buttons & (LEFT | RIGHT);
    if (direction != LEFT && direction != RIGHT) {
        return;
    }
    if (!(events & (LEFT | RIGHT)) && (uint32_t)(now - last_move) < REPEAT_MS) return;
    uint8_t previous_x = player_x;
    if (direction == LEFT && player_x > 0) player_x--;
    if (direction == RIGHT && player_x < MAX_CAR_X) player_x++;
    // A blocked press at the edge is not a move and must not renew cooldown.
    if (player_x == previous_x) return;
    last_move = now;
    if (collision()) start_crash(now);
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

    // Keep the startup logo visible until any debounced button press.
    while (1) {
        now = millis();

        if ((uint32_t)(now - last_poll) >= POLL_MS) {
            last_poll = now;
            handle_input(button_events(read_buttons(), now), now);
        }

        update_game(now);

        if ((uint32_t)(now - last_frame) >= FRAME_MS) {
            last_frame = now;
            render(now);
        }
    }
}
