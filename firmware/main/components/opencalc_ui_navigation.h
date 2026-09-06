#pragma once

#include "board_init.h"

#include <stdbool.h>
#include <stdint.h>

typedef void (*opencalc_navigation_key_fn)(int row, int col, void *context);

void opencalc_navigation_reset_all(void);
void opencalc_navigation_reset_ui(void);
void opencalc_navigation_reset_game(void);
bool opencalc_navigation_ui_key_held(void);
void opencalc_navigation_process_game(
    const bool matrix[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS],
    opencalc_navigation_key_fn press, void *context);
void opencalc_navigation_process_ui(
    const bool matrix[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS],
    bool modifier_active, uint32_t now_ticks,
    uint32_t initial_delay_ticks, uint32_t repeat_interval_ticks,
    opencalc_navigation_key_fn press, void *context);
