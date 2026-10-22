#ifndef ASSETS_H
#define ASSETS_H

#include <avr/pgmspace.h>
#include <stdint.h>
#include "board_config.h"

#define CAR_WIDTH 3
#define CAR_HEIGHT 5

extern const uint8_t car[CAR_HEIGHT] PROGMEM;
extern const uint8_t digits[10][5] PROGMEM;
extern const uint8_t explosion[5] PROGMEM;
extern const uint8_t trophy[7] PROGMEM;
extern const uint8_t play_icon[5] PROGMEM;
extern const uint8_t logo[DISPLAY_HEIGHT] PROGMEM;

uint8_t car_row(uint8_t row);

#endif
