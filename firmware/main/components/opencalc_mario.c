#include "opencalc_mario.h"

#include "board_init.h"
#include "opencalc_persist.h"

#include "esp_timer.h"

#include <stdio.h>
#include <sys/stat.h>

static bool s_active = false;
static int64_t s_start_us = 0;
static int64_t s_last_draw_us = 0;
static uint32_t s_high_score = 0;
static uint32_t s_score = 0;

static void save_score(void)
{
    if (s_score > s_high_score) {
        s_high_score = s_score;
        opencalc_persist_set_u32("hs_mario", s_high_score);
    }
}

bool opencalc_mario_rom_available(void)
{
    struct stat st;
    return stat("/data/mario.nes", &st) == 0 && st.st_size > 0;
}

void opencalc_mario_init(void)
{
    s_active = false;
    s_high_score = opencalc_persist_get_u32("hs_mario", 0);
}

void opencalc_mario_enter(void)
{
    s_active = true;
    s_start_us = esp_timer_get_time();
    s_last_draw_us = 0;
    s_score = 0;
    board_display_lock();
    board_draw_text_screen("Mario ROM found\nNES backend pending\narrows move\nY=B Zoom=A\nEnter=start");
    board_display_unlock();
}

bool opencalc_mario_active(void)
{
    return s_active;
}

void opencalc_mario_tick(void)
{
    if (!s_active) {
        return;
    }

    int64_t now = esp_timer_get_time();
    s_score = (uint32_t)((now - s_start_us) / 1000000);
    if (now - s_last_draw_us < 500000) {
        return;
    }
    s_last_draw_us = now;
    save_score();
}

bool opencalc_mario_press_button_number(int number)
{
    if (!s_active) {
        return false;
    }

    if (number == 46) {
        save_score();
        s_active = false;
        return true;
    }

    printf("mario button %d\n", number);
    return true;
}
