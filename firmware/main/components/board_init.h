#pragma once

/**
 * board_init.h
 *
 * Initializes the ILI9341 SPI LCD.
 */
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_KEYPAD_ROWS 10
#define BOARD_KEYPAD_COLS 5

typedef enum {
    BOARD_KEY_ROLE_NONE = 0,
    BOARD_KEY_ROLE_HOME,
    BOARD_KEY_ROLE_POWER,
} board_key_role_t;

typedef struct {
    const char *normal;
    const char *second;
    const char *alpha;
    board_key_role_t role;
} board_key_t;

void board_init(void);
void board_enter_deep_sleep(void);
void board_draw_text_screen(const char *text);
void board_display_lock(void);
void board_display_unlock(void);
void board_draw_rgb888_frame_320x200(const uint32_t *pixels);
void board_draw_rgb888_frame_320x240(const uint32_t *pixels);
void board_draw_rgb565_frame_320x200(const uint16_t *pixels);
void board_draw_indexed8_frame_256x240(const uint8_t *pixels, const uint32_t *palette_rgb888);
void board_set_backlight_brightness(int percent);
int board_get_backlight_brightness(void);
bool board_battery_get_percent(int *percent);
bool board_battery_get_voltage_mv(int *millivolts);
bool board_battery_is_charging(void);
void board_set_event_task(TaskHandle_t task);
const board_key_t *board_keypad_key_at(int row, int col);
bool board_keypad_take_interrupt(void);
bool board_keypad_scan(int *row, int *col);
bool board_keypad_scan_matrix(bool pressed[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS]);
bool board_keypad_read_raw_levels(char rows[BOARD_KEYPAD_ROWS + 1], char cols[BOARD_KEYPAD_COLS + 1]);
bool board_power_button_pressed(void);
bool board_touch_take_interrupt(void);
bool board_touch_scan(int *x, int *y);

#ifdef __cplusplus
}
#endif
