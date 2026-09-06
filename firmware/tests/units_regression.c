#include "opencalc_units.h"

#include <stdio.h>
#include <string.h>

static int expect_result(const char *expression, const char *expected)
{
    char output[128];
    opencalc_units_status_t status = opencalc_units_eval(expression, output, sizeof(output));
    if (status != OPENCALC_UNITS_OK || strcmp(output, expected) != 0) {
        fprintf(stderr, "FAIL %s: expected <%s>, got status %d <%s>\n",
                expression, expected, status, output);
        return 1;
    }
    printf("PASS %s -> %s\n", expression, output);
    return 0;
}

int main(void)
{
    int failed = 0;
    char output[128];

    failed |= expect_result("5 m / 2 s", "2.5 m/s");
    failed |= expect_result("5m/2s", "2.5 m/s");
    failed |= expect_result("100 cm + 1 m", "2 m");
    failed |= expect_result("3 ft + 12 in", "1.2192 m");
    failed |= expect_result("2 kg * 9.81 m/s^2", "19.62 kg*m/s^2");
    failed |= expect_result("1 L + 500 mL", "0.0015 m^3");
    failed |= expect_result("60 km / 2 h", "8.333333333 m/s");

    if (opencalc_units_eval("1+1", output, sizeof(output)) != OPENCALC_UNITS_NOT_APPLICABLE) {
        fprintf(stderr, "FAIL ordinary expression was consumed by unit evaluator\n");
        failed = 1;
    }
    if (opencalc_units_eval("1 m + 1 s", output, sizeof(output)) != OPENCALC_UNITS_ERROR ||
        strstr(output, "incompatible units") == NULL) {
        fprintf(stderr, "FAIL incompatible dimensions were accepted: %s\n", output);
        failed = 1;
    }

    if (!failed) puts("unit regression tests passed");
    return failed ? 1 : 0;
}
