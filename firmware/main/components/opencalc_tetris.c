// opencalc_tetris.c
//
// Tetris for the OpenCalc board (320x240 ILI9341, ESP32-S3).
//
// Drawing follows exactly the same approach as opencalc_ui.c:
//   - the shared UI RGB888 framebuffer in PSRAM
//   - simple software primitives (pixel / filled rect / border / 5x7 text)
//   - a single board_draw_rgb888_frame_320x240() call per frame to push it
// so it drops into the project next to opencalc_ui.c/opencalc_doom.c and
// looks/feels like part of the same firmware rather than a bolted-on demo.
//
// Wiring it in (see the notes at the bottom of this file / the chat reply
// for the full explanation):
//   1. Call opencalc_tetris_init() once from app_main(), next to
//      opencalc_ui_init().
//   2. Call opencalc_tetris_enter() from wherever you want to launch the
//      game (a home-screen icon, a button combo, a menu item...).
//   3. In the app_main() loop, check opencalc_tetris_active() the same way
//      opencalc_ui_doom_active() is checked, and call opencalc_tetris_tick()
//      instead of opencalc_ui_tick() while it's true.
//   4. Forward button numbers to opencalc_tetris_press_button_number()
//      first while active, the same way s_doom_active short-circuits into
//      opencalc_doom_press_button_number() in opencalc_ui.c.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"
#include "esp_timer.h"

#include <stdio.h>
#include <string.h>

#include "board_init.h"
#include "opencalc_audio.h"
#include "opencalc_config.h"
#include "opencalc_persist.h"
#include "opencalc_tetris.h"
#include "opencalc_ui_canvas.h"
#include "tetris_core.h"

#define UI_W 320
#define UI_H 240

/* ---- palette (matches the dark theme used by opencalc_ui.c) ---- */
#define T_BG        0x0b0d10u
#define T_SURFACE   0x171b21u
#define T_BORDER    0x3a4250u
#define T_ACCENT    0x4aa3ffu
#define T_TEXT      0xf4f7fbu
#define T_MUTED     0x9aa8b6u
#define T_GRID      0x1b2027u
#define T_GHOST     0x4a5462u
#define T_DANGER    0xff4d4du

static const uint32_t PIECE_COLOR[T_PIECE_COUNT] = {
    0x36e2f7u, /* I cyan   */
    0xf7d34au, /* O yellow */
    0xc879f2u, /* T purple */
    0x5be36bu, /* S green  */
    0xf75b5bu, /* Z red    */
    0x4a90f7u, /* J blue   */
    0xf7973au, /* L orange */
};

/* ---- framebuffer + primitives (self-contained, same style as opencalc_ui.c) ---- */
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

/* Each glyph is 5 columns wide in a 7-row cell; advance one extra column
 * of spacing so characters never touch (this is what made text look
 * "funky" before -- too little space between glyphs). */
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
        if (c == ' ') {
            cx += GLYPH_ADVANCE * scale;
            continue;
        }
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
    for (int i = 0; s[i] != '\0'; i++) {
        n += GLYPH_ADVANCE * scale;
    }
    return n > 0 ? n - 2 * scale : 0;
}

/* Flat, solid modern blocks -- fill the entire cell exactly, no gap and no
 * inset, so a tetromino reads as one seamless shape and every block's
 * edges land exactly on the same grid lines used everywhere else. */
static void draw_block(int x, int y, int size, uint32_t color)
{
    rect(x, y, size, size, color);
}

/* Ghost outline uses the identical (x, y, size) box as draw_block, just
 * unfilled, so it lines up pixel-for-pixel with where the piece will land
 * instead of floating inset inside the cell. */
static void draw_ghost(int x, int y, int size)
{
    border(x, y, size, size, T_GHOST);
}

/* ---- layout ----
 * Board is centered vertically, spanning nearly the full screen height.
 * Left column: title / hold / high score / score / level.
 * Right column: next 4 pieces. No "lines" panel. */
#define CELL       11
#define BOARD_W_PX (T_BOARD_W * CELL)   /* 110 */
#define BOARD_H_PX (20 * CELL)          /* 220 */
#define BOARD_Y    ((UI_H - BOARD_H_PX) / 2)  /* 10 */
#define BOARD_X    ((UI_W - BOARD_W_PX) / 2)  /* 105 */

