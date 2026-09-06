#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "opencalc_giac.h"

#define OPENCALC_CALC_EXPR_MAX 768
#define OPENCALC_CALC_RESULT_MAX 1024
#define OPENCALC_CALC_HISTORY_MAX 16

typedef bool (*opencalc_calc_catalog_expand_fn)(const char *input,
                                                 char *out,
                                                 size_t out_size,
                                                 void *context);

typedef struct {
    const char *expression;
    const char *ans;
    int complex_mode;
    int display_format;
    int print_mode;
    bool degrees;
    unsigned timeout_ms;
    opencalc_giac_cancel_fn should_cancel;
    void *cancel_context;
    opencalc_calc_catalog_expand_fn expand_catalog;
    void *catalog_context;
} opencalc_calc_eval_request_t;

typedef struct {
    bool ok;
    bool update_ans;
    opencalc_giac_status_t giac_status;
    char output[OPENCALC_CALC_RESULT_MAX];
} opencalc_calc_eval_result_t;

void opencalc_calc_evaluate(const opencalc_calc_eval_request_t *request,
                            opencalc_calc_eval_result_t *result);

void opencalc_calc_history_reset(void);
void opencalc_calc_history_push(const char *expression, const char *result);
int opencalc_calc_history_count(void);
const char *opencalc_calc_history_expression(int index);
const char *opencalc_calc_history_result(int index);
int opencalc_calc_history_selected(void);
bool opencalc_calc_history_answer_selected(void);
void opencalc_calc_history_clear_selection(void);
void opencalc_calc_history_select_expression(void);
void opencalc_calc_history_select_answer(void);
void opencalc_calc_history_move_up(void);
void opencalc_calc_history_move_down(void);
const char *opencalc_calc_history_selected_text(void);
void opencalc_calc_history_export(
    int *count,
    char expressions[OPENCALC_CALC_HISTORY_MAX][OPENCALC_CALC_EXPR_MAX],
    char results[OPENCALC_CALC_HISTORY_MAX][OPENCALC_CALC_RESULT_MAX]);
bool opencalc_calc_history_import(
    int count,
    const char expressions[OPENCALC_CALC_HISTORY_MAX][OPENCALC_CALC_EXPR_MAX],
    const char results[OPENCALC_CALC_HISTORY_MAX][OPENCALC_CALC_RESULT_MAX]);
