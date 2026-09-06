#pragma once

#include "freertos/FreeRTOS.h"

#include <stdbool.h>

#define OPENCALC_WORKSHEET_LIST_COUNT 6
#define OPENCALC_WORKSHEET_LIST_CAPACITY 999
#define OPENCALC_WORKSHEET_MATRIX_COUNT 10
#define OPENCALC_WORKSHEET_MATRIX_MAX_N 99

typedef double opencalc_list_bank_t[OPENCALC_WORKSHEET_LIST_COUNT]
                                   [OPENCALC_WORKSHEET_LIST_CAPACITY];
typedef int opencalc_list_counts_t[OPENCALC_WORKSHEET_LIST_COUNT];
typedef double opencalc_matrix_bank_t[OPENCALC_WORKSHEET_MATRIX_COUNT]
                                     [OPENCALC_WORKSHEET_MATRIX_MAX_N]
                                     [OPENCALC_WORKSHEET_MATRIX_MAX_N];
typedef int opencalc_matrix_dimensions_t[OPENCALC_WORKSHEET_MATRIX_COUNT];

void opencalc_worksheet_model_init(void);
bool opencalc_worksheet_model_lock(TickType_t wait);
void opencalc_worksheet_model_unlock(void);

opencalc_list_bank_t *opencalc_worksheet_lists(void);
opencalc_list_counts_t *opencalc_worksheet_list_counts(void);
opencalc_matrix_bank_t *opencalc_worksheet_matrices(void);
opencalc_matrix_dimensions_t *opencalc_worksheet_matrix_rows(void);
opencalc_matrix_dimensions_t *opencalc_worksheet_matrix_cols(void);
