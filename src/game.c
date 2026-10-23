#include <avr/pgmspace.h>
#include <string.h>
#include "game.h"
#include "collision.h"
#include "spawn.h"

static game_t game_data;

static void add_points(game_t *game, uint16_t points) {
    // Widen before adding so uint8_t cannot wrap before the cap.
    uint16_t total = game->points + points;
    game->points = total > SCORE_MAX ? SCORE_MAX : total;
}

static void recover_life(game_t *game) {
    game->damage_free_levels++;
    if (game->damage_free_levels >= LIFE_RECOVERY_LEVELS) {
        if (game->lives < GAME_STARTING_LIVES) {
            game->lives++;
        }
        // Full health consumes the streak too; extra hearts cannot be banked.
        game->damage_free_levels = 0;
    }
}

// Expand the config list into a Flash-resident table.
#define LEVEL_INTERVAL(interval) interval,
static const uint16_t level_step_ms[GAME_LEVEL_COUNT] PROGMEM = {
    GAME_LEVELS(LEVEL_INTERVAL)
};
#undef LEVEL_INTERVAL

void game_init(void) {
    memset(&game_data, 0, sizeof(game_data));
    game_data.state = MENU;
    game_data.player_x = PLAYER_START_X;
    game_data.random_state = 0xACE1U;
}

const game_t *game_get_state(void) {
    return &game_data;
}

uint16_t game_step_interval_ms(const game_t *game) {
    return pgm_read_word(&level_step_ms[game->level]);
}

static uint8_t random_position(game_t *game) {
    // Xorshift generates cheap pseudo-random positions from a 16-bit state.
    game->random_state ^= game->random_state << 7;
    game->random_state ^= game->random_state >> 9;
    game->random_state ^= game->random_state << 8;
    return game->random_state % CAR_POSITIONS;
}

static uint8_t player_has_collided(const game_t *game) {
    for (uint8_t i = 0; i < GAME_MAX_OBSTACLES; i++) {
        if (!game->obstacles[i].active) {
            continue;
        }

        if (player_overlaps_obstacle(game->player_x, game->obstacles[i].x, game->obstacles[i].y)) {
            return 1;
        }
    }

    return 0;
}

static void activate_obstacle(game_t *game, uint8_t slot, uint8_t x) {
    game->obstacles[slot].x = x;
    game->obstacles[slot].y = SPAWN_Y;
    game->obstacles[slot].active = 1;
    game->obstacles[slot].near_miss = 0;
    game->spawned++;
    game->steps_since_spawn = 0;
}

static void try_spawn(game_t *game, uint32_t now) {
    if (game->spawned >= GAME_CARS_PER_LEVEL || game->steps_since_spawn < GAME_SPAWN_GAP_ROWS) {
        return;
    }

    uint8_t active_count = 0;
    uint8_t free_slot = GAME_MAX_OBSTACLES;

    for (uint8_t i = 0; i < GAME_MAX_OBSTACLES; i++) {
        if (game->obstacles[i].active) {
            active_count++;
        } else {
            free_slot = i;
        }
    }

    if (free_slot == GAME_MAX_OBSTACLES) {
        return;
    }

    uint8_t first_position;
    if (active_count == 0) {
        // Prefer the opposite edge to keep an empty road moving.
        first_position = game->player_x <= MAX_CAR_X / 2 ? MAX_CAR_X : 0;
    } else {
        first_position = random_position(game);
    }

    uint8_t position;
    if (spawn_choose_position(game, free_slot, first_position, now, &position)) {
        activate_obstacle(game, free_slot, position);
    }
}

static void begin_level(game_t *game, uint32_t now) {
    memset(game->obstacles, 0, sizeof(game->obstacles));
    game->player_x = PLAYER_START_X;
    game->spawned = 0;
    game->passed = 0;
    game->steps_since_spawn = GAME_SPAWN_GAP_ROWS;
    game->last_step = now;
    game->last_move = now;
    game->state = PLAYING;
    try_spawn(game, now);
}

static void reset_game(game_t *game, uint32_t now) {
    game->level = 0;
    game->lives = GAME_STARTING_LIVES;
    game->damage_free_levels = 0;
    game->points = 0;
    game->random_state ^= (uint16_t)now;
    // Xorshift would stay at zero forever.
    if (!game->random_state) {
        game->random_state = 0xACE1U;
    }
    begin_level(game, now);
}

static void start_crash(game_t *game, uint32_t now) {
    // One life per collision, even if several input/update checks detect it.
    if (game->state != PLAYING) {
        return;
    }

    if (game->lives > 0) {
        game->lives--;
    }
    game->damage_free_levels = 0;

    game->state = CRASH_ANIMATION;
    game->crash_started_at = now;
}

