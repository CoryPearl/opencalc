#pragma once

#include <stdbool.h>

void opencalc_ui_init(void);
void opencalc_ui_start_worker(void);
bool opencalc_ui_prepare_script_worker(void);
void opencalc_ui_draw(void);
void opencalc_ui_tick(void);
void opencalc_ui_handle_keypad_interrupt(void);
void opencalc_ui_handle_touch_interrupt(void);
bool opencalc_ui_press_button_number(int number);
bool opencalc_ui_queue_button_number(int number);
void opencalc_ui_handle_serial_buttons(void);
bool opencalc_ui_doom_active(void);
void opencalc_ui_tick_doom(void);
bool opencalc_ui_matrix_boot_stress_test(void);
