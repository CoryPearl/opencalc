#include "opencalc_math.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void near(double actual, double expected)
{
    assert(fabs(actual - expected) < 1e-9);
}

int main(void)
{
    double real = 0.0;
    double imag = 0.0;
    char name[OPENCALC_VARIABLE_NAME_MAX];
    char expanded[128];

    opencalc_math_variables_reset();
    assert(opencalc_math_variable_count() == 0);
    assert(opencalc_math_eval_expression("A=2+3", &real));
    near(real, 5.0);
    assert(opencalc_math_eval_expression("A*4", &real));
    near(real, 20.0);
    assert(opencalc_math_eval_expression("B:=7", &real));
    assert(opencalc_math_eval_expression("B+1", &real));
    near(real, 8.0);
    assert(opencalc_math_variable_set("E", 12.0, 0.0));
    assert(opencalc_math_variable_set("I", 13.0, 0.0));
    assert(opencalc_math_eval_expression("E+I", &real));
    near(real, 25.0);

    assert(opencalc_math_eval_expression("radius=6", &real));
    assert(opencalc_math_eval_expression("pi*radius^2", &real));
    near(real, 3.14159265358979323846 * 36.0);
    assert(opencalc_math_assignment_name(" velocity = 9.5", name, sizeof(name)));
    assert(strcmp(name, "velocity") == 0);

    assert(opencalc_math_eval_complex_expression("Z=2+3i", &real, &imag));
    near(real, 2.0);
    near(imag, 3.0);
    assert(opencalc_math_eval_complex_expression("Z*2", &real, &imag));
    near(real, 4.0);
    near(imag, 6.0);

    assert(opencalc_math_variable_rename("radius", "diameter"));
    assert(!opencalc_math_variable_get("radius", NULL, NULL));
    assert(opencalc_math_variable_get("DIAMETER", &real, &imag));
    near(real, 6.0);
    assert(opencalc_math_variable_delete("diameter"));
    assert(!opencalc_math_variable_get("diameter", NULL, NULL));

    assert(!opencalc_math_variable_name_valid("2bad"));
    assert(!opencalc_math_variable_name_valid("sin"));
    assert(!opencalc_math_variable_name_valid("name-that-is-too-long"));

    assert(opencalc_math_substitute_variables("A+Z", expanded, sizeof(expanded)));
    assert(strstr(expanded, "(5)") != NULL);
    assert(strstr(expanded, "i") != NULL);
    assert(opencalc_math_substitute_variables("e+E+i+I", expanded, sizeof(expanded)));
    assert(strstr(expanded, "e+(12)") != NULL);
    assert(strstr(expanded, "i+(13)") != NULL);

    puts("variable regression tests passed");
    return 0;
}
