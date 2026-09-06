#pragma once

#include <stddef.h>
#include <stdint.h>

#define OPENCALC_UI_WIDTH 320
#define OPENCALC_UI_HEIGHT 240

uint32_t *opencalc_ui_canvas_pixels(void);
size_t opencalc_ui_canvas_size_bytes(void);
void opencalc_ui_canvas_pixel(int x, int y, uint32_t color);
void opencalc_ui_canvas_clear(uint32_t color);
void opencalc_ui_canvas_rect(int x, int y, int width, int height, uint32_t color);
void opencalc_ui_canvas_border(int x, int y, int width, int height, uint32_t color);
void opencalc_ui_canvas_line(int x0, int y0, int x1, int y1, uint32_t color);
void opencalc_ui_canvas_circle(int cx, int cy, int radius, uint32_t color);
