// opencalc_breakout.c
//
// Breakout for the OpenCalc board (320x240 ILI9341, ESP32-S3). Same
// drawing approach and layout family as opencalc_tetris.c /
// opencalc_snake.c: the shared RGB888 UI canvas, the same 5x7 bitmap
// font, the same left info column (x=4..97) and 200x200 arena at
// (110,20) used by Snake.
//
// Physical left/right input is sampled every frame so the paddle follows a
// held key. Serial button commands remain discrete taps for bring-up.
//
// Wiring in: identical pattern to opencalc_tetris.c / opencalc_snake.c --
//   opencalc_breakout_init() once at boot, opencalc_breakout_enter() to
//   launch, opencalc_breakout_active()/opencalc_breakout_tick() in the
//   main loop, and opencalc_breakout_press_button_number() from the
//   button dispatcher.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_timer.h"

#include <stdio.h>
#include <string.h>

#include "board_init.h"
#include "opencalc_audio.h"
#include "opencalc_persist.h"
#include "opencalc_breakout.h"
#include "opencalc_ui_canvas.h"
#include "breakout_core.h"

#define UI_W 320
#define UI_H 240
#define BREAKOUT_PADDLE_SPEED 220.0f

#define C_BG      0x0b0d10u
#define C_SURFACE 0x171b21u
#define C_BORDER  0x3a4250u
#define C_ACCENT  0x4aa3ffu
#define C_TEXT    0xf4f7fbu
#define C_MUTED   0x9aa8b6u
#define C_DANGER  0xff4d4du
#define C_PADDLE  0x4aa3ffu
#define C_BALL    0xf4f7fbu

static const uint32_t ROW_COLOR[BRK_ROWS] = {
    0xf75b5bu, /* row 0 (top, worth most) red    */
    0xf7973au, /* orange */
    0xf7d34au, /* yellow */
    0x5be36bu, /* green  */
    0x36e2f7u, /* row 4 (bottom, worth least) cyan */
};

/* Extra hit points on a brick lighten it toward white, so a 2-3hp brick
 * visibly looks tougher than a 1hp brick of the same row. */
static uint32_t brick_color(int row, int hp)
{
    uint32_t base = ROW_COLOR[row];
    if (hp <= 1) return base;
    uint32_t r = (base >> 16) & 0xff, g = (base >> 8) & 0xff, b = base & 0xff;
    float mix = (hp >= 3) ? 0.55f : 0.3f;
    r = (uint32_t)(r + (255 - r) * mix);
    g = (uint32_t)(g + (255 - g) * mix);
    b = (uint32_t)(b + (255 - b) * mix);
    return (r << 16) | (g << 8) | b;
}

static const uint32_t POWERUP_COLOR[5] = {
    0, /* unused */
    0x36e2f7u, /* extra ball: cyan */
    0xff4d4du, /* extra life: red/danger so it stands out as precious */
    0xc879f2u, /* slow ball: purple */
    0xf7d34au, /* wide paddle: yellow */
};

static const char *POWERUP_LABEL[5] = { "", "B", "+", "S", "W" };

static uint32_t *s_frame;

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

/* ---- arena layout: same position/size convention as Snake/Tetris ---- */
#define ARENA_X   110
#define ARENA_Y   20
#define ARENA_W   200
#define ARENA_H   200

#define LEFT_X    4
#define LEFT_W    (ARENA_X - 8 - LEFT_X)

static breakout_t s_game;
static bool s_active = false;
static int64_t s_last_us = 0;
static long s_high_score = 0;
static bool s_high_score_dirty = false;

static void breakout_note_score(void)
{
    if (s_game.score > s_high_score) {
        s_high_score = s_game.score;
        s_high_score_dirty = true;
    }
}

