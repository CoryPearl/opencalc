// opencalc_snake.c
//
// Snake for the OpenCalc board (320x240 ILI9341, ESP32-S3). Same drawing
// approach as opencalc_tetris.c / opencalc_ui.c: one RGB888 framebuffer in
// PSRAM, simple software primitives, the same 5x7 bitmap font, and the
// same board_draw_rgb888_frame_320x240() push per frame. The board/panel
// layout (left info column at x=4..97, arena at x=110,y=20,200x200) also
// matches opencalc_tetris.c so all the mini-apps feel like one family.
//
// Wiring in: identical pattern to opencalc_tetris.c --
//   opencalc_snake_init() once at boot, opencalc_snake_enter() to launch,
//   opencalc_snake_active()/opencalc_snake_tick() in the main loop, and
//   opencalc_snake_press_button_number() from the button dispatcher.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_attr.h"
#include "esp_timer.h"

#include <stdio.h>
#include <string.h>

#include "board_init.h"
#include "opencalc_audio.h"
#include "opencalc_persist.h"
#include "opencalc_snake.h"
#include "snake_core.h"

#define UI_W 320
#define UI_H 240

#define C_BG      0x0b0d10u
#define C_SURFACE 0x171b21u
#define C_BORDER  0x3a4250u
#define C_ACCENT  0x4aa3ffu
#define C_TEXT    0xf4f7fbu
#define C_MUTED   0x9aa8b6u
#define C_GRID    0x1b2027u
#define C_DANGER  0xff4d4du
#define C_SNAKE_HEAD 0x5be36bu
#define C_SNAKE_BODY 0x36c95fu
#define C_FOOD    0xf75b5bu

static EXT_RAM_BSS_ATTR uint32_t s_frame[UI_W * UI_H];

static const uint8_t FONT[40][7] = {
    {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e}, {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e},
    {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f}, {0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e},
    {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02}, {0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e},
    {0x06,0x08,0x10,0x1e,0x11,0x11,0x0e}, {0x1f,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e}, {0x0e,0x11,0x11,0x0f,0x01,0x02,0x0c},
    {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11}, {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e},
    {0x0e,0x11,0x10,0x10,0x10,0x11,0x0e}, {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e},
    {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f}, {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10},
    {0x0e,0x11,0x10,0x17,0x11,0x11,0x0e}, {0x11,0x11,0x11,0x1f,0x11,0x11,0x11},
    {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e}, {0x07,0x02,0x02,0x02,0x12,0x12,0x0c},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1f},
    {0x11,0x1b,0x15,0x15,0x11,0x11,0x11}, {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e}, {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10},
    {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d}, {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11},
    {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e}, {0x1f,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0e}, {0x11,0x11,0x11,0x11,0x11,0x0a,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0a}, {0x11,0x11,0x0a,0x04,0x0a,0x11,0x11},
    {0x11,0x11,0x0a,0x04,0x04,0x04,0x04}, {0x1f,0x01,0x02,0x04,0x08,0x10,0x1f},
    {0x0a,0x0a,0x1f,0x0a,0x1f,0x0a,0x0a},
    {0x0e,0x11,0x01,0x02,0x04,0x00,0x04},
    {0x01,0x02,0x02,0x04,0x08,0x08,0x10}, /* '/' */
    {0x00,0x1f,0x00,0x00,0x1f,0x00,0x00}, /* '=' */
};

static const uint8_t *font_for(char c)
{
    if (c >= '0' && c <= '9') return FONT[c - '0'];
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return FONT[10 + c - 'A'];
    if (c == '#') return FONT[36];
    if (c == '/') return FONT[38];
    if (c == '=') return FONT[39];
    return FONT[37];
}

#define GLYPH_W 5
#define GLYPH_H 7
#define GLYPH_ADVANCE (GLYPH_W + 2)

static void px(int x, int y, uint32_t color)
{
    if (x >= 0 && x < UI_W && y >= 0 && y < UI_H) {
        s_frame[y * UI_W + x] = color;
    }
}

static void rect(int x, int y, int w, int h, uint32_t color)
{
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            px(xx, yy, color);
        }
    }
}

