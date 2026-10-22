#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>
#include "game.h"

void renderer_draw(const game_t *game, uint32_t now);

#endif
