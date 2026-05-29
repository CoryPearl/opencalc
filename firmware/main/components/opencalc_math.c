#include "opencalc_math.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *s;
    double x;
    bool ok;
} graph_parser_t;

static bool s_angle_degrees = true;

void opencalc_math_set_degrees(bool degrees)
{
    s_angle_degrees = degrees;
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

    if (*p->s == 'x' || *p->s == 'X') {
        p->s++;
        return p->x;
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
    if (expr == NULL || out == NULL) {
        return false;
    }

    graph_parser_t p = {.s = expr, .x = x, .ok = true};
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

bool opencalc_math_derivative_expression(const char *expr, char *out, size_t out_size)
{
    char e[64];
    double c = 0.0;
    double n = 0.0;
    compact_expr(expr, e, sizeof(e));

    if (out == NULL || out_size == 0 || e[0] == '\0') {
        return false;
    }
    if (strcmp(e, "x") == 0) {
        snprintf(out, out_size, "1");
        return true;
    }
    if (sscanf(e, "%lf", &c) == 1 && strchr(e, 'x') == NULL) {
        snprintf(out, out_size, "0");
        return true;
    }
    if (sscanf(e, "x^%lf", &n) == 1) {
        if (fabs(n - 1.0) < 0.000001) {
            snprintf(out, out_size, "1");
        } else {
            snprintf(out, out_size, "%.6g*x^%.6g", n, n - 1.0);
        }
        return true;
    }
    if (sscanf(e, "%lf*x", &c) == 1 && strchr(e, '^') == NULL) {
        snprintf(out, out_size, "%.6g", c);
        return true;
    }
    if (strcmp(e, "sin(x)") == 0) {
        snprintf(out, out_size, "cos(x)");
        return true;
    }
    if (strcmp(e, "cos(x)") == 0) {
        snprintf(out, out_size, "-sin(x)");
        return true;
    }
    if (strcmp(e, "tan(x)") == 0) {
        snprintf(out, out_size, "1/cos(x)^2");
        return true;
    }
    if (strcmp(e, "ln(x)") == 0) {
        snprintf(out, out_size, "1/x");
        return true;
    }
    if (strcmp(e, "log(x)") == 0) {
        snprintf(out, out_size, "1/(x*ln(10))");
        return true;
    }
    if (strcmp(e, "sqrt(x)") == 0) {
        snprintf(out, out_size, "1/(2*sqrt(x))");
        return true;
    }
    if (strcmp(e, "e^x") == 0 || strcmp(e, "e^(x)") == 0) {
        snprintf(out, out_size, "e^x");
        return true;
    }

    return false;
}

bool opencalc_math_integral_expression(const char *expr, char *out, size_t out_size)
{
    char e[64];
    double c = 0.0;
    double n = 0.0;
    compact_expr(expr, e, sizeof(e));

    if (out == NULL || out_size == 0 || e[0] == '\0') {
        return false;
    }
    if (strcmp(e, "x") == 0) {
        snprintf(out, out_size, "0.5*x^2+C");
        return true;
    }
    if (strcmp(e, "1/x") == 0) {
        snprintf(out, out_size, "ln(abs(x))+C");
        return true;
    }
    if (sscanf(e, "%lf", &c) == 1 && strchr(e, 'x') == NULL) {
        snprintf(out, out_size, "%.6g*x+C", c);
        return true;
    }
    if (sscanf(e, "x^%lf", &n) == 1 && fabs(n + 1.0) > 0.000001) {
        snprintf(out, out_size, "x^%.6g/%.6g+C", n + 1.0, n + 1.0);
        return true;
    }
    if (sscanf(e, "%lf*x", &c) == 1 && strchr(e, '^') == NULL) {
        snprintf(out, out_size, "%.6g*x^2+C", c / 2.0);
        return true;
    }
    if (strcmp(e, "sin(x)") == 0) {
        snprintf(out, out_size, "-cos(x)+C");
        return true;
    }
    if (strcmp(e, "cos(x)") == 0) {
        snprintf(out, out_size, "sin(x)+C");
        return true;
    }
    if (strcmp(e, "tan(x)") == 0) {
        snprintf(out, out_size, "-ln(abs(cos(x)))+C");
        return true;
    }
    if (strcmp(e, "ln(x)") == 0) {
        snprintf(out, out_size, "x*ln(x)-x+C");
        return true;
    }
    if (strcmp(e, "sqrt(x)") == 0) {
        snprintf(out, out_size, "(2/3)*x^(3/2)+C");
        return true;
    }
    if (strcmp(e, "cbrt(x)") == 0) {
        snprintf(out, out_size, "(3/4)*x^(4/3)+C");
        return true;
    }
    if (strcmp(e, "e^x") == 0 || strcmp(e, "e^(x)") == 0) {
        snprintf(out, out_size, "e^x+C");
        return true;
    }

    return false;
}

typedef struct {
    const char *s;
    double x;
    bool has_x;
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

static bool calc_eval_expression_with_x(const char *expr, double x, double *out)
{
    if (expr == NULL || out == NULL) {
        return false;
    }

    calc_parser_t p = {.s = expr, .x = x, .has_x = true, .ok = true};
    double value = calc_parse_expr(&p);
    calc_skip_ws(&p);
    if (!p.ok || *p.s != '\0' || !isfinite(value)) {
        return false;
    }

    *out = value;
    return true;
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

    double x = calc_parse_expr(p);
    calc_skip_ws(p);
    if (!p->ok || *p->s != ')') {
        p->ok = false;
        return 0.0;
    }
    p->s++;

    double h = fmax(1e-5, fabs(x) * 1e-5);
    double y1 = 0.0;
    double y2 = 0.0;
    if (!calc_eval_expression_with_x(expr, x + h, &y1) ||
        !calc_eval_expression_with_x(expr, x - h, &y2)) {
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

    const int n = 128;
    double step = (b - a) / (double)n;
    double total = 0.0;
    for (int i = 0; i <= n; i++) {
        double x = a + step * (double)i;
        double y = 0.0;
        if (!calc_eval_expression_with_x(expr, x, &y)) {
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
    if (calc_match_word(p, "x")) {
        if (p->has_x) {
            return p->x;
        }
        p->ok = false;
        return 0.0;
    }

    if (isalpha((unsigned char)*p->s)) {
        char name[16] = {0};
        int i = 0;
        while ((isalnum((unsigned char)*p->s) || *p->s == '_') && i < (int)sizeof(name) - 1) {
            name[i++] = *p->s++;
        }
        name[i] = '\0';
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

    calc_parser_t p = {.s = expr, .x = 0.0, .has_x = false, .ok = true};
    double value = calc_parse_expr(&p);
    calc_skip_ws(&p);
    if (!p.ok || *p.s != '\0' || !isfinite(value)) {
        return false;
    }

    *out = value;
    return true;
}
