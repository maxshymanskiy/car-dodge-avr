# Car Dodge

A simple C game for Arduino Uno (ATmega328P, 16 MHz) and a vertical 8x32 display with four MAX7219 drivers. No Arduino Framework.

## Wiring

| Uno | Connection |
| --- | --- |
| D10 | CS/LOAD of all MAX7219 drivers |
| D11 | DIN of the first driver |
| D13 | CLK of all drivers |
| D3 | Left button |
| D2 | Right button |
| D4 | Pause button |

Connect each button between its pin and GND. Internal pull-ups are enabled. All modules share 5V and GND; connect each driver's DOUT to the next driver's DIN.

## Controls

On power-up, a vertical CAR logo with a car is displayed. Press any button (D2, D3 or D4) to start; this first press only starts the game.

Move the bottom car left or right; hold a button to keep moving. One obstacle car enters from the top at a pseudorandom horizontal position and moves down. A new car spawns after the previous one leaves the screen.

D4 toggles pause. A collision freezes the game; press D2 or D3 again to restart.

## Configuration

All game code and settings are in `src/main.c`. Existing `lib/spi` and `lib/millis` libraries are reused.

The car and startup logo bitmaps use HEX notation. Each sprite byte represents one row; bit 0 is the leftmost pixel. The car bitmap is `{0x02, 0x07, 0x02, 0x07, 0x05}`.

- `STEP_MS = 140`: obstacle movement interval per pixel.
- `REPEAT_MS = 120`: held-button repeat interval.
- `DEBOUNCE_MS = 25`: button debounce time.
- `BRIGHTNESS = 5`: display brightness, 0 to 15.
- `module_order = {3, 2, 1, 0}`: module order from top to bottom.
- `module_rotation = {1, 1, 1, 1}`: clockwise quarter turns per module.
- `FLIP_SCREEN = 0`: no additional screen rotation.

## Build and run

From the project directory:

```sh
pio run -e uno
```

In SimulIDE, use Arduino Uno at 16 MHz and a vertically rotated Max72xx Matrix with Size=4. Load `.pio/build/uno/firmware.hex`.

To upload to a physical Uno, replace COM5 with your port:

```sh
pio run -e uno -t upload --upload-port COM5
```
