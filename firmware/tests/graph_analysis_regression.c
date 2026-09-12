#include "opencalc_graph_analysis.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static opencalc_graph_analysis_request_t cartesian_request(void)
{
    opencalc_graph_analysis_request_t request;
    memset(&request, 0, sizeof(request));
    request.graphing_mode = 0;
    request.enabled[0] = true;
    snprintf(request.exprs[0], sizeof(request.exprs[0]), "x^2-4");
    request.xmin = -5.0;
    request.xmax = 5.0;
    request.ymin = -5.0;
    request.ymax = 10.0;
    request.tmin = 0.0;
    request.tmax = 360.0;
    request.nmin = 0.0;
    request.nmax = 20.0;
    request.trace = true;
    request.trace_x = 3.0;
    request.trace_fn = 0;
    return request;
}

static double status_value(const char *status)
{
    const char *equals = strrchr(status, '=');
    assert(equals != NULL);
    return strtod(equals + 1, NULL);
}

static int poi_count(const opencalc_graph_poi_t *points, int count,
                     opencalc_graph_poi_type_t type)
{
    int matches = 0;
    for (int i = 0; i < count; i++) matches += points[i].type == type;
    return matches;
}

int main(void)
{
    opencalc_graph_analysis_request_t request = cartesian_request();
    opencalc_graph_analysis_result_t result;

    request.selection = 0;
    assert(opencalc_graph_analysis_run(&request, true, &result));
    assert(strstr(result.status, "y 5") != NULL);

    request.selection = 1;
    request.trace_x = 1.5;
    assert(opencalc_graph_analysis_run(&request, true, &result));
    assert(fabs(fabs(result.trace_x) - 2.0) < 1e-5);

    request.selection = 6;
    request.trace_x = 3.0;
    assert(opencalc_graph_analysis_run(&request, true, &result));
    assert(fabs(status_value(result.status) - 6.0) < 1e-3);

    request.selection = 7;
    request.trace_x = 3.0;
    assert(opencalc_graph_analysis_run(&request, true, &result));
    assert(fabs(status_value(result.status) + 3.0) < 1e-3);

    opencalc_graph_poi_t points[OPENCALC_GRAPH_POI_LIMIT];
    snprintf(request.exprs[0], sizeof(request.exprs[0]), "(x-0.137)^2");
    int count = opencalc_graph_analysis_collect_pois(
        &request, false, points, OPENCALC_GRAPH_POI_LIMIT);
    assert(poi_count(points, count, OPENCALC_GRAPH_POI_ZERO) >= 1);

    request.enabled[1] = true;
    snprintf(request.exprs[0], sizeof(request.exprs[0]), "0");
    snprintf(request.exprs[1], sizeof(request.exprs[1]), "(x-0.101)*(x-0.109)");
    count = opencalc_graph_analysis_collect_pois(
        &request, false, points, OPENCALC_GRAPH_POI_LIMIT);
    assert(poi_count(points, count, OPENCALC_GRAPH_POI_INTERSECTION) >= 2);

    memset(&request, 0, sizeof(request));
    request.graphing_mode = 1;
    request.selection = 0;
    request.param_enabled[0] = true;
    snprintf(request.param_x[0], sizeof(request.param_x[0]), "cos(t)");
    snprintf(request.param_y[0], sizeof(request.param_y[0]), "sin(t)");
    request.tmin = 0.0;
    request.tmax = 360.0;
    request.xmin = request.ymin = -2.0;
    request.xmax = request.ymax = 2.0;
    request.trace = true;
    request.trace_x = 90.0;
    assert(opencalc_graph_analysis_run(&request, true, &result));
    assert(strstr(result.status, "x 6.123e-17 y 1") != NULL);

    snprintf(request.param_x[0], sizeof(request.param_x[0]), "t");
    snprintf(request.param_y[0], sizeof(request.param_y[0]), "(t-1.234)^2");
    request.tmin = 0.0;
    request.tmax = 2.0;
    count = opencalc_graph_analysis_collect_pois(
        &request, false, points, OPENCALC_GRAPH_POI_LIMIT);
    assert(poi_count(points, count, OPENCALC_GRAPH_POI_ZERO) >= 1);

    memset(&request, 0, sizeof(request));
    request.graphing_mode = 2;
    request.polar_enabled[0] = true;
    snprintf(request.polar_exprs[0], sizeof(request.polar_exprs[0]), "(t-1.234)^2");
    request.tmin = 0.0;
    request.tmax = 2.0;
    request.xmin = request.ymin = -2.0;
    request.xmax = request.ymax = 2.0;
    count = opencalc_graph_analysis_collect_pois(
        &request, false, points, OPENCALC_GRAPH_POI_LIMIT);
    assert(poi_count(points, count, OPENCALC_GRAPH_POI_ZERO) >= 1);

    puts("PASS graph analysis worker");
    return 0;
}
