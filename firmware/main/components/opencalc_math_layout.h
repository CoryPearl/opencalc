#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int (*opencalc_math_measure_fn)(const char *text, size_t length,
                                        bool compact, void *context);
typedef void (*opencalc_math_text_fn)(int x, int y, const char *text,
                                     size_t length, bool compact,
                                     uint32_t color, void *context);
typedef void (*opencalc_math_line_fn)(int x0, int y0, int x1, int y1,
                                     uint32_t color, void *context);
typedef void (*opencalc_math_cursor_fn)(int x, int y, int height,
                                       bool compact, void *context);

typedef struct {
    opencalc_math_measure_fn measure;
    opencalc_math_text_fn text;
    opencalc_math_line_fn line;
    opencalc_math_cursor_fn cursor;
    void *context;
} opencalc_math_layout_callbacks_t;

typedef struct {
    int width;
    int ascent;
    int descent;
    int cursor_x;
    int cursor_y;
    bool cursor_valid;
    bool complete;
} opencalc_math_layout_result_t;

// Draws around a baseline at (x, y). Pass SIZE_MAX to hide the edit cursor.
opencalc_math_layout_result_t opencalc_math_layout(
    const char *expression, size_t cursor, int x, int y, bool compact,
    uint32_t color, bool draw, const opencalc_math_layout_callbacks_t *callbacks);
