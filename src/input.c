#include <avr/io.h>
#include "board_config.h"
#include "game_config.h"
#include "input.h"

static uint8_t stable_buttons;
static uint8_t candidate;
static uint32_t changed_at;

void input_init(void) {
    DDRD &= ~((1 << LEFT_PIN) | (1 << RIGHT_PIN) | (1 << PAUSE_PIN));
    PORTD |= (1 << LEFT_PIN) | (1 << RIGHT_PIN) | (1 << PAUSE_PIN);
    stable_buttons = 0;
    candidate = 0;
    changed_at = 0;
}

static uint8_t read_buttons(void) {
    uint8_t input = PIND;
    uint8_t pressed = 0;

    if (!(input & (1 << LEFT_PIN))) pressed |= LEFT;
    if (!(input & (1 << RIGHT_PIN))) pressed |= RIGHT;
    if (!(input & (1 << PAUSE_PIN))) pressed |= PAUSE;
    return pressed;
}

static uint8_t button_events(uint8_t pressed, uint32_t now) {
    if (pressed != candidate) {
        candidate = pressed;
        changed_at = now;
    }
    uint32_t time_since_change = now - changed_at;
    if (time_since_change < INPUT_DEBOUNCE_MS || stable_buttons == candidate) {
        return 0;
    }

    // Report new presses only; held buttons remain in stable_buttons.
    uint8_t events = candidate & ~stable_buttons;
    stable_buttons = candidate;
    return events;
}

input_state_t input_poll(uint32_t now) {
    uint8_t pressed_buttons = read_buttons();
    uint8_t events = button_events(pressed_buttons, now);
    input_state_t input = {stable_buttons, events};
    return input;
}
