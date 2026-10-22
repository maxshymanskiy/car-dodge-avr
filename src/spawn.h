#ifndef SPAWN_H
#define SPAWN_H

#include <stdint.h>
#include "game.h"

// Selects a safe X for an available slot without changing gameplay state.
// game.c controls spawn timing, slot allocation and activation.
uint8_t spawn_choose_position(const game_t *game, uint8_t slot, uint8_t first_position, uint32_t now, uint8_t *position);

#endif
