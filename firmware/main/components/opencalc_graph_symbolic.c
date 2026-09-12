#include "opencalc_graph_symbolic.h"

#include <stdio.h>

bool opencalc_graph_symbolic_polar_derivative_command(const char *expression,
                                                       char *out, size_t out_size)
{
    if (expression == NULL || out == NULL || out_size == 0) return false;

    int written = snprintf(
        out, out_size,
        "normal(((diff((%s),t)*sin(t)+(%s)*cos(t)))/(diff((%s),t)*cos(t)-(%s)*sin(t)))",
        expression, expression, expression, expression);
    return written >= 0 && (size_t)written < out_size;
}
