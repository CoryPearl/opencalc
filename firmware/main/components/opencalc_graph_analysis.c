#include "opencalc_graph_analysis.h"

#include "opencalc_graph_series.h"
#include "opencalc_math.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define ANALYSIS_SAMPLES 384
#define INTERSECTION_BASE_SAMPLES 96
#define CURVE_INTERSECTION_SAMPLES 192
#define REFINE_ITERATIONS 36
#define ADAPTIVE_INTERSECTION_DEPTH 5
#define PI 3.14159265358979323846

static int series_count(const opencalc_graph_analysis_request_t *request)
{
    switch (request->graphing_mode) {
    case 1: return OPENCALC_GRAPH_PARAM_COUNT;
    case 2: return OPENCALC_GRAPH_POLAR_COUNT;
    case 3: return OPENCALC_GRAPH_SEQUENCE_COUNT;
    default: return OPENCALC_GRAPH_FUNCTION_COUNT;
    }
}

static bool series_enabled(const opencalc_graph_analysis_request_t *request, int series)
{
    if (request == NULL || series < 0) return false;
    switch (request->graphing_mode) {
    case 1:
        return series < OPENCALC_GRAPH_PARAM_COUNT && request->param_enabled[series] &&
            request->param_x[series][0] != '\0' && request->param_y[series][0] != '\0';
    case 2:
        return series < OPENCALC_GRAPH_POLAR_COUNT && request->polar_enabled[series] &&
            request->polar_exprs[series][0] != '\0';
    case 3:
        return series < OPENCALC_GRAPH_SEQUENCE_COUNT && request->seq_enabled[series] &&
            request->seq_exprs[series][0] != '\0';
    default:
        return series < OPENCALC_GRAPH_FUNCTION_COUNT && request->enabled[series] &&
            request->exprs[series][0] != '\0';
    }
}

static bool sequence_eval(const char *text, double input, double *value)
{
    if (!isfinite(input) || input < 0.0 || input > 4096.0) return false;
    int n = (int)floor(input + 0.5);
    if (fabs(input - n) > 1e-7) return false;
    opencalc_graph_sequence_t sequence;
    return opencalc_graph_sequence_parse(text, &sequence) &&
        opencalc_graph_sequence_eval(&sequence, n, graph_eval_expression_var, value);
}

static bool eval_series(const opencalc_graph_analysis_request_t *request, bool degrees,
                        int series, double input, double *x, double *y, double *metric)
{
    if (!series_enabled(request, series) || x == NULL || y == NULL || metric == NULL) return false;
    switch (request->graphing_mode) {
    case 1:
        if (!graph_eval_expression_var(request->param_x[series], 't', input, x) ||
            !graph_eval_expression_var(request->param_y[series], 't', input, y)) return false;
        *metric = *y;
        break;
    case 2: {
        double radius = 0.0;
        if (!graph_eval_expression_var(request->polar_exprs[series], 't', input,
                                       &radius)) return false;
        double radians = degrees ? input * PI / 180.0 : input;
        *x = radius * cos(radians);
        *y = radius * sin(radians);
        *metric = radius;
        break;
    }
    case 3:
        if (!sequence_eval(request->seq_exprs[series], input, y)) return false;
        *x = input;
        *metric = *y;
        break;
    default:
        if (!graph_eval_expression(request->exprs[series], input, y)) return false;
        *x = input;
        *metric = *y;
        break;
    }
    return isfinite(*x) && isfinite(*y) && isfinite(*metric);
}

static bool eval_component(const opencalc_graph_analysis_request_t *request, bool degrees,
                           int series, double input, bool x_component, double *value)
{
    double x = 0.0, y = 0.0, metric = 0.0;
    if (!eval_series(request, degrees, series, input, &x, &y, &metric)) return false;
    *value = x_component ? x : y;
    return true;
}

static bool refine_series_crossing(const opencalc_graph_analysis_request_t *request,
                                   bool degrees, int series, double low, double high,
                                   bool x_component, double *input, double *x, double *y)
{
    double low_value = 0.0, high_value = 0.0;
    if (!eval_component(request, degrees, series, low, x_component, &low_value) ||
        !eval_component(request, degrees, series, high, x_component, &high_value)) return false;
    if (fabs(low_value) <= 1e-12 || fabs(high_value) <= 1e-12) {
        *input = fabs(low_value) <= fabs(high_value) ? low : high;
        double metric = 0.0;
        return eval_series(request, degrees, series, *input, x, y, &metric);
    }
    if ((low_value < 0.0) == (high_value < 0.0)) return false;
    for (int i = 0; i < REFINE_ITERATIONS; i++) {
        double middle = (low + high) * 0.5;
        double value = 0.0;
        if (!eval_component(request, degrees, series, middle, x_component, &value)) return false;
        if (fabs(value) <= 1e-12) {
            low = high = middle;
            break;
        }
        if ((low_value < 0.0) != (value < 0.0)) high = middle;
        else {
            low = middle;
            low_value = value;
        }
    }
    *input = (low + high) * 0.5;
    double metric = 0.0;
    return eval_series(request, degrees, series, *input, x, y, &metric);
}

