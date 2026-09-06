#include "opencalc_calc.h"

#include "opencalc_cas.h"
#include "opencalc_math.h"
#include "opencalc_units.h"

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#else
#define EXT_RAM_BSS_ATTR
#endif

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EXT_RAM_BSS_ATTR char s_history_expression[OPENCALC_CALC_HISTORY_MAX][OPENCALC_CALC_EXPR_MAX];
static EXT_RAM_BSS_ATTR char s_history_result[OPENCALC_CALC_HISTORY_MAX][OPENCALC_CALC_RESULT_MAX];
static int s_history_count;
static int s_history_selected = -1;
static bool s_history_answer_selected;

static bool take_wrapped(const char *input, const char *name, char *out, size_t out_size)
{
    size_t name_len = strlen(name);
    if (input == NULL || name == NULL || out == NULL || out_size == 0 ||
        strncmp(input, name, name_len) != 0 || input[name_len] != '(') return false;
    const char *start = input + name_len + 1;
    const char *end = strrchr(start, ')');
    if (end == NULL || end <= start) return false;
    size_t length = (size_t)(end - start);
    if (length >= out_size) length = out_size - 1;
    memcpy(out, start, length);
    out[length] = '\0';
    return true;
}

static void expand_ans(const char *input, const char *ans, char *out, size_t out_size)
{
    size_t used = 0;
    if (out_size == 0) return;
    for (size_t i = 0; input[i] != '\0' && used + 1 < out_size;) {
        bool left = i == 0 || (!isalnum((unsigned char)input[i - 1]) && input[i - 1] != '_');
        bool match = input[i + 1] != '\0' && input[i + 2] != '\0' &&
            tolower((unsigned char)input[i]) == 'a' &&
            tolower((unsigned char)input[i + 1]) == 'n' &&
            tolower((unsigned char)input[i + 2]) == 's';
        bool right = match && !isalnum((unsigned char)input[i + 3]) && input[i + 3] != '_';
        if (left && right) {
            size_t length = strlen(ans);
            if (used + length + 2 >= out_size) break;
            out[used++] = '(';
            memcpy(out + used, ans, length);
            used += length;
            out[used++] = ')';
            i += 3;
        } else {
            out[used++] = input[i++];
        }
    }
    out[used] = '\0';
}

static bool contains_complex_unit(const char *expression)
{
    for (const char *p = expression; p != NULL && *p != '\0'; p++) {
        if ((*p == 'i' || *p == 'I') &&
            (p == expression || !isalpha((unsigned char)p[-1])) &&
            !isalpha((unsigned char)p[1])) return true;
    }
    return false;
}

static void format_real(double value, int display_format, char *out, size_t out_size)
{
    if (display_format == 1) {
        snprintf(out, out_size, "%.9e", value);
        return;
    }
    if (display_format == 2 && value != 0.0 && isfinite(value)) {
        int exponent = (int)floor(log10(fabs(value)) / 3.0) * 3;
        double mantissa = value / pow(10.0, exponent);
        snprintf(out, out_size, "%.9g e%+d", mantissa, exponent);
        return;
    }
    snprintf(out, out_size, "%.10g", value);
}

static void format_complex(double real, double imag, int mode, int display_format, bool degrees,
                           char *out, size_t out_size)
{
    if (fabs(imag) < 1e-10) {
        format_real(real, display_format, out, out_size);
    } else if (mode == 2) {
        double theta = atan2(imag, real);
        if (degrees) theta *= 180.0 / 3.14159265358979323846;
        char radius[48];
        char angle[48];
        format_real(hypot(real, imag), display_format, radius, sizeof(radius));
        format_real(theta, display_format, angle, sizeof(angle));
        snprintf(out, out_size, "%s e^(%s i)", radius, angle);
    } else {
        char real_text[48];
        char imag_text[48];
        format_real(real, display_format, real_text, sizeof(real_text));
        format_real(fabs(imag), display_format, imag_text, sizeof(imag_text));
        snprintf(out, out_size, "%s%c%si", real_text, imag < 0.0 ? '-' : '+', imag_text);
    }
}

static void apply_display_format(char *text, size_t text_size, int display_format)
{
    if (text == NULL || display_format == 0) return;
    char *end = NULL;
    double value = strtod(text, &end);
    while (end != NULL && isspace((unsigned char)*end)) end++;
    if (end != text && end != NULL && *end == '\0' && isfinite(value)) {
        format_real(value, display_format, text, text_size);
    }
}

