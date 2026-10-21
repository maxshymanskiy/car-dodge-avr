/* Compile the actual game with only the hardware calls replaced. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#define main firmware_main
#include "../src/main.c"
#undef main

uint8_t PORTB, DDRB, PORTD, DDRD, PIND;
void spi_init(void) {}
uint8_t spi_transfer(uint8_t value) { return value; }
void millis_init(void) {}
uint32_t millis(void) { return 0; }

/* Independent path oracle: full position/cooldown states, not bit masks. */
static int8_t memo[33][6][3];
static uint8_t route_safe(uint8_t x, uint8_t depth) {
    for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) continue;
        int y = obstacles[i].y + depth;
        for (int py = 0; py < CAR_HEIGHT; py++) {
            for (int px = 0; px < CAR_WIDTH; px++) {
                if (!(car_row(py) & (1 << px))) continue;
                int ey = PLAYER_Y + py - y;
                int ex = x + px - obstacles[i].x;
                if (ey >= 0 && ey < CAR_HEIGHT && ex >= 0 && ex < CAR_WIDTH &&
                    (car_row(ey) & (1 << ex))) return 0;
            }
        }
    }
    return 1;
}

static int route(uint8_t depth, uint8_t x, uint8_t wait) {
    if (!route_safe(x, depth)) return 0;
    if (depth == 32) return 1;
    if (memo[depth][x][wait] >= 0) return memo[depth][x][wait];
    int result = 0;
    uint8_t movement_wait = (REPEAT_MS + POLL_MS + step_interval() - 1) / step_interval();
    for (int dx = -1; dx <= 1; dx++) {
        int nx = x + dx;
        if (nx < 0 || nx > 5 || (dx && wait)) continue;
        if (!route_safe(nx, depth)) continue;
        uint8_t next_wait = dx ? movement_wait - 1 : (wait ? wait - 1 : 0);
        if (route(depth + 1, nx, next_wait)) { result = 1; break; }
    }
    memo[depth][x][wait] = result;
    return result;
}

static void check_collisions(void) {
    for (int px = 0; px <= 5; px++) {
        player_x = px;
        for (int ex = 0; ex <= 5; ex++) {
            for (int y = SPAWN_Y; y <= 32; y++) {
                memset(obstacles, 0, sizeof(obstacles));
                obstacles[0] = (obstacle_t){ex, y, 1};
                assert(collision() == !route_safe(px, 0));
            }
        }
    }
}

static void check_states(void) {
    state = MENU;
    render(0);
    assert(framebuffer[1] == pgm_read_byte(&logo[1]));
    assert(framebuffer[25] == pgm_read_byte(&play_icon[0]));
    handle_input(0, 10);
    assert(state == MENU);
    handle_input(PAUSE, 100);
    assert(state == PLAYING && level == 1 && player_x == 2 && spawned == 1);
    handle_input(PAUSE, 200);
    assert(state == PAUSED);
    int8_t y = obstacles[2].y;
    update_game(10000);
    assert(obstacles[2].y == y);
    handle_input(PAUSE, 10200);
    assert(state == PLAYING && last_step == 10100);
    update_game(10249);
    assert(obstacles[2].y == y);
    update_game(10250);
    assert(obstacles[2].y == y + 1);

    /* Millis rollover during pause and crash. */
    reset_game(UINT32_MAX - 200);
    toggle_pause(UINT32_MAX - 100);
    toggle_pause(99);
    assert(state == PLAYING && last_step == UINT32_MAX - 200 + (uint32_t)200);
    start_crash(UINT32_MAX - 100);
    handle_input(LEFT, 0);
    assert(state == CRASH_ANIMATION);
    update_game((uint32_t)(UINT32_MAX - 100 + CRASH_FRAME_MS * CRASH_FRAMES - 1));
    assert(state == CRASH_ANIMATION);
    update_game((uint32_t)(UINT32_MAX - 100 + CRASH_FRAME_MS * CRASH_FRAMES));
    assert(state == GAME_OVER);
    handle_input(0, 2000);
    assert(state == GAME_OVER);
    handle_input(RIGHT, 2005);
    assert(state == PLAYING && level == 1 && player_x == 2);

    /* HUD never contains entering car pixels; 10 fits in one module. */
    memset(framebuffer, 0, sizeof(framebuffer));
    draw_car(2, SPAWN_Y);
    for (int row = 0; row < PLAYFIELD_TOP; row++) assert(framebuffer[row] == 0);
    level = 10;
    render(2010);
    uint8_t hud[8];
    memcpy(hud, framebuffer, 8);
    for (int row = 1; row <= 5; row++) assert(hud[row] != 0);
    for (int frame = 0; frame < CRASH_FRAMES; frame++) {
        state = CRASH_ANIMATION;
        state_since = 3000;
        render(3000 + frame * CRASH_FRAME_MS);
        assert(memcmp(hud, framebuffer, 8) == 0);
    }
    state = VICTORY;
    render(5000);
    assert(framebuffer[26] == 0x55 && framebuffer[27] == 0xAA);
    handle_input(LEFT, 5010);
    assert(state == PLAYING && level == 1);

    /* Existing module transform must map all 256 pixels uniquely. */
    uint8_t seen[4][8] = {{0}};
    for (int yy = 0; yy < 32; yy++) for (int xx = 0; xx < 8; xx++) {
        uint8_t module, row, bit;
        display_map(xx, yy, &module, &row, &bit);
        assert(module < 4 && row < 8 && bit < 8);
        assert(!(seen[module][row] & (1 << bit)));
        seen[module][row] |= 1 << bit;
    }
}