static void toggle_pause(game_t *game, uint32_t now) {
    if (game->state == PLAYING) {
        game->state = PAUSED;
        game->paused_since = now;
    } else if (game->state == PAUSED) {
        // Shift timers so paused time does not advance movement.
        uint32_t duration = now - game->paused_since;
        game->last_step += duration;
        game->last_move += duration;
        game->state = PLAYING;
    }
}

static void update_game(game_t *game, uint32_t now) {
    if (game->state == CRASH_ANIMATION) {
        if ((uint32_t)(now - game->crash_started_at) >= CRASH_FRAME_MS * CRASH_FRAMES) {
            if (game->lives > 0) {
                begin_level(game, now);
            } else {
                game->state = GAME_OVER;
            }
        }
        return;
    }
    if (game->state != PLAYING || (uint32_t)(now - game->last_step) < game_step_interval_ms(game)) {
        return;
    }

    game->last_step = now;
    // Saturate the counter so delayed spawns cannot wrap it back to zero.
    if (game->steps_since_spawn < 255) {
        game->steps_since_spawn++;
    }

    uint8_t passed_dodges = 0;
    for (uint8_t i = 0; i < GAME_MAX_OBSTACLES; i++) {
        if (!game->obstacles[i].active) {
            continue;
        }

        game->obstacles[i].y++;
        if (game->obstacles[i].y >= DISPLAY_HEIGHT) {
            passed_dodges += game->obstacles[i].near_miss;
            game->obstacles[i].active = 0;
            game->passed++;
        }
    }
    if (player_has_collided(game)) {
        start_crash(game, now);
        return;
    }

    // A crash cancels pending dodge bonuses, including cars passing this tick.
    add_points(game, (uint16_t)passed_dodges * SCORE_PER_DODGE);
    if (game->passed == GAME_CARS_PER_LEVEL) {
        recover_life(game);
        add_points(game, SCORE_PER_LEVEL);
        if (game->level == GAME_LEVEL_COUNT - 1) {
            add_points(game, (uint16_t)game->lives * SCORE_PER_LIFE);
            game->state = VICTORY;
        } else {
            game->level++;
            begin_level(game, now);
        }
        return;
    }
    try_spawn(game, now);
}

static void move_player(game_t *game, uint8_t direction, uint8_t direction_was_pressed, uint32_t now) {
    if (direction != LEFT && direction != RIGHT) {
        return;
    }

    uint32_t time_since_last_move = now - game->last_move;
    if (!direction_was_pressed && time_since_last_move < PLAYER_REPEAT_MS) {
        return;
    }

    uint8_t previous_x = game->player_x;

    if (direction == LEFT && game->player_x > 0) {
        game->player_x--;
    }

    if (direction == RIGHT && game->player_x < MAX_CAR_X) {
        game->player_x++;
    }

    // A blocked press at the edge is not a move and must not renew cooldown.
    if (game->player_x == previous_x) {
        return;
    }

    game->last_move = now;

    if (player_has_collided(game)) {
        start_crash(game, now);
        return;
    }

    for (uint8_t i = 0; i < GAME_MAX_OBSTACLES; i++) {
        obstacle_t *obstacle = &game->obstacles[i];
        if (obstacle->active && !obstacle->near_miss &&
            player_dodged_obstacle(previous_x, game->player_x, obstacle->x, obstacle->y)) {
            // Commit only after this obstacle leaves the road without a crash.
            obstacle->near_miss = 1;
        }
    }
}

static void handle_input(game_t *game, input_state_t input, uint32_t now) {
    uint8_t events = input.pressed;

    if (game->state == VICTORY || game->state == GAME_OVER) {
        if (events & PAUSE) {
            reset_game(game, now);
        }
        return;
    }
    if (game->state == MENU) {
        if (events) {
            reset_game(game, now);
        }
        return;
    }
    if (game->state == CRASH_ANIMATION) {
        return;
    }
    if (events & PAUSE) {
        toggle_pause(game, now);
        return;
    }

    if (game->state == PAUSED) {
        return;
    }

    uint8_t direction = input.held & (LEFT | RIGHT);
    uint8_t direction_just_pressed = events & (LEFT | RIGHT);
    move_player(game, direction, direction_just_pressed, now);
}

void game_handle_input(input_state_t input, uint32_t now) {
    handle_input(&game_data, input, now);
}

void game_update(uint32_t now) {
    update_game(&game_data, now);
}
