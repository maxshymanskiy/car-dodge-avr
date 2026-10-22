#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

// Edit these settings, then rebuild and upload the firmware.
#define GAME_STARTING_LIVES 3		// 1..3 lives, shared across all levels.
#define GAME_CARS_PER_LEVEL 10		// 1..255 cars to pass per level; more makes levels longer.
#define GAME_MAX_OBSTACLES 3		// 1..8 simultaneous cars, including those entering the road; doesn't make sense if GAME_SPAWN_GAP_ROWS is to high.
#define GAME_SPAWN_GAP_ROWS 18		// 1..255 row steps between spawns; lower allows denser traffic.

// 1..10 entries, in level order (0..9); X(value) is ms per falling pixel.
// Use 1..65535 ms; lower is faster. Keep SPAWN_COOLDOWN_COUNT(value) <= 32.
#define GAME_LEVELS(X) \
    X(100) \
    X(90) \
    X(80) \
    X(70) \
    X(60) \
    X(55) \
    X(50) \
    X(45) \
    X(40) \
    X(35)

#define INPUT_POLL_MS 5UL		    // 1..1000 ms between button reads; lower reduces input delay.
#define INPUT_DEBOUNCE_MS 25UL		// 1..1000 ms a button state must stay stable.
#define PLAYER_REPEAT_MS 120UL		// 1..1000 ms between moves while holding a direction.
#define SPAWN_REACTION_MS 150UL		// 0..1000 ms reaction allowance; higher makes spawning safer.

#define RENDER_FRAME_MS 20UL		// 1..1000 ms between screen updates; lower gives smoother animation.
#define MENU_BLINK_MS 400UL		    // 1..10000 ms per on/off phase of the play prompt.
#define HP_BLINK_PERIOD_MS 1000UL		// 1..10000 ms per full life-dot blink cycle.
#define HP_BLINK_OFF_MS 100UL		// 0..HP_BLINK_PERIOD_MS; time life dots stay off per cycle.
#define CRASH_FRAME_MS 80UL		    // 1..1000 ms per explosion frame.
#define CRASH_FRAMES 12		        // 1..255 frames; duration = CRASH_FRAMES * CRASH_FRAME_MS.

#define COUNT_LEVEL(interval) + 1
enum { GAME_LEVEL_COUNT = 0 GAME_LEVELS(COUNT_LEVEL) };		// Count entries in the level list.
#undef COUNT_LEVEL

// Derived planner values; tune the settings above instead.
#define SPAWN_MOVE_DELAY_MS (PLAYER_REPEAT_MS + INPUT_POLL_MS)		// Derived movement delay, including worst-case input polling.
#define SPAWN_MAX_DELAY_MS \
    ((SPAWN_REACTION_MS > SPAWN_MOVE_DELAY_MS) \
    ? SPAWN_REACTION_MS \
    : SPAWN_MOVE_DELAY_MS)		// Use the longer delay.
#define SPAWN_COOLDOWN_COUNT(interval) \
    ((SPAWN_MAX_DELAY_MS + (interval) - 1) / (interval) + 1)		// Round up, plus the ready state; keep <= 32 for AVR stack use.

#endif
