#pragma once

#include <stdbool.h>

void opencalc_mario_init(void);
bool opencalc_mario_rom_available(void);
void opencalc_mario_enter(void);
bool opencalc_mario_active(void);
void opencalc_mario_tick(void);
bool opencalc_mario_press_button_number(int number);
