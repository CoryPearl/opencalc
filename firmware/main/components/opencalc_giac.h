#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OPENCALC_GIAC_OK = 0,
    OPENCALC_GIAC_ERROR,
    OPENCALC_GIAC_TIMEOUT,
    OPENCALC_GIAC_CANCELLED,
    OPENCALC_GIAC_UNAVAILABLE,
} opencalc_giac_status_t;

typedef bool (*opencalc_giac_cancel_fn)(void *context);

/*
 * Evaluate one expression with the Giac/KhiCAS engine. The implementation
 * serializes requests and executes Giac on a large PSRAM-backed task stack.
 */
bool opencalc_giac_eval(const char *expression,
                        bool degrees,
                        char *out,
                        size_t out_size);

/*
 * Bounded evaluation for UI and script jobs. Timeout/cancellation requests a
 * cooperative Giac abort, waits briefly for the evaluator to unwind, and
 * discards the interrupted context before accepting more CAS work.
 */
opencalc_giac_status_t opencalc_giac_eval_timed(const char *expression,
                                                bool degrees,
                                                char *out,
                                                size_t out_size,
                                                unsigned timeout_ms,
                                                opencalc_giac_cancel_fn should_cancel,
                                                void *cancel_context);

/* Releases Giac's persistent context and variables. */
void opencalc_giac_reset(void);

/* Runs a small serial-logged engine smoke test; returns the failure count. */
int opencalc_giac_self_test(void);

#ifdef __cplusplus
}
#endif
