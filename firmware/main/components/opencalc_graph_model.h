#pragma once

#include <stdbool.h>
#include <stdint.h>

#define OPENCALC_GRAPH_FUNCTION_COUNT 10
#define OPENCALC_GRAPH_PARAM_COUNT 6
#define OPENCALC_GRAPH_POLAR_COUNT 6
#define OPENCALC_GRAPH_SEQUENCE_COUNT 3
#define OPENCALC_GRAPH_EXPRESSION_MAX 96

typedef struct {
    char expressions[OPENCALC_GRAPH_FUNCTION_COUNT][OPENCALC_GRAPH_EXPRESSION_MAX];
    bool enabled[OPENCALC_GRAPH_FUNCTION_COUNT];
    char param_x[OPENCALC_GRAPH_PARAM_COUNT][OPENCALC_GRAPH_EXPRESSION_MAX];
    char param_y[OPENCALC_GRAPH_PARAM_COUNT][OPENCALC_GRAPH_EXPRESSION_MAX];
    bool param_enabled[OPENCALC_GRAPH_PARAM_COUNT];
    char polar_expressions[OPENCALC_GRAPH_POLAR_COUNT][OPENCALC_GRAPH_EXPRESSION_MAX];
    bool polar_enabled[OPENCALC_GRAPH_POLAR_COUNT];
    char sequence_expressions[OPENCALC_GRAPH_SEQUENCE_COUNT][OPENCALC_GRAPH_EXPRESSION_MAX];
    bool sequence_enabled[OPENCALC_GRAPH_SEQUENCE_COUNT];
    double t_min, t_max, n_min, n_max;
    int selection;
    double x_min, x_max, y_min, y_max, x_tick, y_tick;
    int window_selection, calc_selection, format_selection, style_series;
    uint8_t styles[OPENCALC_GRAPH_FUNCTION_COUNT];
    bool zoom_mode, split, background_enabled, background_loaded;
    double table_x_start, table_step;
    int table_function_start, table_rows, table_precision, table_setup_selection;
    bool grid, trace;
    double trace_x;
    int trace_function;
    char symbolic_derivative[160];
    char symbolic_integral[160];
    char symbolic_roots[160];
    char symbolic_asymptotes[160];
    bool tangent_enabled, integral_shade_enabled;
    int overlay_function;
    double overlay_x;
    int mode;
} opencalc_graph_model_t;

opencalc_graph_model_t *opencalc_graph_model(void);
void opencalc_graph_model_reset(void);
