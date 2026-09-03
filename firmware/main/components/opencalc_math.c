#include "opencalc_math.h"

#include <complex.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *s;
    double x;
    char variable;
    bool ok;
} graph_parser_t;

static bool s_angle_degrees = true;
static double s_calc_vars[26];
static double complex s_calc_complex_vars[26];
static bool s_calc_var_set[26];

void opencalc_math_set_degrees(bool degrees)
{
    s_angle_degrees = degrees;
}

bool opencalc_math_degrees_enabled(void)
{
    return s_angle_degrees;
}

static double angle_to_radians(double value)
{
    return s_angle_degrees ? value * 3.14159265358979323846 / 180.0 : value;
}

static double radians_to_angle(double value)
{
    return s_angle_degrees ? value * 180.0 / 3.14159265358979323846 : value;
}

static void graph_skip_ws(graph_parser_t *p)
{
    while (isspace((unsigned char)*p->s)) {
        p->s++;
    }
}

static bool graph_match(graph_parser_t *p, const char *word)
{
    graph_skip_ws(p);
    size_t len = strlen(word);
    if (strncmp(p->s, word, len) == 0) {
        if (isalpha((unsigned char)word[len - 1]) &&
            (isalnum((unsigned char)p->s[len]) || p->s[len] == '_')) {
            return false;
        }
        p->s += len;
        return true;
    }
    return false;
}

static double graph_parse_expr(graph_parser_t *p);

static bool graph_take_expression_arg(graph_parser_t *p, char *out, size_t out_size)
{
    graph_skip_ws(p);
    if (*p->s != '(' || out == NULL || out_size == 0) {
        p->ok = false;
        return false;
    }
    p->s++;

    const char *start = p->s;
    int depth = 0;
    while (*p->s != '\0') {
        if (*p->s == '(') {
            depth++;
        } else if (*p->s == ')') {
            if (depth == 0) {
                p->ok = false;
                return false;
            }
            depth--;
        } else if (*p->s == ',' && depth == 0) {
            size_t len = (size_t)(p->s - start);
            if (len >= out_size) {
                p->ok = false;
                return false;
            }
            memcpy(out, start, len);
            out[len] = '\0';
            p->s++;
            return true;
        }
        p->s++;
    }

    p->ok = false;
    return false;
}

static bool graph_take_single_expression_arg(graph_parser_t *p, char *out, size_t out_size)
{
    graph_skip_ws(p);
    if (*p->s != '(' || out == NULL || out_size == 0) {
        p->ok = false;
        return false;
    }
    p->s++;

    const char *start = p->s;
    int depth = 0;
    while (*p->s != '\0') {
        if (*p->s == '(') {
            depth++;
        } else if (*p->s == ')') {
            if (depth == 0) {
                size_t len = (size_t)(p->s - start);
                if (len >= out_size) {
                    p->ok = false;
                    return false;
                }
                memcpy(out, start, len);
                out[len] = '\0';
                p->s++;
                return true;
            }
            depth--;
        }
        p->s++;
    }

    p->ok = false;
    return false;
}

static double graph_eval_derivative(graph_parser_t *p)
{
    char expr[64];
    if (!graph_take_single_expression_arg(p, expr, sizeof(expr))) {
        return 0.0;
    }

    double h = fmax(1e-5, fabs(p->x) * 1e-5);
    double y1 = 0.0;
    double y2 = 0.0;
    if (!graph_eval_expression(expr, p->x + h, &y1) ||
        !graph_eval_expression(expr, p->x - h, &y2)) {
        p->ok = false;
        return 0.0;
    }
    return (y1 - y2) / (2.0 * h);
}

static double graph_eval_integral(graph_parser_t *p)
{
    char expr[64];
    if (!graph_take_expression_arg(p, expr, sizeof(expr))) {
        return 0.0;
    }

    double a = graph_parse_expr(p);
    graph_skip_ws(p);
    if (*p->s != ',') {
        p->ok = false;
        return 0.0;
    }
    p->s++;

    double b = graph_parse_expr(p);
    graph_skip_ws(p);
    if (*p->s != ')') {
        p->ok = false;
        return 0.0;
    }
    p->s++;

    const int n = 96;
    double step = (b - a) / (double)n;
    double total = 0.0;
    for (int i = 0; i <= n; i++) {
        double x = a + step * (double)i;
        double y = 0.0;
        if (!graph_eval_expression(expr, x, &y)) {
            p->ok = false;
            return 0.0;
        }
        total += y * (i == 0 || i == n ? 0.5 : 1.0);
    }
    return total * step;
}

static bool graph_parse_two_args(graph_parser_t *p, double *a, double *b)
{
    graph_skip_ws(p);
    if (*p->s != '(') {
        p->ok = false;
        return false;
    }
    p->s++;

    *a = graph_parse_expr(p);
    graph_skip_ws(p);
    if (*p->s != ',') {
        p->ok = false;
        return false;
    }
    p->s++;

    *b = graph_parse_expr(p);
    graph_skip_ws(p);
    if (*p->s != ')') {
        p->ok = false;
        return false;
    }
    p->s++;
    return true;
}

static bool graph_parse_args(graph_parser_t *p, double *args, int *count, int max_args)
{
    graph_skip_ws(p);
    if (*p->s != '(') {
        p->ok = false;
        return false;
    }
    p->s++;

    *count = 0;
    graph_skip_ws(p);
    if (*p->s == ')') {
        p->s++;
        return true;
    }

    while (*count < max_args) {
        args[*count] = graph_parse_expr(p);
        (*count)++;
        if (!p->ok) {
            return false;
        }

        graph_skip_ws(p);
        if (*p->s == ',') {
            p->s++;
            continue;
        }
        if (*p->s == ')') {
            p->s++;
            return true;
        }
        p->ok = false;
        return false;
    }

    p->ok = false;
    return false;
}

static double math_factorial(double value, bool *ok)
{
    if (value < 0.0 || fabs(value - round(value)) > 0.0000001 || value > 170.0) {
        *ok = false;
        return 0.0;
    }

    double result = 1.0;
    for (int i = 2; i <= (int)round(value); i++) {
        result *= (double)i;
    }
    return result;
}

static double math_gcd(double a, double b)
{
    long long x = llabs((long long)round(a));
    long long y = llabs((long long)round(b));
    while (y != 0) {
        long long r = x % y;
        x = y;
        y = r;
    }
    return (double)x;
}

static double math_lcm(double a, double b)
{
    long long x = llabs((long long)round(a));
    long long y = llabs((long long)round(b));
    double g = math_gcd((double)x, (double)y);
    if (g == 0.0) {
        return 0.0;
    }
    return fabs((double)(x / (long long)g) * (double)y);
}

