#include "opencalc_inequality.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    opencalc_inequality_problem_t problem;
    opencalc_inequality_solution_t solution;
    char intervals[256];
    bool matches = false;

    if (!opencalc_inequality_parse("x^2-4>=0", &problem) || problem.count != 1 ||
        !opencalc_inequality_solve_1d(&problem, 100.0, &solution) ||
        !opencalc_inequality_format_intervals(&solution, intervals, sizeof(intervals)) ||
        strstr(intervals, "-2") == NULL || strstr(intervals, "2") == NULL) return 1;

    if (!opencalc_inequality_parse("x>=0 and x<=1", &problem) || problem.count != 2 ||
        !opencalc_inequality_evaluate(&problem, 0.5, 0.0, &matches) || !matches ||
        !opencalc_inequality_evaluate(&problem, 2.0, 0.0, &matches) || matches) return 2;

    if (!opencalc_inequality_parse("x<0 or x>2", &problem) || !problem.join_or ||
        !opencalc_inequality_evaluate(&problem, -1.0, 0.0, &matches) || !matches ||
        !opencalc_inequality_evaluate(&problem, 1.0, 0.0, &matches) || matches) return 3;

    if (!opencalc_inequality_parse("x^2+y^2<=25", &problem) ||
        !opencalc_inequality_evaluate(&problem, 3.0, 4.0, &matches) || !matches ||
        !opencalc_inequality_evaluate(&problem, 6.0, 0.0, &matches) || matches) return 4;

    if (!opencalc_inequality_parse("x!=0", &problem) ||
        !opencalc_inequality_solve_1d(&problem, 100.0, &solution) || solution.interval_count != 2) return 5;

    if (!opencalc_inequality_parse("-2<x<2", &problem) || problem.count != 2 || problem.join_or ||
        !opencalc_inequality_evaluate(&problem, 0.0, 0.0, &matches) || !matches ||
        !opencalc_inequality_evaluate(&problem, 3.0, 0.0, &matches) || matches) return 6;

    if (!opencalc_inequality_parse("abs(x)<=3", &problem) ||
        !opencalc_inequality_solve_1d(&problem, 20.0, &solution) ||
        !opencalc_inequality_format_intervals(&solution, intervals, sizeof(intervals)) ||
        strstr(intervals, "-3") == NULL || strstr(intervals, "3") == NULL) return 7;

    if (!opencalc_inequality_parse("1/x>0", &problem) ||
        !opencalc_inequality_solve_1d(&problem, 20.0, &solution) ||
        solution.critical_count == 0) return 8;

    puts("PASS inequality parser and interval solver");
    return 0;
}
