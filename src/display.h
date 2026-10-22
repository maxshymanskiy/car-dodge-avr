#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include "board_config.h"

void display_init(void);
void display_flush(const uint8_t framebuffer[DISPLAY_HEIGHT]);

#endif
