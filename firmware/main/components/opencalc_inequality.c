#include "opencalc_inequality.h"

#include "opencalc_math.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INEQ_EPSILON 1e-8

const char *opencalc_inequality_relation_text(opencalc_inequality_relation_t relation)
{
    static const char *const names[] = {"<", "<=", ">", ">=", "!="};
    return (unsigned)relation <= OPENCALC_INEQ_NE ? names[relation] : "?";
}

static void trim_copy(const char *start, size_t length, char *out, size_t out_size)
{
    while (length > 0 && isspace((unsigned char)*start)) {
        start++;
        length--;
    }
    while (length > 0 && isspace((unsigned char)start[length - 1])) length--;
    if (length >= out_size) length = out_size - 1;
    memcpy(out, start, length);
    out[length] = '\0';
}

static bool parse_clause(const char *text, size_t length, opencalc_inequality_clause_t *clause)
{
    const char *operator_at = NULL;
    size_t operator_length = 0;
    opencalc_inequality_relation_t relation = OPENCALC_INEQ_LT;
    int depth = 0;
    for (size_t i = 0; i < length; i++) {
        if (text[i] == '(' || text[i] == '[' || text[i] == '{') depth++;
        else if (text[i] == ')' || text[i] == ']' || text[i] == '}') depth--;
        if (depth != 0) continue;
        if (i + 1 < length && text[i] == '<' && text[i + 1] == '=') {
            operator_at = text + i; operator_length = 2; relation = OPENCALC_INEQ_LE; break;
        }
        if (i + 1 < length && text[i] == '>' && text[i + 1] == '=') {
            operator_at = text + i; operator_length = 2; relation = OPENCALC_INEQ_GE; break;
        }
        if (i + 1 < length && text[i] == '!' && text[i + 1] == '=') {
            operator_at = text + i; operator_length = 2; relation = OPENCALC_INEQ_NE; break;
        }
        if (text[i] == '<' || text[i] == '>') {
            operator_at = text + i; operator_length = 1;
            relation = text[i] == '<' ? OPENCALC_INEQ_LT : OPENCALC_INEQ_GT;
            break;
        }
    }
    if (operator_at == NULL || depth < 0) return false;
    trim_copy(text, (size_t)(operator_at - text), clause->lhs, sizeof(clause->lhs));
    const char *right = operator_at + operator_length;
    trim_copy(right, length - (size_t)(right - text), clause->rhs, sizeof(clause->rhs));
    clause->relation = relation;
    return clause->lhs[0] != '\0' && clause->rhs[0] != '\0';
}

static bool word_at(const char *text, size_t length, size_t at, const char *word)
{
    size_t word_length = strlen(word);
    if (at + word_length > length || strncmp(text + at, word, word_length) != 0) return false;
    bool left_ok = at == 0 || isspace((unsigned char)text[at - 1]);
    bool right_ok = at + word_length == length || isspace((unsigned char)text[at + word_length]);
    return left_ok && right_ok;
}

static size_t relation_length_at(const char *text, size_t length, size_t at)
{
    if (at >= length) return 0;
    if (at + 1 < length &&
        ((text[at] == '<' && text[at + 1] == '=') ||
         (text[at] == '>' && text[at + 1] == '=') ||
         (text[at] == '!' && text[at + 1] == '='))) return 2;
    return text[at] == '<' || text[at] == '>' ? 1 : 0;
}

static bool parse_chained_relation(const char *text, size_t length,
                                   opencalc_inequality_problem_t *problem)
{
    size_t positions[2] = {0};
    size_t operator_lengths[2] = {0};
    int count = 0;
    int depth = 0;
    for (size_t i = 0; i < length; i++) {
        if (text[i] == '(' || text[i] == '[' || text[i] == '{') depth++;
        else if (text[i] == ')' || text[i] == ']' || text[i] == '}') depth--;
        if (depth != 0) continue;
        size_t operator_length = relation_length_at(text, length, i);
        if (operator_length == 0) continue;
        if (count >= 2) return false;
        positions[count] = i;
        operator_lengths[count] = operator_length;
        count++;
        i += operator_length - 1;
    }
    if (count != 2 || positions[0] + operator_lengths[0] >= positions[1]) return false;
    if (!parse_clause(text, positions[1], &problem->clauses[0])) return false;
    size_t second_start = positions[0] + operator_lengths[0];
    if (!parse_clause(text + second_start, length - second_start, &problem->clauses[1])) return false;
    problem->count = 2;
    problem->join_or = false;
    return true;
}

