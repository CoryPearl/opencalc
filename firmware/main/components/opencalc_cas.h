#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "opencalc_giac.h"

/*
 * Symbolic CAS front-end for calculator expressions.
 *
 * This is separate from opencalc_math.c's numeric parser so exact symbolic
 * behavior can grow without destabilizing graph/game/runtime code. It returns
 * true only when the expression was handled symbolically.
 */
bool opencalc_cas_eval(const char *expr, char *out, size_t out_size);

bool opencalc_cas_eval_timed(const char *expr,
                             char *out,
                             size_t out_size,
                             unsigned timeout_ms,
                             opencalc_giac_cancel_fn should_cancel,
                             void *cancel_context,
                             opencalc_giac_status_t *giac_status);
