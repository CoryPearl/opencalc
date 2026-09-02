#include "opencalc_mario.h"

extern "C" {
#include "board_init.h"
#include "opencalc_config.h"
#include "opencalc_persist.h"
#include "opencalc_power.h"
}

#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "mario/bus.h"
#include "mario/cartridge.h"
#include "mario/controller.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define NES_W 256
#define NES_H 240
#define INPUT_HOLD_US 180000
#define NES_FRAME_PERIOD_US 16639LL
#define NES_MAX_CATCHUP_FRAMES 3
#define NES_MAX_BACKLOG_FRAMES 6

static Bus *s_bus = nullptr;
static Cartridge *s_cart = nullptr;
static uint8_t *s_nes_frame = nullptr;
static bool s_active = false;
static int64_t s_started_us = 0;
static int64_t s_last_emu_tick_us = 0;
static int64_t s_emu_time_accumulator_us = 0;
static int64_t s_input_until_us[50];
static uint32_t s_high_score = 0;
static uint32_t s_score = 0;
static uint32_t s_emu_frames_since_log = 0;
static uint32_t s_display_frames_since_log = 0;
static uint64_t s_tick_work_us_since_log = 0;
static uint32_t s_tick_count_since_log = 0;
static uint64_t s_dropped_time_us_since_log = 0;
static int64_t s_last_timing_log_us = 0;

static const uint32_t s_nes_palette[64] = {
    0x626262, 0x001FB2, 0x2404C8, 0x5200B2, 0x730076, 0x800024, 0x730B00, 0x522800,
    0x244400, 0x005700, 0x005C00, 0x005324, 0x003C76, 0x000000, 0x000000, 0x000000,
    0xABABAB, 0x0D57FF, 0x4B30FF, 0x8A13FF, 0xBC08D6, 0xD21269, 0xC72E00, 0x9D5400,
    0x607B00, 0x209800, 0x00A300, 0x009942, 0x007DB4, 0x000000, 0x000000, 0x000000,
    0xFFFFFF, 0x53AEFF, 0x9085FF, 0xD365FF, 0xFF57FF, 0xFF5DCF, 0xFF7757, 0xFA9E00,
    0xBDC700, 0x7AE700, 0x43F611, 0x26EF7E, 0x2CD5F6, 0x4E4E4E, 0x000000, 0x000000,
    0xFFFFFF, 0xB6E1FF, 0xCED1FF, 0xE9C3FF, 0xFFBCFF, 0xFFBDF4, 0xFFC6C3, 0xFFD59A,
    0xE9E681, 0xCEF481, 0xB6FB9A, 0xA9FAC3, 0xA9F0F4, 0xB8B8B8, 0x000000, 0x000000,
};

static bool button_held(int number)
{
    return number >= 1 && number <= 50 && esp_timer_get_time() < s_input_until_us[number - 1];
}

static bool matrix_button_held(
    const bool matrix[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS],
    int number)
{
    if (number < 1 || number > BOARD_KEYPAD_ROWS * BOARD_KEYPAD_COLS) {
        return false;
    }

    int index = number - 1;
    return matrix[index / BOARD_KEYPAD_COLS][index % BOARD_KEYPAD_COLS];
}

static void hold_button(int number)
{
    if (number >= 1 && number <= 50) {
        s_input_until_us[number - 1] = esp_timer_get_time() + INPUT_HOLD_US;
    }
}

static void save_score(void)
{
    if (s_score > s_high_score) {
        s_high_score = s_score;
        opencalc_persist_set_u32("hs_mario", s_high_score);
    }
}

