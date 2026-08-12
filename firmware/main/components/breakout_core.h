/*
 * breakout_core.h
 *
 * Platform-independent Breakout logic. Same pattern as tetris_core.h /
 * snake_core.h / dino_core.h: no drawing, no I/O. Works in the same
 * 200x200 "arena" pixel space that opencalc_breakout.c / breakout_raylib.c
 * both place at the same on-screen position, so frontends just offset by
 * the arena's top-left corner and draw 1:1.
 */
#ifndef BREAKOUT_CORE_H
#define BREAKOUT_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define BRK_ARENA_W   200.0f
#define BRK_ARENA_H   200.0f

#define BRK_ROWS      5
#define BRK_COLS      10
#define BRK_BRICK_W   (BRK_ARENA_W / BRK_COLS) /* 20 */
#define BRK_BRICK_H   8.0f
#define BRK_BRICK_TOP 14.0f
#define BRK_BRICK_GAP 2.0f
#define BRK_BRICK_MAX_HP 3

#define BRK_PADDLE_W_BASE   34.0f
#define BRK_PADDLE_W_MIN    20.0f
#define BRK_PADDLE_SHRINK_PER_LEVEL 1.4f
#define BRK_PADDLE_H  6.0f
#define BRK_PADDLE_Y  (BRK_ARENA_H - 14.0f)
#define BRK_PADDLE_STEP 12.0f /* discrete move step, e.g. per ESP32 button press */

#define BRK_BALL_R    3.0f
#define BRK_BASE_SPEED 95.0f  /* px/sec at level 1 */
#define BRK_SPEED_PER_LEVEL 15.0f
#define BRK_MAX_SPEED  260.0f
#define BRK_MIN_VY_RATIO 0.45f /* keeps bounces from going near-horizontal -> no shaky paddle jitter */

#define BRK_MAX_BALLS      4
#define BRK_MAX_POWERUPS   4
#define BRK_POWERUP_FALL_SPEED 55.0f
#define BRK_POWERUP_DROP_PCT   18   /* 0..99 chance a destroyed brick drops one */
#define BRK_WIDE_PADDLE_BONUS  16.0f
#define BRK_WIDE_PADDLE_MS     9000.0f
#define BRK_SLOW_BALL_FACTOR   0.55f
#define BRK_SLOW_BALL_MS       7000.0f

typedef enum {
    BRK_POWERUP_EXTRA_BALL = 1,
    BRK_POWERUP_EXTRA_LIFE = 2,
    BRK_POWERUP_SLOW_BALL  = 3,
    BRK_POWERUP_WIDE_PADDLE = 4,
} brk_powerup_type_t;

typedef struct {
    float x, y;
    int8_t type;
    bool active;
} brk_powerup_t;

typedef struct {
    float x, y, vx, vy;
    bool active;
} brk_ball_t;

typedef struct {
    int8_t bricks[BRK_ROWS][BRK_COLS]; /* remaining hit points; 0 = destroyed */

    brk_ball_t balls[BRK_MAX_BALLS];
    bool ball_launched; /* false = a single ball is riding the paddle, waiting to launch */

    float paddle_x;     /* left edge */
    float paddle_w_base; /* shrinks slightly each level */
    float wide_timer_ms; /* > 0 while the wide-paddle powerup is active */
    float slow_timer_ms; /* > 0 while the slow-ball powerup is active */

    brk_powerup_t powerups[BRK_MAX_POWERUPS];

    int lives;
    long score;
    int level;

    bool game_over;
    bool paused;

    uint32_t rng_state;
} breakout_t;

static inline uint32_t brk_rand(breakout_t *t)
{
    uint32_t x = t->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    t->rng_state = x ? x : 0xBEEF1u;
    return t->rng_state;
}

/* Brick hit points scale with level: higher levels put tougher (2-3 hp)
 * bricks in the top rows, while the bottom rows stay 1 hp longer so
 * early levels aren't a wall of sponge bricks. */
static inline int8_t brk_brick_hp_for(int level, int row)
{
    int tier = (level - 1) / 2 - row;
    if (tier < 0) tier = 0;
    if (tier > BRK_BRICK_MAX_HP - 1) tier = BRK_BRICK_MAX_HP - 1;
    return (int8_t)(1 + tier);
}

static inline void breakout_fill_bricks(breakout_t *t)
{
    for (int r = 0; r < BRK_ROWS; r++) {
        for (int c = 0; c < BRK_COLS; c++) {
            t->bricks[r][c] = brk_brick_hp_for(t->level, r);
        }
    }
}

