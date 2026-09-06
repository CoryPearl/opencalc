#include "opencalc_conics.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int close_to(double actual, double expected)
{
    return fabs(actual - expected) < 1e-7;
}

int main(void)
{
    opencalc_conic_t q;
    opencalc_conic_analysis_t a;
    char upper[96];
    char lower[96];

    opencalc_conic_circle(&q, 2.0, -3.0, 5.0);
    if (!opencalc_conic_analyze(&q, &a) || a.kind != OPENCALC_CONIC_CIRCLE ||
        !close_to(a.center_x, 2.0) || !close_to(a.center_y, -3.0) ||
        !close_to(a.semi_axis_1, 5.0) || opencalc_conic_y_expressions(&q, upper, sizeof(upper), lower, sizeof(lower)) != 2) {
        return 1;
    }

    opencalc_conic_axis(&q, OPENCALC_CONIC_ELLIPSE, 1.0, 2.0, 6.0, 4.0, 30.0);
    if (!opencalc_conic_analyze(&q, &a) || a.kind != OPENCALC_CONIC_ELLIPSE ||
        !a.rotated || !close_to(a.center_x, 1.0) || !close_to(a.center_y, 2.0)) return 2;

    opencalc_conic_axis(&q, OPENCALC_CONIC_HYPERBOLA, 0.0, 0.0, 4.0, 2.0, 90.0);
    if (!opencalc_conic_analyze(&q, &a) || a.kind != OPENCALC_CONIC_HYPERBOLA ||
        !(a.eccentricity > 1.0)) return 3;

    opencalc_conic_axis(&q, OPENCALC_CONIC_PARABOLA, 1.0, -1.0, 2.0, 0.0, 90.0);
    if (!opencalc_conic_analyze(&q, &a) || a.kind != OPENCALC_CONIC_PARABOLA ||
        fabs(opencalc_conic_evaluate(&q, 1.0, -1.0)) > 1e-8) return 4;

    if (!opencalc_conic_circle_through_points(&q, 1.0, 0.0, 0.0, 1.0, -1.0, 0.0) ||
        !opencalc_conic_analyze(&q, &a) || !close_to(a.center_x, 0.0) ||
        !close_to(a.center_y, 0.0) || !close_to(a.semi_axis_1, 1.0)) return 5;

    opencalc_conic_general(&q, 1.0, 0.0, -1.0, 0.0, 0.0, 0.0);
    if (!opencalc_conic_analyze(&q, &a) || !a.degenerate ||
        a.kind != OPENCALC_CONIC_HYPERBOLA) return 6;

    opencalc_conic_axis(&q, OPENCALC_CONIC_ELLIPSE, -4.0, 7.0, 3.0, 8.0, 15.0);
    if (!opencalc_conic_analyze(&q, &a) || !close_to(a.focal_length, sqrt(55.0)) ||
        !close_to(a.eccentricity, sqrt(55.0) / 8.0)) return 7;

    char tiny[12];
    if (opencalc_conic_y_expressions(&q, tiny, sizeof(tiny), lower, sizeof(lower)) != 0 ||
        tiny[0] != '\0') return 8;

    if (opencalc_conic_y_expressions(&q, upper, sizeof(upper), lower, sizeof(lower)) != 2 ||
        strchr(upper, '^') == NULL || strchr(lower, '^') == NULL) return 9;

    const double circle_points[10] = {
        1.0, 0.0, 0.0, 1.0, -1.0, 0.0, 0.0, -1.0,
        0.7071067811865476, 0.7071067811865476
    };
    if (!opencalc_conic_through_five_points(&q, circle_points) ||
        !opencalc_conic_analyze(&q, &a) || a.kind != OPENCALC_CONIC_CIRCLE ||
        !close_to(a.center_x, 0.0) || !close_to(a.center_y, 0.0) ||
        !close_to(a.semi_axis_1, 1.0)) return 10;

    puts("PASS conics geometry");
    return 0;
}
