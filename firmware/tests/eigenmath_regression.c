#include "opencalc_eigenmath.h"

#include <stdio.h>
#include <string.h>

static int expect_result(const char *expression, const char *expected)
{
    char output[256];
    if (!opencalc_eigenmath_eval(expression, output, sizeof(output))) {
        fprintf(stderr, "FAIL %s: symbolic engine rejected expression\n", expression);
        return 1;
    }
    if (strcmp(output, expected) != 0) {
        fprintf(stderr, "FAIL %s: expected <%s> got <%s>\n", expression, expected, output);
        return 1;
    }
    printf("PASS %s -> %s\n", expression, output);
    return 0;
}

static int expect_error(const char *expression)
{
    char output[256];
    if (!opencalc_eigenmath_eval(expression, output, sizeof(output)) || strstr(output, "Stop:") == NULL) {
        fprintf(stderr, "FAIL %s: expected a captured CAS error, got <%s>\n", expression, output);
        return 1;
    }
    printf("PASS %s -> %s\n", expression, output);
    return 0;
}

int main(void)
{
    int failed = 0;
    failed |= expect_result("x+x", "2 x");
    failed |= expect_result("simplify(sin(x)^2+cos(x)^2)", "1");
    failed |= expect_result("deriv(sin(x)*exp(x),x)", "cos(x) exp(x) + sin(x) exp(x)");
    failed |= expect_result("int(sin(x),x)", "-cos(x)");
    failed |= expect_result("sum(k,1,10,k)", "55");
    failed |= expect_result("solve(x^3-6*x^2+11*x-6=0,x)", "(1,2,3)");
    failed |= expect_result("defint(x^2,x,0,3)", "9");
    failed |= expect_result("det(((1,2),(3,4)))", "-2");
    failed |= expect_result("expand((x+1)*sin(x))", "x sin(x) + sin(x)");
    failed |= expect_result("integrate(cos(x),x)", "sin(x)");
    failed |= expect_result("gamma(5)", "24");
    failed |= expect_result("1+gamma(5)", "25");
    failed |= expect_result("erf(0)", "0");
    failed |= expect_result("1+deriv(x^2,x)", "2 x + 1");
    failed |= expect_result("simplify(i^2)", "-1");
    failed |= expect_result("limit((x^2-1)/(x-1),x,1)", "2");
    failed |= expect_result("limit(sin(x)/x,x,0)", "1");
    failed |= expect_result("a=7", "a = 7");
    failed |= expect_result("a^2+1", "50");
    failed |= expect_error("deriv(sin(x),)");
    failed |= expect_result("x+x", "2 x");
    return failed ? 1 : 0;
}
