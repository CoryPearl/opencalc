#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OPENCALC_CONIC_CIRCLE = 0,
    OPENCALC_CONIC_PARABOLA,
    OPENCALC_CONIC_ELLIPSE,
    OPENCALC_CONIC_HYPERBOLA,
    OPENCALC_CONIC_GENERAL,
} opencalc_conic_kind_t;

typedef struct {
    double A;
    double B;
    double C;
    double D;
    double E;
    double F;
    opencalc_conic_kind_t source_kind;
    bool valid;
} opencalc_conic_t;

typedef struct {
    opencalc_conic_kind_t kind;
    double discriminant;
    double rotation_degrees;
    bool rotated;
    bool degenerate;
    bool real_graph;
    bool center_valid;
    double center_x;
    double center_y;
    double translated_constant;
    double eigenvalue_1;
    double eigenvalue_2;
    double semi_axis_1;
    double semi_axis_2;
    double focal_length;
    double eccentricity;
} opencalc_conic_analysis_t;

void opencalc_conic_circle(opencalc_conic_t *conic, double h, double k, double radius);
void opencalc_conic_axis(opencalc_conic_t *conic, opencalc_conic_kind_t kind,
                         double h, double k, double a, double b, double angle_degrees);
void opencalc_conic_general(opencalc_conic_t *conic, double A, double B, double C,
                            double D, double E, double F);
bool opencalc_conic_analyze(const opencalc_conic_t *conic, opencalc_conic_analysis_t *analysis);
double opencalc_conic_evaluate(const opencalc_conic_t *conic, double x, double y);
bool opencalc_conic_tangent(const opencalc_conic_t *conic, double x, double y,
                            double tolerance, double *gradient_x, double *gradient_y);
bool opencalc_conic_circle_through_points(opencalc_conic_t *conic,
                                          double x1, double y1, double x2, double y2,
                                          double x3, double y3);
bool opencalc_conic_through_five_points(opencalc_conic_t *conic,
                                        const double points[10]);
int opencalc_conic_y_expressions(const opencalc_conic_t *conic,
                                 char *upper, size_t upper_size,
                                 char *lower, size_t lower_size);
const char *opencalc_conic_kind_name(opencalc_conic_kind_t kind);

#ifdef __cplusplus
}
#endif
