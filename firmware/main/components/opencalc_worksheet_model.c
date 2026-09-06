#include "opencalc_worksheet_model.h"

#include "freertos/semphr.h"

#include "esp_attr.h"

static EXT_RAM_BSS_ATTR opencalc_list_bank_t s_lists;
static opencalc_list_counts_t s_list_counts;
static EXT_RAM_BSS_ATTR opencalc_matrix_bank_t s_matrices;
static opencalc_matrix_dimensions_t s_matrix_rows;
static opencalc_matrix_dimensions_t s_matrix_cols;

static StaticSemaphore_t s_mutex_storage;
static SemaphoreHandle_t s_mutex;

void opencalc_worksheet_model_init(void)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_storage);
    }
}

bool opencalc_worksheet_model_lock(TickType_t wait)
{
    opencalc_worksheet_model_init();
    return s_mutex != NULL && xSemaphoreTake(s_mutex, wait) == pdTRUE;
}

void opencalc_worksheet_model_unlock(void)
{
    if (s_mutex != NULL) xSemaphoreGive(s_mutex);
}

opencalc_list_bank_t *opencalc_worksheet_lists(void) { return &s_lists; }
opencalc_list_counts_t *opencalc_worksheet_list_counts(void) { return &s_list_counts; }
opencalc_matrix_bank_t *opencalc_worksheet_matrices(void) { return &s_matrices; }
opencalc_matrix_dimensions_t *opencalc_worksheet_matrix_rows(void) { return &s_matrix_rows; }
opencalc_matrix_dimensions_t *opencalc_worksheet_matrix_cols(void) { return &s_matrix_cols; }