static inline float breakout_paddle_w(const breakout_t *t)
{
    float w = t->paddle_w_base;
    if (t->wide_timer_ms > 0) w += BRK_WIDE_PADDLE_BONUS;
    return w;
}

static inline float breakout_speed_for_level(const breakout_t *t)
{
    float s = BRK_BASE_SPEED + (t->level - 1) * BRK_SPEED_PER_LEVEL;
    if (s > BRK_MAX_SPEED) s = BRK_MAX_SPEED;
    if (t->slow_timer_ms > 0) s *= BRK_SLOW_BALL_FACTOR;
    return s;
}

static inline void breakout_reset_balls(breakout_t *t)
{
    for (int i = 0; i < BRK_MAX_BALLS; i++) {
        t->balls[i].active = false;
    }
    t->ball_launched = false;
    float pw = breakout_paddle_w(t);
    t->balls[0].active = true;
    t->balls[0].x = t->paddle_x + pw / 2.0f;
    t->balls[0].y = BRK_PADDLE_Y - BRK_BALL_R - 0.5f;
    t->balls[0].vx = 0;
    t->balls[0].vy = 0;
}

static inline void breakout_clear_powerups(breakout_t *t)
{
    for (int i = 0; i < BRK_MAX_POWERUPS; i++) t->powerups[i].active = false;
}

static inline void breakout_init(breakout_t *t, uint32_t seed)
{
    memset(t, 0, sizeof(*t));
    t->rng_state = seed ? seed : 0xBEEF1u;
    t->lives = 3;
    t->level = 1;
    t->paddle_w_base = BRK_PADDLE_W_BASE;
    t->paddle_x = (BRK_ARENA_W - t->paddle_w_base) / 2.0f;
    breakout_fill_bricks(t);
    breakout_clear_powerups(t);
    breakout_reset_balls(t);
}

static inline void breakout_move_paddle(breakout_t *t, float dx)
{
    if (t->game_over || t->paused) {
        return;
    }
    float pw = breakout_paddle_w(t);
    t->paddle_x += dx;
    if (t->paddle_x < 0) t->paddle_x = 0;
    if (t->paddle_x > BRK_ARENA_W - pw) t->paddle_x = BRK_ARENA_W - pw;
    if (!t->ball_launched) {
        t->balls[0].x = t->paddle_x + pw / 2.0f;
    }
}

/* Keep a ball's vertical speed component from collapsing toward zero --
 * a near-horizontal bounce off the paddle used to cause the ball to
 * clip the paddle again next frame (and again the frame after), which
 * reads as visible shaking. Re-derive vx/vy from the clamped angle so
 * total speed is preserved. */
static inline void brk_clamp_angle(float *vx, float *vy)
{
    float speed = sqrtf((*vx) * (*vx) + (*vy) * (*vy));
    if (speed < 1e-4f) return;
    float min_vy = speed * BRK_MIN_VY_RATIO;
    if (fabsf(*vy) < min_vy) {
        float sign_y = (*vy < 0) ? -1.0f : 1.0f;
        float new_vy = min_vy * sign_y;
        float remaining = speed * speed - new_vy * new_vy;
        float new_vx_mag = remaining > 0 ? sqrtf(remaining) : 0.0f;
        float sign_x = (*vx < 0) ? -1.0f : 1.0f;
        *vy = new_vy;
        *vx = new_vx_mag * sign_x;
    }
}

static inline void breakout_launch(breakout_t *t)
{
    if (t->game_over || t->paused || t->ball_launched) {
        return;
    }
    float speed = breakout_speed_for_level(t);
    float kick = ((int)(brk_rand(t) % 61) - 30) / 100.0f; /* -0.30..0.30 */
    float vx = speed * kick;
    float vy = -speed;
    brk_clamp_angle(&vx, &vy);
    t->balls[0].vx = vx;
    t->balls[0].vy = vy;
    t->ball_launched = true;
}

static inline bool breakout_all_cleared(const breakout_t *t)
{
    for (int r = 0; r < BRK_ROWS; r++) {
        for (int c = 0; c < BRK_COLS; c++) {
            if (t->bricks[r][c]) {
                return false;
            }
        }
    }
    return true;
}

static inline void breakout_next_level(breakout_t *t)
{
    t->level++;
    t->paddle_w_base = BRK_PADDLE_W_BASE - (t->level - 1) * BRK_PADDLE_SHRINK_PER_LEVEL;
    if (t->paddle_w_base < BRK_PADDLE_W_MIN) t->paddle_w_base = BRK_PADDLE_W_MIN;
    t->wide_timer_ms = 0;
    t->slow_timer_ms = 0;
    breakout_fill_bricks(t);
    breakout_clear_powerups(t);
    breakout_reset_balls(t);
}

