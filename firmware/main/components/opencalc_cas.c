#include "opencalc_cas.h"

#include "opencalc_config.h"
#include "opencalc_eigenmath.h"
#include "opencalc_giac.h"
#include "opencalc_math.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAS_POLY_MAX_DEGREE 10
#define CAS_EPSILON 1e-10

typedef struct {
    double coeff[CAS_POLY_MAX_DEGREE + 1];
    int degree;
} cas_poly_t;

typedef struct {
    const char *cursor;
    char variable;
    bool ok;
} cas_poly_parser_t;

typedef struct {
    char *data;
    size_t size;
    size_t used;
    bool ok;
} cas_writer_t;

static bool contains_call(const char *expression, const char *name)
{
    size_t name_length = strlen(name);
    for (const char *cursor = expression; *cursor != '\0'; cursor++) {
        if (cursor != expression &&
            (isalnum((unsigned char)cursor[-1]) || cursor[-1] == '_')) {
            continue;
        }
        size_t i = 0;
        while (i < name_length && cursor[i] != '\0' &&
               tolower((unsigned char)cursor[i]) ==
                   tolower((unsigned char)name[i])) {
            i++;
        }
        if (i != name_length) continue;
        const char *after = cursor + name_length;
        if (isalnum((unsigned char)*after) || *after == '_') continue;
        while (isspace((unsigned char)*after)) after++;
        if (*after == '(') return true;
    }
    return false;
}

static bool prefer_existing_backend_semantics(const char *expression)
{
    static const char *const functions[] = {
        "nDeriv", "fnInt", "frac", "dec", "rand", "randInt",
        "randNorm", "nroot", "root", "csc", "cot", "nPr", "nCr",
        "iPart", "fPart", "remainder", "gamma", "polar", "rect",
        "rationalize",
    };
    for (size_t i = 0; i < sizeof(functions) / sizeof(functions[0]); i++) {
        if (contains_call(expression, functions[i])) return true;
    }
    return false;
}

static bool nearly_zero(double value)
{
    return fabs(value) < CAS_EPSILON;
}

static bool nearly_equal(double a, double b)
{
    return fabs(a - b) < CAS_EPSILON;
}

static cas_poly_t poly_constant(double value)
{
    cas_poly_t result = {0};
    result.coeff[0] = nearly_zero(value) ? 0.0 : value;
    return result;
}

static cas_poly_t poly_variable(void)
{
    cas_poly_t result = {0};
    result.coeff[1] = 1.0;
    result.degree = 1;
    return result;
}

static void poly_normalize(cas_poly_t *poly)
{
    while (poly->degree > 0 && nearly_zero(poly->coeff[poly->degree])) {
        poly->degree--;
    }
    for (int i = 0; i <= poly->degree; i++) {
        if (nearly_zero(poly->coeff[i])) {
            poly->coeff[i] = 0.0;
        }
    }
}

static bool poly_add(cas_poly_t left, cas_poly_t right, double right_sign, cas_poly_t *out)
{
    memset(out, 0, sizeof(*out));
    out->degree = left.degree > right.degree ? left.degree : right.degree;
    for (int i = 0; i <= out->degree; i++) {
        out->coeff[i] = left.coeff[i] + right_sign * right.coeff[i];
    }
    poly_normalize(out);
    return true;
}

static bool poly_multiply(cas_poly_t left, cas_poly_t right, cas_poly_t *out)
{
    if (left.degree + right.degree > CAS_POLY_MAX_DEGREE) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->degree = left.degree + right.degree;
    for (int i = 0; i <= left.degree; i++) {
        for (int j = 0; j <= right.degree; j++) {
            out->coeff[i + j] += left.coeff[i] * right.coeff[j];
        }
    }
    poly_normalize(out);
    return true;
}

static bool poly_power(cas_poly_t base, int exponent, cas_poly_t *out)
{
    if (exponent < 0 || exponent > CAS_POLY_MAX_DEGREE || base.degree * exponent > CAS_POLY_MAX_DEGREE) {
        return false;
    }
    cas_poly_t result = poly_constant(1.0);
    while (exponent > 0) {
        if ((exponent & 1) != 0) {
            cas_poly_t product;
            if (!poly_multiply(result, base, &product)) return false;
            result = product;
        }
        exponent >>= 1;
        if (exponent > 0) {
            cas_poly_t square;
            if (!poly_multiply(base, base, &square)) return false;
            base = square;
        }
    }
    *out = result;
    return true;
}

