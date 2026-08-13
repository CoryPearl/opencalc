#pragma once

#include <stdbool.h>

bool opencalc_doom_wad_available(void);
bool opencalc_doom_start(void);
void opencalc_doom_tick(void);
bool opencalc_doom_press_button_number(int number);
long opencalc_doom_score(void);
