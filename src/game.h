#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "game_config.h"
#include "board_config.h"
#include "assets.h"
#include "input.h"

#define PLAYFIELD_TOP MODULE_SIZE
#define MAX_CAR_X (DISPLAY_WIDTH - CAR_WIDTH)
#define CAR_POSITIONS (MAX_CAR_X + 1)
#define SPAWN_Y (PLAYFIELD_TOP - CAR_HEIGHT)
#define PLAYER_Y (DISPLAY_HEIGHT - CAR_HEIGHT)
#define PLAYER_START_X (MAX_CAR_X / 2)

typedef enum {
    MENU,
    PLAYING,
    PAUSED,
    CRASH_ANIMATION,
    GAME_OVER,
    VICTORY
} game_state_t;

typedef struct {
    uint8_t x;
    int8_t y;
    uint8_t active;
    uint8_t near_miss;
} obstacle_t;

// Owned by game.c. Other modules receive a read-only view.
typedef struct {
    game_state_t state;
    uint8_t player_x;
    obstacle_t obstacles[GAME_MAX_OBSTACLES];
    uint8_t level;
    uint8_t passed;
    uint8_t spawned;
    uint8_t steps_since_spawn;
    uint8_t lives;
    uint8_t damage_free_levels;
    uint8_t points;
    uint16_t random_state;
    uint32_t last_step;
    uint32_t last_move;
    uint32_t paused_since;
    uint32_t crash_started_at;
} game_t;

void game_init(void);
const game_t *game_get_state(void);
uint16_t game_step_interval_ms(const game_t *game);
void game_handle_input(input_state_t input, uint32_t now);
void game_update(uint32_t now);

#endif
