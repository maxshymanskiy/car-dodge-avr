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

On an empty road, the next car appears at the edge opposite the player so spawning cannot stall. With traffic already present, new cars use random horizontal positions. Before accepting one, a small path search checks pixel collisions, movement repeat time and a reaction allowance against all active cars. If no safe route exists, spawning waits for another row step. A safe route exists when a car is spawned; a player can still choose a losing move.

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

Game code/settings are in `src/main.c`. Existing SPI and Timer0/millis libraries are reused unchanged. Updates, input and drawing are separate; game timing is nonblocking. Only the existing LOAD pulse uses microsecond delays.

- `level_speed`, `level_gap`, `level_traffic`: the table above.
- `CARS_PER_LEVEL = 10`, `LEVEL_COUNT = 10`: campaign length (extend tables before changing the level count).
- `REPEAT_MS = 120`, `DEBOUNCE_MS = 25`, `REACTION_MS = 150`.
- `CRASH_FRAME_MS = 80`, `CRASH_FRAMES = 12`: 0.96-second explosion sequence.
- `BRIGHTNESS = 3`, range 0–15.

The planner has six cooldown states, sized for these speed/repeat/reaction settings. Keep the rounded movement/reaction delays below six row steps when tuning them.

Sprites use HEX row masks; bit 0 is the left logical pixel. The car is `{0x02, 0x07, 0x02, 0x07, 0x05}`. Bitmaps live in Flash (`PROGMEM`); the framebuffer uses 32 bytes of SRAM. Collision tests compare lit sprite pixels.

The tested device mapping is preserved: `module_order = {3,2,1,0}`, `module_rotation = {1,1,1,1}`, `module_mirror = {0,0,0,0}`, `FLIP_SCREEN = 0`. Cars move toward increasing y; the player occupies rows 27–31. Text is mirrored like the existing logo to read correctly on this assembly. To check orientation, confirm CAR reads correctly, the level appears in the top block and the player is in the bottom block. Adjust only the mapping arrays if your module assembly differs.
