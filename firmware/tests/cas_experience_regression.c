#include "opencalc_cas_experience.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    opencalc_cas_options_t options;
    opencalc_cas_options_default(&options);
    char command[256];

    assert(opencalc_cas_prepare_expression("solve(x^2=4,x)", &options,
                                           command, sizeof(command)));
    assert(strcmp(command, "solve(x^2=4,x)") == 0);

    options.domain = OPENCALC_CAS_DOMAIN_INTEGER;
    assert(opencalc_cas_prepare_expression("solve(x^2=4,x)", &options,
                                           command, sizeof(command)));
    assert(strcmp(command, "isolve(x^2=4,x)") == 0);

    options.domain = OPENCALC_CAS_DOMAIN_INTERVAL;
    options.interval_min = -3.0;
    options.interval_max = 7.0;
    assert(opencalc_cas_prepare_expression("solve(sin(x)=0,x)", &options,
                                           command, sizeof(command)));
    assert(strcmp(command, "fsolve(sin(x)=0,x=-3..7)") == 0);

    options.domain = OPENCALC_CAS_DOMAIN_REAL;
    strcpy(options.assumptions, "x>0,n integer");
    assert(opencalc_cas_prepare_expression("factor(x^2-1)", &options,
                                           command, sizeof(command)));
    assert(strcmp(command, "factor(x^2-1)") == 0);

    const char *solutions = "[x=-sqrt(2),x=sqrt(2)]";
    opencalc_cas_result_t result;
    opencalc_cas_analyze_result(solutions, &result);
    assert(result.kind == OPENCALC_CAS_RESULT_SOLUTIONS);
    assert(result.item_count == 2);
    assert(result.exact);
    char item[64];
    assert(opencalc_cas_result_item_text(solutions, &result, 1, item, sizeof(item)));
    assert(strcmp(item, "x=sqrt(2)") == 0);

    opencalc_cas_analyze_result("[[1,2],[3,4]]", &result);
    assert(result.kind == OPENCALC_CAS_RESULT_MATRIX);
    assert(result.item_count == 2);

    assert(opencalc_cas_catalog_count() >= 30);
    int index = opencalc_cas_catalog_complete("integ", -1);
    assert(index >= 0);
    assert(strcmp(opencalc_cas_catalog_at((size_t)index)->name, "integrate") == 0);

    puts("CAS experience regression tests passed.");
    return 0;
}
