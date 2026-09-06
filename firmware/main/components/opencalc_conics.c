#include "opencalc_conics.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CONIC_EPSILON 1e-10
#define CONIC_PI 3.14159265358979323846

static double determinant3(const opencalc_conic_t *q)
{
    double b = q->B * 0.5;
    double d = q->D * 0.5;
    double e = q->E * 0.5;
    return q->A * (q->C * q->F - e * e) -
           b * (b * q->F - d * e) +
           d * (b * e - q->C * d);
}

const char *opencalc_conic_kind_name(opencalc_conic_kind_t kind)
{
    static const char *const names[] = {
        "Circle", "Parabola", "Ellipse", "Hyperbola", "General Conic"
    };
    return (unsigned)kind <= OPENCALC_CONIC_GENERAL ? names[kind] : "Unknown";
}

void opencalc_conic_general(opencalc_conic_t *q, double A, double B, double C,
                            double D, double E, double F)
{
    if (q == NULL) return;
    *q = (opencalc_conic_t){A, B, C, D, E, F, OPENCALC_CONIC_GENERAL, true};
    q->valid = isfinite(A) && isfinite(B) && isfinite(C) && isfinite(D) &&
               isfinite(E) && isfinite(F) &&
               (fabs(A) + fabs(B) + fabs(C) + fabs(D) + fabs(E) > CONIC_EPSILON);
}

void opencalc_conic_circle(opencalc_conic_t *q, double h, double k, double radius)
{
    if (q == NULL) return;
    opencalc_conic_general(q, 1.0, 0.0, 1.0, -2.0 * h, -2.0 * k,
                           h * h + k * k - radius * radius);
    q->source_kind = OPENCALC_CONIC_CIRCLE;
    q->valid = q->valid && radius > 0.0;
}

void opencalc_conic_axis(opencalc_conic_t *q, opencalc_conic_kind_t kind,
                         double h, double k, double a, double b, double angle_degrees)
{
    if (q == NULL) return;
    double theta = angle_degrees * CONIC_PI / 180.0;
    double c = cos(theta);
    double s = sin(theta);

    if (kind == OPENCALC_CONIC_PARABOLA) {
        double A = s * s;
        double B = -2.0 * s * c;
        double C = c * c;
        double D = -2.0 * A * h - B * k - 4.0 * a * c;
        double E = -B * h - 2.0 * C * k - 4.0 * a * s;
        double F = A * h * h + B * h * k + C * k * k + 4.0 * a * (c * h + s * k);
        opencalc_conic_general(q, A, B, C, D, E, F);
        q->source_kind = kind;
        q->valid = q->valid && fabs(a) > CONIC_EPSILON;
        return;
    }

    double qa = a > 0.0 ? 1.0 / (a * a) : NAN;
    double qb = b > 0.0 ? 1.0 / (b * b) : NAN;
    if (kind == OPENCALC_CONIC_HYPERBOLA) qb = -qb;
    double A = qa * c * c + qb * s * s;
    double B = 2.0 * c * s * (qa - qb);
    double C = qa * s * s + qb * c * c;
    double D = -2.0 * A * h - B * k;
    double E = -B * h - 2.0 * C * k;
    double F = A * h * h + B * h * k + C * k * k - 1.0;
    opencalc_conic_general(q, A, B, C, D, E, F);
    q->source_kind = kind;
    q->valid = q->valid && a > 0.0 && b > 0.0;
}

double opencalc_conic_evaluate(const opencalc_conic_t *q, double x, double y)
{
    if (q == NULL || !q->valid) return NAN;
    return q->A * x * x + q->B * x * y + q->C * y * y +
           q->D * x + q->E * y + q->F;
}

