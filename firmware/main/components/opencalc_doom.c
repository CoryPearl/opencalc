#include "opencalc_doom.h"

#include "board_init.h"
#include "doomgeneric/d_main.h"
#include "doomgeneric/doomgeneric.h"
#include "doomgeneric/doomkeys.h"
#include "doomgeneric/doomstat.h"
#include "doomgeneric/m_controls.h"
#include "doomgeneric/m_menu.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

static const char *TAG = "doom";

typedef struct {
    int pressed;
    unsigned char key;
} doom_key_event_t;

static doom_key_event_t s_key_events[32];
static int s_key_event_read = 0;
static int s_key_event_write = 0;
static bool s_key_down[256];
static bool s_window_was_pressed = false;
static unsigned char s_window_weapon_key = 0;
static int s_serial_pressed_key = 0;
static int64_t s_serial_release_time_us = 0;
static bool s_doom_started = false;
static EXT_RAM_BSS_ATTR uint32_t s_doom_frame_320x240[320 * 240];
static char *s_doom_argv[] = {
    "opencalc-doom",
    "-iwad",
    "/data/doom1.wad",
    "-nosound",
    "-nomusic",
};
static const int s_doom_argc = sizeof(s_doom_argv) / sizeof(s_doom_argv[0]);
enum { OPENCALC_DOOM_NEXT_WEAPON_KEY = 'n' };

static void doom_clear_input_state(void)
{
    memset(s_key_down, 0, sizeof(s_key_down));
    s_key_event_read = 0;
    s_key_event_write = 0;
    s_window_was_pressed = false;
    s_window_weapon_key = 0;
    s_serial_pressed_key = 0;
    s_serial_release_time_us = 0;
}

static void doom_restart_to_main_menu(void)
{
    doom_clear_input_state();
    key_nextweapon = OPENCALC_DOOM_NEXT_WEAPON_KEY;
    D_StartTitle();
    menuactive = false;
    M_StartControlPanel();
}

static bool doom_key_queue_empty(void)
{
    return s_key_event_read == s_key_event_write;
}

static bool doom_key_queue_full(void)
{
    return ((s_key_event_write + 1) % (int)(sizeof(s_key_events) / sizeof(s_key_events[0]))) == s_key_event_read;
}

static void doom_key_push(int pressed, unsigned char key)
{
    if (key == 0 || doom_key_queue_full()) {
        return;
    }

    s_key_events[s_key_event_write].pressed = pressed;
    s_key_events[s_key_event_write].key = key;
    s_key_event_write = (s_key_event_write + 1) % (int)(sizeof(s_key_events) / sizeof(s_key_events[0]));
}

static unsigned char doom_next_weapon_key(void)
{
    return (unsigned char)(key_nextweapon != 0 ? key_nextweapon : OPENCALC_DOOM_NEXT_WEAPON_KEY);
}

static unsigned char doom_key_for_matrix_position(int row, int col)
{
    // Doom-only controls: Zoom becomes Use/Open and 1-7 select weapons.
    if (row == 0 && col == 2) {
        return KEY_USE;
    }
    if (row == 0 && col == 3) {
        return KEY_TAB;
    }
    if (row == 8 && col >= 1 && col <= 3) {
        return (unsigned char)('1' + (col - 1));
    }
    if (row == 7 && col >= 1 && col <= 3) {
        return (unsigned char)('4' + (col - 1));
    }
    if (row == 6 && col == 1) {
        return '7';
    }
    if (row == 1 && col == 1) {
        return (unsigned char)key_speed;
    }
    if (row == 2 && col == 2) {
        return KEY_ESCAPE;
    }
    if (row == 3 && col == 4) {
        return (unsigned char)key_strafe;
    }
    if (row == 1 && col == 3) {
        return KEY_LEFTARROW;
    }
    if (row == 1 && col == 4) {
        return KEY_UPARROW;
    }
    if (row == 2 && col == 3) {
        return KEY_DOWNARROW;
    }
    if (row == 2 && col == 4) {
        return KEY_RIGHTARROW;
    }
    if (row == 0 && col == 0) {
        return KEY_FIRE;
    }
    if (row == 0 && col == 1) {
        return doom_next_weapon_key();
    }
    if (row == 9 && col == 4) {
        return KEY_ENTER;
    }
    return 0;
}

static void doom_set_key_state(unsigned char key, bool down)
{
    if (key == 0) {
        return;
    }

    if (s_key_down[key] == down) {
        return;
    }

    s_key_down[key] = down;
    doom_key_push(down ? 1 : 0, key);
}

