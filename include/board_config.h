#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// Buttons are active-low on port D. Display LOAD is on port B.
#define LATCH_PIN PB2		    // Port B pin; Uno D10, MAX7219 LOAD/CS.
#define RIGHT_PIN PD2		    // Port D pin; Uno D2, active-low right button.
#define LEFT_PIN PD3		    // Port D pin; Uno D3, active-low left button.
#define PAUSE_PIN PD4		    // Port D pin; Uno D4, active-low stop/resume button.
#define DISPLAY_BRIGHTNESS 3	// 0..15 MAX7219 intensity; higher is brighter.
#define FLIP_SCREEN 1		    // 0 = normal; 1 = rotate the whole logical screen 180 degrees.

// Logical blocks top-to-bottom; chain index 0 is nearest the MCU.
#define DISPLAY_MODULE_ORDER {3, 2, 1, 0}		// Top-to-bottom chain indices; use 0..3 once each, 0 nearest MCU.
#define DISPLAY_MODULE_ROTATION {1, 1, 1, 1}	// Per logical block: 0..3 clockwise quarter turns.
#define DISPLAY_MODULE_MIRROR {0, 0, 0, 0}		// Per logical block: 1 mirrors horizontally before rotation.

// Fixed geometry of this assembly, not difficulty settings.
#define DISPLAY_WIDTH 8		// Logical screen columns; keep 8 for this hardware.
#define MODULE_SIZE 8		// Rows/columns per MAX7219 matrix; keep 8.
#define MODULE_COUNT 4		// Vertical matrix count; keep 4 for the current layouts.
#define DISPLAY_HEIGHT (MODULE_SIZE * MODULE_COUNT)		// Derived logical screen height: 32 rows.

#endif
