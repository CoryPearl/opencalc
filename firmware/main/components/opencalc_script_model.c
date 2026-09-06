#include "opencalc_script_model.h"

#include <string.h>

static opencalc_script_model_t s_script;

opencalc_script_model_t *opencalc_script_model(void) { return &s_script; }

void opencalc_script_model_reset(void)
{
    memset(&s_script, 0, sizeof(s_script));
}
