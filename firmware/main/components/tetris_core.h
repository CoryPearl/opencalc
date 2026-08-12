/*
 * tetris_core.h
 *
 * Platform-independent Tetris game logic (board, pieces, gravity, scoring,
 * 7-bag randomizer, hold, next queue, simple SRS-style rotation + wall
 * kicks). No drawing and no I/O in here at all -- the ESP32 build
 * (opencalc_tetris.c) and the raylib desktop build (tetris_raylib.c) both
 * `#include` this file and are responsible only for input + rendering.
 *
 * Single-header, static-inline style so it can be dropped into either
 * project without a separate translation unit.
 */
#ifndef TETRIS_CORE_H
#define TETRIS_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define T_BOARD_W        10
#define T_BOARD_H        24   /* 20 visible rows + 4 hidden spawn rows on top */
#define T_VISIBLE_TOP    4    /* row index where the visible playfield starts */
#define T_NEXT_COUNT     4
#define T_LOCK_DELAY_MS  500
#define T_LOCK_RESET_MAX 15

typedef enum {
    T_PIECE_I = 0,
    T_PIECE_O,
    T_PIECE_T,
    T_PIECE_S,
    T_PIECE_Z,
    T_PIECE_J,
    T_PIECE_L,
    T_PIECE_COUNT
} tetris_piece_t;

/* 4x4 rotation bitmaps, bit = row*4+col, row0 = top of the 4x4 box. */
static const uint16_t TETRIS_SHAPES[T_PIECE_COUNT][4] = {
    /* I */ { 0x00F0, 0x4444, 0x0F00, 0x2222 },
    /* O */ { 0x0066, 0x0066, 0x0066, 0x0066 },
    /* T */ { 0x0072, 0x0262, 0x0270, 0x0232 },
    /* S */ { 0x0036, 0x0462, 0x0360, 0x0231 },
    /* Z */ { 0x0063, 0x0264, 0x0630, 0x0132 },
    /* J */ { 0x0071, 0x0226, 0x0470, 0x0322 },
    /* L */ { 0x0074, 0x0622, 0x0170, 0x0223 },
};

/* Basic wall-kick offsets tried in order after a rotation fails in place.
 * Not full guideline SRS kick tables, but covers the common cases (wall
 * pushes + one floor kick) and feels right in play. */
static const int8_t TETRIS_KICKS[5][2] = {
    { 0, 0}, {-1, 0}, { 1, 0}, {-2, 0}, { 2, 0},
};

typedef struct {
    int8_t board[T_BOARD_H][T_BOARD_W]; /* 0 = empty, 1..7 = piece color id */

    tetris_piece_t bag[T_PIECE_COUNT];
    int bag_pos;
    uint32_t rng_state;

    tetris_piece_t next_queue[T_NEXT_COUNT];

    tetris_piece_t cur_piece;
    int cur_rot;
    int cur_x, cur_y;

    tetris_piece_t hold_piece;
    bool has_hold;
    bool can_hold;

    long score;
    int level;
    int lines;

    float gravity_timer_ms;
    float lock_timer_ms;
    bool grounded;
    int lock_resets;
    bool prev_soft_drop;

    bool clearing;
    float clear_timer_ms;
    bool clear_rows[T_BOARD_H];
    int clear_count;

    bool game_over;
    bool paused;

    int last_clear_count; /* for score-popup / UI, 0..4 (4 = tetris) */
} tetris_t;

static inline uint32_t tetris_rand(tetris_t *t)
{
    /* xorshift32 */
    uint32_t x = t->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    t->rng_state = x ? x : 0x9E3779B9u;
    return t->rng_state;
}

static inline void tetris_refill_bag(tetris_t *t)
{
    for (int i = 0; i < T_PIECE_COUNT; i++) {
        t->bag[i] = (tetris_piece_t)i;
    }
    for (int i = T_PIECE_COUNT - 1; i > 0; i--) {
        int j = tetris_rand(t) % (uint32_t)(i + 1);
        tetris_piece_t tmp = t->bag[i];
        t->bag[i] = t->bag[j];
        t->bag[j] = tmp;
    }
    t->bag_pos = 0;
}

static inline tetris_piece_t tetris_next_from_bag(tetris_t *t)
{
    if (t->bag_pos >= T_PIECE_COUNT) {
        tetris_refill_bag(t);
    }
    return t->bag[t->bag_pos++];
}

static inline uint16_t tetris_shape(tetris_piece_t p, int rot)
{
    return TETRIS_SHAPES[p][rot & 3];
}

static inline bool tetris_cell_set(uint16_t shape, int row, int col)
{
    return (shape & (1u << (row * 4 + col))) != 0;
}

/* Does piece `p` at rotation `rot`, top-left board position (x,y) fit
 * without colliding with walls/floor/locked cells? */