static void breakout_save_high_score(void)
{
    if (!s_high_score_dirty) return;
    opencalc_persist_set_u32("hs_breakout", (uint32_t)s_high_score);
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

    for (int r = 0; r < BRK_ROWS; r++) {
        for (int c = 0; c < BRK_COLS; c++) {
            int hp = s_game.bricks[r][c];
            if (!hp) continue;
            int bx = ARENA_X + (int)(c * BRK_BRICK_W);
            int by = ARENA_Y + (int)(BRK_BRICK_TOP + r * (BRK_BRICK_H + BRK_BRICK_GAP));
            rect(bx, by, (int)(BRK_BRICK_W - BRK_BRICK_GAP), (int)BRK_BRICK_H, brick_color(r, hp));
        }
    }

    for (int i = 0; i < BRK_MAX_POWERUPS; i++) {
        const brk_powerup_t *p = &s_game.powerups[i];
        if (!p->active) continue;
        int px0 = ARENA_X + (int)p->x - 4;
        int py0 = ARENA_Y + (int)p->y - 4;
        rect(px0, py0, 8, 8, POWERUP_COLOR[p->type]);
        border(px0, py0, 8, 8, C_BG);
        text(px0 + 2, py0 + 1, POWERUP_LABEL[p->type], C_BG, 1);
    }

    float pw = breakout_paddle_w(&s_game);
    rect(ARENA_X + (int)s_game.paddle_x, ARENA_Y + (int)BRK_PADDLE_Y,
         (int)pw, (int)BRK_PADDLE_H, C_PADDLE);

    int ball_size = (int)(BRK_BALL_R * 2);
    for (int i = 0; i < BRK_MAX_BALLS; i++) {
        const brk_ball_t *b = &s_game.balls[i];
        if (!b->active) continue;
        rect(ARENA_X + (int)(b->x - BRK_BALL_R), ARENA_Y + (int)(b->y - BRK_BALL_R),
             ball_size, ball_size, C_BALL);
    }

    text(LEFT_X, ARENA_Y, "BREAKOUT", C_ACCENT, 1);

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

    y = draw_panel_box(LEFT_X, y, LEFT_W, 26, "LIVES");
    snprintf(buf, sizeof(buf), "%d", s_game.lives);
    text(LEFT_X + 4, y - 26 + 13, buf, C_TEXT, 1);
    y += 8;

    text(LEFT_X, y, "LEFT/RIGHT MOVE", C_MUTED, 1); y += 8;
    text(LEFT_X, y, "Enter=LAUNCH", C_MUTED, 1); y += 8;
    text(LEFT_X, y, "Back=PAUSE", C_MUTED, 1); y += 8;
    if (s_game.wide_timer_ms > 0) {
        text(LEFT_X, y, "WIDE ON", C_ACCENT, 1); y += 8;
    }
    if (s_game.slow_timer_ms > 0) {
        text(LEFT_X, y, "SLOW ON", C_ACCENT, 1); y += 8;
    }

    if (!s_game.ball_launched && !s_game.game_over && !s_game.paused) {
        const char *msg = "Enter=LAUNCH";
        int w = text_width(msg, 1);
        text(ARENA_X + (ARENA_W - w) / 2, ARENA_Y + ARENA_H / 2, msg, C_MUTED, 1);
    }

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
        const char *msg2 = "50=RETRY";
        int w2 = text_width(msg2, 1);
        text(ARENA_X + (ARENA_W - w2) / 2, ARENA_Y + ARENA_H / 2 + 2, msg2, C_MUTED, 1);
    }

    board_display_lock();
    board_draw_rgb888_frame_320x240(s_frame);
    board_display_unlock();
}

void opencalc_breakout_init(void)
{
    s_frame = opencalc_ui_canvas_pixels();
    memset(&s_game, 0, sizeof(s_game));
    s_active = false;
    s_high_score = (long)opencalc_persist_get_u32("hs_breakout", 0);
    s_high_score_dirty = false;
}

void opencalc_breakout_enter(void)
{
    breakout_init(&s_game, (uint32_t)esp_timer_get_time());
    s_active = true;
    s_last_us = esp_timer_get_time();
    opencalc_audio_play_tone(523, 70, 65);
    draw_frame();
}

bool opencalc_breakout_active(void) { return s_active; }

void opencalc_breakout_tick(void)
{
    if (!s_active) return;
    int64_t now = esp_timer_get_time();
    float dt_ms = (float)(now - s_last_us) / 1000.0f;
    s_last_us = now;
    if (dt_ms > 200.0f) dt_ms = 200.0f;

    bool matrix[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
    board_keypad_scan_matrix(matrix);
    bool left_held = matrix[1][3];
    bool right_held = matrix[2][4];
    if (left_held != right_held) {
        float direction = left_held ? -1.0f : 1.0f;
        breakout_move_paddle(&s_game,
                             direction * BREAKOUT_PADDLE_SPEED * (dt_ms / 1000.0f));
    }

    long previous_score = s_game.score;
    int previous_lives = s_game.lives;
    int previous_level = s_game.level;
    bool was_game_over = s_game.game_over;
    breakout_step(&s_game, dt_ms);
    if (s_game.score > previous_score) {
        opencalc_audio_play_tone(760, 28, 55);
    }
    if (s_game.level > previous_level) {
        opencalc_audio_play_tone(1047, 150, 85);
    } else if (s_game.lives < previous_lives) {
        opencalc_audio_play_tone(150, 220, 85);
    }
    if (!was_game_over && s_game.game_over) {
        opencalc_audio_play_tone(110, 400, 90);
    }
    breakout_note_score();
    if (s_game.game_over) breakout_save_high_score();
    draw_frame();
}

bool opencalc_breakout_press_button_number(int number)
{
    if (!s_active) return false;

    switch (number) {
    case 46: breakout_save_high_score(); s_active = false; return true;
    case 13: breakout_toggle_pause(&s_game); break;
    case 9:  breakout_move_paddle(&s_game, -BRK_PADDLE_STEP); break; /* left */
    case 15: breakout_move_paddle(&s_game, BRK_PADDLE_STEP); break;  /* right */
    case 50:
        if (s_game.game_over) {
            opencalc_breakout_enter();
        } else {
            breakout_launch(&s_game);
            opencalc_audio_play_tone(440, 60, 65);
        }
        break;
    default:
        return true;
    }

    breakout_note_score();
    if (s_game.game_over) breakout_save_high_score();
    draw_frame();
    return true;
}
