#include "opencalc_math.h"

#include <math.h>
#include <stdio.h>

static int expect_close(const char *name, double actual, double expected, double tolerance)
{
    if (!isfinite(actual) || fabs(actual - expected) > tolerance) {
        fprintf(stderr, "FAIL %s: expected %.12g got %.12g\n", name, expected, actual);
        return 1;
    }
    printf("PASS %s -> %.10g\n", name, actual);
    return 0;
}

static int expect_eval(const char *name, const char *expression, char variable,
                       double input, double expected)
{
    double actual = 0.0;
    if (!graph_eval_expression_var(expression, variable, input, &actual)) {
        fprintf(stderr, "FAIL %s: expression rejected\n", name);
        return 1;
    }
    return expect_close(name, actual, expected, 1e-8);
}

int main(void)
{
    double xy = 0.0;
    if (!graph_eval_expression_xy("x^2+y^2", 3.0, 4.0, &xy) || fabs(xy - 25.0) > 1e-9) {
        return 20;
    }
    int failed = 0;
    opencalc_math_set_degrees(true);
    failed |= expect_eval("cartesian y=x^2", "x^2", 'x', -3.0, 9.0);
    failed |= expect_eval("parametric x(t)", "cos(t)", 't', 90.0, 0.0);
    failed |= expect_eval("parametric y(t)", "sin(t)", 't', 90.0, 1.0);
    failed |= expect_eval("polar r(theta)", "2*cos(t)", 't', 60.0, 1.0);
    failed |= expect_eval("sequence u(n)", "n^2+n", 'n', 5.0, 30.0);

    opencalc_math_set_degrees(false);
    failed |= expect_eval("radian parametric", "sin(t)", 't', 1.5707963267948966, 1.0);

    graph_view_t view = {
        .xmin = -10.0,
        .xmax = 10.0,
        .ymin = -5.0,
        .ymax = 5.0,
        .screen_w = 320,
        .screen_top = 0,
        .screen_bottom = 239,
    };
    failed |= expect_close("screen x origin", graph_screen_x(&view, 0.0), 159.0, 1.0);
    failed |= expect_close("world x round trip", graph_world_x(&view, graph_screen_x(&view, 3.0)), 3.0, 0.07);
    failed |= expect_close("screen y origin", graph_screen_y(&view, 0.0), 120.0, 1.0);
    return failed ? 1 : 0;
}