static bool refine_series_extremum(const opencalc_graph_analysis_request_t *request,
                                   bool degrees, int series, double low, double high,
                                   bool maximum, double *input, double *x, double *y)
{
    const double ratio = 0.3819660112501051;
    double left = low + ratio * (high - low);
    double right = high - ratio * (high - low);
    double left_value = 0.0, right_value = 0.0;
    if (!eval_component(request, degrees, series, left, false, &left_value) ||
        !eval_component(request, degrees, series, right, false, &right_value)) return false;
    for (int i = 0; i < REFINE_ITERATIONS; i++) {
        bool keep_right = maximum ? left_value < right_value : left_value > right_value;
        if (keep_right) {
            low = left;
            left = right;
            left_value = right_value;
            right = high - ratio * (high - low);
            if (!eval_component(request, degrees, series, right, false, &right_value)) return false;
        } else {
            high = right;
            right = left;
            right_value = left_value;
            left = low + ratio * (high - low);
            if (!eval_component(request, degrees, series, left, false, &left_value)) return false;
        }
    }
    *input = (low + high) * 0.5;
    double metric = 0.0;
    return eval_series(request, degrees, series, *input, x, y, &metric);
}

static bool refine_series_tangent_zero(const opencalc_graph_analysis_request_t *request,
                                       bool degrees, int series, double low, double high,
                                       double tolerance, double *input, double *x, double *y)
{
    const double ratio = 0.3819660112501051;
    double left = low + ratio * (high - low);
    double right = high - ratio * (high - low);
    double left_value = 0.0, right_value = 0.0;
    if (!eval_component(request, degrees, series, left, false, &left_value) ||
        !eval_component(request, degrees, series, right, false, &right_value)) return false;
    for (int i = 0; i < REFINE_ITERATIONS; i++) {
        if (fabs(left_value) > fabs(right_value)) {
            low = left;
            left = right;
            left_value = right_value;
            right = high - ratio * (high - low);
            if (!eval_component(request, degrees, series, right, false, &right_value)) return false;
        } else {
            high = right;
            right = left;
            right_value = left_value;
            left = low + ratio * (high - low);
            if (!eval_component(request, degrees, series, left, false, &left_value)) return false;
        }
    }
    *input = fabs(left_value) <= fabs(right_value) ? left : right;
    double metric = 0.0;
    if (!eval_series(request, degrees, series, *input, x, y, &metric)) return false;
    return fabs(*y) <= tolerance;
}

static void input_range(const opencalc_graph_analysis_request_t *request,
                        double *minimum, double *maximum)
{
    if (request->graphing_mode == 1 || request->graphing_mode == 2) {
        *minimum = request->tmin;
        *maximum = request->tmax;
    } else if (request->graphing_mode == 3) {
        *minimum = request->nmin;
        *maximum = request->nmax;
    } else {
        *minimum = request->xmin;
        *maximum = request->xmax;
    }
}

static const char *series_prefix(const opencalc_graph_analysis_request_t *request)
{
    switch (request->graphing_mode) {
    case 1: return "P";
    case 2: return "r";
    case 3: return "u";
    default: return "Y";
    }
}

const char *opencalc_graph_poi_label(opencalc_graph_poi_type_t type)
{
    switch (type) {
    case OPENCALC_GRAPH_POI_ZERO: return "zero";
    case OPENCALC_GRAPH_POI_Y_INTERCEPT: return "y-int";
    case OPENCALC_GRAPH_POI_MIN: return "min";
    case OPENCALC_GRAPH_POI_LOCAL_MAX: return "max";
    case OPENCALC_GRAPH_POI_INTERSECTION: return "intersect";
    default: return "point";
    }
}

void opencalc_graph_poi_add(opencalc_graph_poi_t *points, int *count, int capacity,
                            opencalc_graph_poi_type_t type, int series, int other_series,
                            double input, double x, double y)
{
    if (points == NULL || count == NULL || *count >= capacity ||
        !isfinite(input) || !isfinite(x) || !isfinite(y)) return;
    for (int i = 0; i < *count; i++) {
        if (points[i].type == type && points[i].fn == series &&
            points[i].other_fn == other_series &&
            fabs(points[i].input - input) <= 1e-6 * fmax(1.0, fabs(input)) &&
            fabs(points[i].x - x) <= 1e-6 * fmax(1.0, fabs(x)) &&
            fabs(points[i].y - y) <= 1e-6 * fmax(1.0, fabs(y))) return;
    }
    points[*count] = (opencalc_graph_poi_t) {
        .input = input, .x = x, .y = y, .fn = series,
        .other_fn = other_series, .type = type,
    };
    (*count)++;
}

