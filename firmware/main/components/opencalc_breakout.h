// opencalc_breakout.h
// Breakout mini-app for the OpenCalc 320x240 ILI9341 board.
#pragma once

#include <stdbool.h>

void opencalc_breakout_init(void);
void opencalc_breakout_enter(void);
bool opencalc_breakout_active(void);
void opencalc_breakout_tick(void);
bool opencalc_breakout_press_button_number(int number);