static inline bool tetris_fits(const tetris_t *t, tetris_piece_t p, int rot, int x, int y)
{
    uint16_t shape = tetris_shape(p, rot);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!tetris_cell_set(shape, r, c)) {
                continue;
            }
            int bx = x + c;
            int by = y + r;
            if (bx < 0 || bx >= T_BOARD_W || by < 0 || by >= T_BOARD_H) {
                return false;
            }
            if (t->board[by][bx] != 0) {
                return false;
            }
        }
    }
    return true;
}

static inline void tetris_spawn(tetris_t *t, tetris_piece_t p)
{
    t->cur_piece = p;
    t->cur_rot = 0;
    t->cur_x = (T_BOARD_W - 4) / 2;
    /* Spawn so the piece is immediately visible starting at the second
     * row of the visible playfield (screen row index 1), not hidden up
     * in the buffer rows above the board. Every shape's occupied rows
     * within its 4x4 box start at local row 0, except I which starts at
     * local row 1 -- account for that so all pieces land on the same
     * visible row when they first appear. */
    t->cur_y = (p == T_PIECE_I) ? T_VISIBLE_TOP : T_VISIBLE_TOP + 1;
    t->gravity_timer_ms = 0;
    t->lock_timer_ms = 0;
    t->grounded = false;
    t->lock_resets = 0;
    t->can_hold = true;

    if (!tetris_fits(t, t->cur_piece, t->cur_rot, t->cur_x, t->cur_y)) {
        t->game_over = true;
    }
}

static inline void tetris_pull_next(tetris_t *t)
{
    tetris_piece_t p = t->next_queue[0];
    for (int i = 0; i < T_NEXT_COUNT - 1; i++) {
        t->next_queue[i] = t->next_queue[i + 1];
    }
    t->next_queue[T_NEXT_COUNT - 1] = tetris_next_from_bag(t);
    tetris_spawn(t, p);
}

static inline int tetris_gravity_interval_ms(int level)
{
    int ms = 1000 - (level - 1) * 70;
    if (ms < 80) {
        ms = 80;
    }
    return ms;
}

static inline void tetris_init(tetris_t *t, uint32_t seed)
{
    memset(t, 0, sizeof(*t));
    t->rng_state = seed ? seed : 0xC0FFEEu;
    t->level = 1;
    t->has_hold = false;
    t->can_hold = true;

    tetris_refill_bag(t);
    for (int i = 0; i < T_NEXT_COUNT; i++) {
        t->next_queue[i] = tetris_next_from_bag(t);
    }
    tetris_spawn(t, tetris_next_from_bag(t));
}

/* Clears any completed rows currently in the board (called once the
 * clear animation finishes) and shifts everything above down. Updates
 * score/level. */
static inline void tetris_finish_clear(tetris_t *t)
{
    int write_row = T_BOARD_H - 1;
    for (int r = T_BOARD_H - 1; r >= 0; r--) {
        if (t->clear_rows[r]) {
            continue;
        }
        if (write_row != r) {
            memcpy(t->board[write_row], t->board[r], sizeof(t->board[r]));
        }
        write_row--;
    }
    for (int r = write_row; r >= 0; r--) {
        memset(t->board[r], 0, sizeof(t->board[r]));
    }

    int n = t->clear_count;
    static const int base_score[5] = {0, 100, 300, 500, 800};
    t->score += (long)base_score[n] * t->level;
    t->lines += n;
    int new_level = 1 + t->lines / 10;
    t->level = new_level;

    memset(t->clear_rows, 0, sizeof(t->clear_rows));
    t->clear_count = 0;
    t->clearing = false;
}

static inline void tetris_lock_piece(tetris_t *t)
{
    uint16_t shape = tetris_shape(t->cur_piece, t->cur_rot);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!tetris_cell_set(shape, r, c)) {
                continue;
            }
            int bx = t->cur_x + c;
            int by = t->cur_y + r;
            if (by >= 0 && by < T_BOARD_H && bx >= 0 && bx < T_BOARD_W) {
                t->board[by][bx] = (int8_t)(t->cur_piece + 1);
            }
        }
    }

    int cleared = 0;
    for (int r = 0; r < T_BOARD_H; r++) {
        bool full = true;
        for (int c = 0; c < T_BOARD_W; c++) {
            if (t->board[r][c] == 0) {
                full = false;
                break;
            }
        }
        if (full) {
            t->clear_rows[r] = true;
            cleared++;
        }
    }
    t->last_clear_count = cleared;

    if (cleared > 0) {
        t->clear_count = cleared;
        t->clearing = true;
        t->clear_timer_ms = 260.0f; /* flash duration before rows collapse */
    } else {
        tetris_pull_next(t);
    }
}

static inline bool tetris_try_move(tetris_t *t, int dx, int dy)
{
    if (t->game_over || t->paused || t->clearing) {
        return false;
    }
    int nx = t->cur_x + dx;
    int ny = t->cur_y + dy;
    if (!tetris_fits(t, t->cur_piece, t->cur_rot, nx, ny)) {
        return false;
    }
    t->cur_x = nx;
    t->cur_y = ny;
    if (t->grounded && t->lock_resets < T_LOCK_RESET_MAX) {
        t->lock_timer_ms = 0;
        t->lock_resets++;
    }
    return true;
}

