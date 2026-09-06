#pragma once

#include <stdbool.h>
#include <stddef.h>

#define OPENCALC_INEQ_MAX_CLAUSES 4
#define OPENCALC_INEQ_MAX_CRITICAL 24
#define OPENCALC_INEQ_MAX_INTERVALS 16

typedef enum {
    OPENCALC_INEQ_LT = 0,
    OPENCALC_INEQ_LE,
    OPENCALC_INEQ_GT,
    OPENCALC_INEQ_GE,
    OPENCALC_INEQ_NE,
} opencalc_inequality_relation_t;

typedef struct {
    char lhs[80];
    char rhs[80];
    opencalc_inequality_relation_t relation;
} opencalc_inequality_clause_t;

typedef struct {
    opencalc_inequality_clause_t clauses[OPENCALC_INEQ_MAX_CLAUSES];
    int count;
    bool join_or;
} opencalc_inequality_problem_t;

typedef struct {
    double low;
    double high;
    bool low_infinite;
    bool high_infinite;
    bool low_closed;
    bool high_closed;
} opencalc_inequality_interval_t;

typedef struct {
    opencalc_inequality_interval_t intervals[OPENCALC_INEQ_MAX_INTERVALS];
    int interval_count;
    double critical[OPENCALC_INEQ_MAX_CRITICAL];
    int critical_count;
    bool empty;
    bool all_real;
    bool approximate;
} opencalc_inequality_solution_t;

bool opencalc_inequality_parse(const char *text, opencalc_inequality_problem_t *problem);
bool opencalc_inequality_evaluate(const opencalc_inequality_problem_t *problem,
                                  double x, double y, bool *matches);
bool opencalc_inequality_solve_1d(const opencalc_inequality_problem_t *problem,
                                  double search_limit,
                                  opencalc_inequality_solution_t *solution);
bool opencalc_inequality_format_intervals(const opencalc_inequality_solution_t *solution,
                                          char *out, size_t out_size);
const char *opencalc_inequality_relation_text(opencalc_inequality_relation_t relation);
