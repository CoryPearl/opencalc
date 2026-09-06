#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Runs serial-logged, non-destructive device diagnostics and returns failures. */
int opencalc_self_test_run(void);

#ifdef __cplusplus
}
#endif
