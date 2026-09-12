#include "opencalc_script_model.h"

#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define OPENCALC_SCRIPT_STORAGE EXT_RAM_BSS_ATTR
#else
#define OPENCALC_SCRIPT_STORAGE
#endif

static OPENCALC_SCRIPT_STORAGE opencalc_script_model_t s_script;

opencalc_script_model_t *opencalc_script_model(void) { return &s_script; }

void opencalc_script_model_reset(void)
{
    memset(&s_script, 0, sizeof(s_script));
}
