#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

#define LEFT 1U
#define RIGHT 2U
#define PAUSE 4U

typedef struct {
    uint8_t held;
    uint8_t pressed;
} input_state_t;

void input_init(void);
input_state_t input_poll(uint32_t now);

#endif
