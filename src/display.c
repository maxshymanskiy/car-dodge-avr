#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "spi.h"
#include "display.h"

static const uint8_t module_order[MODULE_COUNT] PROGMEM = DISPLAY_MODULE_ORDER;
static const uint8_t module_rotation[MODULE_COUNT] PROGMEM = DISPLAY_MODULE_ROTATION;
static const uint8_t module_mirror[MODULE_COUNT] PROGMEM = DISPLAY_MODULE_MIRROR;

static void latch(void) {
    PORTB |= (1 << LATCH_PIN);
    _delay_us(1);
    PORTB &= ~(1 << LATCH_PIN);
    _delay_us(1);
}

static void send_to_max7219(uint8_t address, uint8_t data) {
    PORTB &= ~(1 << LATCH_PIN);
    for (uint8_t i = 0; i < MODULE_COUNT; i++) {
        spi_transfer(address);
        spi_transfer(data);
    }
    latch();
}

void display_init(void) {
    // Hardware SS must be an output before enabling SPI master mode.
    PORTB &= ~(1 << LATCH_PIN);
    DDRB |= (1 << LATCH_PIN);
    spi_init();

    send_to_max7219(0x0F, 0); // Display test off.
    send_to_max7219(0x0C, 0); // Shutdown while clearing.
    send_to_max7219(0x09, 0); // No BCD decoding.
    send_to_max7219(0x0B, MODULE_SIZE - 1); // Scan all eight rows.
    send_to_max7219(0x0A, DISPLAY_BRIGHTNESS);
    for (uint8_t row = 1; row <= MODULE_SIZE; row++) {
        send_to_max7219(row, 0);
    }
    send_to_max7219(0x0C, 1);
}

static void display_map(uint8_t x, uint8_t y, uint8_t *module, uint8_t *row, uint8_t *bit) {
    if (FLIP_SCREEN) {
        x = DISPLAY_WIDTH - 1 - x;
        y = DISPLAY_HEIGHT - 1 - y;
    }

    // Convert screen coordinates to local matrix coordinates before rotation.
    uint8_t block = y / MODULE_SIZE;
    uint8_t u = x;
    uint8_t v = y % MODULE_SIZE;

    if (pgm_read_byte(&module_mirror[block])) {
        u = MODULE_SIZE - 1 - u;
    }

    switch (pgm_read_byte(&module_rotation[block]) & 3) {
        case 1: {
            uint8_t temporary = u;
            u = MODULE_SIZE - 1 - v;
            v = temporary;
            break;
        }

        case 2:
            u = MODULE_SIZE - 1 - u;
            v = MODULE_SIZE - 1 - v;
            break;

        case 3: {
            uint8_t temporary = u;
            u = v;
            v = MODULE_SIZE - 1 - temporary;
            break;
        }

        default:
            break;
    }

    *module = pgm_read_byte(&module_order[block]);
    *row = v;
    *bit = u;
}

void display_flush(const uint8_t framebuffer[DISPLAY_HEIGHT]) {
    uint8_t physical[MODULE_COUNT][MODULE_SIZE] = {{0}};

    for (uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
        for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
            if (!(framebuffer[y] & (1U << x))) {
                continue;
            }

            uint8_t module;
            uint8_t row;
            uint8_t bit;
            display_map(x, y, &module, &row, &bit);
            physical[module][row] |= (uint8_t)(1U << bit);
        }
    }

    for (uint8_t row = 0; row < MODULE_SIZE; row++) {
        PORTB &= ~(1 << LATCH_PIN);
        // The first transmitted pair travels to the farthest driver.
        for (int8_t module = MODULE_COUNT - 1; module >= 0; module--) {
            spi_transfer(row + 1);
            spi_transfer(physical[module][row]);
        }
        latch();
    }
}
