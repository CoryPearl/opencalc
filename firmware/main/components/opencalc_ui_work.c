#include "opencalc_ui_work.h"

#include "esp_heap_caps.h"
#include "opencalc_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdint.h>
#include <string.h>

#define WORK_QUEUE_DEPTH 4

static QueueHandle_t s_jobs;
static QueueHandle_t s_results;
static TaskHandle_t s_task;
static size_t s_job_size;
static size_t s_result_size;
static opencalc_ui_work_execute_fn s_execute;

typedef struct {
    void *job;
    void *result;
} work_item_t;

static void *work_alloc(size_t size)
{
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    void *memory = size <= psram_free &&
                           psram_free - size >= OPENCALC_PSRAM_RESERVE_BYTES
                       ? heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                       : NULL;
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (memory == NULL && size <= internal_free &&
        internal_free - size >= OPENCALC_INTERNAL_HEAP_RESERVE_BYTES) {
        memory = heap_caps_calloc(1, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return memory;
}

static void work_task(void *context)
{
    (void)context;
    for (;;) {
        work_item_t item = {0};
        if (xQueueReceive(s_jobs, &item, portMAX_DELAY) != pdTRUE ||
            item.job == NULL || item.result == NULL) {
            if (item.job != NULL) heap_caps_free(item.job);
            if (item.result != NULL) heap_caps_free(item.result);
            continue;
        }
        s_execute(item.job, item.result);
        heap_caps_free(item.job);
        item.job = NULL;
        if (xQueueSend(s_results, &item.result, portMAX_DELAY) != pdTRUE) {
            heap_caps_free(item.result);
        }
    }
}

bool opencalc_ui_work_start(size_t job_size,
                            size_t result_size,
                            unsigned stack_size,
                            int core,
                            opencalc_ui_work_execute_fn execute)
{
    if (s_task != NULL) {
        return s_job_size == job_size && s_result_size == result_size && s_execute == execute;
    }
    if (job_size == 0 || result_size == 0 || execute == NULL) return false;
    if (s_jobs == NULL) s_jobs = xQueueCreate(WORK_QUEUE_DEPTH, sizeof(work_item_t));
    if (s_results == NULL) s_results = xQueueCreate(WORK_QUEUE_DEPTH, sizeof(void *));
    if (s_jobs == NULL || s_results == NULL) return false;

    s_job_size = job_size;
    s_result_size = result_size;
    s_execute = execute;
    BaseType_t created = xTaskCreatePinnedToCoreWithCaps(
        work_task, "opencalc_work", stack_size, NULL, 5, &s_task, core,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (created != pdPASS) {
        s_task = NULL;
        s_execute = NULL;
        return false;
    }
    return true;
}

bool opencalc_ui_work_submit(const void *job)
{
    if (!opencalc_ui_work_ready() || job == NULL) return false;
    work_item_t item = {
        .job = work_alloc(s_job_size),
        .result = work_alloc(s_result_size),
    };
    if (item.job == NULL || item.result == NULL) {
        if (item.job != NULL) heap_caps_free(item.job);
        if (item.result != NULL) heap_caps_free(item.result);
        return false;
    }
    memcpy(item.job, job, s_job_size);
    if (xQueueSend(s_jobs, &item, 0) != pdTRUE) {
        heap_caps_free(item.job);
        heap_caps_free(item.result);
        return false;
    }
    return true;
}

void *opencalc_ui_work_take_result(void)
{
    void *result = NULL;
    if (s_results == NULL || xQueueReceive(s_results, &result, 0) != pdTRUE) return NULL;
    return result;
}

bool opencalc_ui_work_ready(void)
{
    return s_task != NULL && s_jobs != NULL && s_results != NULL && s_execute != NULL;
}