static bool segment_intersection(double ax0, double ay0, double ax1, double ay1,
                                 double bx0, double by0, double bx1, double by1,
                                 double *fraction, double *x, double *y)
{
    double arx = ax1 - ax0, ary = ay1 - ay0;
    double brx = bx1 - bx0, bry = by1 - by0;
    double denominator = arx * bry - ary * brx;
    double scale = fabs(arx) + fabs(ary) + fabs(brx) + fabs(bry);
    if (fabs(denominator) <= 1e-12 * fmax(1.0, scale * scale)) return false;
    double dx = bx0 - ax0, dy = by0 - ay0;
    double ta = (dx * bry - dy * brx) / denominator;
    double tb = (dx * ary - dy * arx) / denominator;
    if (ta < -1e-8 || ta > 1.0 + 1e-8 || tb < -1e-8 || tb > 1.0 + 1e-8) return false;
    *fraction = ta;
    *x = ax0 + ta * arx;
    *y = ay0 + ta * ary;
    return isfinite(*x) && isfinite(*y);
}

static bool eval_cartesian(const opencalc_graph_analysis_request_t *request,
                           int series, double x, double *y)
{
    return series_enabled(request, series) &&
        graph_eval_expression(request->exprs[series], x, y);
}

static bool refine_intersection(const opencalc_graph_analysis_request_t *request,
                                int first, int second, double low, double high,
                                double *x, double *y)
{
    double a = 0.0, b = 0.0;
    if (!eval_cartesian(request, first, low, &a) ||
        !eval_cartesian(request, second, low, &b)) return false;
    double low_difference = a - b;
    for (int i = 0; i < 28; i++) {
        double middle = (low + high) * 0.5;
        if (!eval_cartesian(request, first, middle, &a) ||
            !eval_cartesian(request, second, middle, &b)) return false;
        double difference = a - b;
        if ((low_difference <= 0.0 && difference >= 0.0) ||
            (low_difference >= 0.0 && difference <= 0.0)) high = middle;
        else {
            low = middle;
            low_difference = difference;
        }
    }
    *x = (low + high) * 0.5;
    return eval_cartesian(request, first, *x, y);
}

static bool eval_cartesian_difference(const opencalc_graph_analysis_request_t *request,
                                      int first, int second, double input,
                                      double *difference)
{
    double a = 0.0, b = 0.0;
    if (!eval_cartesian(request, first, input, &a) ||
        !eval_cartesian(request, second, input, &b)) return false;
    *difference = a - b;
    return true;
}

static bool refine_tangent_intersection(const opencalc_graph_analysis_request_t *request,
                                        int first, int second, double low, double high,
                                        double tolerance, double *x, double *y)
{
    const double ratio = 0.3819660112501051;
    double left = low + ratio * (high - low);
    double right = high - ratio * (high - low);
    double left_value = 0.0, right_value = 0.0;
    if (!eval_cartesian_difference(request, first, second, left, &left_value) ||
        !eval_cartesian_difference(request, first, second, right, &right_value)) return false;
    for (int i = 0; i < REFINE_ITERATIONS; i++) {
        if (fabs(left_value) > fabs(right_value)) {
            low = left; left = right; left_value = right_value;
            right = high - ratio * (high - low);
            if (!eval_cartesian_difference(request, first, second, right, &right_value)) return false;
        } else {
            high = right; right = left; right_value = left_value;
            left = low + ratio * (high - low);
            if (!eval_cartesian_difference(request, first, second, left, &left_value)) return false;
        }
    }
    *x = fabs(left_value) <= fabs(right_value) ? left : right;
    double difference = 0.0;
    return eval_cartesian_difference(request, first, second, *x, &difference) &&
        fabs(difference) <= tolerance && eval_cartesian(request, first, *x, y);
}

