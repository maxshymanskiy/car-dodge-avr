#include <string.h>
#include "renderer.h"
#include "display.h"
#include "collision.h"

#define LEVEL_DIGIT_X 2
#define LEVEL_DIGIT_Y 1
#define LIFE_DOT_X 1
#define LIFE_DOT_Y 1
#define LIFE_DOT_SPACING 2
#define PLAY_ICON_Y (DISPLAY_HEIGHT - MODULE_SIZE + 1)
#define TROPHY_X 1
#define TROPHY_Y 12

static uint8_t framebuffer[DISPLAY_HEIGHT];

static void set_pixel(int8_t x, int8_t y) {
    if (x >= 0 && x < DISPLAY_WIDTH && y >= 0 && y < DISPLAY_HEIGHT) {
        framebuffer[y] |= (uint8_t)(1 << x);
    }
}

static void draw_car(uint8_t x, int8_t y) {
    for (uint8_t row = 0; row < CAR_HEIGHT; row++) {
        for (uint8_t col = 0; col < CAR_WIDTH; col++) {
            if (car_row(row) & (1U << col)) {
                if (y + row >= PLAYFIELD_TOP) {
                    set_pixel(x + col, y + row);
                }
            }
        }
    }
}

static void draw_icon(const uint8_t *icon, uint8_t height, int8_t x, int8_t y) {
    for (uint8_t row = 0; row < height; row++) {
        uint8_t bits = pgm_read_byte(&icon[row]);
        for (uint8_t col = 0; col < 5; col++) {
            if (bits & (1U << col)) {
                set_pixel(x + col, y + row);
            }
        }
    }
}

static void draw_digit(uint8_t digit, uint8_t x) {
    for (uint8_t row = 0; row < 5; row++) {
        uint8_t bits = pgm_read_byte(&digits[digit][row]);
        for (uint8_t col = 0; col < 3; col++) {
            if (bits & (1U << (2 - col))) {
                set_pixel(DISPLAY_WIDTH - 1 - x - col, LEVEL_DIGIT_Y + row);
            }
        }
    }
}

static void draw_status(const game_t *game, uint32_t now) {
    draw_digit(game->level, LEVEL_DIGIT_X);

    // Life dots sit beside the digit, briefly blinking off once per second.
    if (now % HP_BLINK_PERIOD_MS < HP_BLINK_PERIOD_MS - HP_BLINK_OFF_MS) {
        for (uint8_t i = 0; i < game->lives; i++) {
            set_pixel(LIFE_DOT_X, LIFE_DOT_Y + i * LIFE_DOT_SPACING);
        }
    }

    // Row 6 is blank; row 7 is the divider. Traffic starts in the next module.
    framebuffer[PLAYFIELD_TOP - 1] = 0xFF;
}

static void draw_play_prompt(uint32_t now) {
    if ((now / MENU_BLINK_MS) % 2 != 0) {
        return;
    }

    for (uint8_t row = 0; row < sizeof(play_icon); row++) {
        framebuffer[PLAY_ICON_Y + row] |= pgm_read_byte(&play_icon[row]);
    }
}

static void draw_game_scene(const game_t *game) {
    draw_car(game->player_x, PLAYER_Y);

    for (uint8_t i = 0; i < GAME_MAX_OBSTACLES; i++) {
        if (!game->obstacles[i].active) {
            continue;
        }

        draw_car(game->obstacles[i].x, game->obstacles[i].y);
    }
}

static void draw_crash_scene(const game_t *game) {
    for (uint8_t i = 0; i < GAME_MAX_OBSTACLES; i++) {
        if (!game->obstacles[i].active) {
            continue;
        }

        if (player_overlaps_obstacle(game->player_x, game->obstacles[i].x, game->obstacles[i].y)) {
            continue;
        }

        draw_car(game->obstacles[i].x, game->obstacles[i].y);
    }

    draw_icon(explosion, sizeof(explosion), (int8_t)game->player_x - 1, PLAYER_Y);
}

static void draw_crash(const game_t *game, uint32_t now) {
    uint8_t frame = (uint32_t)(now - game->crash_started_at) / CRASH_FRAME_MS;
    draw_crash_scene(game);

    if (frame >= 8 && frame % 2 != 0) {
        return;
    }

    int8_t center_x = game->player_x + 1;
    int8_t center_y = PLAYER_Y + 2;
    uint8_t radius = frame / 2 + 1;

    if (radius > 4) {
        radius = 4;
    }

    for (uint8_t offset = 1; offset <= radius; offset++) {
        set_pixel(center_x - offset, center_y - offset);
        set_pixel(center_x + offset, center_y - offset);
        set_pixel(center_x - offset, center_y + offset);
        set_pixel(center_x + offset, center_y + offset);
    }
}

static void draw_victory(void) {
    // Full finish flags in modules 1 and 4; trophy across modules 2 and 3.
    for (uint8_t row = 0; row < MODULE_SIZE; row++) {
        uint8_t pattern = (row % 2) ? 0xAA : 0x55;
        framebuffer[row] = pattern;
        framebuffer[DISPLAY_HEIGHT - MODULE_SIZE + row] = pattern;
    }

    draw_icon(trophy, sizeof(trophy), TROPHY_X, TROPHY_Y);
}

void renderer_draw(const game_t *game, uint32_t now) {
    memset(framebuffer, 0, sizeof(framebuffer));
    if (game->state == MENU) {
        memcpy_P(framebuffer, logo, sizeof(framebuffer));
        draw_play_prompt(now);
    } else if (game->state == VICTORY) {
        draw_victory();
    } else {
        draw_status(game, now);

        switch (game->state) {
            case PLAYING:
            case PAUSED:
                draw_game_scene(game);
                break;

            case CRASH_ANIMATION:
                draw_crash(game, now);
                break;

            case GAME_OVER:
                draw_play_prompt(now);
                break;

            case MENU:
            case VICTORY:
                break;
        }
    }
    display_flush(framebuffer);
}
