#include "opencalc_graph_model.h"

#include <string.h>

static opencalc_graph_model_t s_graph;

void opencalc_graph_model_reset(void)
{
    memset(&s_graph, 0, sizeof(s_graph));
    strcpy(s_graph.expressions[0], "x");
    s_graph.enabled[0] = true;
    strcpy(s_graph.param_x[0], "cos(t)");
    strcpy(s_graph.param_y[0], "sin(t)");
    strcpy(s_graph.polar_expressions[0], "1");
    strcpy(s_graph.sequence_expressions[0], "n");
    s_graph.t_max = 360.0;
    s_graph.n_max = 20.0;
    s_graph.x_min = -10.0;
    s_graph.x_max = 10.0;
    s_graph.y_min = -6.0;
    s_graph.y_max = 6.0;
    s_graph.x_tick = 1.0;
    s_graph.y_tick = 1.0;
    s_graph.table_step = 1.0;
    s_graph.table_rows = 9;
    s_graph.table_precision = 2;
    s_graph.grid = true;
    for (int mode = 0; mode < OPENCALC_GRAPH_MODE_COUNT; mode++) {
        for (int series = 0; series < OPENCALC_GRAPH_SERIES_MAX; series++) {
            s_graph.colors[mode][series] = (uint8_t)series;
        }
    }
}

opencalc_graph_model_t *opencalc_graph_model(void) { return &s_graph; }