#define LEFT_X     4
#define LEFT_W     (BOARD_X - 8 - LEFT_X)

#define RIGHT_X    (BOARD_X + BOARD_W_PX + 8)
#define RIGHT_W    (UI_W - 4 - RIGHT_X)

static tetris_t s_game;
static bool s_active = false;
static int64_t s_last_us = 0;
static bool s_left_was_held = false;
static bool s_right_was_held = false;
static int64_t s_left_repeat_us = 0;
static int64_t s_right_repeat_us = 0;
static long s_high_score = 0;
static bool s_high_score_dirty = false;
static int64_t s_last_health_log_us = 0;

#define TETRIS_HORIZONTAL_DAS_US 180000
#define TETRIS_HORIZONTAL_ARR_US 70000

static bool tetris_state_valid(const tetris_t *game)
{
    if (game->bag_pos < 0 || game->bag_pos > T_PIECE_COUNT ||
        game->cur_piece < T_PIECE_I || game->cur_piece >= T_PIECE_COUNT ||
        game->cur_rot < 0 || game->cur_rot > 3 ||
        game->clear_count < 0 || game->clear_count > 4 || game->level < 1) {
        return false;
    }

    for (int i = 0; i < T_NEXT_COUNT; i++) {
        if (game->next_queue[i] < T_PIECE_I || game->next_queue[i] >= T_PIECE_COUNT) {
            return false;
        }
    }
    if (game->has_hold &&
        (game->hold_piece < T_PIECE_I || game->hold_piece >= T_PIECE_COUNT)) {
        return false;
    }
    for (int row = 0; row < T_BOARD_H; row++) {
        for (int col = 0; col < T_BOARD_W; col++) {
            if (game->board[row][col] < 0 || game->board[row][col] > T_PIECE_COUNT) {
                return false;
            }
        }
    }
    return true;
}

static void tetris_log_health(int64_t now_us)
{
#if OPENCALC_DEBUG_TETRIS_HEALTH
    if (now_us - s_last_health_log_us < 5000000) {
        return;
    }

    bool heap_ok = heap_caps_check_integrity_all(false);
    size_t internal_free = heap_ok ? heap_caps_get_free_size(MALLOC_CAP_INTERNAL) : 0;
    size_t largest_block = heap_ok
        ? heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)
        : 0;
    printf("tetris health heap=%s internal_free=%lu largest=%lu stack_free=%lu\n",
           heap_ok ? "ok" : "CORRUPT",
           (unsigned long)internal_free,
           (unsigned long)largest_block,
           (unsigned long)uxTaskGetStackHighWaterMark(NULL));
    fflush(stdout);
    s_last_health_log_us = now_us;
#else
    (void)now_us;
#endif
}

static void tetris_note_score(void)
{
    if (s_game.score > s_high_score) {
        s_high_score = s_game.score;
        s_high_score_dirty = true;
    }
}

static void tetris_save_high_score(void)
{
    if (!s_high_score_dirty) {
        return;
    }
    opencalc_persist_set_u32("hs_tetris", (uint32_t)s_high_score);
    s_high_score_dirty = false;
}

/* ---- rendering ---- */

static void draw_mini_piece(int x, int y, tetris_piece_t p, int cell)
{
    if (p < T_PIECE_I || p >= T_PIECE_COUNT) {
        return;
    }
    uint16_t shape = tetris_shape(p, 0);
    int minc = 4, maxc = -1, minr = 4, maxr = -1;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (tetris_cell_set(shape, r, c)) {
                if (c < minc) minc = c;
                if (c > maxc) maxc = c;
                if (r < minr) minr = r;
                if (r > maxr) maxr = r;
            }
        }
    }
    int wpx = (maxc - minc + 1) * cell;
    int hpx = (maxr - minr + 1) * cell;
    int ox = x - wpx / 2;
    int oy = y - hpx / 2;
    for (int r = minr; r <= maxr; r++) {
        for (int c = minc; c <= maxc; c++) {
            if (tetris_cell_set(shape, r, c)) {
                draw_block(ox + (c - minc) * cell, oy + (r - minr) * cell, cell, PIECE_COLOR[p]);
            }
        }
    }
}