static void parser_skip_space(cas_poly_parser_t *parser)
{
    while (isspace((unsigned char)*parser->cursor)) parser->cursor++;
}

static cas_poly_t parse_poly_expression(cas_poly_parser_t *parser);
static cas_poly_t parse_poly_power(cas_poly_parser_t *parser);

static cas_poly_t parse_poly_primary(cas_poly_parser_t *parser)
{
    parser_skip_space(parser);
    if (*parser->cursor == '(') {
        parser->cursor++;
        cas_poly_t value = parse_poly_expression(parser);
        parser_skip_space(parser);
        if (*parser->cursor != ')') parser->ok = false;
        else parser->cursor++;
        return value;
    }

    if (isdigit((unsigned char)*parser->cursor) || *parser->cursor == '.') {
        char *end = NULL;
        double value = strtod(parser->cursor, &end);
        if (end == parser->cursor || !isfinite(value)) parser->ok = false;
        else parser->cursor = end;
        return poly_constant(value);
    }

    if (tolower((unsigned char)*parser->cursor) == parser->variable) {
        parser->cursor++;
        return poly_variable();
    }

    parser->ok = false;
    return poly_constant(0.0);
}

static cas_poly_t parse_poly_unary(cas_poly_parser_t *parser)
{
    parser_skip_space(parser);
    if (*parser->cursor == '+') {
        parser->cursor++;
        return parse_poly_unary(parser);
    }
    if (*parser->cursor == '-') {
        parser->cursor++;
        cas_poly_t value = parse_poly_unary(parser);
        for (int i = 0; i <= value.degree; i++) value.coeff[i] = -value.coeff[i];
        return value;
    }
    return parse_poly_power(parser);
}

static cas_poly_t parse_poly_power(cas_poly_parser_t *parser)
{
    cas_poly_t base = parse_poly_primary(parser);
    parser_skip_space(parser);
    if (*parser->cursor != '^') return base;

    parser->cursor++;
    parser_skip_space(parser);
    char *end = NULL;
    long exponent = strtol(parser->cursor, &end, 10);
    if (end == parser->cursor || exponent < 0 || exponent > CAS_POLY_MAX_DEGREE) {
        parser->ok = false;
        return poly_constant(0.0);
    }
    parser->cursor = end;
    cas_poly_t result;
    if (!poly_power(base, (int)exponent, &result)) parser->ok = false;
    return parser->ok ? result : poly_constant(0.0);
}

static cas_poly_t parse_poly_term(cas_poly_parser_t *parser)
{
    cas_poly_t left = parse_poly_unary(parser);
    while (parser->ok) {
        parser_skip_space(parser);
        char operation = *parser->cursor;
        if (operation != '*' && operation != '/') break;
        parser->cursor++;
        cas_poly_t right = parse_poly_unary(parser);
        if (!parser->ok) break;
        if (operation == '*') {
            cas_poly_t product;
            if (!poly_multiply(left, right, &product)) parser->ok = false;
            else left = product;
        } else if (right.degree != 0 || nearly_zero(right.coeff[0])) {
            parser->ok = false;
        } else {
            for (int i = 0; i <= left.degree; i++) left.coeff[i] /= right.coeff[0];
        }
    }
    return left;
}

static cas_poly_t parse_poly_expression(cas_poly_parser_t *parser)
{
    cas_poly_t left = parse_poly_term(parser);
    while (parser->ok) {
        parser_skip_space(parser);
        char operation = *parser->cursor;
        if (operation != '+' && operation != '-') break;
        parser->cursor++;
        cas_poly_t right = parse_poly_term(parser);
        cas_poly_t result;
        poly_add(left, right, operation == '+' ? 1.0 : -1.0, &result);
        left = result;
    }
    return left;
}

static bool parse_polynomial(const char *expression, char variable, cas_poly_t *out)
{
    cas_poly_parser_t parser = {
        .cursor = expression,
        .variable = (char)tolower((unsigned char)variable),
        .ok = true,
    };
    cas_poly_t result = parse_poly_expression(&parser);
    parser_skip_space(&parser);
    if (!parser.ok || *parser.cursor != '\0') return false;
    poly_normalize(&result);
    *out = result;
    return true;
}

