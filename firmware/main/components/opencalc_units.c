#include "opencalc_units.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    DIM_MASS,
    DIM_LENGTH,
    DIM_TIME,
    DIM_CURRENT,
    DIM_TEMPERATURE,
    DIM_AMOUNT,
    DIM_LUMINOUS,
    DIM_COUNT,
};

typedef struct {
    double value;
    int8_t dimensions[DIM_COUNT];
} unit_value_t;

typedef struct {
    const char *name;
    double scale;
    int8_t dimensions[DIM_COUNT];
} unit_definition_t;

#define DIMS(mass, length, time, current, temperature, amount, luminous) \
    {mass, length, time, current, temperature, amount, luminous}

static const unit_definition_t UNIT_DEFINITIONS[] = {
    {"kg", 1.0, DIMS(1, 0, 0, 0, 0, 0, 0)},
    {"g", 1e-3, DIMS(1, 0, 0, 0, 0, 0, 0)},
    {"mg", 1e-6, DIMS(1, 0, 0, 0, 0, 0, 0)},
    {"lb", 0.45359237, DIMS(1, 0, 0, 0, 0, 0, 0)},
    {"oz", 0.028349523125, DIMS(1, 0, 0, 0, 0, 0, 0)},
    {"m", 1.0, DIMS(0, 1, 0, 0, 0, 0, 0)},
    {"cm", 1e-2, DIMS(0, 1, 0, 0, 0, 0, 0)},
    {"mm", 1e-3, DIMS(0, 1, 0, 0, 0, 0, 0)},
    {"km", 1e3, DIMS(0, 1, 0, 0, 0, 0, 0)},
    {"in", 0.0254, DIMS(0, 1, 0, 0, 0, 0, 0)},
    {"ft", 0.3048, DIMS(0, 1, 0, 0, 0, 0, 0)},
    {"yd", 0.9144, DIMS(0, 1, 0, 0, 0, 0, 0)},
    {"mi", 1609.344, DIMS(0, 1, 0, 0, 0, 0, 0)},
    {"s", 1.0, DIMS(0, 0, 1, 0, 0, 0, 0)},
    {"ms", 1e-3, DIMS(0, 0, 1, 0, 0, 0, 0)},
    {"min", 60.0, DIMS(0, 0, 1, 0, 0, 0, 0)},
    {"h", 3600.0, DIMS(0, 0, 1, 0, 0, 0, 0)},
    {"A", 1.0, DIMS(0, 0, 0, 1, 0, 0, 0)},
    {"K", 1.0, DIMS(0, 0, 0, 0, 1, 0, 0)},
    {"mol", 1.0, DIMS(0, 0, 0, 0, 0, 1, 0)},
    {"cd", 1.0, DIMS(0, 0, 0, 0, 0, 0, 1)},
    {"Hz", 1.0, DIMS(0, 0, -1, 0, 0, 0, 0)},
    {"N", 1.0, DIMS(1, 1, -2, 0, 0, 0, 0)},
    {"Pa", 1.0, DIMS(1, -1, -2, 0, 0, 0, 0)},
    {"J", 1.0, DIMS(1, 2, -2, 0, 0, 0, 0)},
    {"W", 1.0, DIMS(1, 2, -3, 0, 0, 0, 0)},
    {"C", 1.0, DIMS(0, 0, 1, 1, 0, 0, 0)},
    {"V", 1.0, DIMS(1, 2, -3, -1, 0, 0, 0)},
    {"ohm", 1.0, DIMS(1, 2, -3, -2, 0, 0, 0)},
    {"F", 1.0, DIMS(-1, -2, 4, 2, 0, 0, 0)},
    {"T", 1.0, DIMS(1, 0, -2, -1, 0, 0, 0)},
    {"L", 1e-3, DIMS(0, 3, 0, 0, 0, 0, 0)},
    {"mL", 1e-6, DIMS(0, 3, 0, 0, 0, 0, 0)},
};

#undef DIMS

typedef enum {
    TOKEN_END,
    TOKEN_NUMBER,
    TOKEN_IDENTIFIER,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_POWER,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_INVALID,
} token_kind_t;

typedef struct {
    token_kind_t kind;
    double number;
    char identifier[8];
    bool whitespace_before;
} token_t;

typedef struct {
    const char *cursor;
    token_t token;
    bool ok;
    bool saw_unit;
    const char *error;
} unit_parser_t;

static const unit_definition_t *find_unit(const char *name)
{
    for (size_t i = 0; i < sizeof(UNIT_DEFINITIONS) / sizeof(UNIT_DEFINITIONS[0]); i++) {
        if (strcmp(name, UNIT_DEFINITIONS[i].name) == 0) return &UNIT_DEFINITIONS[i];
    }
    return NULL;
}

static unit_value_t scalar_value(double value)
{
    unit_value_t result = {.value = value, .dimensions = {0}};
    return result;
}

static bool same_dimensions(const unit_value_t *left, const unit_value_t *right)
{
    return memcmp(left->dimensions, right->dimensions, sizeof(left->dimensions)) == 0;
}

