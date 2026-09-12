#include "opencalc_math.h"
#include "opencalc_symbols.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void near(double actual, double expected)
{
    assert(fabs(actual - expected) < 1e-9);
}

typedef struct {
    size_t count;
    uint16_t rows;
    uint16_t cols;
    double first;
    double last;
} structured_visit_t;

static bool visit_structured(const opencalc_symbol_t *symbol, const double *values,
                             size_t count, void *context)
{
    structured_visit_t *visit = context;
    visit->count = count;
    visit->rows = symbol->rows;
    visit->cols = symbol->cols;
    visit->first = values[0];
    visit->last = values[count - 1];
    return true;
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
    assert(!opencalc_math_variable_name_valid("mata"));
    assert(!opencalc_math_variable_name_valid("Y10"));

    assert(opencalc_math_substitute_variables("A+Z", expanded, sizeof(expanded)));
    assert(strstr(expanded, "(5)") != NULL);
    assert(strstr(expanded, "i") != NULL);
    assert(opencalc_math_substitute_variables("e+E+i+I", expanded, sizeof(expanded)));
    assert(strstr(expanded, "e+(12)") != NULL);
    assert(strstr(expanded, "i+(13)") != NULL);

    opencalc_symbol_t exact = {
        .type = OPENCALC_SYMBOL_EXPRESSION,
        .flags = OPENCALC_SYMBOL_USER | OPENCALC_SYMBOL_GIAC_SYNC,
    };
    snprintf(exact.name, sizeof(exact.name), "exactValue");
    snprintf(exact.text, sizeof(exact.text), "sqrt(2)");
    assert(opencalc_symbol_set(&exact));
    assert(opencalc_symbol_get("EXACTVALUE", &exact));
    assert(exact.type == OPENCALC_SYMBOL_EXPRESSION);
    assert(strcmp(exact.text, "sqrt(2)") == 0);
    assert(opencalc_math_substitute_variables("exactValue+1", expanded, sizeof(expanded)));
    assert(strcmp(expanded, "(sqrt(2))+1") == 0);

    double large_list[999];
    for (size_t i = 0; i < 999; i++) large_list[i] = (double)i;
    assert(opencalc_symbol_set_real_list(
        "L1", OPENCALC_SYMBOL_READ_ONLY | OPENCALC_SYMBOL_GIAC_SYNC,
        OPENCALC_SYMBOL_SOURCE_WORKSHEET_LIST, 0, large_list, 999));
    opencalc_symbol_t list_symbol;
    assert(opencalc_symbol_get("L1", &list_symbol));
    assert(list_symbol.structured && list_symbol.element_count == 999 &&
           list_symbol.source == OPENCALC_SYMBOL_SOURCE_WORKSHEET_LIST);
    uint32_t list_revision = list_symbol.revision;
    assert(opencalc_symbol_set_real_list(
        "L1", OPENCALC_SYMBOL_READ_ONLY | OPENCALC_SYMBOL_GIAC_SYNC,
        OPENCALC_SYMBOL_SOURCE_WORKSHEET_LIST, 0, large_list, 999));
    assert(opencalc_symbol_get("L1", &list_symbol));
    assert(list_symbol.revision == list_revision);
    structured_visit_t visit = {0};
    assert(opencalc_symbol_visit_real_values("L1", visit_structured, &visit));
    assert(visit.count == 999 && visit.rows == 1 && visit.cols == 999 &&
           visit.first == 0.0 && visit.last == 998.0);
    char too_small[64];
    assert(!opencalc_symbol_format_value("L1", too_small, sizeof(too_small)));

    assert(opencalc_symbol_set_text("samples", OPENCALC_SYMBOL_LIST,
                                    OPENCALC_SYMBOL_USER | OPENCALC_SYMBOL_GIAC_SYNC,
                                    "[1,2,3]"));
    assert(opencalc_symbol_get("samples", &list_symbol));
    list_revision = list_symbol.revision;
    assert(opencalc_symbol_rename("samples", "readings"));
    assert(opencalc_symbol_get("readings", &list_symbol));
    assert(list_symbol.structured && list_symbol.revision > list_revision);

    double matrix_storage[2][4] = {{1, 2, 99, 99}, {3, 4, 99, 99}};
    assert(opencalc_symbol_set_real_matrix(
        "matA", OPENCALC_SYMBOL_READ_ONLY | OPENCALC_SYMBOL_GIAC_SYNC,
        OPENCALC_SYMBOL_SOURCE_WORKSHEET_MATRIX, 0,
        &matrix_storage[0][0], 2, 2, 4));
    char matrix_text[64];
    assert(opencalc_symbol_format_value("matA", matrix_text, sizeof(matrix_text)));
    assert(strcmp(matrix_text, "[[1,2],[3,4]]") == 0);

    puts("variable regression tests passed");
    return 0;
}