static void writer_append(cas_writer_t *writer, const char *text)
{
    if (!writer->ok) return;
    size_t length = strlen(text);
    if (writer->used + length + 1 > writer->size) {
        writer->ok = false;
        return;
    }
    memcpy(writer->data + writer->used, text, length + 1);
    writer->used += length;
}

static void writer_number(cas_writer_t *writer, double value)
{
    char text[32];
    if (nearly_zero(value)) value = 0.0;
    if (nearly_equal(value, round(value))) snprintf(text, sizeof(text), "%.0f", round(value));
    else snprintf(text, sizeof(text), "%.10g", value);
    writer_append(writer, text);
}

static bool format_polynomial(const cas_poly_t *poly, char variable, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) return false;
    out[0] = '\0';
    cas_writer_t writer = {.data = out, .size = out_size, .ok = true};
    bool wrote = false;
    for (int power = poly->degree; power >= 0; power--) {
        double coefficient = poly->coeff[power];
        if (nearly_zero(coefficient)) continue;
        if (wrote) writer_append(&writer, coefficient < 0.0 ? "-" : "+");
        else if (coefficient < 0.0) writer_append(&writer, "-");

        double magnitude = fabs(coefficient);
        if (power == 0 || !nearly_equal(magnitude, 1.0)) {
            writer_number(&writer, magnitude);
            if (power > 0) writer_append(&writer, "*");
        }
        if (power > 0) {
            char symbol[2] = {variable, '\0'};
            writer_append(&writer, symbol);
            if (power > 1) {
                writer_append(&writer, "^");
                writer_number(&writer, power);
            }
        }
        wrote = true;
    }
    if (!wrote) writer_append(&writer, "0");
    return writer.ok;
}

static bool take_wrapped(const char *expression, const char *name, char *inner, size_t inner_size)
{
    while (isspace((unsigned char)*expression)) expression++;
    size_t name_length = strlen(name);
    if (strlen(expression) < name_length) return false;
    for (size_t i = 0; i < name_length; i++) {
        if (tolower((unsigned char)expression[i]) != tolower((unsigned char)name[i])) return false;
    }
    const char *open = expression + name_length;
    while (isspace((unsigned char)*open)) open++;
    if (*open != '(') return false;
    const char *end = expression + strlen(expression);
    while (end > open && isspace((unsigned char)end[-1])) end--;
    if (end <= open + 1 || end[-1] != ')') return false;
    size_t length = (size_t)((end - 1) - (open + 1));
    if (length + 1 > inner_size) return false;
    memcpy(inner, open + 1, length);
    inner[length] = '\0';
    return true;
}

static int find_top_level(const char *text, char target)
{
    int depth = 0;
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == '(') depth++;
        else if (text[i] == ')') depth--;
        else if (text[i] == target && depth == 0) return i;
    }
    return -1;
}

static bool extract_expression_and_variable(char *inner, char *variable)
{
    *variable = 'x';
    int comma = find_top_level(inner, ',');
    if (comma < 0) return true;
    char *argument = inner + comma + 1;
    while (isspace((unsigned char)*argument)) argument++;
    if (!isalpha((unsigned char)argument[0]) || argument[1] != '\0') return false;
    *variable = (char)tolower((unsigned char)argument[0]);
    inner[comma] = '\0';
    return true;
}

static bool cas_expand(char *inner, char *out, size_t out_size)
{
    char variable = 'x';
    if (!extract_expression_and_variable(inner, &variable)) return false;
    cas_poly_t poly;
    return parse_polynomial(inner, variable, &poly) && format_polynomial(&poly, variable, out, out_size);
}

static double poly_evaluate(const cas_poly_t *poly, double value)
{
    double result = poly->coeff[poly->degree];
    for (int i = poly->degree - 1; i >= 0; i--) result = result * value + poly->coeff[i];
    return result;
}

