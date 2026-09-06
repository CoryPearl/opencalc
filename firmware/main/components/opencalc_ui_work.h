#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef void (*opencalc_ui_work_execute_fn)(const void *job, void *result);

bool opencalc_ui_work_start(size_t job_size,
                            size_t result_size,
                            unsigned stack_size,
                            int core,
                            opencalc_ui_work_execute_fn execute);
bool opencalc_ui_work_submit(const void *job);
void *opencalc_ui_work_take_result(void);
bool opencalc_ui_work_ready(void);
