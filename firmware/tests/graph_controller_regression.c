#include "opencalc_cas.h"
#include "opencalc_graph_controller.h"
#include "opencalc_math.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

bool opencalc_giac_eval(const char *expression, bool degrees, char *out, size_t out_size)
{
    (void)degrees;
    snprintf(out, out_size, "%s", expression);
    return true;
}

bool opencalc_cas_eval(const char *expression, char *out, size_t out_size)
{
    (void)expression;
    (void)out;
    (void)out_size;
    return false;
}

int main(void)
{
    opencalc_graph_symbolic_request_t request = {
        .graphing_mode = 0,
        .series = 2,
        .fingerprint = 42,
        .range_first = 0,
        .range_last = 10,
        .primary = "x^2+1",
    };
    opencalc_graph_symbolic_result_t result;
    assert(opencalc_graph_symbolic_analyze(
        &request, false, graph_eval_expression_var, &result));
    assert(strcmp(result.derivative, "diff((x^2+1),x)") == 0);
    assert(result.series == 2 && result.fingerprint == 42);

    request.graphing_mode = 3;
    snprintf(request.primary, sizeof(request.primary), "rec(u(n-1)+u(n-2),0,1)");
    assert(opencalc_graph_symbolic_analyze(
        &request, false, graph_eval_expression_var, &result));
    assert(strstr(result.integral, "= 143") != NULL);
    assert(strstr(result.roots, "n=0") != NULL);
    assert(strstr(result.asymptotes, "increasing") != NULL);

    puts("PASS graph symbolic controller");
    return 0;
}