static void format_fraction(double value, char *out, size_t out_size)
{
    double magnitude = fabs(value);
    long long numerator = (long long)llround(magnitude);
    long long denominator = 1;
    double error = fabs(magnitude - (double)numerator);
    for (long long candidate = 1; candidate <= 100000; candidate++) {
        long long candidate_numerator = (long long)llround(magnitude * candidate);
        double candidate_error = fabs(magnitude - (double)candidate_numerator / candidate);
        if (candidate_error < error) {
            numerator = candidate_numerator;
            denominator = candidate;
            error = candidate_error;
            if (error < 1e-10) break;
        }
    }
    if (error > 1e-7) {
        snprintf(out, out_size, "%.10g", value);
        return;
    }
    long long a = numerator;
    long long b = denominator;
    while (b != 0) {
        long long remainder = a % b;
        a = b;
        b = remainder;
    }
    if (a > 1) {
        numerator /= a;
        denominator /= a;
    }
    if (value < 0.0) numerator = -numerator;
    if (denominator == 1) snprintf(out, out_size, "%lld", numerator);
    else snprintf(out, out_size, "%lld/%lld", numerator, denominator);
}

void opencalc_calc_evaluate(const opencalc_calc_eval_request_t *request,
                            opencalc_calc_eval_result_t *result)
{
    memset(result, 0, sizeof(*result));
    result->giac_status = OPENCALC_GIAC_ERROR;
    if (request == NULL || request->expression == NULL || request->ans == NULL) {
        snprintf(result->output, sizeof(result->output), "error");
        return;
    }

    char inner[96];
    char expanded[OPENCALC_CALC_EXPR_MAX + OPENCALC_CALC_RESULT_MAX];
    char substituted[sizeof(expanded)];
    expand_ans(request->expression, request->ans, expanded, sizeof(expanded));

    char assignment[OPENCALC_VARIABLE_NAME_MAX];
    if (opencalc_math_assignment_name(expanded, assignment, sizeof(assignment))) {
        double real = 0.0, imag = 0.0;
        if (opencalc_math_eval_complex_expression(expanded, &real, &imag)) {
            format_complex(real, imag, request->complex_mode, request->display_format, request->degrees,
                           result->output, sizeof(result->output));
            result->ok = result->update_ans = true;
        } else {
            snprintf(result->output, sizeof(result->output), "invalid assignment");
        }
        return;
    }

    opencalc_units_status_t units = opencalc_units_eval(
        expanded, result->output, sizeof(result->output));
    if (units != OPENCALC_UNITS_NOT_APPLICABLE) {
        result->ok = result->update_ans = units == OPENCALC_UNITS_OK;
        return;
    }
    if (!opencalc_math_substitute_variables(expanded, substituted, sizeof(substituted))) {
        snprintf(result->output, sizeof(result->output), "expression too long");
        return;
    }
    if (request->expand_catalog != NULL &&
        !request->expand_catalog(substituted, expanded, sizeof(expanded), request->catalog_context)) {
        snprintf(result->output, sizeof(result->output), "stored value is too large");
        return;
    }
    if (request->expand_catalog == NULL) snprintf(expanded, sizeof(expanded), "%s", substituted);

    if (opencalc_cas_eval_timed(expanded, result->output, sizeof(result->output),
                                request->timeout_ms, request->should_cancel,
                                request->cancel_context, &result->giac_status)) {
        apply_display_format(result->output, sizeof(result->output), request->display_format);
        size_t length = strlen(result->output);
        result->ok = true;
        result->update_ans = length < 3 || strcmp(result->output + length - 3, "...") != 0;
        return;
    }
    if (result->giac_status == OPENCALC_GIAC_CANCELLED ||
        result->giac_status == OPENCALC_GIAC_TIMEOUT) {
        snprintf(result->output, sizeof(result->output), "%s",
                 result->giac_status == OPENCALC_GIAC_CANCELLED ? "cancelled" : "CAS timed out");
        return;
    }

    if (take_wrapped(request->expression, "deriv", inner, sizeof(inner))) {
        if (!opencalc_math_derivative_expression(inner, result->output, sizeof(result->output)))
            snprintf(result->output, sizeof(result->output), "unsupported deriv");
        return;
    }
    if (take_wrapped(request->expression, "int", inner, sizeof(inner))) {
        if (!opencalc_math_integral_expression(inner, result->output, sizeof(result->output)))
            snprintf(result->output, sizeof(result->output), "unsupported int");
        return;
    }

    bool complex = request->complex_mode != 0 || contains_complex_unit(expanded);
    if (complex) {
        double real = 0.0, imag = 0.0;
        if (opencalc_math_eval_complex_expression(expanded, &real, &imag)) {
            format_complex(real, imag, request->complex_mode, request->display_format, request->degrees,
                           result->output, sizeof(result->output));
            result->ok = result->update_ans = true;
            return;
        }
    } else {
        double value = 0.0;
        if (opencalc_math_eval_expression(expanded, &value)) {
            if (take_wrapped(request->expression, "frac", inner, sizeof(inner)) ||
                take_wrapped(request->expression, "Frac", inner, sizeof(inner)) ||
                take_wrapped(request->expression, "FRAC", inner, sizeof(inner))) {
                format_fraction(value, result->output, sizeof(result->output));
            } else {
                format_real(value, request->display_format,
                            result->output, sizeof(result->output));
            }
            result->ok = result->update_ans = true;
            return;
        }
    }
    snprintf(result->output, sizeof(result->output), "error");
}

