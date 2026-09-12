#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "opencalc_cas_experience.h"

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

typedef struct {
    bool degrees;
    opencalc_cas_domain_t domain;
    char assumptions[OPENCALC_CAS_ASSUMPTIONS_MAX];
} opencalc_giac_options_t;

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
 * retires the interrupted context and worker before accepting more CAS work.
 */
opencalc_giac_status_t opencalc_giac_eval_timed(const char *expression,
                                                bool degrees,
                                                char *out,
                                                size_t out_size,
                                                unsigned timeout_ms,
                                                opencalc_giac_cancel_fn should_cancel,
                                                void *cancel_context);

opencalc_giac_status_t opencalc_giac_eval_timed_options(
    const char *expression,
    const opencalc_giac_options_t *options,
    char *out,
    size_t out_size,
    unsigned timeout_ms,
    opencalc_giac_cancel_fn should_cancel,
    void *cancel_context);

opencalc_giac_status_t opencalc_giac_eval_timed_structured(
    const char *expression,
    const opencalc_giac_options_t *options,
    char *out,
    size_t out_size,
    opencalc_cas_result_t *structured,
    unsigned timeout_ms,
    opencalc_giac_cancel_fn should_cancel,
    void *cancel_context);

/* Releases Giac's persistent context and variables. */
void opencalc_giac_reset(void);

/* True when a stuck Giac worker cannot be recovered safely in-process. */
bool opencalc_giac_recovery_required(void);

/* Runs a small serial-logged engine smoke test; returns the failure count. */
int opencalc_giac_self_test(void);

#ifdef __cplusplus
}
#endif