static void poly_divide_linear(cas_poly_t *poly, double root)
{
    double next[CAS_POLY_MAX_DEGREE + 1] = {0};
    next[poly->degree - 1] = poly->coeff[poly->degree];
    for (int i = poly->degree - 2; i >= 0; i--) {
        next[i] = poly->coeff[i + 1] + root * next[i + 1];
    }
    memset(poly->coeff, 0, sizeof(poly->coeff));
    memcpy(poly->coeff, next, sizeof(next));
    poly->degree--;
    poly_normalize(poly);
}

static void writer_linear_factor(cas_writer_t *writer, char variable, double root)
{
    char symbol[2] = {variable, '\0'};
    writer_append(writer, "(");
    writer_append(writer, symbol);
    writer_append(writer, root < 0.0 ? "+" : "-");
    writer_number(writer, fabs(root));
    writer_append(writer, ")");
}

static bool cas_factor(char *inner, char *out, size_t out_size)
{
    char variable = 'x';
    if (!extract_expression_and_variable(inner, &variable)) return false;
    cas_poly_t poly;
    if (!parse_polynomial(inner, variable, &poly) || poly.degree < 2 || out_size == 0) return false;

    out[0] = '\0';
    cas_writer_t writer = {.data = out, .size = out_size, .ok = true};
    double leading = poly.coeff[poly.degree];
    if (!nearly_equal(leading, 1.0)) {
        writer_number(&writer, leading);
        writer_append(&writer, "*");
        for (int i = 0; i <= poly.degree; i++) poly.coeff[i] /= leading;
    }

    int factors = 0;
    while (poly.degree > 2) {
        bool found = false;
        for (int candidate = -32; candidate <= 32; candidate++) {
            if (nearly_zero(poly_evaluate(&poly, candidate))) {
                writer_linear_factor(&writer, variable, candidate);
                poly_divide_linear(&poly, candidate);
                factors++;
                found = true;
                break;
            }
        }
        if (!found) break;
    }

    if (poly.degree == 2) {
        double discriminant = poly.coeff[1] * poly.coeff[1] - 4.0 * poly.coeff[2] * poly.coeff[0];
        if (discriminant >= -CAS_EPSILON) {
            if (discriminant < 0.0) discriminant = 0.0;
            double first = (-poly.coeff[1] - sqrt(discriminant)) / (2.0 * poly.coeff[2]);
            double second = (-poly.coeff[1] + sqrt(discriminant)) / (2.0 * poly.coeff[2]);
            writer_linear_factor(&writer, variable, first);
            writer_linear_factor(&writer, variable, second);
            factors += 2;
            poly.degree = 0;
        }
    }

    if (poly.degree > 0) {
        char remainder[96];
        if (!format_polynomial(&poly, variable, remainder, sizeof(remainder))) return false;
        writer_append(&writer, factors > 0 ? "*(" : "(");
        writer_append(&writer, remainder);
        writer_append(&writer, ")");
    }
    return writer.ok && factors > 0;
}

static bool cas_solve(char *inner, char *out, size_t out_size)
{
    char variable = 'x';
    int comma = find_top_level(inner, ',');
    if (comma >= 0) {
        char *argument = inner + comma + 1;
        while (isspace((unsigned char)*argument)) argument++;
        if (!isalpha((unsigned char)argument[0]) || argument[1] != '\0') return false;
        variable = (char)tolower((unsigned char)argument[0]);
        inner[comma] = '\0';
    }
    int equal = find_top_level(inner, '=');
    if (equal < 0) return false;
    inner[equal] = '\0';

    cas_poly_t left;
    cas_poly_t right;
    if (!parse_polynomial(inner, variable, &left) || !parse_polynomial(inner + equal + 1, variable, &right)) return false;
    cas_poly_t poly;
    poly_add(left, right, -1.0, &poly);
    char symbol[2] = {variable, '\0'};
    if (poly.degree == 0) {
        snprintf(out, out_size, nearly_zero(poly.coeff[0]) ? "all real %s" : "no solution", symbol);
        return true;
    }
    if (poly.degree == 1) {
        char root[32] = {0};
        cas_writer_t writer = {.data = root, .size = sizeof(root), .ok = true};
        writer_number(&writer, -poly.coeff[0] / poly.coeff[1]);
        snprintf(out, out_size, "%s=%s", symbol, root);
        return true;
    }
    if (poly.degree != 2) return false;

    double discriminant = poly.coeff[1] * poly.coeff[1] - 4.0 * poly.coeff[2] * poly.coeff[0];
    double denominator = 2.0 * poly.coeff[2];
    if (discriminant >= -CAS_EPSILON) {
        if (discriminant < 0.0) discriminant = 0.0;
        char first[32] = {0};
        char second[32] = {0};
        cas_writer_t one = {.data = first, .size = sizeof(first), .ok = true};
        cas_writer_t two = {.data = second, .size = sizeof(second), .ok = true};
        writer_number(&one, (-poly.coeff[1] + sqrt(discriminant)) / denominator);
        writer_number(&two, (-poly.coeff[1] - sqrt(discriminant)) / denominator);
        snprintf(out, out_size, "%s=%s or %s", symbol, first, second);
    } else {
        char real[24] = {0};
        char imaginary[24] = {0};
        cas_writer_t re = {.data = real, .size = sizeof(real), .ok = true};
        cas_writer_t im = {.data = imaginary, .size = sizeof(imaginary), .ok = true};
        writer_number(&re, -poly.coeff[1] / denominator);
        writer_number(&im, sqrt(-discriminant) / fabs(denominator));
        snprintf(out, out_size, "%s=%s+%si or %s-%si", symbol, real, imaginary, real, imaginary);
    }
    return true;
}