static void scan_cartesian_intersection_interval(
    const opencalc_graph_analysis_request_t *request, int first, int second,
    double low, double high, int depth, opencalc_graph_poi_t *points,
    int *count, int capacity)
{
    if (*count >= capacity) return;
    double middle = (low + high) * 0.5;
    double a0 = 0.0, am = 0.0, a1 = 0.0;
    double b0 = 0.0, bm = 0.0, b1 = 0.0;
    if (!eval_cartesian(request, first, low, &a0) ||
        !eval_cartesian(request, first, middle, &am) ||
        !eval_cartesian(request, first, high, &a1) ||
        !eval_cartesian(request, second, low, &b0) ||
        !eval_cartesian(request, second, middle, &bm) ||
        !eval_cartesian(request, second, high, &b1)) return;
    double xspan = fabs(request->xmax - request->xmin);
    double yspan = fabs(request->ymax - request->ymin);
    if (!opencalc_graph_segment_is_continuous(low, a0, middle, am, high, a1, xspan, yspan) ||
        !opencalc_graph_segment_is_continuous(low, b0, middle, bm, high, b1, xspan, yspan)) return;

    double d0 = a0 - b0, dm = am - bm, d1 = a1 - b1;
    double near = fmin(fabs(d0), fmin(fabs(dm), fabs(d1)));
    double curvature = fabs(d0 - 2.0 * dm + d1);
    double local_scale = fmax(1e-12, fmax(fabs(d0), fmax(fabs(dm), fabs(d1))));
    bool sign_left = (d0 < 0.0) != (dm < 0.0) || fabs(d0) <= 1e-12 || fabs(dm) <= 1e-12;
    bool sign_right = (dm < 0.0) != (d1 < 0.0) || fabs(dm) <= 1e-12 || fabs(d1) <= 1e-12;
    bool subdivide = depth < ADAPTIVE_INTERSECTION_DEPTH &&
        (curvature > local_scale * 0.02 || near < fmax(1e-6, yspan * 0.02));
    if (subdivide) {
        scan_cartesian_intersection_interval(request, first, second, low, middle,
                                             depth + 1, points, count, capacity);
        scan_cartesian_intersection_interval(request, first, second, middle, high,
                                             depth + 1, points, count, capacity);
        return;
    }

    if (sign_left) {
        double x = 0.0, y = 0.0;
        if (refine_intersection(request, first, second, low, middle, &x, &y)) {
            opencalc_graph_poi_add(points, count, capacity,
                OPENCALC_GRAPH_POI_INTERSECTION, first, second, x, x, y);
        }
    }
    if (sign_right && *count < capacity) {
        double x = 0.0, y = 0.0;
        if (refine_intersection(request, first, second, middle, high, &x, &y)) {
            opencalc_graph_poi_add(points, count, capacity,
                OPENCALC_GRAPH_POI_INTERSECTION, first, second, x, x, y);
        }
    }
    if (!sign_left && !sign_right && near <= fmax(1e-6, yspan * 1e-4)) {
        double x = 0.0, y = 0.0;
        if (refine_tangent_intersection(request, first, second, low, high,
                                        fmax(1e-8, yspan * 1e-7), &x, &y)) {
            opencalc_graph_poi_add(points, count, capacity,
                OPENCALC_GRAPH_POI_INTERSECTION, first, second, x, x, y);
        }
    }
}

static bool refine_curve_intersection(const opencalc_graph_analysis_request_t *request,
                                      bool degrees, int first, int second,
                                      double first_low, double first_high,
                                      double second_low, double second_high,
                                      double *first_input, double *x, double *y)
{
    double ta = (first_low + first_high) * 0.5;
    double tb = (second_low + second_high) * 0.5;
    double ha = fmax((first_high - first_low) * 1e-3, 1e-8);
    double hb = fmax((second_high - second_low) * 1e-3, 1e-8);
    for (int iteration = 0; iteration < 14; iteration++) {
        double ax = 0.0, ay = 0.0, am = 0.0, bx = 0.0, by = 0.0, bm = 0.0;
        double ax0 = 0.0, ay0 = 0.0, ax1 = 0.0, ay1 = 0.0;
        double bx0 = 0.0, by0 = 0.0, bx1 = 0.0, by1 = 0.0, metric = 0.0;
        if (!eval_series(request, degrees, first, ta, &ax, &ay, &am) ||
            !eval_series(request, degrees, second, tb, &bx, &by, &bm) ||
            !eval_series(request, degrees, first, fmax(first_low, ta - ha), &ax0, &ay0, &metric) ||
            !eval_series(request, degrees, first, fmin(first_high, ta + ha), &ax1, &ay1, &metric) ||
            !eval_series(request, degrees, second, fmax(second_low, tb - hb), &bx0, &by0, &metric) ||
            !eval_series(request, degrees, second, fmin(second_high, tb + hb), &bx1, &by1, &metric)) return false;
        double adenom = fmin(first_high, ta + ha) - fmax(first_low, ta - ha);
        double bdenom = fmin(second_high, tb + hb) - fmax(second_low, tb - hb);
        if (adenom <= 0.0 || bdenom <= 0.0) return false;
        double dax = (ax1 - ax0) / adenom, day = (ay1 - ay0) / adenom;
        double dbx = (bx1 - bx0) / bdenom, dby = (by1 - by0) / bdenom;
        double fx = ax - bx, fy = ay - by;
        double determinant = dbx * day - dax * dby;
        if (fabs(determinant) <= 1e-14) break;
        double delta_a = (dby * fx - dbx * fy) / determinant;
        double delta_b = (day * fx - dax * fy) / determinant;
        ta = fmin(first_high, fmax(first_low, ta + delta_a));
        tb = fmin(second_high, fmax(second_low, tb + delta_b));
    }
    double bx = 0.0, by = 0.0, metric = 0.0;
    if (!eval_series(request, degrees, first, ta, x, y, &metric) ||
        !eval_series(request, degrees, second, tb, &bx, &by, &metric)) return false;
    double error = hypot((*x - bx) / fmax(1e-9, fabs(request->xmax - request->xmin)),
                         (*y - by) / fmax(1e-9, fabs(request->ymax - request->ymin)));
    *first_input = ta;
    *x = (*x + bx) * 0.5;
    *y = (*y + by) * 0.5;
    return error <= 1e-5;
}