bool opencalc_conic_analyze(const opencalc_conic_t *q, opencalc_conic_analysis_t *out)
{
    if (q == NULL || out == NULL || !q->valid) return false;
    memset(out, 0, sizeof(*out));
    out->kind = OPENCALC_CONIC_GENERAL;
    out->discriminant = q->B * q->B - 4.0 * q->A * q->C;
    out->rotation_degrees = 0.5 * atan2(q->B, q->A - q->C) * 180.0 / CONIC_PI;
    out->rotated = fabs(q->B) > CONIC_EPSILON;

    double scale = fmax(1.0, fabs(q->A) + fabs(q->B) + fabs(q->C) +
                             fabs(q->D) + fabs(q->E) + fabs(q->F));
    out->degenerate = fabs(determinant3(q)) <= CONIC_EPSILON * scale * scale * scale;
    double delta_tolerance = CONIC_EPSILON * scale * scale;
    if (fabs(out->discriminant) <= delta_tolerance) {
        out->kind = OPENCALC_CONIC_PARABOLA;
    } else if (out->discriminant > 0.0) {
        out->kind = OPENCALC_CONIC_HYPERBOLA;
    } else if (fabs(q->A - q->C) <= CONIC_EPSILON * scale &&
               fabs(q->B) <= CONIC_EPSILON * scale) {
        out->kind = OPENCALC_CONIC_CIRCLE;
    } else {
        out->kind = OPENCALC_CONIC_ELLIPSE;
    }

    double center_det = 4.0 * q->A * q->C - q->B * q->B;
    if (fabs(center_det) > delta_tolerance) {
        out->center_valid = true;
        out->center_x = (q->B * q->E - 2.0 * q->C * q->D) / center_det;
        out->center_y = (q->B * q->D - 2.0 * q->A * q->E) / center_det;
        out->translated_constant = opencalc_conic_evaluate(q, out->center_x, out->center_y);
    }

    double spread = hypot(q->A - q->C, q->B);
    out->eigenvalue_1 = (q->A + q->C - spread) * 0.5;
    out->eigenvalue_2 = (q->A + q->C + spread) * 0.5;
    out->real_graph = !out->degenerate;
    if (out->center_valid) {
        double axis1_sq = -out->translated_constant / out->eigenvalue_1;
        double axis2_sq = -out->translated_constant / out->eigenvalue_2;
        if (out->kind == OPENCALC_CONIC_CIRCLE || out->kind == OPENCALC_CONIC_ELLIPSE) {
            out->real_graph = axis1_sq > 0.0 && axis2_sq > 0.0;
        } else if (out->kind == OPENCALC_CONIC_HYPERBOLA) {
            out->real_graph = fabs(out->translated_constant) > CONIC_EPSILON;
        }
        out->semi_axis_1 = axis1_sq > 0.0 ? sqrt(axis1_sq) : 0.0;
        out->semi_axis_2 = axis2_sq > 0.0 ? sqrt(axis2_sq) : 0.0;
        double major = fmax(out->semi_axis_1, out->semi_axis_2);
        double minor = fmin(out->semi_axis_1, out->semi_axis_2);
        if ((out->kind == OPENCALC_CONIC_CIRCLE || out->kind == OPENCALC_CONIC_ELLIPSE) && major > 0.0) {
            out->focal_length = sqrt(fmax(0.0, major * major - minor * minor));
            out->eccentricity = out->focal_length / major;
        } else if (out->kind == OPENCALC_CONIC_HYPERBOLA) {
            double positive = out->semi_axis_1 > 0.0 ? out->semi_axis_1 : out->semi_axis_2;
            double negative_sq = out->semi_axis_1 > 0.0 ? fabs(axis2_sq) : fabs(axis1_sq);
            if (positive > 0.0) {
                out->focal_length = sqrt(positive * positive + negative_sq);
                out->eccentricity = out->focal_length / positive;
            }
        }
    }
    return true;
}

bool opencalc_conic_tangent(const opencalc_conic_t *q, double x, double y,
                            double tolerance, double *gx, double *gy)
{
    if (q == NULL || gx == NULL || gy == NULL || !q->valid) return false;
    double scale = fmax(1.0, fabs(q->A) + fabs(q->B) + fabs(q->C) +
                             fabs(q->D) + fabs(q->E) + fabs(q->F));
    if (fabs(opencalc_conic_evaluate(q, x, y)) > fabs(tolerance) * scale) return false;
    *gx = 2.0 * q->A * x + q->B * y + q->D;
    *gy = q->B * x + 2.0 * q->C * y + q->E;
    return hypot(*gx, *gy) > CONIC_EPSILON;
}

bool opencalc_conic_circle_through_points(opencalc_conic_t *q,
                                          double x1, double y1, double x2, double y2,
                                          double x3, double y3)
{
    double det = 2.0 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));
    if (q == NULL || fabs(det) <= CONIC_EPSILON) return false;
    double n1 = x1 * x1 + y1 * y1;
    double n2 = x2 * x2 + y2 * y2;
    double n3 = x3 * x3 + y3 * y3;
    double h = (n1 * (y2 - y3) + n2 * (y3 - y1) + n3 * (y1 - y2)) / det;
    double k = (n1 * (x3 - x2) + n2 * (x1 - x3) + n3 * (x2 - x1)) / det;
    double radius = hypot(x1 - h, y1 - k);
    opencalc_conic_circle(q, h, k, radius);
    return q->valid;
}