static bool ensure_buffers(void)
{
    if (s_nes_frame == nullptr) {
        s_nes_frame = (uint8_t *)heap_caps_calloc(NES_W * NES_H, sizeof(uint8_t),
                                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    return s_nes_frame != nullptr;
}

static uint8_t controller_state(void)
{
    bool matrix[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
    board_keypad_scan_matrix(matrix);

#define MARIO_BUTTON_DOWN(number) (matrix_button_held(matrix, (number)) || button_held(number))
    uint8_t state = 0;
    if (MARIO_BUTTON_DOWN(3)) state |= (uint8_t)CONTROLLER::A;                   // Zoom
    if (MARIO_BUTTON_DOWN(1)) state |= (uint8_t)CONTROLLER::B;                   // Y=
    if (MARIO_BUTTON_DOWN(13)) state |= (uint8_t)CONTROLLER::Select;             // Back
    if (MARIO_BUTTON_DOWN(50)) state |= (uint8_t)CONTROLLER::Start;              // Enter
    if (MARIO_BUTTON_DOWN(10)) state |= (uint8_t)CONTROLLER::Up;
    if (MARIO_BUTTON_DOWN(14)) state |= (uint8_t)CONTROLLER::Down;
    if (MARIO_BUTTON_DOWN(9)) state |= (uint8_t)CONTROLLER::Left;
    if (MARIO_BUTTON_DOWN(15)) state |= (uint8_t)CONTROLLER::Right;
#undef MARIO_BUTTON_DOWN
    return state;
}

static void draw_frame(void)
{
    if (!s_nes_frame) {
        return;
    }
    board_draw_indexed8_frame_256x240(s_nes_frame, s_nes_palette);
}

static void reset_timing_stats(int64_t now_us)
{
    s_emu_frames_since_log = 0;
    s_display_frames_since_log = 0;
    s_tick_work_us_since_log = 0;
    s_tick_count_since_log = 0;
    s_dropped_time_us_since_log = 0;
    s_last_timing_log_us = now_us;
}

static void log_timing_stats(int64_t now_us)
{
#if OPENCALC_DEBUG_LOG_FPS
    int64_t interval_us = now_us - s_last_timing_log_us;
    if (interval_us < 1000000) {
        return;
    }

    uint32_t emu_fps_x10 = (uint32_t)(((uint64_t)s_emu_frames_since_log * 10000000ULL) /
                                      (uint64_t)interval_us);
    uint32_t display_fps_x10 =
        (uint32_t)(((uint64_t)s_display_frames_since_log * 10000000ULL) /
                   (uint64_t)interval_us);
    uint32_t average_tick_us = s_tick_count_since_log > 0
        ? (uint32_t)(s_tick_work_us_since_log / s_tick_count_since_log)
        : 0;

    printf("mario emu=%lu.%lu fps display=%lu.%lu fps avg_tick=%luus backlog=%lldus dropped=%lluus\n",
           (unsigned long)(emu_fps_x10 / 10),
           (unsigned long)(emu_fps_x10 % 10),
           (unsigned long)(display_fps_x10 / 10),
           (unsigned long)(display_fps_x10 % 10),
           (unsigned long)average_tick_us,
           (long long)s_emu_time_accumulator_us,
           (unsigned long long)s_dropped_time_us_since_log);
    fflush(stdout);
    reset_timing_stats(now_us);
#else
    (void)now_us;
#endif
}

static void destroy_emu(void)
{
    delete s_bus;
    delete s_cart;
    s_bus = nullptr;
    s_cart = nullptr;
    opencalc_power_set_performance_required(false);
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
    ensure_buffers();
}

void opencalc_mario_enter(void)
{
    memset(s_input_until_us, 0, sizeof(s_input_until_us));
    s_score = 0;
    s_started_us = esp_timer_get_time();
    s_emu_time_accumulator_us = NES_FRAME_PERIOD_US;

    board_display_lock();
    if (!ensure_buffers()) {
        board_draw_text_screen("Mario\nNo PSRAM");
        board_display_unlock();
        return;
    }

    destroy_emu();
    s_cart = new Cartridge("/data/mario.nes", ROMBackend::LRU);
    if (s_cart == nullptr || !s_cart->isValid()) {
        destroy_emu();
        board_draw_text_screen("Mario\nROM load failed");
        board_display_unlock();
        return;
    }

    s_bus = new Bus();
    if (s_bus == nullptr) {
        destroy_emu();
        board_draw_text_screen("Mario\nNES bus failed");
        board_display_unlock();
        return;
    }

    s_bus->insertCartridge(s_cart);
    s_bus->connectFramebuffer(s_nes_frame);
    s_bus->reset();
    opencalc_power_set_performance_required(true);
    s_last_emu_tick_us = esp_timer_get_time();
    reset_timing_stats(s_last_emu_tick_us);
    s_active = true;
    board_draw_text_screen("Mario\nNES loading");
    board_display_unlock();
}

bool opencalc_mario_active(void)
{
    return s_active;
}

void opencalc_mario_tick(void)
{
    if (!s_active || s_bus == nullptr) {
        return;
    }

    int64_t tick_started_us = esp_timer_get_time();
    int64_t now = tick_started_us;
    s_score = (uint32_t)((now - s_started_us) / 1000000);

    int64_t elapsed_us = now - s_last_emu_tick_us;
    s_last_emu_tick_us = now;
    if (elapsed_us < 0) {
        elapsed_us = 0;
    }
    s_emu_time_accumulator_us += elapsed_us;
    int64_t max_backlog_us = NES_FRAME_PERIOD_US * NES_MAX_BACKLOG_FRAMES;
    if (s_emu_time_accumulator_us > max_backlog_us) {
        s_dropped_time_us_since_log +=
            (uint64_t)(s_emu_time_accumulator_us - max_backlog_us);
        s_emu_time_accumulator_us = max_backlog_us;
    }

    int frames_due = (int)(s_emu_time_accumulator_us / NES_FRAME_PERIOD_US);
    if (frames_due < 1) {
        return;
    }
    if (frames_due > NES_MAX_CATCHUP_FRAMES) {
        frames_due = NES_MAX_CATCHUP_FRAMES;
    }
    s_emu_time_accumulator_us -= (int64_t)frames_due * NES_FRAME_PERIOD_US;

    s_bus->controller = controller_state();
    for (int frame = 0; frame < frames_due; frame++) {
        /* CPU/game time always advances at the NES rate. Only the final frame
         * in a catch-up batch spends time rendering all 240 PPU scanlines. */
        s_bus->clock(frame == frames_due - 1);
    }
    s_emu_frames_since_log += (uint32_t)frames_due;

    board_display_lock();
    draw_frame();
    board_display_unlock();
    s_display_frames_since_log++;
    s_tick_work_us_since_log += (uint64_t)(esp_timer_get_time() - tick_started_us);
    s_tick_count_since_log++;
    log_timing_stats(esp_timer_get_time());
}

bool opencalc_mario_press_button_number(int number)
{
    if (!s_active) {
        return false;
    }

    if (number == 46) {
        save_score();
        destroy_emu();
        s_active = false;
        return true;
    }

    switch (number) {
    case 1:  // B/run
    case 3:  // A/jump
    case 9:
    case 10:
    case 13:
    case 14:
    case 15:
    case 50:
        hold_button(number);
        return true;
    default:
        return true;
    }
}