static void parser_fail(unit_parser_t *parser, const char *message)
{
    if (parser->ok) parser->error = message;
    parser->ok = false;
}

static void next_token(unit_parser_t *parser)
{
    bool whitespace = false;
    while (isspace((unsigned char)*parser->cursor)) {
        whitespace = true;
        parser->cursor++;
    }
    parser->token = (token_t){.kind = TOKEN_END, .whitespace_before = whitespace};
    char ch = *parser->cursor;
    if (ch == '\0') return;

    if (isdigit((unsigned char)ch) || ch == '.') {
        char *end = NULL;
        parser->token.number = strtod(parser->cursor, &end);
        if (end == parser->cursor || !isfinite(parser->token.number)) {
            parser->token.kind = TOKEN_INVALID;
            parser_fail(parser, "invalid number");
            return;
        }
        parser->cursor = end;
        parser->token.kind = TOKEN_NUMBER;
        return;
    }
    if (isalpha((unsigned char)ch)) {
        size_t length = 0;
        while (isalpha((unsigned char)*parser->cursor)) {
            if (length + 1 < sizeof(parser->token.identifier)) {
                parser->token.identifier[length++] = *parser->cursor;
            }
            parser->cursor++;
        }
        parser->token.identifier[length] = '\0';
        parser->token.kind = TOKEN_IDENTIFIER;
        return;
    }

    parser->cursor++;
    switch (ch) {
    case '+': parser->token.kind = TOKEN_PLUS; break;
    case '-': parser->token.kind = TOKEN_MINUS; break;
    case '*': parser->token.kind = TOKEN_MULTIPLY; break;
    case '/': parser->token.kind = TOKEN_DIVIDE; break;
    case '^': parser->token.kind = TOKEN_POWER; break;
    case '(': parser->token.kind = TOKEN_LEFT_PAREN; break;
    case ')': parser->token.kind = TOKEN_RIGHT_PAREN; break;
    default:
        parser->token.kind = TOKEN_INVALID;
        parser_fail(parser, "unsupported unit syntax");
        break;
    }
}

static unit_value_t parse_expression(unit_parser_t *parser);

static unit_value_t parse_primary(unit_parser_t *parser)
{
    if (parser->token.kind == TOKEN_PLUS || parser->token.kind == TOKEN_MINUS) {
        bool negative = parser->token.kind == TOKEN_MINUS;
        next_token(parser);
        unit_value_t value = parse_primary(parser);
        if (negative) value.value = -value.value;
        return value;
    }
    if (parser->token.kind == TOKEN_NUMBER) {
        unit_value_t value = scalar_value(parser->token.number);
        next_token(parser);
        return value;
    }
    if (parser->token.kind == TOKEN_IDENTIFIER) {
        const unit_definition_t *unit = find_unit(parser->token.identifier);
        if (unit == NULL) {
            parser_fail(parser, "unknown unit");
            return scalar_value(0.0);
        }
        unit_value_t value = scalar_value(unit->scale);
        memcpy(value.dimensions, unit->dimensions, sizeof(value.dimensions));
        parser->saw_unit = true;
        next_token(parser);
        return value;
    }
    if (parser->token.kind == TOKEN_LEFT_PAREN) {
        next_token(parser);
        unit_value_t value = parse_expression(parser);
        if (parser->token.kind != TOKEN_RIGHT_PAREN) {
            parser_fail(parser, "missing )");
            return value;
        }
        next_token(parser);
        return value;
    }
    parser_fail(parser, "expected value");
    return scalar_value(0.0);
}

static unit_value_t parse_power(unit_parser_t *parser)
{
    unit_value_t value = parse_primary(parser);
    if (!parser->ok || parser->token.kind != TOKEN_POWER) return value;
    next_token(parser);
    unit_value_t exponent = parse_primary(parser);
    int rounded = (int)llround(exponent.value);
    if (!parser->ok || !same_dimensions(&exponent, &(unit_value_t){0}) ||
        fabs(exponent.value - rounded) > 1e-10 || rounded < -12 || rounded > 12) {
        parser_fail(parser, "unit power must be an integer");
        return value;
    }
    value.value = pow(value.value, rounded);
    for (int i = 0; i < DIM_COUNT; i++) value.dimensions[i] *= rounded;
    if (!isfinite(value.value)) parser_fail(parser, "invalid unit power");
    return value;
}

static bool begins_implicit_factor(const token_t *token)
{
    return token->kind == TOKEN_IDENTIFIER ||
           (token->whitespace_before &&
            (token->kind == TOKEN_NUMBER || token->kind == TOKEN_LEFT_PAREN));
}

static unit_value_t multiply_values(unit_value_t left, unit_value_t right, int direction)
{
    left.value = direction > 0 ? left.value * right.value : left.value / right.value;
    for (int i = 0; i < DIM_COUNT; i++) {
        left.dimensions[i] += direction * right.dimensions[i];
    }
    return left;
}

/* Implicit products bind within each side of a slash: 5 m / 2 s = (5m)/(2s). */
static unit_value_t parse_implicit_product(unit_parser_t *parser)
{
    unit_value_t value = parse_power(parser);
    while (parser->ok && begins_implicit_factor(&parser->token)) {
        value = multiply_values(value, parse_power(parser), 1);
    }
    return value;
}