int opencalc_graph_analysis_collect_pois(const opencalc_graph_analysis_request_t *request,
                                         bool degrees, opencalc_graph_poi_t *points,
                                         int capacity)
{
    if (request == NULL || points == NULL || capacity <= 0) return 0;
    int count = 0, active[OPENCALC_GRAPH_FUNCTION_COUNT], active_count = 0;
    double minimum = 0.0, maximum = 0.0;
    input_range(request, &minimum, &maximum);
    int samples = ANALYSIS_SAMPLES;
    if (request->graphing_mode == 3) {
        minimum = ceil(minimum);
        maximum = floor(maximum);
        samples = (int)(maximum - minimum);
        if (samples > ANALYSIS_SAMPLES) samples = ANALYSIS_SAMPLES;
    }
    double step = request->graphing_mode == 3 ? 1.0 : (maximum - minimum) / samples;
    if (samples < 1 || step <= 0.0) return 0;

    for (int series = 0; series < series_count(request); series++) {
        if (!series_enabled(request, series)) continue;
        active[active_count++] = series;
        double y2 = 0.0;
        double x1 = 0.0, y1 = 0.0, m1 = 0.0;
        bool have2 = false;
        bool have1 = eval_series(request, degrees, series, minimum, &x1, &y1, &m1);
        if (request->graphing_mode != 3 && have1 && fabs(y1) <= 1e-12) {
            opencalc_graph_poi_add(points, &count, capacity,
                OPENCALC_GRAPH_POI_ZERO, series, -1, minimum, x1, y1);
        }
        if (request->graphing_mode == 0 && minimum <= 0.0 && maximum >= 0.0) {
            double y0 = 0.0;
            if (eval_cartesian(request, series, 0.0, &y0))
                opencalc_graph_poi_add(points, &count, capacity,
                    OPENCALC_GRAPH_POI_Y_INTERCEPT, series, -1, 0.0, 0.0, y0);
        }
        for (int sample = 1; sample <= samples && count < capacity; sample++) {
            double input = minimum + step * sample;
            double x = 0.0, y = 0.0, metric = 0.0;
            bool have = eval_series(request, degrees, series, input, &x, &y, &metric);
            double middle_input = input - step * 0.5;
            double middle_x = 0.0, middle_y = 0.0, middle_metric = 0.0;
            bool connected = have1 && have &&
                eval_series(request, degrees, series, middle_input,
                            &middle_x, &middle_y, &middle_metric) &&
                opencalc_graph_segment_is_continuous(
                    x1, y1, middle_x, middle_y, x, y,
                    request->xmax - request->xmin, request->ymax - request->ymin);
            bool zero = request->graphing_mode == 3 ? have && fabs(y) <= 1e-10 :
                connected && !(fabs(y1) <= 1e-12 && fabs(y) <= 1e-12) &&
                ((y1 <= 0.0 && y >= 0.0) || (y1 >= 0.0 && y <= 0.0));
            if (zero) {
                double zero_x = x, zero_y = y, zero_input = input;
                bool refined = request->graphing_mode == 3 ||
                    refine_series_crossing(request, degrees, series, input - step, input,
                                           false, &zero_input, &zero_x, &zero_y);
                if (refined) {
                    opencalc_graph_poi_add(points, &count, capacity,
                        OPENCALC_GRAPH_POI_ZERO, series, -1, zero_input, zero_x, zero_y);
                }
            }
            if (request->graphing_mode != 3 && have2 && connected &&
                !(fabs(y2) <= 1e-12 && fabs(y1) <= 1e-12 && fabs(y) <= 1e-12) &&
                (y2 < 0.0) == (y1 < 0.0) && (y1 < 0.0) == (y < 0.0) &&
                fabs(y1) <= fabs(y2) && fabs(y1) <= fabs(y) &&
                fabs(y1) <= fmax(1e-6, fabs(request->ymax - request->ymin) * 0.01)) {
                double zero_input = 0.0, zero_x = 0.0, zero_y = 0.0;
                if (refine_series_tangent_zero(
                        request, degrees, series, input - 2.0 * step, input,
                        fmax(1e-8, fabs(request->ymax - request->ymin) * 1e-7),
                        &zero_input, &zero_x, &zero_y)) {
                    opencalc_graph_poi_add(points, &count, capacity,
                        OPENCALC_GRAPH_POI_ZERO, series, -1, zero_input, zero_x, zero_y);
                }
            }
            if (request->graphing_mode != 0 && connected &&
                ((x1 <= 0.0 && x >= 0.0) || (x1 >= 0.0 && x <= 0.0))) {
                double intercept_input = 0.0, intercept_x = 0.0, intercept_y = 0.0;
                if (refine_series_crossing(request, degrees, series, input - step, input,
                                           true, &intercept_input, &intercept_x, &intercept_y)) {
                    opencalc_graph_poi_add(points, &count, capacity,
                        OPENCALC_GRAPH_POI_Y_INTERCEPT, series, -1,
                        intercept_input, intercept_x, intercept_y);
                }
            }
            if (have2 && connected) {
                opencalc_graph_poi_type_t type;
                bool extremum = false;
                if (y1 < y2 && y1 < y) {
                    type = OPENCALC_GRAPH_POI_MIN;
                    extremum = true;
                } else if (y1 > y2 && y1 > y) {
                    type = OPENCALC_GRAPH_POI_LOCAL_MAX;
                    extremum = true;
                }
                if (extremum) {
                    double extremum_input = input - step;
                    double extremum_x = x1, extremum_y = y1;
                    if (request->graphing_mode != 3) {
                        (void)refine_series_extremum(
                            request, degrees, series, input - 2.0 * step, input,
                            type == OPENCALC_GRAPH_POI_LOCAL_MAX,
                            &extremum_input, &extremum_x, &extremum_y);
                    }
                    opencalc_graph_poi_add(points, &count, capacity, type,
                        series, -1, extremum_input, extremum_x, extremum_y);
                }
            }
            y2 = y1; have2 = have1;
            x1 = x; y1 = y; m1 = metric; have1 = have;
        }
    }

    for (int ai = 0; ai < active_count; ai++) {
        for (int bi = ai + 1; bi < active_count && count < capacity; bi++) {
            int first = active[ai], second = active[bi];
            if (request->graphing_mode == 0) {
                double pair_step = (maximum - minimum) / INTERSECTION_BASE_SAMPLES;
                for (int sample = 0; sample < INTERSECTION_BASE_SAMPLES && count < capacity; sample++) {
                    scan_cartesian_intersection_interval(
                        request, first, second, minimum + pair_step * sample,
                        minimum + pair_step * (sample + 1), 0, points, &count, capacity);
                }
                continue;
            }

            double ax[ANALYSIS_SAMPLES + 1], ay[ANALYSIS_SAMPLES + 1];
            double bx[ANALYSIS_SAMPLES + 1], by[ANALYSIS_SAMPLES + 1];
            bool valid_a[ANALYSIS_SAMPLES + 1], valid_b[ANALYSIS_SAMPLES + 1];
            int intersection_samples = request->graphing_mode == 3
                ? samples : CURVE_INTERSECTION_SAMPLES;
            double intersection_step = request->graphing_mode == 3
                ? step : (maximum - minimum) / intersection_samples;
            for (int sample = 0; sample <= intersection_samples; sample++) {
                double input = minimum + intersection_step * sample, metric = 0.0;
                valid_a[sample] = eval_series(request, degrees, first, input,
                                               &ax[sample], &ay[sample], &metric);
                valid_b[sample] = eval_series(request, degrees, second, input,
                                               &bx[sample], &by[sample], &metric);
            }
            for (int sample = 1; sample <= intersection_samples && count < capacity; sample++) {
                if (!valid_a[sample - 1] || !valid_a[sample] ||
                    !valid_b[sample - 1] || !valid_b[sample]) continue;
                if (request->graphing_mode == 3) {
                    if (fabs(ay[sample] - by[sample]) <=
                        fmax(1e-9, fabs(request->ymax - request->ymin) * 1e-7)) {
                        double input = minimum + intersection_step * sample;
                        opencalc_graph_poi_add(points, &count, capacity,
                            OPENCALC_GRAPH_POI_INTERSECTION, first, second, input,
                            ax[sample], (ay[sample] + by[sample]) * 0.5);
                    }
                    continue;
                }
                for (int other = 1; other <= intersection_samples && count < capacity; other++) {
                    if (!valid_b[other - 1] || !valid_b[other]) continue;
                    double fraction = 0.0, x = 0.0, y = 0.0;
                    if (segment_intersection(ax[sample - 1], ay[sample - 1], ax[sample], ay[sample],
                                             bx[other - 1], by[other - 1], bx[other], by[other],
                                             &fraction, &x, &y)) {
                        double first_input = minimum + intersection_step * (sample - 1 + fraction);
                        (void)refine_curve_intersection(
                            request, degrees, first, second,
                            minimum + intersection_step * (sample - 1),
                            minimum + intersection_step * sample,
                            minimum + intersection_step * (other - 1),
                            minimum + intersection_step * other,
                            &first_input, &x, &y);
                        opencalc_graph_poi_add(points, &count, capacity,
                            OPENCALC_GRAPH_POI_INTERSECTION, first, second,
                            first_input, x, y);
                    }
                }
            }
        }
    }
    return count;
}

