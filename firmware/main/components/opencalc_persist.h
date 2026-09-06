#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void opencalc_persist_init(void);
uint32_t opencalc_persist_get_u32(const char *key, uint32_t fallback);
bool opencalc_persist_set_u32(const char *key, uint32_t value);
bool opencalc_persist_get_string(const char *key, char *value, size_t value_size);
bool opencalc_persist_set_string(const char *key, const char *value);
bool opencalc_persist_erase(const char *key);
void opencalc_persist_factory_reset(void);

#ifdef __cplusplus
}
#endif