static double graph_parse_primary(graph_parser_t *p)
{
    graph_skip_ws(p);

    if (*p->s == '(') {
        p->s++;
        double v = graph_parse_expr(p);
        graph_skip_ws(p);
        if (*p->s == ')') {
            p->s++;
        } else {
            p->ok = false;
        }
        return v;
    }

    if (*p->s == 'x' || *p->s == 'X' || *p->s == 't' || *p->s == 'T' || *p->s == 'n' || *p->s == 'N') {
        char found = (char)tolower((unsigned char)*p->s);
        p->s++;
        if (found == (char)tolower((unsigned char)p->variable)) {
            return p->x;
        }
        p->ok = false;
        return 0.0;
    }

    if (graph_match(p, "pi")) {
        return 3.14159265358979323846;
    }
    if (graph_match(p, "e")) {
        return 2.71828182845904523536;
    }

    if (graph_match(p, "sin")) return sin(angle_to_radians(graph_parse_primary(p)));
    if (graph_match(p, "cos")) return cos(angle_to_radians(graph_parse_primary(p)));
    if (graph_match(p, "tan")) return tan(angle_to_radians(graph_parse_primary(p)));
    if (graph_match(p, "sec")) return 1.0 / cos(angle_to_radians(graph_parse_primary(p)));
    if (graph_match(p, "csc")) return 1.0 / sin(angle_to_radians(graph_parse_primary(p)));
    if (graph_match(p, "cot")) return 1.0 / tan(angle_to_radians(graph_parse_primary(p)));
    if (graph_match(p, "asin")) return radians_to_angle(asin(graph_parse_primary(p)));
    if (graph_match(p, "acos")) return radians_to_angle(acos(graph_parse_primary(p)));
    if (graph_match(p, "atan")) return radians_to_angle(atan(graph_parse_primary(p)));
    if (graph_match(p, "deriv")) return graph_eval_derivative(p);
    if (graph_match(p, "nDeriv")) return graph_eval_derivative(p);
    if (graph_match(p, "fnInt")) return graph_eval_integral(p);
    if (graph_match(p, "sqrt")) return sqrt(graph_parse_primary(p));
    if (graph_match(p, "cbrt")) return cbrt(graph_parse_primary(p));
    if (graph_match(p, "nroot")) {
        double index = 0.0;
        double value = 0.0;
        if (!graph_parse_two_args(p, &index, &value) || index == 0.0) {
            p->ok = false;
            return 0.0;
        }
        return pow(value, 1.0 / index);
    }
    if (graph_match(p, "root")) {
        double a = 0.0;
        double b = 0.0;
        if (!graph_parse_two_args(p, &a, &b) || b == 0.0) {
            p->ok = false;
            return 0.0;
        }
        return pow(a, 1.0 / b);
    }
    if (graph_match(p, "frac")) {
        double a = 0.0;
        double b = 0.0;
        if (!graph_parse_two_args(p, &a, &b) || b == 0.0) {
            p->ok = false;
            return 0.0;
        }
        return a / b;
    }
    if (graph_match(p, "min")) {
        double a = 0.0;
        double b = 0.0;
        return graph_parse_two_args(p, &a, &b) ? fmin(a, b) : 0.0;
    }
    if (graph_match(p, "max")) {
        double a = 0.0;
        double b = 0.0;
        return graph_parse_two_args(p, &a, &b) ? fmax(a, b) : 0.0;
    }
    if (graph_match(p, "abs")) return fabs(graph_parse_primary(p));
    if (graph_match(p, "ln")) return log(graph_parse_primary(p));
    if (graph_match(p, "log")) return log10(graph_parse_primary(p));

    double args[4] = {0};
    int count = 0;
    if (graph_match(p, "dec")) {
        return graph_parse_args(p, args, &count, 4) && count == 1 ? args[0] : 0.0;
    }
    if (graph_match(p, "round")) {
        if (!graph_parse_args(p, args, &count, 4) || (count != 1 && count != 2)) return 0.0;
        double scale = pow(10.0, count == 2 ? args[1] : 0.0);
        return round(args[0] * scale) / scale;
    }
    if (graph_match(p, "iPart")) return graph_parse_args(p, args, &count, 4) && count == 1 ? trunc(args[0]) : 0.0;
    if (graph_match(p, "fPart")) return graph_parse_args(p, args, &count, 4) && count == 1 ? args[0] - trunc(args[0]) : 0.0;
    if (graph_match(p, "remainder")) return graph_parse_args(p, args, &count, 4) && count == 2 ? fmod(args[0], args[1]) : 0.0;
    if (graph_match(p, "gcd")) return graph_parse_args(p, args, &count, 4) && count == 2 ? math_gcd(args[0], args[1]) : 0.0;
    if (graph_match(p, "lcm")) return graph_parse_args(p, args, &count, 4) && count == 2 ? math_lcm(args[0], args[1]) : 0.0;
    if (graph_match(p, "rand")) return graph_parse_args(p, args, &count, 4) && count == 0 ? (double)rand() / (double)RAND_MAX : 0.0;
    if (graph_match(p, "randInt")) {
        if (!graph_parse_args(p, args, &count, 4) || count != 2) return 0.0;
        int lo = (int)round(args[0]);
        int hi = (int)round(args[1]);
        if (hi < lo) {
            p->ok = false;
            return 0.0;
        }
        return (double)(lo + rand() % (hi - lo + 1));
    }
    if (graph_match(p, "randNorm")) {
        if (!graph_parse_args(p, args, &count, 4) || count != 2) return 0.0;
        double u1 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
        double u2 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
        double z = sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265358979323846 * u2);
        return args[0] + z * args[1];
    }
    if (graph_match(p, "conj")) return graph_parse_args(p, args, &count, 4) && count == 1 ? args[0] : 0.0;
    if (graph_match(p, "real")) return graph_parse_args(p, args, &count, 4) && count == 1 ? args[0] : 0.0;
    if (graph_match(p, "imag")) return graph_parse_args(p, args, &count, 4) && count == 1 ? 0.0 : 0.0;
    if (graph_match(p, "angle")) return graph_parse_args(p, args, &count, 4) && count == 1 ? (args[0] < 0.0 ? 3.14159265358979323846 : 0.0) : 0.0;
    if (graph_match(p, "nPr")) {
        if (!graph_parse_args(p, args, &count, 4) || count != 2) return 0.0;
        bool ok = true;
        double n = round(args[0]);
        double r = round(args[1]);
        if (r < 0.0 || n < 0.0 || r > n) {
            p->ok = false;
            return 0.0;
        }
        double nf = math_factorial(n, &ok);
        double rf = math_factorial(r, &ok);
        double nrf = math_factorial(n - r, &ok);
        if (!ok) {
            p->ok = false;
            return 0.0;
        }
        (void)rf;
        return nf / nrf;
    }
    if (graph_match(p, "nCr")) {
        if (!graph_parse_args(p, args, &count, 4) || count != 2) return 0.0;
        bool ok = true;
        double n = round(args[0]);
        double r = round(args[1]);
        if (r < 0.0 || n < 0.0 || r > n) {
            p->ok = false;
            return 0.0;
        }
        double nf = math_factorial(n, &ok);
        double rf = math_factorial(r, &ok);
        double nrf = math_factorial(n - r, &ok);
        if (!ok) {
            p->ok = false;
            return 0.0;
        }
        return nf / (rf * nrf);
    }

    char *end = NULL;
    double v = strtod(p->s, &end);
    if (end != p->s) {
        p->s = end;
        return v;
    }

    p->ok = false;
    return 0.0;
}

static double graph_parse_unary(graph_parser_t *p)
{
    graph_skip_ws(p);
    if (*p->s == '+') {
        p->s++;
        return graph_parse_unary(p);
    }
    if (*p->s == '-') {
        p->s++;
        return -graph_parse_unary(p);
    }
    return graph_parse_primary(p);
}

