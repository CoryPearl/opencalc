#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void opencalc_persist_init(void);
uint32_t opencalc_persist_get_u32(const char *key, uint32_t fallback);
bool opencalc_persist_set_u32(const char *key, uint32_t value);
void opencalc_persist_factory_reset(void);

#ifdef __cplusplus
}
#endif