bool opencalc_inequality_parse(const char *text, opencalc_inequality_problem_t *problem)
{
    if (text == NULL || problem == NULL) return false;
    memset(problem, 0, sizeof(*problem));
    size_t length = strlen(text);
    bool has_join = false;
    for (size_t i = 0; i < length; i++) {
        if (word_at(text, length, i, "and") || word_at(text, length, i, "or")) {
            has_join = true;
            break;
        }
    }
    if (!has_join && parse_chained_relation(text, length, problem)) return true;
    memset(problem, 0, sizeof(*problem));
    size_t start = 0;
    int depth = 0;
    int join_kind = 0;
    for (size_t i = 0; i <= length; i++) {
        if (i < length && (text[i] == '(' || text[i] == '[' || text[i] == '{')) depth++;
        else if (i < length && (text[i] == ')' || text[i] == ']' || text[i] == '}')) depth--;
        bool at_end = i == length;
        int found_join = 0;
        size_t join_length = 0;
        if (!at_end && depth == 0 && word_at(text, length, i, "and")) {
            found_join = 1; join_length = 3;
        } else if (!at_end && depth == 0 && word_at(text, length, i, "or")) {
            found_join = 2; join_length = 2;
        }
        if (!at_end && found_join == 0) continue;
        if (problem->count >= OPENCALC_INEQ_MAX_CLAUSES ||
            !parse_clause(text + start, i - start, &problem->clauses[problem->count])) return false;
        problem->count++;
        if (found_join != 0) {
            if (join_kind != 0 && join_kind != found_join) return false;
            join_kind = found_join;
            i += join_length;
            while (i < length && isspace((unsigned char)text[i])) i++;
            start = i;
            i--;
        }
    }
    problem->join_or = join_kind == 2;
    return problem->count > 0 && depth == 0;
}

static bool relation_matches(opencalc_inequality_relation_t relation, double difference)
{
    switch (relation) {
    case OPENCALC_INEQ_LT: return difference < -INEQ_EPSILON;
    case OPENCALC_INEQ_LE: return difference <= INEQ_EPSILON;
    case OPENCALC_INEQ_GT: return difference > INEQ_EPSILON;
    case OPENCALC_INEQ_GE: return difference >= -INEQ_EPSILON;
    case OPENCALC_INEQ_NE: return fabs(difference) > INEQ_EPSILON;
    default: return false;
    }
}

static bool evaluate_clause(const opencalc_inequality_clause_t *clause,
                            double x, double y, double *difference, bool *matches)
{
    double lhs = 0.0, rhs = 0.0;
    if (!graph_eval_expression_xy(clause->lhs, x, y, &lhs) ||
        !graph_eval_expression_xy(clause->rhs, x, y, &rhs)) return false;
    *difference = lhs - rhs;
    *matches = relation_matches(clause->relation, *difference);
    return true;
}

bool opencalc_inequality_evaluate(const opencalc_inequality_problem_t *problem,
                                  double x, double y, bool *matches)
{
    if (problem == NULL || matches == NULL || problem->count <= 0) return false;
    bool combined = problem->join_or ? false : true;
    for (int i = 0; i < problem->count; i++) {
        double difference = 0.0;
        bool clause_matches = false;
        if (!evaluate_clause(&problem->clauses[i], x, y, &difference, &clause_matches)) return false;
        combined = problem->join_or ? (combined || clause_matches) : (combined && clause_matches);
    }
    *matches = combined;
    return true;
}

static void add_critical(opencalc_inequality_solution_t *solution, double value)
{
    if (!isfinite(value)) return;
    for (int i = 0; i < solution->critical_count; i++) {
        if (fabs(solution->critical[i] - value) < 1e-5) return;
    }
    if (solution->critical_count < OPENCALC_INEQ_MAX_CRITICAL) {
        solution->critical[solution->critical_count++] = fabs(value) < 1e-10 ? 0.0 : value;
    }
}

static int compare_double(const void *left, const void *right)
{
    double a = *(const double *)left, b = *(const double *)right;
    return (a > b) - (a < b);
}