static inline int breakout_active_ball_count(const breakout_t *t)
{
    int n = 0;
    for (int i = 0; i < BRK_MAX_BALLS; i++) if (t->balls[i].active) n++;
    return n;
}

static inline void breakout_spawn_extra_ball(breakout_t *t)
{
    int src = -1;
    for (int i = 0; i < BRK_MAX_BALLS; i++) {
        if (t->balls[i].active) { src = i; break; }
    }
    if (src < 0) return;
    int slot = -1;
    for (int i = 0; i < BRK_MAX_BALLS; i++) {
        if (!t->balls[i].active) { slot = i; break; }
    }
    if (slot < 0) return; /* already at max balls */

    brk_ball_t *s = &t->balls[src];
    brk_ball_t *b = &t->balls[slot];
    b->active = true;
    b->x = s->x;
    b->y = s->y;
    /* mirror-ish angle so the new ball fans out rather than overlapping */
    float vx = -s->vx + ((int)(brk_rand(t) % 41) - 20);
    float vy = -fabsf(s->vy);
    brk_clamp_angle(&vx, &vy);
    b->vx = vx;
    b->vy = vy;
}

static inline void breakout_apply_powerup(breakout_t *t, int8_t type)
{
    switch (type) {
    case BRK_POWERUP_EXTRA_BALL:
        breakout_spawn_extra_ball(t);
        break;
    case BRK_POWERUP_EXTRA_LIFE:
        t->lives++;
        break;
    case BRK_POWERUP_SLOW_BALL:
        t->slow_timer_ms = BRK_SLOW_BALL_MS;
        break;
    case BRK_POWERUP_WIDE_PADDLE:
        t->wide_timer_ms = BRK_WIDE_PADDLE_MS;
        break;
    default:
        break;
    }
}

static inline void brk_maybe_drop_powerup(breakout_t *t, float bx, float by)
{
    if ((int)(brk_rand(t) % 100) >= BRK_POWERUP_DROP_PCT) return;
    int slot = -1;
    for (int i = 0; i < BRK_MAX_POWERUPS; i++) {
        if (!t->powerups[i].active) { slot = i; break; }
    }
    if (slot < 0) return;

    static const int8_t kinds[8] = {
        BRK_POWERUP_EXTRA_BALL, BRK_POWERUP_EXTRA_BALL,
        BRK_POWERUP_WIDE_PADDLE, BRK_POWERUP_WIDE_PADDLE,
        BRK_POWERUP_SLOW_BALL, BRK_POWERUP_SLOW_BALL,
        BRK_POWERUP_EXTRA_BALL, BRK_POWERUP_EXTRA_LIFE, /* rarest */
    };
    int8_t type = kinds[brk_rand(t) % 8];

    brk_powerup_t *p = &t->powerups[slot];
    p->active = true;
    p->x = bx;
    p->y = by;
    p->type = type;
}

