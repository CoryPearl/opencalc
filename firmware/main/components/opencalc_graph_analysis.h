#pragma once

#include "opencalc_graph_model.h"

#include <stdbool.h>

#define OPENCALC_GRAPH_POI_LIMIT 16

typedef enum {
    OPENCALC_GRAPH_POI_ZERO,
    OPENCALC_GRAPH_POI_Y_INTERCEPT,
    OPENCALC_GRAPH_POI_MIN,
    OPENCALC_GRAPH_POI_LOCAL_MAX,
    OPENCALC_GRAPH_POI_INTERSECTION,
} opencalc_graph_poi_type_t;

typedef struct {
    double input;
    double x;
    double y;
    int fn;
    int other_fn;
    opencalc_graph_poi_type_t type;
} opencalc_graph_poi_t;

typedef struct {
    int selection;
    int graphing_mode;
    char exprs[OPENCALC_GRAPH_FUNCTION_COUNT][OPENCALC_GRAPH_EXPRESSION_MAX];
    bool enabled[OPENCALC_GRAPH_FUNCTION_COUNT];
    char param_x[OPENCALC_GRAPH_PARAM_COUNT][OPENCALC_GRAPH_EXPRESSION_MAX];
    char param_y[OPENCALC_GRAPH_PARAM_COUNT][OPENCALC_GRAPH_EXPRESSION_MAX];
    bool param_enabled[OPENCALC_GRAPH_PARAM_COUNT];
    char polar_exprs[OPENCALC_GRAPH_POLAR_COUNT][OPENCALC_GRAPH_EXPRESSION_MAX];
    bool polar_enabled[OPENCALC_GRAPH_POLAR_COUNT];
    char seq_exprs[OPENCALC_GRAPH_SEQUENCE_COUNT][OPENCALC_GRAPH_EXPRESSION_MAX];
    bool seq_enabled[OPENCALC_GRAPH_SEQUENCE_COUNT];
    double xmin;
    double xmax;
    double ymin;
    double ymax;
    double tmin;
    double tmax;
    double nmin;
    double nmax;
    bool trace;
    double trace_x;
    int trace_fn;
} opencalc_graph_analysis_request_t;

typedef struct {
    char status[72];
    bool trace;
    double trace_x;
    int trace_fn;
} opencalc_graph_analysis_result_t;

const char *opencalc_graph_poi_label(opencalc_graph_poi_type_t type);
void opencalc_graph_poi_add(opencalc_graph_poi_t *points, int *count, int capacity,
                            opencalc_graph_poi_type_t type, int series, int other_series,
                            double input, double x, double y);
int opencalc_graph_analysis_collect_pois(const opencalc_graph_analysis_request_t *request,
                                         bool degrees, opencalc_graph_poi_t *points,
                                         int capacity);
bool opencalc_graph_analysis_run(const opencalc_graph_analysis_request_t *request,
                                 bool degrees, opencalc_graph_analysis_result_t *result);