static inline bool tetris_try_rotate(tetris_t *t, int dir)
{
    if (t->game_over || t->paused || t->clearing) {
        return false;
    }
    if (t->cur_piece == T_PIECE_O) {
        return true; /* O never needs to actually rotate */
    }
    int new_rot = (t->cur_rot + (dir > 0 ? 1 : 3)) & 3;
    for (size_t i = 0; i < sizeof(TETRIS_KICKS) / sizeof(TETRIS_KICKS[0]); i++) {
        int kx = TETRIS_KICKS[i][0];
        int ky = TETRIS_KICKS[i][1];
        if (tetris_fits(t, t->cur_piece, new_rot, t->cur_x + kx, t->cur_y + ky)) {
            t->cur_rot = new_rot;
            t->cur_x += kx;
            t->cur_y += ky;
            if (t->grounded && t->lock_resets < T_LOCK_RESET_MAX) {
                t->lock_timer_ms = 0;
                t->lock_resets++;
            }
            return true;
        }
    }
    return false;
}

static inline int tetris_ghost_y(const tetris_t *t)
{
    int y = t->cur_y;
    while (tetris_fits(t, t->cur_piece, t->cur_rot, t->cur_x, y + 1)) {
        y++;
    }
    return y;
}

static inline void tetris_hard_drop(tetris_t *t)
{
    if (t->game_over || t->paused || t->clearing) {
        return;
    }
    int dropped = 0;
    while (tetris_fits(t, t->cur_piece, t->cur_rot, t->cur_x, t->cur_y + 1)) {
        t->cur_y++;
        dropped++;
    }
    t->score += 2 * dropped;
    tetris_lock_piece(t);
}

static inline void tetris_hold(tetris_t *t)
{
    if (t->game_over || t->paused || t->clearing || !t->can_hold) {
        return;
    }
    tetris_piece_t cur = t->cur_piece;
    if (t->has_hold) {
        tetris_piece_t swapped = t->hold_piece;
        t->hold_piece = cur;
        tetris_spawn(t, swapped);
    } else {
        t->hold_piece = cur;
        t->has_hold = true;
        tetris_pull_next(t);
    }
    t->can_hold = false;
}

static inline void tetris_soft_drop_step(tetris_t *t, long *out_score_delta)
{
    if (tetris_try_move(t, 0, 1)) {
        t->score += 1;
        if (out_score_delta) {
            (*out_score_delta) += 1;
        }
    }
}

/* Advance the simulation by dt_ms milliseconds. `soft_drop` speeds up
 * gravity while held (does not by itself move the piece every call --
 * gravity timer handles both cases uniformly). */
static inline void tetris_step(tetris_t *t, float dt_ms, bool soft_drop)
{
    if (t->game_over || t->paused) {
        return;
    }

    if (t->clearing) {
        t->clear_timer_ms -= dt_ms;
        if (t->clear_timer_ms <= 0) {
            tetris_finish_clear(t);
            tetris_pull_next(t);
        }
        return;
    }

    int interval = tetris_gravity_interval_ms(t->level);
    if (soft_drop) {
        /* Soft drop is faster than normal gravity but shouldn't feel like
         * a full hard drop. */
        interval = interval / 9;
        if (interval < 30) {
            interval = 30;
        }
    }

    /* Gravity time banked while falling at the *old* interval must not
     * suddenly all fire at once when the interval changes (e.g. the
     * instant Down is pressed) -- that's what caused the piece to jump
     * several rows the moment soft drop engaged. Clear the bank on any
     * transition between normal and soft-drop gravity. */
    if (soft_drop != t->prev_soft_drop) {
        t->gravity_timer_ms = 0;
        t->prev_soft_drop = soft_drop;
    }

    t->gravity_timer_ms += dt_ms;
    while (t->gravity_timer_ms >= interval) {
        t->gravity_timer_ms -= interval;
        if (tetris_fits(t, t->cur_piece, t->cur_rot, t->cur_x, t->cur_y + 1)) {
            t->cur_y++;
            if (soft_drop) {
                t->score += 1;
            }
            t->grounded = false;
            t->lock_timer_ms = 0;
        } else {
            t->grounded = true;
        }
    }

    if (t->grounded) {
        if (!tetris_fits(t, t->cur_piece, t->cur_rot, t->cur_x, t->cur_y + 1)) {
            t->lock_timer_ms += dt_ms;
            if (t->lock_timer_ms >= T_LOCK_DELAY_MS) {
                tetris_lock_piece(t);
            }
        } else {
            t->grounded = false;
            t->lock_timer_ms = 0;
        }
    }
}

static inline void tetris_toggle_pause(tetris_t *t)
{
    if (!t->game_over) {
        t->paused = !t->paused;
    }
}

#endif /* TETRIS_CORE_H */