static int draw_panel_box(int x, int y, int w, int h, const char *title)
{
    rect(x, y, w, h, T_SURFACE);
    border(x, y, w, h, T_BORDER);
    text(x + 4, y + 4, title, T_MUTED, 1);
    return y + h;
}

static void draw_tetris_frame(void)
{
    rect(0, 0, UI_W, UI_H, T_BG);

    /* --- board --- */
    border(BOARD_X - 1, BOARD_Y - 1, BOARD_W_PX + 2, BOARD_H_PX + 2, T_BORDER);
    rect(BOARD_X, BOARD_Y, BOARD_W_PX, BOARD_H_PX, T_SURFACE);
    for (int c = 1; c < T_BOARD_W; c++) {
        rect(BOARD_X + c * CELL, BOARD_Y, 1, BOARD_H_PX, T_GRID);
    }
    for (int r = 1; r < 20; r++) {
        rect(BOARD_X, BOARD_Y + r * CELL, BOARD_W_PX, 1, T_GRID);
    }

    for (int r = T_VISIBLE_TOP; r < T_BOARD_H; r++) {
        int sy = BOARD_Y + (r - T_VISIBLE_TOP) * CELL;
        bool flashing = s_game.clearing && s_game.clear_rows[r] &&
                         (((int)(s_game.clear_timer_ms / 60)) % 2 == 0);
        for (int c = 0; c < T_BOARD_W; c++) {
            int8_t v = s_game.board[r][c];
            if (v < 1 || v > T_PIECE_COUNT) continue;
            int sx = BOARD_X + c * CELL;
            draw_block(sx, sy, CELL, flashing ? T_TEXT : PIECE_COLOR[v - 1]);
        }
    }

    if (!s_game.clearing) {
        uint16_t shape = tetris_shape(s_game.cur_piece, s_game.cur_rot);

        if (!s_game.game_over) {
            int gy = tetris_ghost_y(&s_game);
            for (int r = 0; r < 4; r++) {
                for (int c = 0; c < 4; c++) {
                    if (!tetris_cell_set(shape, r, c)) continue;
                    int by = gy + r;
                    int bx = s_game.cur_x + c;
                    if (by < T_VISIBLE_TOP) continue;
                    draw_ghost(BOARD_X + bx * CELL, BOARD_Y + (by - T_VISIBLE_TOP) * CELL, CELL);
                }
            }
        }

        /* Even on game over, draw the piece that couldn't fit -- that's
         * the block that pushed the stack over the top, so it should
         * stay visible (overlapping the stack) instead of vanishing. */
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (!tetris_cell_set(shape, r, c)) continue;
                int by = s_game.cur_y + r;
                int bx = s_game.cur_x + c;
                if (by < T_VISIBLE_TOP) continue;
                draw_block(BOARD_X + bx * CELL, BOARD_Y + (by - T_VISIBLE_TOP) * CELL, CELL,
                           PIECE_COLOR[s_game.cur_piece]);
            }
        }
    }

    /* --- left column: title / hold / high score / score / level --- */
    text(LEFT_X, BOARD_Y, "TETRIS", T_ACCENT, 1);

    int y = BOARD_Y + 16;
    y = draw_panel_box(LEFT_X, y, LEFT_W, 36, "HOLD");
    if (s_game.has_hold) {
        draw_mini_piece(LEFT_X + LEFT_W / 2, y - 36 + 22, s_game.hold_piece, 6);
    }
    y += 3;

    char buf[24];
    y = draw_panel_box(LEFT_X, y, LEFT_W, 26, "HIGH SCORE");
    snprintf(buf, sizeof(buf), "%ld", s_high_score);
    text(LEFT_X + 4, y - 26 + 13, buf, T_TEXT, 1);
    y += 3;

    y = draw_panel_box(LEFT_X, y, LEFT_W, 26, "SCORE");
    snprintf(buf, sizeof(buf), "%ld", s_game.score);
    text(LEFT_X + 4, y - 26 + 13, buf, T_TEXT, 1);
    y += 3;

    y = draw_panel_box(LEFT_X, y, LEFT_W, 26, "LEVEL");
    snprintf(buf, sizeof(buf), "%d", s_game.level);
    text(LEFT_X + 4, y - 26 + 13, buf, T_TEXT, 1);
    y += 3;

    y = draw_panel_box(LEFT_X, y, LEFT_W, 26, "LINES");
    snprintf(buf, sizeof(buf), "%d", s_game.lines);
    text(LEFT_X + 4, y - 26 + 13, buf, T_TEXT, 1);
    y += 6;

    /* key instructions, bottom-left, under the info panels */
    text(LEFT_X, y, "R/L MOVE", T_MUTED, 1); y += 8;
    text(LEFT_X, y, "Up/Down ROTATE", T_MUTED, 1); y += 8;
    text(LEFT_X, y, "Y= DROP", T_MUTED, 1); y += 8;
    text(LEFT_X, y, "Window HOLD", T_MUTED, 1); y += 8;
    text(LEFT_X, y, "Back PAUSE", T_MUTED, 1);

    /* --- right column: next 4 pieces --- */
    text(RIGHT_X, BOARD_Y, "NEXT", T_ACCENT, 1);
    int ny = BOARD_Y + 16;
    int box_h = 36;
    for (int i = 0; i < T_NEXT_COUNT; i++) {
        rect(RIGHT_X, ny, RIGHT_W, box_h, T_SURFACE);
        border(RIGHT_X, ny, RIGHT_W, box_h, T_BORDER);
        draw_mini_piece(RIGHT_X + RIGHT_W / 2, ny + box_h / 2, s_game.next_queue[i], 7);
        ny += box_h + 5;
    }

    /* --- overlays --- */
    if (s_game.paused && !s_game.game_over) {
        rect(BOARD_X, BOARD_Y + BOARD_H_PX / 2 - 12, BOARD_W_PX, 24, T_BG);
        const char *msg = "PAUSED";
        int w = text_width(msg, 1);
        text(BOARD_X + (BOARD_W_PX - w) / 2, BOARD_Y + BOARD_H_PX / 2 - 4, msg, T_ACCENT, 1);
    }

    if (s_game.game_over) {
        rect(BOARD_X, BOARD_Y + BOARD_H_PX / 2 - 20, BOARD_W_PX, 40, T_BG);
        border(BOARD_X, BOARD_Y + BOARD_H_PX / 2 - 20, BOARD_W_PX, 40, T_DANGER);
        const char *msg = "GAME OVER";
        int w = text_width(msg, 1);
        text(BOARD_X + (BOARD_W_PX - w) / 2, BOARD_Y + BOARD_H_PX / 2 - 12, msg, T_DANGER, 1);
        const char *msg2 = "Enter=RETRY";
        int w2 = text_width(msg2, 1);
        text(BOARD_X + (BOARD_W_PX - w2) / 2, BOARD_Y + BOARD_H_PX / 2 + 2, msg2, T_MUTED, 1);
    }

    board_display_lock();
    board_draw_rgb888_frame_320x240(s_frame);
    board_display_unlock();
}

