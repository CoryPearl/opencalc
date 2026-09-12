#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    OPENCALC_GRAPH_BACKGROUND_STRETCH = 0,
    OPENCALC_GRAPH_BACKGROUND_FIT,
    OPENCALC_GRAPH_BACKGROUND_FILL,
} opencalc_graph_background_mode_t;

bool opencalc_graph_background_load_bmp(const char *path, uint32_t *pixels,
                                        int output_width, int output_height,
                                        uint32_t background_color,
                                        opencalc_graph_background_mode_t mode,
                                        char *status, size_t status_size);