bool opencalc_inequality_solve_1d(const opencalc_inequality_problem_t *problem,
                                  double search_limit,
                                  opencalc_inequality_solution_t *solution)
{
    if (problem == NULL || solution == NULL || problem->count <= 0 || search_limit <= 0.0) return false;
    memset(solution, 0, sizeof(*solution));
    solution->approximate = true;
    const int samples = 2048;
    double step = (2.0 * search_limit) / samples;
    for (int clause = 0; clause < problem->count; clause++) {
        double previous_x = -search_limit;
        double previous_difference = 0.0;
        bool previous_match = false;
        bool previous_valid = evaluate_clause(&problem->clauses[clause], previous_x, 0.0,
                                              &previous_difference, &previous_match);
        for (int i = 1; i <= samples; i++) {
            double x = -search_limit + step * i;
            double difference = 0.0;
            bool match = false;
            bool valid = evaluate_clause(&problem->clauses[clause], x, 0.0, &difference, &match);
            bool sign_change = previous_difference * difference < 0.0;
            bool landed_on_root = fabs(difference) <= INEQ_EPSILON &&
                                  fabs(previous_difference) > INEQ_EPSILON;
            if (previous_valid && valid && (sign_change || landed_on_root)) {
                double lo = previous_x, hi = x, flo = previous_difference;
                for (int iteration = 0; iteration < 36; iteration++) {
                    double mid = (lo + hi) * 0.5;
                    double fmid = 0.0;
                    bool ignored = false;
                    if (!evaluate_clause(&problem->clauses[clause], mid, 0.0, &fmid, &ignored)) break;
                    if ((flo <= 0.0 && fmid >= 0.0) || (flo >= 0.0 && fmid <= 0.0)) hi = mid;
                    else { lo = mid; flo = fmid; }
                }
                add_critical(solution, (lo + hi) * 0.5);
            } else if (previous_valid != valid) {
                add_critical(solution, (previous_x + x) * 0.5);
            }
            previous_x = x;
            previous_difference = difference;
            previous_valid = valid;
        }
    }
    qsort(solution->critical, (size_t)solution->critical_count, sizeof(double), compare_double);

    int regions = solution->critical_count + 1;
    for (int region = 0; region < regions && solution->interval_count < OPENCALC_INEQ_MAX_INTERVALS; region++) {
        double low = region == 0 ? -search_limit : solution->critical[region - 1];
        double high = region == solution->critical_count ? search_limit : solution->critical[region];
        double sample = (low + high) * 0.5;
        bool match = false;
        if (!opencalc_inequality_evaluate(problem, sample, 0.0, &match) || !match) continue;
        opencalc_inequality_interval_t *interval = &solution->intervals[solution->interval_count++];
        interval->low = low;
        interval->high = high;
        interval->low_infinite = region == 0;
        interval->high_infinite = region == solution->critical_count;
        bool endpoint_match = false;
        interval->low_closed = !interval->low_infinite &&
            opencalc_inequality_evaluate(problem, low, 0.0, &endpoint_match) && endpoint_match;
        interval->high_closed = !interval->high_infinite &&
            opencalc_inequality_evaluate(problem, high, 0.0, &endpoint_match) && endpoint_match;
    }
    for (int i = 0; i < solution->critical_count &&
                    solution->interval_count < OPENCALC_INEQ_MAX_INTERVALS; i++) {
        bool match = false;
        if (!opencalc_inequality_evaluate(problem, solution->critical[i], 0.0, &match) || !match) continue;
        bool covered = false;
        for (int j = 0; j < solution->interval_count; j++) {
            opencalc_inequality_interval_t *interval = &solution->intervals[j];
            if ((!interval->low_infinite && fabs(interval->low - solution->critical[i]) < 1e-5 && interval->low_closed) ||
                (!interval->high_infinite && fabs(interval->high - solution->critical[i]) < 1e-5 && interval->high_closed)) {
                covered = true;
                break;
            }
        }
        if (!covered) {
            opencalc_inequality_interval_t *point = &solution->intervals[solution->interval_count++];
            point->low = solution->critical[i];
            point->high = solution->critical[i];
            point->low_closed = true;
            point->high_closed = true;
        }
    }
    solution->empty = solution->interval_count == 0;
    solution->all_real = solution->interval_count == 1 &&
        solution->intervals[0].low_infinite && solution->intervals[0].high_infinite;
    return true;
}

static bool append_text(char *out, size_t out_size, const char *text)
{
    size_t used = strlen(out), length = strlen(text);
    if (used + length >= out_size) return false;
    memcpy(out + used, text, length + 1);
    return true;
}

bool opencalc_inequality_format_intervals(const opencalc_inequality_solution_t *solution,
                                          char *out, size_t out_size)
{
    if (solution == NULL || out == NULL || out_size == 0) return false;
    out[0] = '\0';
    if (solution->empty) return append_text(out, out_size, "empty set");
    if (solution->all_real) return append_text(out, out_size, "(-inf, inf)");
    for (int i = 0; i < solution->interval_count; i++) {
        const opencalc_inequality_interval_t *interval = &solution->intervals[i];
        char part[80];
        char low[24], high[24];
        if (interval->low_infinite) snprintf(low, sizeof(low), "-inf");
        else snprintf(low, sizeof(low), "%.8g", interval->low);
        if (interval->high_infinite) snprintf(high, sizeof(high), "inf");
        else snprintf(high, sizeof(high), "%.8g", interval->high);
        snprintf(part, sizeof(part), "%s%c%s, %s%c",
                 i == 0 ? "" : " U ", interval->low_closed ? '[' : '(', low, high,
                 interval->high_closed ? ']' : ')');
        if (!append_text(out, out_size, part)) return false;
    }
    return true;
}