static void doom_poll_keypad(void)
{
    bool matrix[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
    bool active[256] = {0};

    board_keypad_scan_matrix(matrix);

    for (int row = 0; row < BOARD_KEYPAD_ROWS; row++) {
        for (int col = 0; col < BOARD_KEYPAD_COLS; col++) {
            if (!matrix[row][col]) {
                continue;
            }

            if (row == 0 && col == 1) {
                continue;
            }

            unsigned char key = doom_key_for_matrix_position(row, col);
            if (key != 0) {
                active[key] = true;
            }
        }
    }

    bool window_pressed = matrix[0][1];
    if (window_pressed && !s_window_was_pressed) {
        s_window_weapon_key = doom_next_weapon_key();
        doom_set_key_state(s_window_weapon_key, true);
    } else if (!window_pressed && s_window_was_pressed && s_window_weapon_key != 0) {
        doom_set_key_state(s_window_weapon_key, false);
        s_window_weapon_key = 0;
    } else if (window_pressed && s_window_weapon_key != 0) {
        active[s_window_weapon_key] = true;
    }
    s_window_was_pressed = window_pressed;

    for (int key = 1; key < 256; key++) {
        if ((unsigned char)key == s_window_weapon_key) {
            continue;
        }
        doom_set_key_state((unsigned char)key, active[key]);
    }
}

static void doom_poll_serial_release(void)
{
    if (s_serial_pressed_key == 0) {
        return;
    }

    if (esp_timer_get_time() >= s_serial_release_time_us) {
        doom_key_push(0, (unsigned char)s_serial_pressed_key);
        s_serial_pressed_key = 0;
        s_serial_release_time_us = 0;
    }
}

bool opencalc_doom_wad_available(void)
{
    struct stat st;
    return stat("/data/doom1.wad", &st) == 0 && st.st_size > 0;
}

bool opencalc_doom_start(void)
{
    if (!opencalc_doom_wad_available()) {
        ESP_LOGW(TAG, "Missing /data/doom1.wad");
        return false;
    }

    if (s_doom_started) {
        doom_restart_to_main_menu();
        return true;
    }

    ESP_LOGI(TAG, "Starting Doom with /data/doom1.wad");
    key_nextweapon = OPENCALC_DOOM_NEXT_WEAPON_KEY;
    board_display_lock();
    board_draw_text_screen("doom");
    board_display_unlock();
    doomgeneric_Create(s_doom_argc, s_doom_argv);
    doom_restart_to_main_menu();
    s_doom_started = true;
    return true;
}

void opencalc_doom_tick(void)
{
    if (s_doom_started) {
        doomgeneric_Tick();
    }
}

void DG_Init(void)
{
}

void DG_DrawFrame(void)
{
    const uint32_t *src = (const uint32_t *)DG_ScreenBuffer;
    if (src == NULL) {
        return;
    }

    for (int y = 0; y < 240; y++) {
        int src_y = (y * 200) / 240;
        const uint32_t *src_row = src + src_y * 320;
        uint32_t *dst_row = s_doom_frame_320x240 + y * 320;
        for (int x = 0; x < 320; x++) {
            dst_row[x] = src_row[x];
        }
    }

    board_display_lock();
    board_draw_rgb888_frame_320x240(s_doom_frame_320x240);
    board_display_unlock();
    vTaskDelay(pdMS_TO_TICKS(1));
}

bool opencalc_doom_press_button_number(int number)
{
    if (number < 1 || number > BOARD_KEYPAD_ROWS * BOARD_KEYPAD_COLS) {
        return false;
    }

    int index = number - 1;
    int row = index / BOARD_KEYPAD_COLS;
    int col = index % BOARD_KEYPAD_COLS;
    unsigned char key = doom_key_for_matrix_position(row, col);
    if (key == 0) {
        printf("doom button %d has no action\n", number);
        return false;
    }

    if (s_serial_pressed_key != 0) {
        doom_key_push(0, (unsigned char)s_serial_pressed_key);
    }

    s_serial_pressed_key = key;
    s_serial_release_time_us = esp_timer_get_time() + 150000;
    doom_key_push(1, key);
    printf("doom button %d -> key 0x%02x\n", number, key);
    return true;
}

void DG_SleepMs(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

uint32_t DG_GetTicksMs(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
    doom_poll_keypad();
    doom_poll_serial_release();

    if (doom_key_queue_empty()) {
        return 0;
    }

    if (pressed != NULL) {
        *pressed = s_key_events[s_key_event_read].pressed;
    }
    if (doomKey != NULL) {
        *doomKey = s_key_events[s_key_event_read].key;
    }

    s_key_event_read = (s_key_event_read + 1) % (int)(sizeof(s_key_events) / sizeof(s_key_events[0]));
    return 1;
}

void DG_SetWindowTitle(const char *title)
{
    ESP_LOGI(TAG, "Title: %s", title ? title : "");
}
