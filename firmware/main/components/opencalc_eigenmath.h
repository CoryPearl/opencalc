#ifndef OPENCALC_EIGENMATH_H
#define OPENCALC_EIGENMATH_H

#include <stdbool.h>
#include <stddef.h>

/* Evaluates expressions that require the general symbolic engine. */
bool opencalc_eigenmath_eval(const char *expression, char *out, size_t out_size);

#endif