bool opencalc_conic_through_five_points(opencalc_conic_t *q,
                                        const double points[10])
{
    if (q == NULL || points == NULL) return false;
    double matrix[5][6];
    for (int row = 0; row < 5; row++) {
        double x = points[row * 2];
        double y = points[row * 2 + 1];
        if (!isfinite(x) || !isfinite(y)) return false;
        matrix[row][0] = x * x;
        matrix[row][1] = x * y;
        matrix[row][2] = y * y;
        matrix[row][3] = x;
        matrix[row][4] = y;
        matrix[row][5] = 1.0;
    }

    int pivots[5];
    bool is_pivot[6] = {false, false, false, false, false, false};
    int rank = 0;
    for (int column = 0; column < 6 && rank < 5; column++) {
        int best = rank;
        for (int row = rank + 1; row < 5; row++) {
            if (fabs(matrix[row][column]) > fabs(matrix[best][column])) best = row;
        }
        if (fabs(matrix[best][column]) <= CONIC_EPSILON) continue;
        if (best != rank) {
            for (int j = 0; j < 6; j++) {
                double swap = matrix[rank][j];
                matrix[rank][j] = matrix[best][j];
                matrix[best][j] = swap;
            }
        }
        double divisor = matrix[rank][column];
        for (int j = 0; j < 6; j++) matrix[rank][j] /= divisor;
        for (int row = 0; row < 5; row++) {
            if (row == rank) continue;
            double factor = matrix[row][column];
            for (int j = 0; j < 6; j++) matrix[row][j] -= factor * matrix[rank][j];
        }
        pivots[rank] = column;
        is_pivot[column] = true;
        rank++;
    }
    if (rank != 5) return false;

    int free_column = 0;
    while (free_column < 6 && is_pivot[free_column]) free_column++;
    if (free_column >= 6) return false;
    double coefficients[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    coefficients[free_column] = 1.0;
    for (int row = 0; row < rank; row++) {
        coefficients[pivots[row]] = -matrix[row][free_column];
    }
    double scale = 0.0;
    for (int i = 0; i < 6; i++) scale = fmax(scale, fabs(coefficients[i]));
    if (scale <= CONIC_EPSILON) return false;
    for (int i = 0; i < 6; i++) coefficients[i] /= scale;
    opencalc_conic_general(q, coefficients[0], coefficients[1], coefficients[2],
                           coefficients[3], coefficients[4], coefficients[5]);
    return q->valid;
}

int opencalc_conic_y_expressions(const opencalc_conic_t *q,
                                 char *upper, size_t upper_size,
                                 char *lower, size_t lower_size)
{
    if (q == NULL || !q->valid || upper == NULL || upper_size == 0) return 0;
    upper[0] = '\0';
    if (lower != NULL && lower_size > 0) lower[0] = '\0';
    if (fabs(q->C) > CONIC_EPSILON) {
        double q2 = q->B * q->B - 4.0 * q->C * q->A;
        double q1 = 2.0 * q->B * q->E - 4.0 * q->C * q->D;
        double q0 = q->E * q->E - 4.0 * q->C * q->F;
        int written = snprintf(upper, upper_size,
                               "(-(%g*x%+g)+sqrt(%g*x^2%+g*x%+g))/(%g)",
                               q->B, q->E, q2, q1, q0, 2.0 * q->C);
        if (written < 0 || (size_t)written >= upper_size) {
            upper[0] = '\0';
            return 0;
        }
        if (lower != NULL && lower_size > 0) {
            written = snprintf(lower, lower_size,
                               "(-(%g*x%+g)-sqrt(%g*x^2%+g*x%+g))/(%g)",
                               q->B, q->E, q2, q1, q0, 2.0 * q->C);
            if (written < 0 || (size_t)written >= lower_size) {
                upper[0] = '\0';
                lower[0] = '\0';
                return 0;
            }
        } else {
            upper[0] = '\0';
            return 0;
        }
        return 2;
    }
    if (fabs(q->B) > CONIC_EPSILON || fabs(q->E) > CONIC_EPSILON) {
        int written = snprintf(upper, upper_size, "-(%g*x^2%+g*x%+g)/(%g*x%+g)",
                               q->A, q->D, q->F, q->B, q->E);
        if (written < 0 || (size_t)written >= upper_size) {
            upper[0] = '\0';
            return 0;
        }
        return 1;
    }
    return 0;
}