static bool ensure_trace(const opencalc_graph_analysis_request_t *request,
                         bool *trace, double *input, int *series)
{
    if (*trace && series_enabled(request, *series)) return true;
    for (int i = 0; i < series_count(request); i++) {
        if (series_enabled(request, i)) {
            double minimum = 0.0, maximum = 0.0;
            input_range(request, &minimum, &maximum);
            *trace = true;
            *series = i;
            *input = (minimum + maximum) * 0.5;
            return true;
        }
    }
    return false;
}

static bool value_at_trace(const opencalc_graph_analysis_request_t *request, bool degrees,
                           opencalc_graph_analysis_result_t *result)
{
    if (!ensure_trace(request, &result->trace, &result->trace_x,
                      &result->trace_fn)) return false;
    double x = 0.0, y = 0.0, metric = 0.0;
    if (!eval_series(request, degrees, result->trace_fn, result->trace_x,
                     &x, &y, &metric)) return false;
    if (request->graphing_mode == 1)
        snprintf(result->status, sizeof(result->status), "value P%d t %.4g x %.4g y %.4g",
                 result->trace_fn + 1, result->trace_x, x, y);
    else if (request->graphing_mode == 2)
        snprintf(result->status, sizeof(result->status), "value r%d t %.4g r %.4g",
                 result->trace_fn + 1, result->trace_x, metric);
    else if (request->graphing_mode == 3)
        snprintf(result->status, sizeof(result->status), "value u%d n %.0f = %.6g",
                 result->trace_fn + 1, result->trace_x, y);
    else
        snprintf(result->status, sizeof(result->status), "value Y%d x %.4g y %.6g",
                 result->trace_fn + 1, result->trace_x, y);
    return true;
}

