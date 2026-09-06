#pragma once

#include <stddef.h>

#define OPENCALC_SCRIPT_EDITOR_CAPACITY 2048

typedef struct {
    char text[OPENCALC_SCRIPT_EDITOR_CAPACITY];
    char name[32];
    size_t length;
    size_t cursor;
    int scroll_line;
    int scroll_column;
} opencalc_script_model_t;

opencalc_script_model_t *opencalc_script_model(void);
void opencalc_script_model_reset(void);