static double graph_parse_power(graph_parser_t *p)
{
    double left = graph_parse_unary(p);
    graph_skip_ws(p);
    if (*p->s == '^') {
        p->s++;
        double right = graph_parse_power(p);
        left = pow(left, right);
    }
    return left;
}

static double graph_parse_postfix(graph_parser_t *p)
{
    double v = graph_parse_power(p);
    while (true) {
        graph_skip_ws(p);
        if (*p->s != '!') {
            return v;
        }
        p->s++;
        v = math_factorial(v, &p->ok);
    }
}

static double graph_parse_term(graph_parser_t *p)
{
    double v = graph_parse_postfix(p);
    while (true) {
        graph_skip_ws(p);
        if (*p->s == '*') {
            p->s++;
            v *= graph_parse_postfix(p);
        } else if (*p->s == '/') {
            p->s++;
            double d = graph_parse_postfix(p);
            if (d == 0.0) {
                p->ok = false;
                return 0.0;
            }
            v /= d;
        } else if (*p->s == '%') {
            p->s++;
            double d = graph_parse_postfix(p);
            if (d == 0.0) {
                p->ok = false;
                return 0.0;
            }
            v = fmod(v, d);
        } else {
            return v;
        }
    }
}

static double graph_parse_expr(graph_parser_t *p)
{
    double v = graph_parse_term(p);
    while (true) {
        graph_skip_ws(p);
        if (*p->s == '+') {
            p->s++;
            v += graph_parse_term(p);
        } else if (*p->s == '-') {
            p->s++;
            v -= graph_parse_term(p);
        } else {
            return v;
        }
    }
}

bool graph_eval_expression(const char *expr, double x, double *out)
{
    return graph_eval_expression_var(expr, 'x', x, out);
}

bool graph_eval_expression_var(const char *expr, char variable, double value, double *out)
{
    if (expr == NULL || out == NULL) {
        return false;
    }

    graph_parser_t p = {.s = expr, .x = value, .variable = variable, .ok = true};
    double y = graph_parse_expr(&p);
    graph_skip_ws(&p);
    if (!p.ok || *p.s != '\0' || !isfinite(y)) {
        return false;
    }
    *out = y;
    return true;
}

int graph_screen_x(const graph_view_t *view, double x)
{
    return (int)((x - view->xmin) * (double)(view->screen_w - 1) / (view->xmax - view->xmin));
}

int graph_screen_y(const graph_view_t *view, double y)
{
    return view->screen_bottom - (int)((y - view->ymin) * (double)(view->screen_bottom - view->screen_top) / (view->ymax - view->ymin));
}

double graph_world_x(const graph_view_t *view, int px)
{
    return view->xmin + (double)px * (view->xmax - view->xmin) / (double)(view->screen_w - 1);
}

static void compact_expr(const char *expr, char *out, size_t out_size)
{
    size_t j = 0;
    if (out_size == 0) {
        return;
    }
    for (size_t i = 0; expr != NULL && expr[i] != '\0' && j + 1 < out_size; i++) {
        if (!isspace((unsigned char)expr[i])) {
            out[j++] = expr[i];
        }
    }
    out[j] = '\0';
}

static void symbolic_extract_expr_and_var(char *expr, char *variable)
{
    int depth = 0;
    char *last_comma = NULL;
    *variable = 'x';
    for (char *p = expr; *p != '\0'; p++) {
        if (*p == '(') {
            depth++;
        } else if (*p == ')' && depth > 0) {
            depth--;
        } else if (*p == ',' && depth == 0) {
            last_comma = p;
        }
    }

    if (last_comma != NULL && isalpha((unsigned char)last_comma[1]) && last_comma[2] == '\0') {
        *variable = (char)tolower((unsigned char)last_comma[1]);
        *last_comma = '\0';
    }
}

