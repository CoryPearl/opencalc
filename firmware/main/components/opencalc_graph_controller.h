#pragma once

#include "opencalc_graph_model.h"
#include "opencalc_graph_series.h"

#include <stdbool.h>
#include <stdint.h>

#define OPENCALC_GRAPH_SYMBOLIC_TEXT_MAX 160

typedef struct {
    int graphing_mode;
    int series;
    uint32_t fingerprint;
    int range_first;
    int range_last;
    char primary[OPENCALC_GRAPH_EXPRESSION_MAX];
    char secondary[OPENCALC_GRAPH_EXPRESSION_MAX];
} opencalc_graph_symbolic_request_t;

typedef struct {
    int graphing_mode;
    int series;
    uint32_t fingerprint;
    char derivative[OPENCALC_GRAPH_SYMBOLIC_TEXT_MAX];
    char integral[OPENCALC_GRAPH_SYMBOLIC_TEXT_MAX];
    char roots[OPENCALC_GRAPH_SYMBOLIC_TEXT_MAX];
    char asymptotes[OPENCALC_GRAPH_SYMBOLIC_TEXT_MAX];
} opencalc_graph_symbolic_result_t;

bool opencalc_graph_symbolic_analyze(
    const opencalc_graph_symbolic_request_t *request,
    bool degrees,
    opencalc_graph_scalar_eval_fn numeric_eval,
    opencalc_graph_symbolic_result_t *result);