void opencalc_calc_history_reset(void)
{
    memset(s_history_expression, 0, sizeof(s_history_expression));
    memset(s_history_result, 0, sizeof(s_history_result));
    s_history_count = 0;
    s_history_selected = -1;
    s_history_answer_selected = false;
}

void opencalc_calc_history_push(const char *expression, const char *result)
{
    if (expression == NULL || result == NULL) return;
    if (s_history_count == OPENCALC_CALC_HISTORY_MAX) {
        memmove(s_history_expression, s_history_expression + 1,
                sizeof(s_history_expression[0]) * (OPENCALC_CALC_HISTORY_MAX - 1));
        memmove(s_history_result, s_history_result + 1,
                sizeof(s_history_result[0]) * (OPENCALC_CALC_HISTORY_MAX - 1));
        s_history_count--;
    }
    snprintf(s_history_expression[s_history_count], sizeof(s_history_expression[0]), "%s", expression);
    snprintf(s_history_result[s_history_count], sizeof(s_history_result[0]), "%s", result);
    s_history_count++;
    opencalc_calc_history_clear_selection();
}

int opencalc_calc_history_count(void) { return s_history_count; }

const char *opencalc_calc_history_expression(int index)
{
    return index >= 0 && index < s_history_count ? s_history_expression[index] : "";
}

const char *opencalc_calc_history_result(int index)
{
    return index >= 0 && index < s_history_count ? s_history_result[index] : "";
}

int opencalc_calc_history_selected(void) { return s_history_selected; }
bool opencalc_calc_history_answer_selected(void) { return s_history_answer_selected; }

void opencalc_calc_history_clear_selection(void)
{
    s_history_selected = -1;
    s_history_answer_selected = false;
}

void opencalc_calc_history_select_expression(void)
{
    if (s_history_selected >= 0) s_history_answer_selected = false;
}

void opencalc_calc_history_select_answer(void)
{
    if (s_history_selected >= 0) s_history_answer_selected = true;
}

void opencalc_calc_history_move_up(void)
{
    if (s_history_count == 0) return;
    if (s_history_selected < 0) {
        s_history_selected = s_history_count - 1;
        s_history_answer_selected = true;
    } else if (s_history_answer_selected) {
        s_history_answer_selected = false;
    } else if (s_history_selected > 0) {
        s_history_selected--;
        s_history_answer_selected = true;
    }
}

void opencalc_calc_history_move_down(void)
{
    if (s_history_selected < 0) return;
    if (!s_history_answer_selected) {
        s_history_answer_selected = true;
    } else if (s_history_selected + 1 < s_history_count) {
        s_history_selected++;
        s_history_answer_selected = false;
    } else {
        opencalc_calc_history_clear_selection();
    }
}

const char *opencalc_calc_history_selected_text(void)
{
    if (s_history_selected < 0 || s_history_selected >= s_history_count) return NULL;
    return s_history_answer_selected ? s_history_result[s_history_selected]
                                     : s_history_expression[s_history_selected];
}

void opencalc_calc_history_export(
    int *count,
    char expressions[OPENCALC_CALC_HISTORY_MAX][OPENCALC_CALC_EXPR_MAX],
    char results[OPENCALC_CALC_HISTORY_MAX][OPENCALC_CALC_RESULT_MAX])
{
    if (count != NULL) *count = s_history_count;
    if (expressions != NULL) memcpy(expressions, s_history_expression, sizeof(s_history_expression));
    if (results != NULL) memcpy(results, s_history_result, sizeof(s_history_result));
}

bool opencalc_calc_history_import(
    int count,
    const char expressions[OPENCALC_CALC_HISTORY_MAX][OPENCALC_CALC_EXPR_MAX],
    const char results[OPENCALC_CALC_HISTORY_MAX][OPENCALC_CALC_RESULT_MAX])
{
    if (count < 0 || count > OPENCALC_CALC_HISTORY_MAX || expressions == NULL || results == NULL) {
        return false;
    }
    memcpy(s_history_expression, expressions, sizeof(s_history_expression));
    memcpy(s_history_result, results, sizeof(s_history_result));
    s_history_count = count;
    for (int i = 0; i < OPENCALC_CALC_HISTORY_MAX; i++) {
        s_history_expression[i][OPENCALC_CALC_EXPR_MAX - 1] = '\0';
        s_history_result[i][OPENCALC_CALC_RESULT_MAX - 1] = '\0';
    }
    opencalc_calc_history_clear_selection();
    return true;
}
