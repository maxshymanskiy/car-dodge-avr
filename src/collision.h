#ifndef COLLISION_H
#define COLLISION_H

#include <stdint.h>

// Fixed player Y; compares lit pixels and ignores empty sprite corners.
uint8_t player_overlaps_obstacle(uint8_t player_x, uint8_t obstacle_x, int8_t obstacle_y);

#endif
