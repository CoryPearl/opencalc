#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Evaluate one expression with the Giac/KhiCAS engine. The implementation
 * serializes requests and executes Giac on a large PSRAM-backed task stack.
 */
bool opencalc_giac_eval(const char *expression,
                        bool degrees,
                        char *out,
                        size_t out_size);

/* Releases Giac's persistent context and variables. */
void opencalc_giac_reset(void);

/* Runs a small serial-logged engine smoke test; returns the failure count. */
int opencalc_giac_self_test(void);

#ifdef __cplusplus
}
#endif
