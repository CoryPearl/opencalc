#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    double xmin;
    double xmax;
    double ymin;
    double ymax;
    int screen_w;
    int screen_top;
    int screen_bottom;
} graph_view_t;

bool graph_eval_expression(const char *expr, double x, double *out);
bool graph_eval_expression_var(const char *expr, char variable, double value, double *out);
void opencalc_math_set_degrees(bool degrees);
bool opencalc_math_degrees_enabled(void);
bool opencalc_math_eval_expression(const char *expr, double *out);
bool opencalc_math_eval_complex_expression(const char *expr, double *real, double *imag);
bool opencalc_math_eval_complex_expression_var(const char *expr, char variable, double real_value, double imag_value, double *real, double *imag);
bool opencalc_math_numeric_derivative(const char *expr, char variable, double at, double *real, double *imag);
bool opencalc_math_numeric_integral(const char *expr, char variable, double a, double b, double *real, double *imag);
bool opencalc_math_derivative_expression(const char *expr, char *out, size_t out_size);
bool opencalc_math_integral_expression(const char *expr, char *out, size_t out_size);
int graph_screen_x(const graph_view_t *view, double x);
int graph_screen_y(const graph_view_t *view, double y);
double graph_world_x(const graph_view_t *view, int px);
