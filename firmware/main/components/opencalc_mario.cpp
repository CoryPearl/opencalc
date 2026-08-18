#include "opencalc_mario.h"

extern "C" {
#include "board_init.h"
#include "opencalc_persist.h"
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
#define LCD_W 320
#define LCD_H 240
#define INPUT_HOLD_US 180000

static Bus *s_bus = nullptr;
static Cartridge *s_cart = nullptr;
static uint8_t *s_nes_frame = nullptr;
static uint32_t *s_lcd_frame = nullptr;
static bool s_active = false;
static int64_t s_last_frame_us = 0;
static int64_t s_started_us = 0;
static int64_t s_input_until_us[50];
static uint32_t s_high_score = 0;
static uint32_t s_score = 0;

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
    if (s_lcd_frame == nullptr) {
        s_lcd_frame = (uint32_t *)heap_caps_malloc(LCD_W * LCD_H * sizeof(uint32_t),
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    return s_nes_frame != nullptr && s_lcd_frame != nullptr;
}

static uint8_t controller_state(void)
{
    uint8_t state = 0;
    if (button_held(3)) state |= (uint8_t)CONTROLLER::A;                         // Zoom
    if (button_held(1)) state |= (uint8_t)CONTROLLER::B;                         // Y=
    if (button_held(13)) state |= (uint8_t)CONTROLLER::Select;                   // Back
    if (button_held(50)) state |= (uint8_t)CONTROLLER::Start;                    // Enter
    if (button_held(10)) state |= (uint8_t)CONTROLLER::Up;
    if (button_held(14)) state |= (uint8_t)CONTROLLER::Down;
    if (button_held(9)) state |= (uint8_t)CONTROLLER::Left;
    if (button_held(15)) state |= (uint8_t)CONTROLLER::Right;
    return state;
}

static void draw_frame(void)
{
    if (!s_nes_frame || !s_lcd_frame) {
        return;
    }

    for (int y = 0; y < LCD_H; y++) {
        const uint8_t *src = &s_nes_frame[y * NES_W];
        uint32_t *dst = &s_lcd_frame[y * LCD_W];
        for (int x = 0; x < LCD_W; x++) {
            dst[x] = s_nes_palette[src[(x * NES_W) / LCD_W] & 0x3F];
        }
    }
    board_draw_rgb888_frame_320x240(s_lcd_frame);
}

static void destroy_emu(void)
{
    delete s_bus;
    delete s_cart;
    s_bus = nullptr;
    s_cart = nullptr;
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
    s_last_frame_us = 0;

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

    int64_t now = esp_timer_get_time();
    if (now - s_last_frame_us < 33333) {
        return;
    }
    s_last_frame_us = now;
    s_score = (uint32_t)((now - s_started_us) / 1000000);

    s_bus->controller = controller_state();
    s_bus->clock();

    board_display_lock();
    draw_frame();
    board_display_unlock();
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