bool opencalc_math_derivative_expression(const char *expr, char *out, size_t out_size)
{
    char e[64];
    char variable = 'x';
    double c = 0.0;
    double n = 0.0;
    compact_expr(expr, e, sizeof(e));
    symbolic_extract_expr_and_var(e, &variable);

    if (out == NULL || out_size == 0 || e[0] == '\0') {
        return false;
    }
    char var_expr[2] = {variable, '\0'};
    char pattern[32];

    if (strcmp(e, var_expr) == 0) {
        snprintf(out, out_size, "1");
        return true;
    }
    if (sscanf(e, "%lf", &c) == 1 && strchr(e, variable) == NULL) {
        snprintf(out, out_size, "0");
        return true;
    }
    snprintf(pattern, sizeof(pattern), "%c^%%lf", variable);
    if (sscanf(e, pattern, &n) == 1) {
        if (fabs(n - 1.0) < 0.000001) {
            snprintf(out, out_size, "1");
        } else {
            snprintf(out, out_size, "%.6g*%c^%.6g", n, variable, n - 1.0);
        }
        return true;
    }
    snprintf(pattern, sizeof(pattern), "%%lf*%c^%%lf", variable);
    if (sscanf(e, pattern, &c, &n) == 2) {
        if (fabs(n - 1.0) < 0.000001) {
            snprintf(out, out_size, "%.6g", c);
        } else {
            snprintf(out, out_size, "%.6g*%c^%.6g", c * n, variable, n - 1.0);
        }
        return true;
    }
    snprintf(pattern, sizeof(pattern), "%%lf*%c", variable);
    if (sscanf(e, pattern, &c) == 1 && strchr(e, '^') == NULL) {
        snprintf(out, out_size, "%.6g", c);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "sin(%c)", variable);
    if (strcmp(e, pattern) == 0) {
        snprintf(out, out_size, "cos(%c)", variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "cos(%c)", variable);
    if (strcmp(e, pattern) == 0) {
        snprintf(out, out_size, "-sin(%c)", variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "tan(%c)", variable);
    if (strcmp(e, pattern) == 0) {
        snprintf(out, out_size, "1/cos(%c)^2", variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "ln(%c)", variable);
    if (strcmp(e, pattern) == 0) {
        snprintf(out, out_size, "1/%c", variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "log(%c)", variable);
    if (strcmp(e, pattern) == 0) {
        snprintf(out, out_size, "1/(%c*ln(10))", variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "sqrt(%c)", variable);
    if (strcmp(e, pattern) == 0) {
        snprintf(out, out_size, "1/(2*sqrt(%c))", variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "e^%c", variable);
    char pattern_paren[32];
    snprintf(pattern_paren, sizeof(pattern_paren), "e^(%c)", variable);
    if (strcmp(e, pattern) == 0 || strcmp(e, pattern_paren) == 0) {
        snprintf(out, out_size, "e^%c", variable);
        return true;
    }

    return false;
}

bool opencalc_math_integral_expression(const char *expr, char *out, size_t out_size)
{
    char e[64];
    char variable = 'x';
    double c = 0.0;
    double n = 0.0;
    compact_expr(expr, e, sizeof(e));
    symbolic_extract_expr_and_var(e, &variable);

    if (out == NULL || out_size == 0 || e[0] == '\0') {
        return false;
    }
    char var_expr[2] = {variable, '\0'};
    char pattern[32];

    if (strcmp(e, var_expr) == 0) {
        snprintf(out, out_size, "0.5*%c^2+C", variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "1/%c", variable);
    if (strcmp(e, pattern) == 0) {
        snprintf(out, out_size, "ln(abs(%c))+C", variable);
        return true;
    }
    if (sscanf(e, "%lf", &c) == 1 && strchr(e, variable) == NULL) {
        snprintf(out, out_size, "%.6g*%c+C", c, variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "%c^%%lf", variable);
    if (sscanf(e, pattern, &n) == 1 && fabs(n + 1.0) > 0.000001) {
        snprintf(out, out_size, "%c^%.6g/%.6g+C", variable, n + 1.0, n + 1.0);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "%%lf*%c^%%lf", variable);
    if (sscanf(e, pattern, &c, &n) == 2 && fabs(n + 1.0) > 0.000001) {
        snprintf(out, out_size, "%.6g*%c^%.6g+C", c / (n + 1.0), variable, n + 1.0);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "%%lf*%c", variable);
    if (sscanf(e, pattern, &c) == 1 && strchr(e, '^') == NULL) {
        snprintf(out, out_size, "%.6g*%c^2+C", c / 2.0, variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "sin(%c)", variable);
    if (strcmp(e, pattern) == 0) {
        snprintf(out, out_size, "-cos(%c)+C", variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "cos(%c)", variable);
    if (strcmp(e, pattern) == 0) {
        snprintf(out, out_size, "sin(%c)+C", variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "tan(%c)", variable);
    if (strcmp(e, pattern) == 0) {
        snprintf(out, out_size, "-ln(abs(cos(%c)))+C", variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "ln(%c)", variable);
    if (strcmp(e, pattern) == 0) {
        snprintf(out, out_size, "%c*ln(%c)-%c+C", variable, variable, variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "sqrt(%c)", variable);
    if (strcmp(e, pattern) == 0) {
        snprintf(out, out_size, "(2/3)*%c^(3/2)+C", variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "cbrt(%c)", variable);
    if (strcmp(e, pattern) == 0) {
        snprintf(out, out_size, "(3/4)*%c^(4/3)+C", variable);
        return true;
    }
    snprintf(pattern, sizeof(pattern), "e^%c", variable);
    char pattern_paren[32];
    snprintf(pattern_paren, sizeof(pattern_paren), "e^(%c)", variable);
    if (strcmp(e, pattern) == 0 || strcmp(e, pattern_paren) == 0) {
        snprintf(out, out_size, "e^%c+C", variable);
        return true;
    }

    return false;
}

typedef struct {
    const char *s;
    double x;
    char variable;
    bool has_variable;
    bool ok;
} calc_parser_t;

static void calc_skip_ws(calc_parser_t *p)
{
    while (isspace((unsigned char)*p->s)) {
        p->s++;
    }
}

static bool calc_match_word(calc_parser_t *p, const char *word)
{
    calc_skip_ws(p);
    size_t len = strlen(word);
    if (strncmp(p->s, word, len) != 0) {
        return false;
    }
    if (isalnum((unsigned char)p->s[len]) || p->s[len] == '_') {
        return false;
    }
    p->s += len;
    return true;
}

static double calc_parse_expr(calc_parser_t *p);

static bool calc_eval_expression_with_var(const char *expr, char variable, double value, double *out)
{
    if (expr == NULL || out == NULL) {
        return false;
    }

    calc_parser_t p = {
        .s = expr,
        .x = value,
        .variable = (char)toupper((unsigned char)variable),
        .has_variable = true,
        .ok = true,
    };
    double parsed = calc_parse_expr(&p);
    calc_skip_ws(&p);
    if (!p.ok || *p.s != '\0' || !isfinite(parsed)) {
        return false;
    }

    *out = parsed;
    return true;
}

static void calc_append_symbolic_variable_arg(const char *expr, char variable, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    snprintf(out, out_size, "%s,%c", expr != NULL ? expr : "", (char)tolower((unsigned char)variable));
}

static void calc_strip_plus_c(char *expr)
{
    if (expr == NULL) {
        return;
    }
    size_t len = strlen(expr);
    if (len >= 2 && expr[len - 2] == '+' && expr[len - 1] == 'C') {
        expr[len - 2] = '\0';
    }
}

static bool calc_take_expression_arg(calc_parser_t *p, char *out, size_t out_size)
{
    calc_skip_ws(p);
    if (*p->s != '(' || out == NULL || out_size == 0) {
        return false;
    }
    p->s++;

    const char *start = p->s;
    int depth = 0;
    while (*p->s != '\0') {
        if (*p->s == '(') {
            depth++;
        } else if (*p->s == ')') {
            if (depth == 0) {
                return false;
            }
            depth--;
        } else if (*p->s == ',' && depth == 0) {
            size_t len = (size_t)(p->s - start);
            if (len >= out_size) {
                return false;
            }
            memcpy(out, start, len);
            out[len] = '\0';
            p->s++;
            return true;
        }
        p->s++;
    }

    return false;
}

static double calc_eval_n_deriv(calc_parser_t *p)
{
    char expr[64];
    if (!calc_take_expression_arg(p, expr, sizeof(expr))) {
        p->ok = false;
        return 0.0;
    }

    char variable = 'x';
    calc_skip_ws(p);
    if (isalpha((unsigned char)*p->s)) {
        variable = (char)toupper((unsigned char)*p->s++);
        calc_skip_ws(p);
        if (*p->s != ',') {
            p->ok = false;
            return 0.0;
        }
        p->s++;
    }

    double x = calc_parse_expr(p);
    calc_skip_ws(p);
    if (!p->ok || *p->s != ')') {
        p->ok = false;
        return 0.0;
    }
    p->s++;

    char symbolic_arg[80];
    char symbolic_deriv[96];
    double symbolic_value = 0.0;
    calc_append_symbolic_variable_arg(expr, variable, symbolic_arg, sizeof(symbolic_arg));
    if (opencalc_math_derivative_expression(symbolic_arg, symbolic_deriv, sizeof(symbolic_deriv)) &&
        calc_eval_expression_with_var(symbolic_deriv, variable, x, &symbolic_value)) {
        return symbolic_value;
    }

    double h = fmax(1e-5, fabs(x) * 1e-5);
    double y1 = 0.0;
    double y2 = 0.0;
    if (!calc_eval_expression_with_var(expr, variable, x + h, &y1) ||
        !calc_eval_expression_with_var(expr, variable, x - h, &y2)) {
        p->ok = false;
        return 0.0;
    }
    return (y1 - y2) / (2.0 * h);
}

static double calc_eval_fn_int(calc_parser_t *p)
{
    char expr[64];
    if (!calc_take_expression_arg(p, expr, sizeof(expr))) {
        p->ok = false;
        return 0.0;
    }

    char variable = 'x';
    calc_skip_ws(p);
    if (isalpha((unsigned char)*p->s)) {
        variable = (char)toupper((unsigned char)*p->s++);
        calc_skip_ws(p);
        if (*p->s != ',') {
            p->ok = false;
            return 0.0;
        }
        p->s++;
    }

    double a = calc_parse_expr(p);
    calc_skip_ws(p);
    if (!p->ok || *p->s != ',') {
        p->ok = false;
        return 0.0;
    }
    p->s++;
    double b = calc_parse_expr(p);
    calc_skip_ws(p);
    if (!p->ok || *p->s != ')') {
        p->ok = false;
        return 0.0;
    }
    p->s++;

    char symbolic_arg[80];
    char symbolic_int[96];
    double fa = 0.0;
    double fb = 0.0;
    calc_append_symbolic_variable_arg(expr, variable, symbolic_arg, sizeof(symbolic_arg));
    if (opencalc_math_integral_expression(symbolic_arg, symbolic_int, sizeof(symbolic_int))) {
        calc_strip_plus_c(symbolic_int);
        if (calc_eval_expression_with_var(symbolic_int, variable, a, &fa) &&
            calc_eval_expression_with_var(symbolic_int, variable, b, &fb)) {
            return fb - fa;
        }
    }

    const int n = 128;
    double step = (b - a) / (double)n;
    double total = 0.0;
    for (int i = 0; i <= n; i++) {
        double x = a + step * (double)i;
        double y = 0.0;
        if (!calc_eval_expression_with_var(expr, variable, x, &y)) {
            p->ok = false;
            return 0.0;
        }
        total += y * (i == 0 || i == n ? 0.5 : 1.0);
    }
    return total * step;
}

static double calc_factorial(double value, bool *ok)
{
    if (value < 0.0 || fabs(value - round(value)) > 0.0000001 || value > 170.0) {
        *ok = false;
        return 0.0;
    }

    double result = 1.0;
    for (int i = 2; i <= (int)round(value); i++) {
        result *= (double)i;
    }
    return result;
}

static double calc_gcd(double a, double b)
{
    long long x = llabs((long long)round(a));
    long long y = llabs((long long)round(b));
    while (y != 0) {
        long long r = x % y;
        x = y;
        y = r;
    }
    return (double)x;
}

static double calc_lcm(double a, double b)
{
    long long x = llabs((long long)round(a));
    long long y = llabs((long long)round(b));
    double g = calc_gcd((double)x, (double)y);
    if (g == 0.0) {
        return 0.0;
    }
    return fabs((double)(x / (long long)g) * (double)y);
}

static bool calc_parse_args(calc_parser_t *p, double *args, int *count, int max_args)
{
    calc_skip_ws(p);
    if (*p->s != '(') {
        return false;
    }
    p->s++;

    *count = 0;
    calc_skip_ws(p);
    if (*p->s == ')') {
        p->s++;
        return true;
    }

    while (*count < max_args) {
        args[*count] = calc_parse_expr(p);
        (*count)++;
        if (!p->ok) {
            return false;
        }

        calc_skip_ws(p);
        if (*p->s == ',') {
            p->s++;
            continue;
        }
        if (*p->s == ')') {
            p->s++;
            return true;
        }
        return false;
    }

    return false;
}

static double calc_apply_func(calc_parser_t *p, const char *name)
{
    char lname[16];
    size_t i = 0;
    for (; name[i] != '\0' && i + 1 < sizeof(lname); i++) {
        lname[i] = (char)tolower((unsigned char)name[i]);
    }
    lname[i] = '\0';

    if (strcmp(lname, "nderiv") == 0) return calc_eval_n_deriv(p);
    if (strcmp(lname, "fnint") == 0) return calc_eval_fn_int(p);

    double args[4] = {0};
    int count = 0;
    if (!calc_parse_args(p, args, &count, 4)) {
        p->ok = false;
        return 0.0;
    }

    if (strcmp(lname, "abs") == 0 && count == 1) return fabs(args[0]);
    if (strcmp(lname, "frac") == 0 && count == 2 && args[1] != 0.0) return args[0] / args[1];
    if ((strcmp(lname, "frac") == 0 || strcmp(lname, "dec") == 0) && count == 1) return args[0];
    if (strcmp(lname, "round") == 0 && (count == 1 || count == 2)) {
        double scale = pow(10.0, count == 2 ? args[1] : 0.0);
        return round(args[0] * scale) / scale;
    }
    if (strcmp(lname, "ipart") == 0 && count == 1) return trunc(args[0]);
    if (strcmp(lname, "fpart") == 0 && count == 1) return args[0] - trunc(args[0]);
    if (strcmp(lname, "remainder") == 0 && count == 2) return fmod(args[0], args[1]);
    if (strcmp(lname, "min") == 0 && count == 2) return fmin(args[0], args[1]);
    if (strcmp(lname, "max") == 0 && count == 2) return fmax(args[0], args[1]);
    if (strcmp(lname, "gcd") == 0 && count == 2) return calc_gcd(args[0], args[1]);
    if (strcmp(lname, "lcm") == 0 && count == 2) return calc_lcm(args[0], args[1]);
    if (strcmp(lname, "sin") == 0 && count == 1) return sin(angle_to_radians(args[0]));
    if (strcmp(lname, "cos") == 0 && count == 1) return cos(angle_to_radians(args[0]));
    if (strcmp(lname, "tan") == 0 && count == 1) return tan(angle_to_radians(args[0]));
    if (strcmp(lname, "sec") == 0 && count == 1) return 1.0 / cos(angle_to_radians(args[0]));
    if (strcmp(lname, "csc") == 0 && count == 1) return 1.0 / sin(angle_to_radians(args[0]));
    if (strcmp(lname, "cot") == 0 && count == 1) return 1.0 / tan(angle_to_radians(args[0]));
    if (strcmp(lname, "asin") == 0 && count == 1) return radians_to_angle(asin(args[0]));
    if (strcmp(lname, "acos") == 0 && count == 1) return radians_to_angle(acos(args[0]));
    if (strcmp(lname, "atan") == 0 && count == 1) return radians_to_angle(atan(args[0]));
    if (strcmp(lname, "sqrt") == 0 && count == 1) return sqrt(args[0]);
    if (strcmp(lname, "cbrt") == 0 && count == 1) return cbrt(args[0]);
    if (strcmp(lname, "nroot") == 0 && count == 2 && args[0] != 0.0) return pow(args[1], 1.0 / args[0]);
    if (strcmp(lname, "root") == 0 && count == 2) return pow(args[0], 1.0 / args[1]);
    if (strcmp(lname, "ln") == 0 && count == 1) return log(args[0]);
    if (strcmp(lname, "log") == 0 && count == 1) return log10(args[0]);
    if (strcmp(lname, "rand") == 0 && count == 0) return (double)rand() / (double)RAND_MAX;
    if (strcmp(lname, "randint") == 0 && count == 2) {
        int lo = (int)round(args[0]);
        int hi = (int)round(args[1]);
        if (hi < lo) {
            p->ok = false;
            return 0.0;
        }
        return (double)(lo + rand() % (hi - lo + 1));
    }
    if (strcmp(lname, "randnorm") == 0 && count == 2) {
        double u1 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
        double u2 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
        double z = sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265358979323846 * u2);
        return args[0] + z * args[1];
    }
    if (strcmp(lname, "conj") == 0 && count == 1) return args[0];
    if (strcmp(lname, "real") == 0 && count == 1) return args[0];
    if (strcmp(lname, "imag") == 0 && count == 1) return 0.0;
    if (strcmp(lname, "angle") == 0 && count == 1) return args[0] < 0.0 ? 3.14159265358979323846 : 0.0;
    if ((strcmp(lname, "npr") == 0 || strcmp(lname, "ncr") == 0) && count == 2) {
        bool ok = true;
        double n = round(args[0]);
        double r = round(args[1]);
        if (r < 0.0 || n < 0.0 || r > n) {
            p->ok = false;
            return 0.0;
        }
        double nf = calc_factorial(n, &ok);
        double rf = calc_factorial(r, &ok);
        double nrf = calc_factorial(n - r, &ok);
        if (!ok) {
            p->ok = false;
            return 0.0;
        }
        return strcmp(lname, "npr") == 0 ? nf / nrf : nf / (rf * nrf);
    }

    p->ok = false;
    return 0.0;
}

static double calc_parse_primary(calc_parser_t *p)
{
    calc_skip_ws(p);

    if (*p->s == '(') {
        p->s++;
        double v = calc_parse_expr(p);
        calc_skip_ws(p);
        if (*p->s == ')') {
            p->s++;
        } else {
            p->ok = false;
        }
        return v;
    }

    if (calc_match_word(p, "pi")) return 3.14159265358979323846;
    if (calc_match_word(p, "e")) return 2.71828182845904523536;
    if (isalpha((unsigned char)*p->s)) {
        char name[16] = {0};
        int i = 0;
        while ((isalnum((unsigned char)*p->s) || *p->s == '_') && i < (int)sizeof(name) - 1) {
            name[i++] = *p->s++;
        }
        name[i] = '\0';
        if (name[1] == '\0') {
            if (p->has_variable && toupper((unsigned char)name[0]) == toupper((unsigned char)p->variable)) {
                return p->x;
            }
            int index = toupper((unsigned char)name[0]) - 'A';
            if (index >= 0 && index < 26) {
                if (s_calc_var_set[index]) {
                    if (fabs(cimag(s_calc_complex_vars[index])) > 1e-12) {
                        p->ok = false;
                        return 0.0;
                    }
                    return s_calc_vars[index];
                }
                p->ok = false;
                return 0.0;
            }
        }
        return calc_apply_func(p, name);
    }

    char *end = NULL;
    double v = strtod(p->s, &end);
    if (end != p->s) {
        p->s = end;
        return v;
    }

    p->ok = false;
    return 0.0;
}

static double calc_parse_unary(calc_parser_t *p)
{
    calc_skip_ws(p);
    if (*p->s == '+') {
        p->s++;
        return calc_parse_unary(p);
    }
    if (*p->s == '-') {
        p->s++;
        return -calc_parse_unary(p);
    }
    return calc_parse_primary(p);
}

static double calc_parse_power(calc_parser_t *p)
{
    double left = calc_parse_unary(p);
    calc_skip_ws(p);
    if (*p->s == '^') {
        p->s++;
        left = pow(left, calc_parse_power(p));
    }
    return left;
}

static double calc_parse_postfix(calc_parser_t *p)
{
    double v = calc_parse_power(p);
    while (true) {
        calc_skip_ws(p);
        if (*p->s != '!') {
            return v;
        }
        p->s++;
        v = calc_factorial(v, &p->ok);
    }
}

static double calc_parse_term(calc_parser_t *p)
{
    double v = calc_parse_postfix(p);
    while (true) {
        calc_skip_ws(p);
        if (*p->s == '*') {
            p->s++;
            v *= calc_parse_postfix(p);
        } else if (*p->s == '/') {
            p->s++;
            double d = calc_parse_postfix(p);
            if (d == 0.0) {
                p->ok = false;
                return 0.0;
            }
            v /= d;
        } else if (*p->s == '%') {
            p->s++;
            double d = calc_parse_postfix(p);
            if (d == 0.0) {
                p->ok = false;
                return 0.0;
            }
            v = fmod(v, d);
        } else {
            return v;
        }
    }
}

static double calc_parse_expr(calc_parser_t *p)
{
    double v = calc_parse_term(p);
    while (true) {
        calc_skip_ws(p);
        if (*p->s == '+') {
            p->s++;
            v += calc_parse_term(p);
        } else if (*p->s == '-') {
            p->s++;
            v -= calc_parse_term(p);
        } else {
            return v;
        }
    }
}

bool opencalc_math_eval_expression(const char *expr, double *out)
{
    if (expr == NULL || out == NULL) {
        return false;
    }

    const char *start = expr;
    while (isspace((unsigned char)*start)) {
        start++;
    }
    if (isalpha((unsigned char)start[0])) {
        const char *after_name = start + 1;
        while (isspace((unsigned char)*after_name)) {
            after_name++;
        }
        if (*after_name == '=') {
            int index = toupper((unsigned char)start[0]) - 'A';
            if (index < 0 || index >= 26) {
                return false;
            }
            after_name++;
            calc_parser_t assign = {.s = after_name, .x = 0.0, .variable = 'X', .has_variable = false, .ok = true};
            double assigned = calc_parse_expr(&assign);
            calc_skip_ws(&assign);
            if (!assign.ok || *assign.s != '\0' || !isfinite(assigned)) {
                return false;
            }
            s_calc_vars[index] = assigned;
            s_calc_complex_vars[index] = assigned;
            s_calc_var_set[index] = true;
            *out = assigned;
            return true;
        }
    }

    calc_parser_t p = {.s = expr, .x = 0.0, .variable = 'X', .has_variable = false, .ok = true};
    double value = calc_parse_expr(&p);
    calc_skip_ws(&p);
    if (!p.ok || *p.s != '\0' || !isfinite(value)) {
        return false;
    }

    *out = value;
    return true;
}

typedef struct {
    const char *s;
    char variable;
    double complex variable_value;
    bool has_variable;
    bool ok;
} complex_parser_t;

static void complex_skip_ws(complex_parser_t *p)
{
    while (isspace((unsigned char)*p->s)) {
        p->s++;
    }
}

static bool complex_match_word(complex_parser_t *p, const char *word)
{
    complex_skip_ws(p);
    size_t len = strlen(word);
    if (strncmp(p->s, word, len) != 0) {
        return false;
    }
    if (isalnum((unsigned char)p->s[len]) || p->s[len] == '_') {
        return false;
    }
    p->s += len;
    return true;
}

static double complex_factorial_real(double value, bool *ok)
{
    if (fabs(value - round(value)) > 1e-7 || value < 0.0 || value > 170.0) {
        *ok = false;
        return 0.0;
    }
    double result = 1.0;
    for (int i = 2; i <= (int)round(value); i++) {
        result *= (double)i;
    }
    return result;
}

static double complex complex_parse_expr(complex_parser_t *p);

static bool complex_take_expression_arg(complex_parser_t *p, char *out, size_t out_size)
{
    complex_skip_ws(p);
    if (*p->s != '(' || out == NULL || out_size == 0) {
        p->ok = false;
        return false;
    }
    p->s++;

    const char *start = p->s;
    int depth = 0;
    while (*p->s != '\0') {
        if (*p->s == '(') {
            depth++;
        } else if (*p->s == ')') {
            if (depth == 0) {
                p->ok = false;
                return false;
            }
            depth--;
        } else if (*p->s == ',' && depth == 0) {
            size_t len = (size_t)(p->s - start);
            if (len >= out_size) {
                p->ok = false;
                return false;
            }
            memcpy(out, start, len);
            out[len] = '\0';
            p->s++;
            return true;
        }
        p->s++;
    }

    p->ok = false;
    return false;
}

static double complex complex_eval_n_deriv(complex_parser_t *p)
{
    char expr[64];
    if (!complex_take_expression_arg(p, expr, sizeof(expr))) {
        return 0.0;
    }

    char variable = 'x';
    complex_skip_ws(p);
    if (isalpha((unsigned char)*p->s)) {
        variable = (char)toupper((unsigned char)*p->s++);
        complex_skip_ws(p);
        if (*p->s != ',') {
            p->ok = false;
            return 0.0;
        }
        p->s++;
    }

    double complex at = complex_parse_expr(p);
    complex_skip_ws(p);
    if (!p->ok || *p->s != ')') {
        p->ok = false;
        return 0.0;
    }
    p->s++;

    if (fabs(cimag(at)) > 1e-12) {
        p->ok = false;
        return 0.0;
    }

    double real = 0.0;
    double imag = 0.0;
    if (!opencalc_math_numeric_derivative(expr, variable, creal(at), &real, &imag)) {
        p->ok = false;
        return 0.0;
    }
    return real + imag * I;
}

static double complex complex_eval_fn_int(complex_parser_t *p)
{
    char expr[64];
    if (!complex_take_expression_arg(p, expr, sizeof(expr))) {
        return 0.0;
    }

    char variable = 'x';
    complex_skip_ws(p);
    if (isalpha((unsigned char)*p->s)) {
        variable = (char)toupper((unsigned char)*p->s++);
        complex_skip_ws(p);
        if (*p->s != ',') {
            p->ok = false;
            return 0.0;
        }
        p->s++;
    }

    double complex a = complex_parse_expr(p);
    complex_skip_ws(p);
    if (!p->ok || *p->s != ',') {
        p->ok = false;
        return 0.0;
    }
    p->s++;

    double complex b = complex_parse_expr(p);
    complex_skip_ws(p);
    if (!p->ok || *p->s != ')') {
        p->ok = false;
        return 0.0;
    }
    p->s++;

    if (fabs(cimag(a)) > 1e-12 || fabs(cimag(b)) > 1e-12) {
        p->ok = false;
        return 0.0;
    }

    double real = 0.0;
    double imag = 0.0;
    if (!opencalc_math_numeric_integral(expr, variable, creal(a), creal(b), &real, &imag)) {
        p->ok = false;
        return 0.0;
    }
    return real + imag * I;
}

static bool complex_parse_args(complex_parser_t *p, double complex *args, int *count, int max_args)
{
    complex_skip_ws(p);
    if (*p->s != '(') {
        p->ok = false;
        return false;
    }
    p->s++;

    *count = 0;
    complex_skip_ws(p);
    if (*p->s == ')') {
        p->s++;
        return true;
    }

    while (*count < max_args) {
        args[*count] = complex_parse_expr(p);
        (*count)++;
        if (!p->ok) {
            return false;
        }

        complex_skip_ws(p);
        if (*p->s == ',') {
            p->s++;
            continue;
        }
        if (*p->s == ')') {
            p->s++;
            return true;
        }
        p->ok = false;
        return false;
    }

    p->ok = false;
    return false;
}

static double complex complex_apply_func(complex_parser_t *p, const char *name)
{
    char lname[16];
    size_t i = 0;
    for (; name[i] != '\0' && i + 1 < sizeof(lname); i++) {
        lname[i] = (char)tolower((unsigned char)name[i]);
    }
    lname[i] = '\0';

    if (strcmp(lname, "nderiv") == 0) return complex_eval_n_deriv(p);
    if (strcmp(lname, "fnint") == 0) return complex_eval_fn_int(p);

    double complex args[4] = {0};
    int count = 0;
    if (!complex_parse_args(p, args, &count, 4)) {
        return 0.0;
    }

    if (strcmp(lname, "conj") == 0 && count == 1) return conj(args[0]);
    if (strcmp(lname, "real") == 0 && count == 1) return creal(args[0]);
    if (strcmp(lname, "imag") == 0 && count == 1) return cimag(args[0]);
    if (strcmp(lname, "angle") == 0 && count == 1) return radians_to_angle(carg(args[0]));
    if (strcmp(lname, "abs") == 0 && count == 1) return cabs(args[0]);
    if (strcmp(lname, "sqrt") == 0 && count == 1) {
        if (fabs(cimag(args[0])) < 1e-12 && creal(args[0]) < 0.0) {
            return sqrt(-creal(args[0])) * I;
        }
        return csqrt(args[0]);
    }
    if (strcmp(lname, "cbrt") == 0 && count == 1) return cpow(args[0], 1.0 / 3.0);
    if (strcmp(lname, "nroot") == 0 && count == 2 && cimag(args[0]) == 0.0 && creal(args[0]) != 0.0) {
        return cpow(args[1], 1.0 / creal(args[0]));
    }
    if (strcmp(lname, "sin") == 0 && count == 1) return csin(args[0] * (s_angle_degrees ? 3.14159265358979323846 / 180.0 : 1.0));
    if (strcmp(lname, "cos") == 0 && count == 1) return ccos(args[0] * (s_angle_degrees ? 3.14159265358979323846 / 180.0 : 1.0));
    if (strcmp(lname, "tan") == 0 && count == 1) return ctan(args[0] * (s_angle_degrees ? 3.14159265358979323846 / 180.0 : 1.0));
    if (strcmp(lname, "sec") == 0 && count == 1) return 1.0 / ccos(args[0] * (s_angle_degrees ? 3.14159265358979323846 / 180.0 : 1.0));
    if (strcmp(lname, "csc") == 0 && count == 1) return 1.0 / csin(args[0] * (s_angle_degrees ? 3.14159265358979323846 / 180.0 : 1.0));
    if (strcmp(lname, "cot") == 0 && count == 1) return 1.0 / ctan(args[0] * (s_angle_degrees ? 3.14159265358979323846 / 180.0 : 1.0));
    if (strcmp(lname, "asin") == 0 && count == 1) return casin(args[0]) * (s_angle_degrees ? 180.0 / 3.14159265358979323846 : 1.0);
    if (strcmp(lname, "acos") == 0 && count == 1) return cacos(args[0]) * (s_angle_degrees ? 180.0 / 3.14159265358979323846 : 1.0);
    if (strcmp(lname, "atan") == 0 && count == 1) return catan(args[0]) * (s_angle_degrees ? 180.0 / 3.14159265358979323846 : 1.0);
    if (strcmp(lname, "ln") == 0 && count == 1) return clog(args[0]);
    if (strcmp(lname, "log") == 0 && count == 1) return clog(args[0]) / log(10.0);
    if (strcmp(lname, "frac") == 0 && count == 2 && cabs(args[1]) > 0.0) return args[0] / args[1];
    if ((strcmp(lname, "frac") == 0 || strcmp(lname, "dec") == 0) && count == 1) return args[0];

    p->ok = false;
    return 0.0;
}

static double complex complex_parse_primary(complex_parser_t *p)
{
    complex_skip_ws(p);

    if (*p->s == '(') {
        p->s++;
        double complex v = complex_parse_expr(p);
        complex_skip_ws(p);
        if (*p->s == ')') {
            p->s++;
        } else {
            p->ok = false;
        }
        return v;
    }

    if (complex_match_word(p, "pi")) return 3.14159265358979323846;
    if (complex_match_word(p, "e")) return 2.71828182845904523536;
    if (complex_match_word(p, "i")) return I;

    if (isalpha((unsigned char)*p->s)) {
        char name[16] = {0};
        int i = 0;
        while ((isalnum((unsigned char)*p->s) || *p->s == '_') && i < (int)sizeof(name) - 1) {
            name[i++] = *p->s++;
        }
        name[i] = '\0';
        if (name[1] == '\0') {
            if (p->has_variable && toupper((unsigned char)name[0]) == toupper((unsigned char)p->variable)) {
                return p->variable_value;
            }
            int index = toupper((unsigned char)name[0]) - 'A';
            if (index >= 0 && index < 26 && s_calc_var_set[index]) {
                return s_calc_complex_vars[index];
            }
            p->ok = false;
            return 0.0;
        }
        return complex_apply_func(p, name);
    }

    char *end = NULL;
    double v = strtod(p->s, &end);
    if (end != p->s) {
        p->s = end;
        complex_skip_ws(p);
        if (*p->s == 'i' || *p->s == 'I') {
            p->s++;
            return v * I;
        }
        return v;
    }

    p->ok = false;
    return 0.0;
}

static double complex complex_parse_unary(complex_parser_t *p)
{
    complex_skip_ws(p);
    if (*p->s == '+') {
        p->s++;
        return complex_parse_unary(p);
    }
    if (*p->s == '-') {
        p->s++;
        return -complex_parse_unary(p);
    }
    return complex_parse_primary(p);
}

static double complex complex_parse_power(complex_parser_t *p)
{
    double complex left = complex_parse_unary(p);
    complex_skip_ws(p);
    if (*p->s == '^') {
        p->s++;
        left = cpow(left, complex_parse_power(p));
    }
    return left;
}

static double complex complex_parse_postfix(complex_parser_t *p)
{
    double complex v = complex_parse_power(p);
    while (true) {
        complex_skip_ws(p);
        if (*p->s != '!') {
            return v;
        }
        p->s++;
        if (fabs(cimag(v)) > 1e-12) {
            p->ok = false;
            return 0.0;
        }
        v = complex_factorial_real(creal(v), &p->ok);
    }
}

static double complex complex_parse_term(complex_parser_t *p)
{
    double complex v = complex_parse_postfix(p);
    while (true) {
        complex_skip_ws(p);
        if (*p->s == '*') {
            p->s++;
            v *= complex_parse_postfix(p);
        } else if (*p->s == '/') {
            p->s++;
            double complex d = complex_parse_postfix(p);
            if (cabs(d) <= 1e-15) {
                p->ok = false;
                return 0.0;
            }
            v /= d;
        } else {
            return v;
        }
    }
}

static double complex complex_parse_expr(complex_parser_t *p)
{
    double complex v = complex_parse_term(p);
    while (true) {
        complex_skip_ws(p);
        if (*p->s == '+') {
            p->s++;
            v += complex_parse_term(p);
        } else if (*p->s == '-') {
            p->s++;
            v -= complex_parse_term(p);
        } else {
            return v;
        }
    }
}

bool opencalc_math_eval_complex_expression(const char *expr, double *real, double *imag)
{
    if (expr == NULL || real == NULL || imag == NULL) {
        return false;
    }

    const char *start = expr;
    while (isspace((unsigned char)*start)) {
        start++;
    }
    if (isalpha((unsigned char)start[0])) {
        const char *after_name = start + 1;
        while (isspace((unsigned char)*after_name)) {
            after_name++;
        }
        if (*after_name == '=') {
            int index = toupper((unsigned char)start[0]) - 'A';
            if (index < 0 || index >= 26) {
                return false;
            }
            after_name++;
            complex_parser_t assign = {
                .s = after_name,
                .variable = 'X',
                .variable_value = 0.0,
                .has_variable = false,
                .ok = true,
            };
            double complex assigned = complex_parse_expr(&assign);
            complex_skip_ws(&assign);
            if (!assign.ok || *assign.s != '\0' ||
                !isfinite(creal(assigned)) || !isfinite(cimag(assigned))) {
                return false;
            }
            double assigned_real = fabs(creal(assigned)) < 1e-12 ? 0.0 : creal(assigned);
            double assigned_imag = fabs(cimag(assigned)) < 1e-12 ? 0.0 : cimag(assigned);
            s_calc_complex_vars[index] = assigned_real + assigned_imag * I;
            s_calc_vars[index] = assigned_real;
            s_calc_var_set[index] = true;
            *real = assigned_real;
            *imag = assigned_imag;
            return true;
        }
    }

    return opencalc_math_eval_complex_expression_var(expr, 'x', 0.0, 0.0, real, imag);
}

bool opencalc_math_eval_complex_expression_var(const char *expr, char variable, double real_value, double imag_value, double *real, double *imag)
{
    if (expr == NULL || real == NULL || imag == NULL) {
        return false;
    }

    complex_parser_t p = {
        .s = expr,
        .variable = (char)toupper((unsigned char)variable),
        .variable_value = real_value + imag_value * I,
        .has_variable = true,
        .ok = true,
    };
    double complex value = complex_parse_expr(&p);
    complex_skip_ws(&p);
    if (!p.ok || *p.s != '\0' || !isfinite(creal(value)) || !isfinite(cimag(value))) {
        return false;
    }

    *real = fabs(creal(value)) < 1e-12 ? 0.0 : creal(value);
    *imag = fabs(cimag(value)) < 1e-12 ? 0.0 : cimag(value);
    return true;
}

bool opencalc_math_numeric_derivative(const char *expr, char variable, double at, double *real, double *imag)
{
    if (expr == NULL || real == NULL || imag == NULL) {
        return false;
    }

    double h = fmax(1e-5, fabs(at) * 1e-5);
    double r1 = 0.0;
    double i1 = 0.0;
    double r2 = 0.0;
    double i2 = 0.0;
    if (!opencalc_math_eval_complex_expression_var(expr, variable, at + h, 0.0, &r1, &i1) ||
        !opencalc_math_eval_complex_expression_var(expr, variable, at - h, 0.0, &r2, &i2)) {
        return false;
    }

    *real = (r1 - r2) / (2.0 * h);
    *imag = (i1 - i2) / (2.0 * h);
    if (fabs(*real) < 1e-10) *real = 0.0;
    if (fabs(*imag) < 1e-10) *imag = 0.0;
    return isfinite(*real) && isfinite(*imag);
}

bool opencalc_math_numeric_integral(const char *expr, char variable, double a, double b, double *real, double *imag)
{
    if (expr == NULL || real == NULL || imag == NULL) {
        return false;
    }

    const int n = 160;
    double step = (b - a) / (double)n;
    double total_r = 0.0;
    double total_i = 0.0;
    for (int k = 0; k <= n; k++) {
        double x = a + step * (double)k;
        double r = 0.0;
        double im = 0.0;
        if (!opencalc_math_eval_complex_expression_var(expr, variable, x, 0.0, &r, &im)) {
            return false;
        }
        double weight = (k == 0 || k == n) ? 0.5 : 1.0;
        total_r += r * weight;
        total_i += im * weight;
    }

    *real = total_r * step;
    *imag = total_i * step;
    if (fabs(*real) < 1e-10) *real = 0.0;
    if (fabs(*imag) < 1e-10) *imag = 0.0;
    return isfinite(*real) && isfinite(*imag);
}
