#pragma once

#include <stdbool.h>

void opencalc_power_init(void);
void opencalc_power_set_power_save(bool enabled);
bool opencalc_power_get_power_save(void);
void opencalc_power_set_performance_required(bool required);
