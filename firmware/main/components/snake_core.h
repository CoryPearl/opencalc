/*
 * snake_core.h
 *
 * Platform-independent Snake game logic. Same pattern as tetris_core.h:
 * no drawing, no I/O -- opencalc_snake.c (ESP32) and snake_raylib.c
 * (desktop) both #include this and only handle input + rendering.
 */
#ifndef SNAKE_CORE_H
#define SNAKE_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define SNAKE_COLS     20
#define SNAKE_ROWS     20
#define SNAKE_MAX_LEN  (SNAKE_COLS * SNAKE_ROWS)

typedef struct {
    int8_t body_x[SNAKE_MAX_LEN];
    int8_t body_y[SNAKE_MAX_LEN]; /* body_[0] is the head */
    int length;

    int8_t dir_x, dir_y;
    int8_t pending_dir_x, pending_dir_y; /* queued turn, applied on next move tick */

    int food_x, food_y;

    long score;
    int level;
    float move_timer_ms;

    bool game_over;
    bool paused;

    uint32_t rng_state;
} snake_t;

static inline uint32_t snake_rand(snake_t *s)
{
    uint32_t x = s->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->rng_state = x ? x : 0xA53Fu;
    return s->rng_state;
}

static inline bool snake_cell_has_body(const snake_t *s, int x, int y)
{
    for (int i = 0; i < s->length; i++) {
        if (s->body_x[i] == x && s->body_y[i] == y) {
            return true;
        }
    }
    return false;
}

static inline void snake_place_food(snake_t *s)
{
    /* board is small enough that rejection sampling is fine */
    for (int tries = 0; tries < 1000; tries++) {
        int x = (int)(snake_rand(s) % SNAKE_COLS);
        int y = (int)(snake_rand(s) % SNAKE_ROWS);
        if (!snake_cell_has_body(s, x, y)) {
            s->food_x = x;
            s->food_y = y;
            return;
        }
    }
}

static inline void snake_init(snake_t *s, uint32_t seed)
{
    memset(s, 0, sizeof(*s));
    s->rng_state = seed ? seed : 0xA53Fu;

    s->length = 3;
    int cx = SNAKE_COLS / 2;
    int cy = SNAKE_ROWS / 2;
    for (int i = 0; i < s->length; i++) {
        s->body_x[i] = (int8_t)(cx - i);
        s->body_y[i] = (int8_t)cy;
    }
    s->dir_x = 1;
    s->dir_y = 0;
    s->pending_dir_x = 1;
    s->pending_dir_y = 0;
    s->level = 1;

    snake_place_food(s);
}

/* Ignore direct reversals (they'd just kill you into your own neck). */
static inline void snake_set_direction(snake_t *s, int dx, int dy)
{
    if (dx == -s->dir_x && dy == -s->dir_y && s->length > 1) {
        return;
    }
    if (dx == 0 && dy == 0) {
        return;
    }
    s->pending_dir_x = (int8_t)dx;
    s->pending_dir_y = (int8_t)dy;
}

static inline int snake_interval_ms(int level)
{
    int ms = 160 - (level - 1) * 10;
    if (ms < 60) {
        ms = 60;
    }
    return ms;
}

static inline void snake_step(snake_t *s, float dt_ms)
{
    if (s->game_over || s->paused) {
        return;
    }

    s->move_timer_ms += dt_ms;
    int interval = snake_interval_ms(s->level);
    if (s->move_timer_ms < interval) {
        return;
    }
    s->move_timer_ms -= interval;

    s->dir_x = s->pending_dir_x;
    s->dir_y = s->pending_dir_y;

    int nx = s->body_x[0] + s->dir_x;
    int ny = s->body_y[0] + s->dir_y;

    if (nx < 0 || nx >= SNAKE_COLS || ny < 0 || ny >= SNAKE_ROWS) {
        s->game_over = true;
        return;
    }

    bool eating = (nx == s->food_x && ny == s->food_y);

    /* Self-collision check: the tail cell is about to move away unless
     * we're eating (in which case the tail stays put), so exclude it
     * from the check in the non-eating case. */
    int check_len = eating ? s->length : s->length - 1;
    for (int i = 0; i < check_len; i++) {
        if (s->body_x[i] == nx && s->body_y[i] == ny) {
            s->game_over = true;
            return;
        }
    }

    if (eating && s->length < SNAKE_MAX_LEN) {
        s->length++;
    }
    for (int i = s->length - 1; i > 0; i--) {
        s->body_x[i] = s->body_x[i - 1];
        s->body_y[i] = s->body_y[i - 1];
    }
    s->body_x[0] = (int8_t)nx;
    s->body_y[0] = (int8_t)ny;

    if (eating) {
        s->score += 10;
        if (s->score % 50 == 0) {
            s->level++;
        }
        snake_place_food(s);
    }
}

static inline void snake_toggle_pause(snake_t *s)
{
    if (!s->game_over) {
        s->paused = !s->paused;
    }
}

#endif /* SNAKE_CORE_H */
