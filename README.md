# Car Dodge — Speed only

AVR C for Arduino Uno (ATmega328P, 16 MHz) and four cascaded MAX7219 matrices, mounted vertically as 8×32. No Arduino Framework.

## Play

The startup CAR logo stays visible until any button is pressed. The first press only starts the game.

The top 8×8 module shows the current level; the lower 8×24 is the road. Move the bottom car left/right, or hold a direction to repeat. D4 pauses/resumes. Cars use all eight display columns; there is no score display.

Every game starts at level 0. Pass ten cars to advance, and finish level 9 to win. Levels cannot be selected. Each game starts with three lives, shared across all levels. Each collision costs exactly one life and plays the car explosion for 0.96 seconds. With lives remaining, the current level restarts automatically with its progress cleared and the player centered. After the third crash, a blinking play prompt waits for a fresh button press to start again at level 0 with three lives. Winning fills the top and bottom 8x8 modules with finish flags and centers the trophy across modules 2 and 3. Only a fresh press of the D4 stop/resume button starts a new game after victory.

The crash animation is inspired by classic Brick Game racing, rather than an exact recreation of one handheld model. Reference: [Brick Game remake](https://github.com/vitalibo/Brick-Game-9999-in-1).

## Difficulty

**Speed only.** Movement intervals are milliseconds per falling pixel. Gap is the minimum number of row steps between spawns; the active-car limit includes cars entering above the visible road.

| Level | Movement (ms) | Gap (rows) | Active-car limit |
| --- | --- | --- | --- |
| 0 | 100 | 18 | 3 |
| 1 | 90 | 18 | 3 |
| 2 | 80 | 18 | 3 |
| 3 | 70 | 18 | 3 |
| 4 | 60 | 18 | 3 |
| 5 | 55 | 18 | 3 |
| 6 | 50 | 18 | 3 |
| 7 | 45 | 18 | 3 |
| 8 | 40 | 18 | 3 |
| 9 | 35 | 18 | 3 |

On an empty road, spawning first tries the edge opposite the player; this candidate is checked for safety too. With traffic already present, new cars use random horizontal positions. Before accepting one, a small path search checks pixel collisions, movement repeat time and a reaction allowance against all active cars. If no safe route exists, spawning waits for another row step. A safe route exists when a car is spawned; a player can still choose a losing move.

## Wiring

| Uno | Connection |
| --- | --- |
| D10 / PB2 | LOAD/CS of all MAX7219 drivers |
| D11 / PB3 | DIN of first driver |
| D13 / PB5 | CLK of all drivers |
| D3 / PD3 | Left button |
| D2 / PD2 | Right button |
| D4 / PD4 | Pause button |

Each button connects its pin to GND; internal pull-ups are enabled. No 74HC165 is used. Drivers share 5V/GND; connect each DOUT to the next DIN. Use a supply sized for the matrices and a common ground.

## Code and settings

`src/main.c` only initializes modules and schedules input polling, game updates, and rendering. Existing SPI and Timer0/millis libraries are reused unchanged. Timing is nonblocking; only the display LOAD pulse uses microsecond delays.

| File | Responsibility |
| --- | --- |
| `include/game_config.h` | Difficulty, level intervals, input and animation timing; documented ranges |
| `include/board_config.h` | Pins, brightness, module orientation and fixed display geometry |
| `src/game.c` | Owns `game_t`; movement, state transitions, lives, progression and obstacle activation |
| `src/spawn.c` | Read-only candidate placement and safe-route search |
| `src/collision.c` | Shared pixel-accurate collision geometry |
| `src/input.c` | Button hardware, debouncing and held/newly pressed button snapshots |
| `src/renderer.c` | Framebuffer, status, menus and animations; reads game state |
| `src/display.c` | MAX7219 initialization, physical mapping and SPI frame transfer |
| `src/assets.c` | Flash-resident sprites and glyphs |

### Tuning the game

Edit `include/game_config.h`, then rebuild and upload. Defaults preserve the existing game.

| Setting | Default | Effect |
| --- | --- | --- |
| `GAME_LEVELS(X)` | 100, 90, 80, 70, 60, 55, 50, 45, 40, 35 ms | One `X(interval)` per level; smaller intervals make obstacles faster |
| `GAME_STARTING_LIVES` | 3 | Lives shared across the game; status supports 1..3 |
| `GAME_CARS_PER_LEVEL` | 10 | More cars make each level longer |
| `GAME_MAX_OBSTACLES` | 3 | Maximum simultaneous obstacles, including those entering the road |
| `GAME_SPAWN_GAP_ROWS` | 18 | Smaller gaps permit denser traffic; safe-route search still applies |
| `PLAYER_REPEAT_MS` | 120 | Smaller values move the player faster while holding a direction |
| `SPAWN_REACTION_MS` | 150 | Larger allowances make spawning more conservative |
| `INPUT_POLL_MS` / `INPUT_DEBOUNCE_MS` | 5 / 25 | Input sampling and contact debounce timing |
| `CRASH_FRAME_MS` / `CRASH_FRAMES` | 80 / 12 | Explosion frame timing and duration |
| `HP_BLINK_PERIOD_MS` / `HP_BLINK_OFF_MS` | 1000 / 100 | Life-dot blink timing |

For a faster, denser game, try level intervals `80, 75, 70, 65, 60, 55, 50, 45, 40, 30` and a gap of `14`. For an easier game, increase the intervals and gap. The safety planner can delay spawns, so a smaller gap does not guarantee that every requested spawn occurs immediately.

Level count is derived from `GAME_LEVELS(X)` (1..10 entries). Planner arrays are sized from the selected level's interval and movement/reaction delays, and should stay within 32 cooldown states to keep stack use small. Supported ranges are documented beside the settings and are not enforced by static assertions. Even empty-road candidates go through the safety check.

Sprites use HEX row masks; bit 0 is the left logical pixel. The car is `{0x02, 0x07, 0x02, 0x07, 0x05}`. Bitmaps live in Flash (`PROGMEM`); the framebuffer uses 32 bytes of SRAM. Collision tests compare lit sprite pixels.

The tested device mapping is preserved: `module_order = {3,2,1,0}`, `module_rotation = {1,1,1,1}`, `module_mirror = {0,0,0,0}`, `FLIP_SCREEN = 1`. Cars move toward increasing y; the player occupies rows 27–31. Text is mirrored like the existing logo to read correctly on this assembly. To check orientation, confirm CAR reads correctly, the level appears in the top block and the player is in the bottom block. Adjust only the mapping arrays if your module assembly differs.
