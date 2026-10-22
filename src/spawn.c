#include <string.h>
#include "spawn.h"
#include "collision.h"

static uint8_t blocked_player_positions(const game_t *game, const int8_t y[GAME_MAX_OBSTACLES], uint8_t candidate_slot, uint8_t candidate_x) {
    // Bit x marks a player position blocked by an existing or proposed car.
    uint8_t blocked = 0;

    for (uint8_t player_position = 0;
         player_position <= MAX_CAR_X;
         player_position++) {
        for (uint8_t i = 0; i < GAME_MAX_OBSTACLES; i++) {
            uint8_t obstacle_is_present = game->obstacles[i].active || i == candidate_slot;
            uint8_t obstacle_x = (i == candidate_slot) ? candidate_x : game->obstacles[i].x;

            if (obstacle_is_present &&
                player_overlaps_obstacle(player_position, obstacle_x, y[i])) {
                blocked |= (1U << player_position);
                break;
            }
        }
    }
    return blocked;
}

static uint8_t has_safe_spawn_route(const game_t *game, uint8_t candidate_slot, uint8_t candidate_x, uint32_t now) {
    int8_t y[GAME_MAX_OBSTACLES];
    uint16_t step = game_step_interval_ms(game);
    uint32_t elapsed = now - game->last_move;
    uint8_t move_ticks;
    uint8_t reaction_ticks;
    uint8_t cooldown;

    if (step == 0 || candidate_slot >= GAME_MAX_OBSTACLES) {
        return 0;
    }

    // The selected level determines the required search memory.
    uint8_t cooldown_count = SPAWN_COOLDOWN_COUNT(step);
    // reachable[wait] holds positions with that many steps until a move.
    uint8_t reachable[cooldown_count];
    memset(reachable, 0, sizeof(reachable));

    // Round delays up so the search never assumes faster reactions.
    move_ticks = (SPAWN_MOVE_DELAY_MS + step - 1) / step;
    reaction_ticks = (SPAWN_REACTION_MS + step - 1) / step;

    if (move_ticks >= cooldown_count) {
        return 0;
    }

    if (elapsed >= SPAWN_MOVE_DELAY_MS) {
        cooldown = 0;
    } else {
        cooldown = (SPAWN_MOVE_DELAY_MS - elapsed + step - 1) / step;
    }

    if (cooldown < reaction_ticks) {
        cooldown = reaction_ticks;
    }

    if (cooldown >= cooldown_count) {
        return 0;
    }

    reachable[cooldown] = (1U << game->player_x);
    for (uint8_t i = 0; i < GAME_MAX_OBSTACLES; i++) {
        y[i] = (i == candidate_slot) ? SPAWN_Y : game->obstacles[i].y;
    }

    for (uint8_t tick = 0; tick < DISPLAY_HEIGHT - SPAWN_Y; tick++) {
        uint8_t next[cooldown_count];
        memset(next, 0, sizeof(next));
        // Check safety before player movement and again after cars descend.
        uint8_t free_before = (uint8_t)~blocked_player_positions(
            game, y, candidate_slot, candidate_x);

        for (uint8_t wait = 0; wait < cooldown_count; wait++) {
            uint8_t safe = reachable[wait] & free_before;
            if (wait > 0) {
                next[wait - 1] |= safe;
            } else {
                next[0] |= safe;
            }

            // Ready positions may move one column left or right.
            if (!wait) {
                uint8_t moves = ((safe << 1) | (safe >> 1)) &
                    ((1U << CAR_POSITIONS) - 1) & free_before;
                next[move_ticks - 1] |= moves;
            }
        }

        uint8_t pending = 0;
        for (uint8_t i = 0; i < GAME_MAX_OBSTACLES; i++) {
            uint8_t obstacle_is_present = game->obstacles[i].active || i == candidate_slot;
            if (!obstacle_is_present || y[i] >= DISPLAY_HEIGHT) {
                continue;
            }

            y[i]++;
            if (y[i] < DISPLAY_HEIGHT) {
                pending = 1;
            }
        }

        uint8_t free_after = (uint8_t)~blocked_player_positions(
            game, y, candidate_slot, candidate_x);
        uint8_t any_reachable_position = 0;

        for (uint8_t wait = 0; wait < cooldown_count; wait++) {
            reachable[wait] = next[wait] & free_after;
            any_reachable_position |= reachable[wait];
        }

        if (!any_reachable_position) {
            return 0;
        }

        // A route is safe once every simulated car has left the screen.
        if (!pending) {
            return 1;
        }
    }

    return 0;
}

uint8_t spawn_choose_position(const game_t *game, uint8_t slot, uint8_t first_position, uint32_t now, uint8_t *position) {
    for (uint8_t attempt = 0; attempt < CAR_POSITIONS; attempt++) {
        uint8_t candidate_x = (first_position + attempt) % CAR_POSITIONS;

        if (has_safe_spawn_route(game, slot, candidate_x, now)) {
            *position = candidate_x;
            return 1;
        }
    }

    // No safe choice: let the caller retry on the next obstacle step.
    return 0;
}