static bool cas_derivative(char *inner, char *out, size_t out_size)
{
    char variable = 'x';
    if (!extract_expression_and_variable(inner, &variable)) return false;
    cas_poly_t poly;
    if (parse_polynomial(inner, variable, &poly)) {
        cas_poly_t derivative = {0};
        derivative.degree = poly.degree > 0 ? poly.degree - 1 : 0;
        for (int i = 1; i <= poly.degree; i++) derivative.coeff[i - 1] = poly.coeff[i] * i;
        return format_polynomial(&derivative, variable, out, out_size);
    }

    char expression_with_variable[260];
    snprintf(expression_with_variable, sizeof(expression_with_variable), "%s,%c", inner, variable);
    return opencalc_math_derivative_expression(expression_with_variable, out, out_size);
}

static bool cas_integral(char *inner, char *out, size_t out_size)
{
    char variable = 'x';
    if (!extract_expression_and_variable(inner, &variable)) return false;
    cas_poly_t poly;
    if (parse_polynomial(inner, variable, &poly) && poly.degree < CAS_POLY_MAX_DEGREE) {
        cas_poly_t integral = {0};
        integral.degree = poly.degree + 1;
        for (int i = 0; i <= poly.degree; i++) integral.coeff[i + 1] = poly.coeff[i] / (i + 1.0);
        if (!format_polynomial(&integral, variable, out, out_size) || strlen(out) + 3 > out_size) return false;
        strcat(out, "+C");
        return true;
    }

    char expression_with_variable[260];
    snprintf(expression_with_variable, sizeof(expression_with_variable), "%s,%c", inner, variable);
    return opencalc_math_integral_expression(expression_with_variable, out, out_size);
}

bool opencalc_cas_eval(const char *expression, char *out, size_t out_size)
{
    char inner[256];
    if (expression == NULL || out == NULL || out_size == 0) return false;
#if OPENCALC_ENABLE_GIAC_CAS && defined(ESP_PLATFORM)
    if (!prefer_existing_backend_semantics(expression) &&
        opencalc_giac_eval(expression,
                           opencalc_math_degrees_enabled(),
                           out,
                           out_size)) {
        return true;
    }
#endif
    if (take_wrapped(expression, "expand", inner, sizeof(inner)) && cas_expand(inner, out, out_size)) return true;
    if (take_wrapped(expression, "factor", inner, sizeof(inner)) && cas_factor(inner, out, out_size)) return true;
    if (take_wrapped(expression, "solve", inner, sizeof(inner)) && cas_solve(inner, out, out_size)) return true;
    if (take_wrapped(expression, "deriv", inner, sizeof(inner)) && cas_derivative(inner, out, out_size)) return true;
    if (take_wrapped(expression, "int", inner, sizeof(inner)) && cas_integral(inner, out, out_size)) return true;
    return opencalc_eigenmath_eval(expression, out, out_size);
}