static void check_buttons(void) {
    stable_buttons = 0;
    assert(button_events(LEFT, 100) == 0);
    assert(button_events(0, 110) == 0);
    assert(button_events(LEFT, 115) == 0);
    assert(button_events(LEFT, 139) == 0);
    assert(button_events(LEFT, 140) == LEFT);
    assert(button_events(LEFT, 1000) == 0);
    assert(button_events(0, 1005) == 0);
    assert(button_events(0, 1030) == 0 && stable_buttons == 0);
    assert(button_events(RIGHT, UINT32_MAX - 10) == 0);
    assert(button_events(RIGHT, 14) == RIGHT);

    reset_game(2000);
    memset(obstacles, 0, sizeof(obstacles));
    player_x = 0;
    stable_buttons = LEFT;
    handle_input(LEFT, 2005);
    assert(player_x == 0);
    player_x = 5;
    stable_buttons = RIGHT;
    handle_input(RIGHT, 2010);
    assert(player_x == 5);
    player_x = 2;
    stable_buttons = LEFT | RIGHT;
    handle_input(LEFT | RIGHT, 2015);
    assert(player_x == 2);
    stable_buttons = RIGHT;
    handle_input(RIGHT, 2020);
    assert(player_x == 3);
    handle_input(0, 2139);
    assert(player_x == 3);
    handle_input(0, 2140);
    assert(player_x == 4);
    obstacles[0] = (obstacle_t){5, PLAYER_Y, 1};
    handle_input(RIGHT, 2145);
    assert(state == CRASH_ANIMATION);
}

static void check_planner(void) {
    for (level = 1; level <= LEVEL_COUNT; level++) {
        for (int seed = 1; seed <= 100; seed++) {
            random_state = seed;
            player_x = random_position();
            for (int i = 0; i < MAX_OBSTACLES; i++) {
                obstacles[i] = (obstacle_t){random_position(), 3 + random_position() * 5, 1};
            }
            last_move = 1000;
            memset(memo, -1, sizeof(memo));
            uint8_t initial_wait = (REACTION_MS + step_interval() - 1) / step_interval();
            assert(safe_spawn(1000) == route(0, player_x, initial_wait));
        }
    }
}

static void check_campaigns(void) {
    for (int seed = 1; seed <= 50; seed++) {
        uint32_t now = seed * 100;
        random_state = seed;
        reset_game(now);
        int ticks = 0, completed = 0, max_active = 0;
        while (state == PLAYING && ticks++ < 10000) {
            now = last_step + step_interval();
            uint32_t elapsed = now - last_move;
            uint8_t wait = elapsed >= REPEAT_MS + POLL_MS ? 0 :
                (REPEAT_MS + POLL_MS - elapsed + step_interval() - 1) / step_interval();
            memset(memo, -1, sizeof(memo));
            assert(route(0, player_x, wait));
            /* Prefer staying still; move only when needed for a surviving route. */
            int choices[3] = {0, -1, 1}, selected = 99;
            for (int j = 0; j < 3; j++) {
                int dx = choices[j], nx = player_x + dx;
                if (nx < 0 || nx > 5 || (dx && wait) || !route_safe(nx, 0)) continue;
                uint8_t next_wait = dx ?
                    (REPEAT_MS + POLL_MS + step_interval() - 1) / step_interval() - 1 :
                    (wait ? wait - 1 : 0);
                if (route(1, nx, next_wait)) { selected = dx; break; }
            }
            assert(selected != 99);
            if (selected) {
                stable_buttons = selected < 0 ? LEFT : RIGHT;
                handle_input(stable_buttons, now);
            }
            uint8_t old_level = level, old_passed = passed;
            update_game(now);
            assert(state == PLAYING || state == VICTORY);
            if (level != old_level || state == VICTORY) completed += CARS_PER_LEVEL - old_passed;
            else completed += passed - old_passed;
            assert(spawned <= CARS_PER_LEVEL && passed <= spawned);
            int active = 0;
            for (int i = 0; i < MAX_OBSTACLES; i++) active += obstacles[i].active;
            assert(active <= traffic_limit());
            if (active > max_active) max_active = active;
        }
        assert(state == VICTORY && level == 10 && completed == 100);
        assert(max_active >= 2);
    }
}

int main(void) {
    check_collisions();
    check_states();
    check_buttons();
    check_planner();
    check_campaigns();
    puts("PASS: collisions, states, rollover, HUD, mapping, 1000 planner cases, 50 complete campaigns");
    return 0;
}
