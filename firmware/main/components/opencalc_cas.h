#pragma once

#include <stdbool.h>
#include <stddef.h>

/*
 * Symbolic CAS front-end for calculator expressions.
 *
 * This is separate from opencalc_math.c's numeric parser so exact symbolic
 * behavior can grow without destabilizing graph/game/runtime code. It returns
 * true only when the expression was handled symbolically.
 */
bool opencalc_cas_eval(const char *expr, char *out, size_t out_size);
