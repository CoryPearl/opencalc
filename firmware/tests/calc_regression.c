#include "opencalc_calc.h"

#include <stdio.h>
#include <string.h>

static int failures;

_Static_assert(OPENCALC_CALC_RESULT_MAX >= 1024, "calculator results must hold Giac output");

static bool copy_catalog(const char *input, char *out, size_t out_size, void *context)
{
    (void)context;
    return snprintf(out, out_size, "%s", input) >= 0;
}

static void expect_eval(const char *expression, const char *ans, const char *expected)
{
    opencalc_calc_eval_request_t request = {
        .expression = expression,
        .ans = ans,
        .degrees = false,
        .timeout_ms = 1000,
        .expand_catalog = copy_catalog,
    };
    opencalc_calc_eval_result_t result;
    opencalc_calc_evaluate(&request, &result);
    if (!result.update_ans || strcmp(result.output, expected) != 0) {
        fprintf(stderr, "FAIL %s -> %s (expected %s)\n", expression, result.output, expected);
        failures++;
    } else {
        printf("PASS %s -> %s\n", expression, result.output);
    }
}

static void expect_display(const char *expression, int display_format, const char *expected)
{
    opencalc_calc_eval_request_t request = {
        .expression = expression,
        .ans = "0",
        .display_format = display_format,
        .timeout_ms = 1000,
        .expand_catalog = copy_catalog,
    };
    opencalc_calc_eval_result_t result;
    opencalc_calc_evaluate(&request, &result);
    if (!result.ok || strcmp(result.output, expected) != 0) {
        fprintf(stderr, "FAIL display %d: %s -> %s (expected %s)\n",
                display_format, expression, result.output, expected);
        failures++;
    }
}

int main(void)
{
    expect_eval("2+2", "0", "4");
    expect_eval("Ans+3", "4", "7");
    expect_eval("frac(0.125)", "0", "1/8");
    expect_display("12345", 1, "1.234500000e+04");
    expect_display("12345", 2, "12.345 e+3");

    opencalc_calc_history_reset();
    for (int i = 0; i < 18; i++) {
        char expression[16];
        char result[16];
        snprintf(expression, sizeof(expression), "%d+1", i);
        snprintf(result, sizeof(result), "%d", i + 1);
        opencalc_calc_history_push(expression, result);
    }
    if (opencalc_calc_history_count() != OPENCALC_CALC_HISTORY_MAX ||
        strcmp(opencalc_calc_history_expression(0), "2+1") != 0) {
        fprintf(stderr, "FAIL bounded calculator history\n");
        failures++;
    } else {
        printf("PASS bounded calculator history\n");
    }
    opencalc_calc_history_move_up();
    if (strcmp(opencalc_calc_history_selected_text(), "18") != 0) {
        fprintf(stderr, "FAIL calculator history selection\n");
        failures++;
    } else {
        printf("PASS calculator history selection\n");
    }

    char long_result[400];
    memset(long_result, 'x', sizeof(long_result) - 1);
    long_result[sizeof(long_result) - 1] = '\0';
    opencalc_calc_history_push("expand((x+1)^50)", long_result);
    if (strlen(opencalc_calc_history_result(OPENCALC_CALC_HISTORY_MAX - 1)) !=
        sizeof(long_result) - 1) {
        fprintf(stderr, "FAIL lossless large result history\n");
        failures++;
    } else {
        printf("PASS lossless large result history\n");
    }
    return failures == 0 ? 0 : 1;
}
