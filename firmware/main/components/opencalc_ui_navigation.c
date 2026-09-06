#include "opencalc_ui_navigation.h"

#include <string.h>

static bool s_game_previous[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
static bool s_ui_previous[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
static bool s_repeat_enabled[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
static uint32_t s_repeat_at[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];

static bool repeatable_key(int row, int col)
{
    return (row == 1 && (col == 3 || col == 4)) ||
           (row == 2 && (col == 3 || col == 4));
}

void opencalc_navigation_reset_ui(void)
{
    memset(s_ui_previous, 0, sizeof(s_ui_previous));
    memset(s_repeat_enabled, 0, sizeof(s_repeat_enabled));
    memset(s_repeat_at, 0, sizeof(s_repeat_at));
}

void opencalc_navigation_reset_game(void)
{
    memset(s_game_previous, 0, sizeof(s_game_previous));
}

void opencalc_navigation_reset_all(void)
{
    opencalc_navigation_reset_ui();
    opencalc_navigation_reset_game();
}

bool opencalc_navigation_ui_key_held(void)
{
    for (int row = 0; row < BOARD_KEYPAD_ROWS; row++) {
        for (int col = 0; col < BOARD_KEYPAD_COLS; col++) {
            if (s_ui_previous[row][col]) return true;
        }
    }
    return false;
}

void opencalc_navigation_process_game(
    const bool matrix[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS],
    opencalc_navigation_key_fn press, void *context)
{
    for (int row = 0; row < BOARD_KEYPAD_ROWS; row++) {
        for (int col = 0; col < BOARD_KEYPAD_COLS; col++) {
            if (matrix[row][col] && !s_game_previous[row][col] && press != NULL) {
                press(row, col, context);
            }
            s_game_previous[row][col] = matrix[row][col];
        }
    }
}

void opencalc_navigation_process_ui(
    const bool matrix[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS],
    bool modifier_active, uint32_t now_ticks,
    uint32_t initial_delay_ticks, uint32_t repeat_interval_ticks,
    opencalc_navigation_key_fn press, void *context)
{
    for (int row = 0; row < BOARD_KEYPAD_ROWS; row++) {
        for (int col = 0; col < BOARD_KEYPAD_COLS; col++) {
            if (matrix[row][col] && !s_ui_previous[row][col]) {
                s_repeat_enabled[row][col] = repeatable_key(row, col) && !modifier_active;
                s_repeat_at[row][col] = now_ticks + initial_delay_ticks;
                if (press != NULL) press(row, col, context);
            } else if (matrix[row][col] && s_repeat_enabled[row][col]) {
                if (modifier_active) {
                    s_repeat_enabled[row][col] = false;
                } else if ((int32_t)(now_ticks - s_repeat_at[row][col]) >= 0) {
                    if (press != NULL) press(row, col, context);
                    s_repeat_at[row][col] = now_ticks + repeat_interval_ticks;
                }
            } else if (!matrix[row][col]) {
                s_repeat_enabled[row][col] = false;
            }
            s_ui_previous[row][col] = matrix[row][col];
        }
    }
}
