#pragma once

#include "opencalc_graph_model.h"

#include <stdbool.h>
#include <stddef.h>

/* The polar slope formula embeds one maximum-length graph expression four times. */
#define OPENCALC_GRAPH_SYMBOLIC_COMMAND_MAX \
    (4U * OPENCALC_GRAPH_EXPRESSION_MAX + 128U)

bool opencalc_graph_symbolic_polar_derivative_command(const char *expression,
                                                       char *out, size_t out_size);
