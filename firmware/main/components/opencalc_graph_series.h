#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef bool (*opencalc_graph_scalar_eval_fn)(const char *expression, char variable,
                                               double input, double *value);

typedef enum {
    OPENCALC_GRAPH_SEQUENCE_DIRECT = 0,
    OPENCALC_GRAPH_SEQUENCE_RECURRENCE,
} opencalc_graph_sequence_kind_t;

typedef struct {
    opencalc_graph_sequence_kind_t kind;
    char expression[96];
    char initial0[32];
    char initial1[32];
} opencalc_graph_sequence_t;

#define OPENCALC_GRAPH_SEQUENCE_ZERO_LIMIT 8

typedef enum {
    OPENCALC_GRAPH_SEQUENCE_END_STEADY = 0,
    OPENCALC_GRAPH_SEQUENCE_END_INCREASING,
    OPENCALC_GRAPH_SEQUENCE_END_DECREASING,
    OPENCALC_GRAPH_SEQUENCE_END_OSCILLATING,
    OPENCALC_GRAPH_SEQUENCE_END_DIVERGING,
} opencalc_graph_sequence_end_t;

typedef struct {
    int first_index;
    int last_index;
    int evaluated;
    int zero_count;
    int zero_indices[OPENCALC_GRAPH_SEQUENCE_ZERO_LIMIT];
    double first;
    double previous;
    double last;
    double sum;
    double delta;
    opencalc_graph_sequence_end_t end_behavior;
} opencalc_graph_sequence_analysis_t;

bool opencalc_graph_sequence_parse(const char *text, opencalc_graph_sequence_t *sequence);
bool opencalc_graph_sequence_eval(const opencalc_graph_sequence_t *sequence, int n,
                                  opencalc_graph_scalar_eval_fn evaluate, double *value);
bool opencalc_graph_sequence_analyze(const opencalc_graph_sequence_t *sequence,
                                     int first_index, int last_index,
                                     opencalc_graph_scalar_eval_fn evaluate,
                                     opencalc_graph_sequence_analysis_t *analysis);
const char *opencalc_graph_sequence_end_name(opencalc_graph_sequence_end_t behavior);
bool opencalc_graph_segment_is_continuous(double previous_x, double previous_y,
                                          double midpoint_x, double midpoint_y,
                                          double current_x, double current_y,
                                          double visible_x_span, double visible_y_span);