static bool jump_to_poi(const opencalc_graph_analysis_request_t *request, bool degrees,
                        opencalc_graph_poi_type_t type,
                        opencalc_graph_analysis_result_t *result)
{
    opencalc_graph_poi_t points[OPENCALC_GRAPH_POI_LIMIT];
    int count = opencalc_graph_analysis_collect_pois(request, degrees, points,
                                                      OPENCALC_GRAPH_POI_LIMIT);
    double minimum = 0.0, maximum = 0.0;
    input_range(request, &minimum, &maximum);
    double target = request->trace ? request->trace_x : (minimum + maximum) * 0.5;
    int best = -1;
    for (int i = 0; i < count; i++) {
        if (points[i].type != type) continue;
        if (best < 0 || fabs(points[i].input - target) < fabs(points[best].input - target)) best = i;
    }
    if (best < 0) return false;
    result->trace = true;
    result->trace_x = points[best].input;
    result->trace_fn = points[best].fn;
    snprintf(result->status, sizeof(result->status), "%s %s%d x %.4g y %.4g",
             opencalc_graph_poi_label(type), series_prefix(request), points[best].fn + 1,
             fabs(points[best].x) <= 1e-9 ? 0.0 : points[best].x,
             fabs(points[best].y) <= 1e-9 ? 0.0 : points[best].y);
    return true;
}

