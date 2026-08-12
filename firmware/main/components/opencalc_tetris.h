// opencalc_tetris.h
// Tetris mini-app for the OpenCalc 320x240 ILI9341 board.
#pragma once

#include <stdbool.h>

// Call once at boot (allocates nothing, just resets internal state).
void opencalc_tetris_init(void);

// Enter the Tetris app: resets the board and starts a new game.
// Call this from wherever you launch apps (home screen icon / menu entry),
// the same way s_doom_active gets set to true when Doom is launched.
void opencalc_tetris_enter(void);

// True while Tetris owns the screen. Mirrors opencalc_ui_doom_active().
bool opencalc_tetris_active(void);

// Advance the game + repaint. Call this every loop iteration while
// opencalc_tetris_active() is true, the same way opencalc_ui_tick_doom()
// is called while s_doom_active is true.
void opencalc_tetris_tick(void);

// Route a calculator button number (1-50, same numbering as
// opencalc_ui_press_button_number / the serial button task in main.c)
// into the game. Returns true if Tetris handled it (i.e. it should NOT
// also be dispatched to the normal calculator key handler).
bool opencalc_tetris_press_button_number(int number);