static inline void breakout_step(breakout_t *t, float dt_ms)
{
    if (t->game_over || t->paused) {
        return;
    }

    float dt = dt_ms / 1000.0f;
    if (dt > 0.05f) {
        dt = 0.05f; /* clamp huge steps so balls can't tunnel through */
    }

    if (t->wide_timer_ms > 0) {
        t->wide_timer_ms -= dt_ms;
        if (t->wide_timer_ms < 0) t->wide_timer_ms = 0;
    }
    if (t->slow_timer_ms > 0) {
        t->slow_timer_ms -= dt_ms;
        if (t->slow_timer_ms <= 0) {
            t->slow_timer_ms = 0;
            /* restore full speed on every active ball */
            for (int i = 0; i < BRK_MAX_BALLS; i++) {
                if (!t->balls[i].active) continue;
                float speed = sqrtf(t->balls[i].vx * t->balls[i].vx + t->balls[i].vy * t->balls[i].vy);
                if (speed > 1.0f) {
                    float target = breakout_speed_for_level(t);
                    float scale = target / speed;
                    t->balls[i].vx *= scale;
                    t->balls[i].vy *= scale;
                }
            }
        }
    }

    float pw = breakout_paddle_w(t);
    if (t->paddle_x > BRK_ARENA_W - pw) t->paddle_x = BRK_ARENA_W - pw;

    if (!t->ball_launched) {
        t->balls[0].x = t->paddle_x + pw / 2.0f;
    } else {
        for (int i = 0; i < BRK_MAX_BALLS; i++) {
            brk_ball_t *b = &t->balls[i];
            if (!b->active) continue;

            b->x += b->vx * dt;
            b->y += b->vy * dt;

            if (b->x - BRK_BALL_R < 0) {
                b->x = BRK_BALL_R;
                b->vx = -b->vx;
            } else if (b->x + BRK_BALL_R > BRK_ARENA_W) {
                b->x = BRK_ARENA_W - BRK_BALL_R;
                b->vx = -b->vx;
            }
            if (b->y - BRK_BALL_R < 0) {
                b->y = BRK_BALL_R;
                b->vy = -b->vy;
            }

            /* paddle */
            if (b->vy > 0 &&
                b->y + BRK_BALL_R >= BRK_PADDLE_Y &&
                b->y + BRK_BALL_R <= BRK_PADDLE_Y + BRK_PADDLE_H + 6.0f &&
                b->x >= t->paddle_x - BRK_BALL_R &&
                b->x <= t->paddle_x + pw + BRK_BALL_R) {
                float center = t->paddle_x + pw / 2.0f;
                float offset = (b->x - center) / (pw / 2.0f);
                if (offset < -1.0f) offset = -1.0f;
                if (offset > 1.0f) offset = 1.0f;
                float speed = sqrtf(b->vx * b->vx + b->vy * b->vy);
                float vx = speed * offset * 0.9f;
                float vy2 = speed * speed - vx * vx;
                float vy = -sqrtf(vy2 > 0 ? vy2 : speed * speed * 0.2f);
                brk_clamp_angle(&vx, &vy);
                b->vx = vx;
                b->vy = vy;
                b->y = BRK_PADDLE_Y - BRK_BALL_R;
            }

            /* bricks -- at most one hit per ball per step */
            for (int r = 0; r < BRK_ROWS; r++) {
                bool hit = false;
                for (int c = 0; c < BRK_COLS; c++) {
                    if (!t->bricks[r][c]) continue;
                    float bx = c * BRK_BRICK_W;
                    float by = BRK_BRICK_TOP + r * (BRK_BRICK_H + BRK_BRICK_GAP);
                    float bw = BRK_BRICK_W - BRK_BRICK_GAP;
                    float bh = BRK_BRICK_H;

                    float closest_x = b->x < bx ? bx : (b->x > bx + bw ? bx + bw : b->x);
                    float closest_y = b->y < by ? by : (b->y > by + bh ? by + bh : b->y);
                    float dx = b->x - closest_x;
                    float dy = b->y - closest_y;
                    if (dx * dx + dy * dy > BRK_BALL_R * BRK_BALL_R) {
                        continue;
                    }

                    t->bricks[r][c]--;
                    t->score += (BRK_ROWS - r) * 10;
                    if (t->bricks[r][c] == 0) {
                        brk_maybe_drop_powerup(t, bx + bw / 2.0f, by + bh / 2.0f);
                    }

                    float pen_x = BRK_BALL_R - fabsf(dx);
                    float pen_y = BRK_BALL_R - fabsf(dy);
                    if (pen_x < pen_y) {
                        b->vx = -b->vx;
                    } else {
                        b->vy = -b->vy;
                    }
                    brk_clamp_angle(&b->vx, &b->vy);
                    hit = true;
                    break;
                }
                if (hit) break;
            }

            if (b->y - BRK_BALL_R > BRK_ARENA_H) {
                b->active = false;
            }
        }
    }

    /* falling powerup capsules */
    for (int i = 0; i < BRK_MAX_POWERUPS; i++) {
        brk_powerup_t *p = &t->powerups[i];
        if (!p->active) continue;
        p->y += BRK_POWERUP_FALL_SPEED * dt;
        if (p->y - 4 > BRK_ARENA_H) {
            p->active = false;
            continue;
        }
        if (p->y + 4 >= BRK_PADDLE_Y && p->y - 4 <= BRK_PADDLE_Y + BRK_PADDLE_H &&
            p->x >= t->paddle_x && p->x <= t->paddle_x + pw) {
            breakout_apply_powerup(t, p->type);
            p->active = false;
        }
    }

    if (breakout_all_cleared(t)) {
        breakout_next_level(t);
        return;
    }

    if (t->ball_launched && breakout_active_ball_count(t) == 0) {
        t->lives--;
        if (t->lives <= 0) {
            t->game_over = true;
        } else {
            t->wide_timer_ms = 0;
            t->slow_timer_ms = 0;
            breakout_clear_powerups(t);
            breakout_reset_balls(t);
        }
    }
}

static inline void breakout_toggle_pause(breakout_t *t)
{
    if (!t->game_over) {
        t->paused = !t->paused;
    }
}

#endif /* BREAKOUT_CORE_H */