/* ---- public API ---- */

void opencalc_tetris_init(void)
{
    s_frame = opencalc_ui_canvas_pixels();
    memset(&s_game, 0, sizeof(s_game));
    s_active = false;
    s_high_score = (long)opencalc_persist_get_u32("hs_tetris", 0);
    s_high_score_dirty = false;
}

void opencalc_tetris_enter(void)
{
    uint32_t seed = (uint32_t)esp_timer_get_time();
    tetris_init(&s_game, seed);
    s_active = true;
    s_last_us = esp_timer_get_time();
    s_left_was_held = false;
    s_right_was_held = false;
    s_left_repeat_us = 0;
    s_right_repeat_us = 0;
    s_last_health_log_us = esp_timer_get_time();
    opencalc_audio_play_tone(523, 70, 65);
    draw_tetris_frame();
}

bool opencalc_tetris_active(void)
{
    return s_active;
}

void opencalc_tetris_tick(void)
{
    if (!s_active) {
        return;
    }
    int64_t now = esp_timer_get_time();
    if (!tetris_state_valid(&s_game)) {
        printf("ERROR: Tetris state corruption detected; resetting game safely\n");
        fflush(stdout);
        tetris_init(&s_game, (uint32_t)now);
        s_last_us = now;
    }
    tetris_log_health(now);
    float dt_ms = (float)(now - s_last_us) / 1000.0f;
    s_last_us = now;
    if (dt_ms > 200.0f) {
        dt_ms = 200.0f; /* clamp huge gaps (e.g. first frame) */
    }

    bool matrix[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
    board_keypad_scan_matrix(matrix);

    bool controls_enabled = !s_game.game_over && !s_game.paused;
    bool left_held = controls_enabled && matrix[1][3];
    bool right_held = controls_enabled && matrix[2][4];
    bool soft_drop = controls_enabled && matrix[2][3];

    /* The edge-driven keypad handler performs the first horizontal move.
     * Repeat only after DAS so a tap still moves exactly one column. */
    if (left_held && !right_held) {
        if (!s_left_was_held) {
            s_left_repeat_us = now + TETRIS_HORIZONTAL_DAS_US;
        } else if (now >= s_left_repeat_us) {
            tetris_try_move(&s_game, -1, 0);
            s_left_repeat_us = now + TETRIS_HORIZONTAL_ARR_US;
        }
    } else {
        s_left_repeat_us = 0;
    }

    if (right_held && !left_held) {
        if (!s_right_was_held) {
            s_right_repeat_us = now + TETRIS_HORIZONTAL_DAS_US;
        } else if (now >= s_right_repeat_us) {
            tetris_try_move(&s_game, 1, 0);
            s_right_repeat_us = now + TETRIS_HORIZONTAL_ARR_US;
        }
    } else {
        s_right_repeat_us = 0;
    }

    s_left_was_held = left_held;
    s_right_was_held = right_held;

    int previous_lines = s_game.lines;
    bool was_game_over = s_game.game_over;
    tetris_step(&s_game, dt_ms, soft_drop);
    if (s_game.lines > previous_lines) {
        int cleared = s_game.lines - previous_lines;
        opencalc_audio_play_tone((uint16_t)(620 + cleared * 130),
                                 (uint16_t)(70 + cleared * 25),
                                 85);
    }
    if (!was_game_over && s_game.game_over) {
        opencalc_audio_play_tone(130, 350, 90);
    }
    tetris_note_score();
    if (s_game.game_over) {
        tetris_save_high_score();
    }
    draw_tetris_frame();
}

bool opencalc_tetris_press_button_number(int number)
{
    if (!s_active) {
        return false;
    }

    switch (number) {
    case 46: /* on/home/off -> leave Tetris */
        tetris_save_high_score();
        s_active = false;
        return true;
    case 13: /* back -> pause/resume */
        tetris_toggle_pause(&s_game);
        break;
    case 9: /* left */
        tetris_try_move(&s_game, -1, 0);
        break;
    case 15: /* right */
        tetris_try_move(&s_game, 1, 0);
        break;
    case 14: /* down -> initial step; tick() maintains soft drop while held */
        tetris_try_move(&s_game, 0, 1);
        if (!s_game.game_over) {
            s_game.score += 1;
        }
        break;
    case 10: /* up -> rotate clockwise */
        if (tetris_try_rotate(&s_game, 1)) {
            opencalc_audio_play_tone(700, 28, 45);
        }
        break;
    case 6: /* 2nd -> rotate counter-clockwise */
        if (tetris_try_rotate(&s_game, -1)) {
            opencalc_audio_play_tone(620, 28, 45);
        }
        break;
    case 2: /* window -> hold */
        tetris_hold(&s_game);
        opencalc_audio_play_tone(420, 45, 50);
        break;
    case 1: /* y= -> hard drop, or restart on game over */
        if (s_game.game_over) {
            opencalc_tetris_enter();
        } else {
            tetris_hard_drop(&s_game);
            opencalc_audio_play_tone(190, 55, 75);
        }
        break;
    case 50: /* enter -> restart on game over */
        if (s_game.game_over) {
            opencalc_tetris_enter();
        }
        break;
    default:
        return true; /* swallow all other keys while Tetris owns the screen */
    }

    tetris_note_score();
    if (s_game.game_over) {
        tetris_save_high_score();
    }
    /* The continuously paced tick redraws the game. Avoid presenting a second
     * full LCD frame from the key edge handler in the same UI iteration. */
    return true;
}