static void border(int x, int y, int w, int h, uint32_t color)
{
    rect(x, y, w, 1, color);
    rect(x, y + h - 1, w, 1, color);
    rect(x, y, 1, h, color);
    rect(x + w - 1, y, 1, h, color);
}

static void text(int x, int y, const char *s, uint32_t color, int scale)
{
    int cx = x;
    for (int i = 0; s[i] != '\0'; i++) {
        char c = s[i];
        if (c == ' ') { cx += GLYPH_ADVANCE * scale; continue; }
        const uint8_t *glyph = font_for(c);
        for (int row = 0; row < GLYPH_H; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < GLYPH_W; col++) {
                if (bits & (1 << (GLYPH_W - 1 - col))) {
                    rect(cx + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        cx += GLYPH_ADVANCE * scale;
    }
}

static int text_width(const char *s, int scale)
{
    int n = 0;
    for (int i = 0; s[i] != '\0'; i++) n += GLYPH_ADVANCE * scale;
    return n > 0 ? n - 2 * scale : 0;
}

/* ---- arena layout: matches opencalc_tetris.c's board position/size ---- */
#define CELL      10
#define ARENA_X   110
#define ARENA_Y   20
#define ARENA_W   (SNAKE_COLS * CELL) /* 200 */
#define ARENA_H   (SNAKE_ROWS * CELL) /* 200 */

#define LEFT_X    4
#define LEFT_W    (ARENA_X - 8 - LEFT_X)

static snake_t s_game;
static bool s_active = false;
static int64_t s_last_us = 0;
static long s_high_score = 0;
static bool s_high_score_dirty = false;

static void snake_note_score(void)
{
    if (s_game.score > s_high_score) {
        s_high_score = s_game.score;
        s_high_score_dirty = true;
    }
}

static void snake_save_high_score(void)
{
    if (!s_high_score_dirty) return;
    opencalc_persist_set_u32("hs_snake", (uint32_t)s_high_score);
    s_high_score_dirty = false;
}

static int draw_panel_box(int x, int y, int w, int h, const char *title)
{
    rect(x, y, w, h, C_SURFACE);
    border(x, y, w, h, C_BORDER);
    text(x + 4, y + 4, title, C_MUTED, 1);
    return y + h;
}

static void draw_frame(void)
{
    rect(0, 0, UI_W, UI_H, C_BG);

    border(ARENA_X - 2, ARENA_Y - 2, ARENA_W + 4, ARENA_H + 4, C_BORDER);
    rect(ARENA_X, ARENA_Y, ARENA_W, ARENA_H, C_SURFACE);
    for (int c = 1; c < SNAKE_COLS; c++) {
        rect(ARENA_X + c * CELL, ARENA_Y, 1, ARENA_H, C_GRID);
    }
    for (int r = 1; r < SNAKE_ROWS; r++) {
        rect(ARENA_X, ARENA_Y + r * CELL, ARENA_W, 1, C_GRID);
    }

    rect(ARENA_X + s_game.food_x * CELL, ARENA_Y + s_game.food_y * CELL, CELL, CELL, C_FOOD);

    for (int i = 0; i < s_game.length; i++) {
        uint32_t color = (i == 0) ? C_SNAKE_HEAD : C_SNAKE_BODY;
        rect(ARENA_X + s_game.body_x[i] * CELL, ARENA_Y + s_game.body_y[i] * CELL, CELL, CELL, color);
    }

    text(LEFT_X, ARENA_Y, "SNAKE", C_ACCENT, 1);

    int y = ARENA_Y + 16;
    char buf[24];
    y = draw_panel_box(LEFT_X, y, LEFT_W, 26, "HIGH SCORE");
    snprintf(buf, sizeof(buf), "%ld", s_high_score);
    text(LEFT_X + 4, y - 26 + 13, buf, C_TEXT, 1);
    y += 3;

    y = draw_panel_box(LEFT_X, y, LEFT_W, 26, "SCORE");
    snprintf(buf, sizeof(buf), "%ld", s_game.score);
    text(LEFT_X + 4, y - 26 + 13, buf, C_TEXT, 1);
    y += 3;

    y = draw_panel_box(LEFT_X, y, LEFT_W, 26, "LEVEL");
    snprintf(buf, sizeof(buf), "%d", s_game.level);
    text(LEFT_X + 4, y - 26 + 13, buf, C_TEXT, 1);
    y += 3;

    y = draw_panel_box(LEFT_X, y, LEFT_W, 26, "LENGTH");
    snprintf(buf, sizeof(buf), "%d", s_game.length);
    text(LEFT_X + 4, y - 26 + 13, buf, C_TEXT, 1);
    y += 8;

    text(LEFT_X, y, "LEFT/RIGHT", C_MUTED, 1); y += 8;
    text(LEFT_X, y, "UP/DOWN", C_MUTED, 1); y += 8;
    text(LEFT_X, y, "Back=PAUSE", C_MUTED, 1);

    if (s_game.paused && !s_game.game_over) {
        rect(ARENA_X, ARENA_Y + ARENA_H / 2 - 12, ARENA_W, 24, C_BG);
        const char *msg = "PAUSED";
        int w = text_width(msg, 1);
        text(ARENA_X + (ARENA_W - w) / 2, ARENA_Y + ARENA_H / 2 - 4, msg, C_ACCENT, 1);
    }

    if (s_game.game_over) {
        rect(ARENA_X, ARENA_Y + ARENA_H / 2 - 20, ARENA_W, 40, C_BG);
        border(ARENA_X, ARENA_Y + ARENA_H / 2 - 20, ARENA_W, 40, C_DANGER);
        const char *msg = "GAME OVER";
        int w = text_width(msg, 1);
        text(ARENA_X + (ARENA_W - w) / 2, ARENA_Y + ARENA_H / 2 - 12, msg, C_DANGER, 1);
        const char *msg2 = "Enter=RETRY";
        int w2 = text_width(msg2, 1);
        text(ARENA_X + (ARENA_W - w2) / 2, ARENA_Y + ARENA_H / 2 + 2, msg2, C_MUTED, 1);
    }

    board_display_lock();
    board_draw_rgb888_frame_320x240(s_frame);
    board_display_unlock();
}

void opencalc_snake_init(void)
{
    memset(&s_game, 0, sizeof(s_game));
    s_active = false;
    s_high_score = (long)opencalc_persist_get_u32("hs_snake", 0);
    s_high_score_dirty = false;
}

void opencalc_snake_enter(void)
{
    snake_init(&s_game, (uint32_t)esp_timer_get_time());
    s_active = true;
    s_last_us = esp_timer_get_time();
    opencalc_audio_play_tone(523, 70, 65);
    draw_frame();
}

bool opencalc_snake_active(void) { return s_active; }

void opencalc_snake_tick(void)
{
    if (!s_active) return;
    int64_t now = esp_timer_get_time();
    float dt_ms = (float)(now - s_last_us) / 1000.0f;
    s_last_us = now;
    if (dt_ms > 200.0f) dt_ms = 200.0f;

    long previous_score = s_game.score;
    int previous_level = s_game.level;
    bool was_game_over = s_game.game_over;
    snake_step(&s_game, dt_ms);
    if (s_game.score > previous_score) {
        opencalc_audio_play_tone(s_game.level > previous_level ? 1047 : 880,
                                 s_game.level > previous_level ? 140 : 55,
                                 80);
    }
    if (!was_game_over && s_game.game_over) {
        opencalc_audio_play_tone(120, 350, 90);
    }
    snake_note_score();
    if (s_game.game_over) snake_save_high_score();
    draw_frame();
}

bool opencalc_snake_press_button_number(int number)
{
    if (!s_active) return false;

    switch (number) {
    case 46: snake_save_high_score(); s_active = false; return true;
    case 13: snake_toggle_pause(&s_game); break;
    case 9:  snake_set_direction(&s_game, -1, 0); break;  /* left */
    case 15: snake_set_direction(&s_game, 1, 0); break;   /* right */
    case 10: snake_set_direction(&s_game, 0, -1); break;  /* up */
    case 14: snake_set_direction(&s_game, 0, 1); break;   /* down */
    case 50:
        if (s_game.game_over) {
            opencalc_snake_enter();
        }
        break;
    default:
        return true;
    }

    snake_note_score();
    if (s_game.game_over) snake_save_high_score();
    draw_frame();
    return true;
}
