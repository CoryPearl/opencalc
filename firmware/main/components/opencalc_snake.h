// opencalc_snake.h
// Snake mini-app for the OpenCalc 320x240 ILI9341 board.
#pragma once

#include <stdbool.h>

void opencalc_snake_init(void);
void opencalc_snake_enter(void);
bool opencalc_snake_active(void);
void opencalc_snake_tick(void);
bool opencalc_snake_press_button_number(int number);
