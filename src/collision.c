#include "game.h"
#include "collision.h"

uint8_t player_overlaps_obstacle(uint8_t x, uint8_t obstacle_x, int8_t obstacle_y) {
    for (uint8_t row = 0; row < CAR_HEIGHT; row++) {
        // Align sprite rows on screen; only lit pixels can collide.
        int8_t other_row = PLAYER_Y + row - obstacle_y;
        if (other_row >= 0 && other_row < CAR_HEIGHT &&
            ((car_row(row) << x) & (car_row(other_row) << obstacle_x))) {
            return 1;
        }
    }
    return 0;
}

uint8_t player_dodged_obstacle(uint8_t previous_x, uint8_t player_x, uint8_t obstacle_x, int8_t obstacle_y) {
    return previous_x != player_x &&
        !player_overlaps_obstacle(previous_x, obstacle_x, obstacle_y) &&
        player_overlaps_obstacle(previous_x, obstacle_x, obstacle_y + 1) &&
        !player_overlaps_obstacle(player_x, obstacle_x, obstacle_y) &&
        !player_overlaps_obstacle(player_x, obstacle_x, obstacle_y + 1);
}