static bool derivative_at_trace(const opencalc_graph_analysis_request_t *request, bool degrees,
                                opencalc_graph_analysis_result_t *result)
{
    if (!ensure_trace(request, &result->trace, &result->trace_x,
                      &result->trace_fn)) return false;
    double minimum = 0.0, maximum = 0.0;
    input_range(request, &minimum, &maximum);
    double h = fmax(1e-6, (maximum - minimum) / 2000.0);
    if (request->graphing_mode == 3) h = 1.0;
    double x0 = 0.0, y0 = 0.0, m0 = 0.0, x1 = 0.0, y1 = 0.0, m1 = 0.0;
    double first_input = request->graphing_mode == 3 ? floor(result->trace_x + 0.5) :
        result->trace_x - h;
    if (!eval_series(request, degrees, result->trace_fn, first_input, &x0, &y0, &m0) ||
        !eval_series(request, degrees, result->trace_fn, first_input +
                     (request->graphing_mode == 3 ? 1.0 : 2.0 * h), &x1, &y1, &m1)) return false;
    double denominator = x1 - x0;
    if (fabs(denominator) <= 1e-12) return false;
    const char *label = request->graphing_mode == 3 ? "delta" : "dy/dx";
    snprintf(result->status, sizeof(result->status), "%s %s%d %c %.4g = %.6g",
             label, series_prefix(request), result->trace_fn + 1,
             request->graphing_mode == 3 ? 'n' : (request->graphing_mode == 0 ? 'x' : 't'),
             result->trace_x, (y1 - y0) / denominator);
    return true;
}

static bool integral_to_trace(const opencalc_graph_analysis_request_t *request, bool degrees,
                              opencalc_graph_analysis_result_t *result)
{
    if (!ensure_trace(request, &result->trace, &result->trace_x,
                      &result->trace_fn)) return false;
    double low = 0.0, high = result->trace_x, sign = 1.0;
    if (high < low) { double swap = low; low = high; high = swap; sign = -1.0; }
    double sum = 0.0;
    if (request->graphing_mode == 3) {
        for (int n = (int)ceil(low); n <= (int)floor(high); n++) {
            double x = 0.0, y = 0.0, metric = 0.0;
            if (!eval_series(request, degrees, result->trace_fn, n, &x, &y, &metric)) return false;
            sum += y;
        }
        snprintf(result->status, sizeof(result->status), "sum u%d 0..%.0f = %.6g",
                 result->trace_fn + 1, result->trace_x, sum * sign);
        return true;
    }
    double step = (high - low) / ANALYSIS_SAMPLES;
    double previous_x = 0.0, previous_y = 0.0, previous_metric = 0.0;
    bool have_previous = false;
    for (int i = 0; i <= ANALYSIS_SAMPLES; i++) {
        double input = low + step * i, x = 0.0, y = 0.0, metric = 0.0;
        if (!eval_series(request, degrees, result->trace_fn, input, &x, &y, &metric)) return false;
        if (request->graphing_mode == 1 && have_previous)
            sum += (previous_y + y) * 0.5 * (x - previous_x);
        else if (request->graphing_mode == 2 && have_previous)
            sum += 0.25 * (previous_metric * previous_metric + metric * metric) * step *
                (degrees ? PI / 180.0 : 1.0);
        else if (request->graphing_mode == 0)
            sum += metric * (i == 0 || i == ANALYSIS_SAMPLES ? 0.5 : 1.0) * step;
        previous_x = x; previous_y = y; previous_metric = metric; have_previous = true;
    }
    snprintf(result->status, sizeof(result->status), "%s %s%d 0..%.4g = %.6g",
             request->graphing_mode == 2 ? "area" : "int", series_prefix(request),
             result->trace_fn + 1, result->trace_x, sum * sign);
    return true;
}

bool opencalc_graph_analysis_run(const opencalc_graph_analysis_request_t *request,
                                 bool degrees, opencalc_graph_analysis_result_t *result)
{
    if (request == NULL || result == NULL) return false;
    memset(result, 0, sizeof(*result));
    result->trace = request->trace;
    result->trace_x = request->trace_x;
    result->trace_fn = request->trace_fn;
    switch (request->selection) {
    case 0: return value_at_trace(request, degrees, result);
    case 1: return jump_to_poi(request, degrees, OPENCALC_GRAPH_POI_ZERO, result);
    case 2: return jump_to_poi(request, degrees, OPENCALC_GRAPH_POI_MIN, result);
    case 3: return jump_to_poi(request, degrees, OPENCALC_GRAPH_POI_LOCAL_MAX, result);
    case 4: return jump_to_poi(request, degrees, OPENCALC_GRAPH_POI_INTERSECTION, result);
    case 5: return jump_to_poi(request, degrees, OPENCALC_GRAPH_POI_Y_INTERCEPT, result);
    case 6: return derivative_at_trace(request, degrees, result);
    case 7: return integral_to_trace(request, degrees, result);
    default: return false;
    }
}
