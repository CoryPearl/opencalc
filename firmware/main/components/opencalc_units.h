#pragma once

#include <stddef.h>

typedef enum {
    OPENCALC_UNITS_NOT_APPLICABLE = 0,
    OPENCALC_UNITS_OK,
    OPENCALC_UNITS_ERROR,
} opencalc_units_status_t;

/* Evaluate a scalar expression containing supported physical units. */
opencalc_units_status_t opencalc_units_eval(const char *expression,
                                            char *output,
                                            size_t output_size);