static unit_value_t parse_term(unit_parser_t *parser)
{
    unit_value_t value = parse_implicit_product(parser);
    while (parser->ok &&
           (parser->token.kind == TOKEN_MULTIPLY || parser->token.kind == TOKEN_DIVIDE)) {
        int direction = parser->token.kind == TOKEN_MULTIPLY ? 1 : -1;
        next_token(parser);
        unit_value_t right = parse_implicit_product(parser);
        if (direction < 0 && right.value == 0.0) {
            parser_fail(parser, "division by zero");
            return value;
        }
        value = multiply_values(value, right, direction);
    }
    return value;
}

static unit_value_t parse_expression(unit_parser_t *parser)
{
    unit_value_t value = parse_term(parser);
    while (parser->ok && (parser->token.kind == TOKEN_PLUS || parser->token.kind == TOKEN_MINUS)) {
        bool subtract = parser->token.kind == TOKEN_MINUS;
        next_token(parser);
        unit_value_t right = parse_term(parser);
        if (!same_dimensions(&value, &right)) {
            parser_fail(parser, "incompatible units");
            return value;
        }
        value.value += subtract ? -right.value : right.value;
    }
    return value;
}

static bool expression_requests_units(const char *expression)
{
    const char *cursor = expression;
    while (*cursor) {
        if (isdigit((unsigned char)*cursor) || *cursor == '.') {
            char *end = NULL;
            (void)strtod(cursor, &end);
            if (end != cursor) {
                while (isspace((unsigned char)*end)) end++;
                if (isalpha((unsigned char)*end)) {
                    char name[8];
                    size_t length = 0;
                    while (isalpha((unsigned char)*end) && length + 1 < sizeof(name)) {
                        name[length++] = *end++;
                    }
                    name[length] = '\0';
                    if (find_unit(name) != NULL) return true;
                }
                cursor = end;
                continue;
            }
        }
        cursor++;
    }
    return false;
}

static bool append_text(char *output, size_t output_size, const char *text)
{
    size_t used = strlen(output);
    size_t length = strlen(text);
    if (used + length + 1 > output_size) return false;
    memcpy(output + used, text, length + 1);
    return true;
}

static bool append_dimension(char *output, size_t output_size,
                             const char *name, int exponent, bool *first)
{
    if (!*first && !append_text(output, output_size, "*")) return false;
    if (!append_text(output, output_size, name)) return false;
    if (exponent != 1) {
        char power[8];
        snprintf(power, sizeof(power), "^%d", exponent);
        if (!append_text(output, output_size, power)) return false;
    }
    *first = false;
    return true;
}

static bool format_value(const unit_value_t *value, char *output, size_t output_size)
{
    static const char *BASE_NAMES[DIM_COUNT] = {"kg", "m", "s", "A", "K", "mol", "cd"};
    int written = snprintf(output, output_size, "%.10g", fabs(value->value) < 5e-13 ? 0.0 : value->value);
    if (written < 0 || (size_t)written >= output_size) return false;

    bool has_dimensions = false;
    for (int i = 0; i < DIM_COUNT; i++) has_dimensions |= value->dimensions[i] != 0;
    if (!has_dimensions) return true;
    if (!append_text(output, output_size, " ")) return false;

    bool first = true;
    for (int i = 0; i < DIM_COUNT; i++) {
        if (value->dimensions[i] > 0 &&
            !append_dimension(output, output_size, BASE_NAMES[i], value->dimensions[i], &first)) return false;
    }
    bool has_denominator = false;
    for (int i = 0; i < DIM_COUNT; i++) has_denominator |= value->dimensions[i] < 0;
    if (has_denominator) {
        if (!append_text(output, output_size, "/")) return false;
        first = true;
        for (int i = 0; i < DIM_COUNT; i++) {
            if (value->dimensions[i] < 0 &&
                !append_dimension(output, output_size, BASE_NAMES[i], -value->dimensions[i], &first)) return false;
        }
    }
    return true;
}

opencalc_units_status_t opencalc_units_eval(const char *expression,
                                            char *output,
                                            size_t output_size)
{
    if (expression == NULL || output == NULL || output_size == 0 ||
        !expression_requests_units(expression)) return OPENCALC_UNITS_NOT_APPLICABLE;

    unit_parser_t parser = {.cursor = expression, .ok = true};
    next_token(&parser);
    unit_value_t value = parse_expression(&parser);
    if (parser.ok && parser.token.kind != TOKEN_END) parser_fail(&parser, "unexpected token");
    if (!parser.ok || !parser.saw_unit || !isfinite(value.value)) {
        snprintf(output, output_size, "unit error: %s", parser.error ? parser.error : "invalid expression");
        return OPENCALC_UNITS_ERROR;
    }
    if (!format_value(&value, output, output_size)) {
        snprintf(output, output_size, "unit result too long");
        return OPENCALC_UNITS_ERROR;
    }
    return OPENCALC_UNITS_OK;
}
