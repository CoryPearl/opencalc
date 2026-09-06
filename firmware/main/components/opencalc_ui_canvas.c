#include "opencalc_ui_canvas.h"

#include "esp_attr.h"

#include <stdlib.h>

static EXT_RAM_BSS_ATTR uint32_t s_pixels[OPENCALC_UI_WIDTH * OPENCALC_UI_HEIGHT];

uint32_t *opencalc_ui_canvas_pixels(void) { return s_pixels; }
size_t opencalc_ui_canvas_size_bytes(void) { return sizeof(s_pixels); }

void opencalc_ui_canvas_pixel(int x, int y, uint32_t color)
{
    if (x >= 0 && x < OPENCALC_UI_WIDTH && y >= 0 && y < OPENCALC_UI_HEIGHT) {
        s_pixels[y * OPENCALC_UI_WIDTH + x] = color;
    }
}

void opencalc_ui_canvas_clear(uint32_t color)
{
    for (int i = 0; i < OPENCALC_UI_WIDTH * OPENCALC_UI_HEIGHT; i++) s_pixels[i] = color;
}

void opencalc_ui_canvas_rect(int x, int y, int width, int height, uint32_t color)
{
    for (int py = y; py < y + height; py++) {
        for (int px = x; px < x + width; px++) opencalc_ui_canvas_pixel(px, py, color);
    }
}

void opencalc_ui_canvas_border(int x, int y, int width, int height, uint32_t color)
{
    opencalc_ui_canvas_rect(x, y, width, 1, color);
    opencalc_ui_canvas_rect(x, y + height - 1, width, 1, color);
    opencalc_ui_canvas_rect(x, y, 1, height, color);
    opencalc_ui_canvas_rect(x + width - 1, y, 1, height, color);
}

void opencalc_ui_canvas_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        opencalc_ui_canvas_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int twice = 2 * error;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

void opencalc_ui_canvas_circle(int cx, int cy, int radius, uint32_t color)
{
    int x = radius;
    int y = 0;
    int error = 0;
    while (x >= y) {
        opencalc_ui_canvas_pixel(cx + x, cy + y, color);
        opencalc_ui_canvas_pixel(cx + y, cy + x, color);
        opencalc_ui_canvas_pixel(cx - y, cy + x, color);
        opencalc_ui_canvas_pixel(cx - x, cy + y, color);
        opencalc_ui_canvas_pixel(cx - x, cy - y, color);
        opencalc_ui_canvas_pixel(cx - y, cy - x, color);
        opencalc_ui_canvas_pixel(cx + y, cy - x, color);
        opencalc_ui_canvas_pixel(cx + x, cy - y, color);
        y++;
        if (error <= 0) error += 2 * y + 1;
        if (error > 0) { x--; error -= 2 * x + 1; }
    }
}
