#include "opencalc_cas.h"

#include <stdio.h>
#include <string.h>

static int expect_cas(const char *expr, const char *expected)
{
    char out[128];
    if (!opencalc_cas_eval(expr, out, sizeof(out))) {
        fprintf(stderr, "FAIL %s: not handled by CAS\n", expr);
        return 1;
    }
    if (strcmp(out, expected) != 0) {
        fprintf(stderr, "FAIL %s: expected <%s> got <%s>\n", expr, expected, out);
        return 1;
    }
    printf("PASS %s -> %s\n", expr, out);
    return 0;
}

static int expect_numeric_passthrough(const char *expr)
{
    char out[128] = "unchanged";
    if (opencalc_cas_eval(expr, out, sizeof(out))) {
        fprintf(stderr, "FAIL %s: ordinary arithmetic was consumed by CAS as <%s>\n", expr, out);
        return 1;
    }
    printf("PASS %s -> numeric evaluator\n", expr);
    return 0;
}

int main(void)
{
    int failed = 0;

    failed |= expect_numeric_passthrough("1+1");
    failed |= expect_numeric_passthrough("8^8");

    failed |= expect_cas("expand((x+3)^2)", "x^2+6*x+9");
    failed |= expect_cas("EXPAND((x-4)^2)", "x^2-8*x+16");
    failed |= expect_cas("expand((x+1)^4)", "x^4+4*x^3+6*x^2+4*x+1");
    failed |= expect_cas("expand((2*x-1)*(x+4))", "2*x^2+7*x-4");
    failed |= expect_cas("expand(-x^2+2*x)", "-x^2+2*x");
    failed |= expect_cas("factor(x^2-5*x+6)", "(x-2)(x-3)");
    failed |= expect_cas("factor(x^3-6*x^2+11*x-6)", "(x-1)(x-2)(x-3)");
    failed |= expect_cas("solve(x^2-5*x+6=0)", "x=3 or 2");
    failed |= expect_cas("solve(x^2+1=0)", "x=0+1i or 0-1i");
    failed |= expect_cas("solve(x^2+2x+4)", "x=-1+1.732050808i or -1-1.732050808i");
    failed |= expect_cas("solve(2*y+4=0,y)", "y=-2");
    failed |= expect_cas("deriv((x+1)*(x-1)+x^3,x)", "3*x^2+2*x");
    failed |= expect_cas("int(3*x^2+2*x+1,x)", "x^3+x^2+x+C");
    failed |= expect_cas("deriv(sin(x)*exp(x),x)", "cos(x) exp(x) + sin(x) exp(x)");
    failed |= expect_cas("integrate(cos(x),x)", "sin(x)");
    failed |= expect_cas("expand((x+1)*sin(x))", "x sin(x) + sin(x)");
    failed |= expect_cas("limit(sin(x)/x,x,0)", "1");
    failed |= expect_cas("gamma(5)", "24");
    failed |= expect_cas("1+gamma(5)", "25");
    failed |= expect_cas("simplify(i^2)", "-1");

    return failed ? 1 : 0;
}
