#include "opencalc_math_layout.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int text_calls;
    int line_calls;
    int cursor_calls;
    char text[256];
} fixture_t;

static int measure(const char *text, size_t length, bool compact, void *context)
{
    (void)context;
    int width = 0;
    for (size_t i = 0; i < length; i++) width += text[i] == ' ' ? 4 : (compact ? 6 : 12);
    return width;
}

static void text(int x, int y, const char *value, size_t length, bool compact,
                 uint32_t color, void *context)
{
    (void)x; (void)y; (void)compact; (void)color;
    fixture_t *fixture = context;
    fixture->text_calls++;
    size_t used = strlen(fixture->text);
    if (used + length + 2 < sizeof(fixture->text)) {
        memcpy(fixture->text + used, value, length);
        fixture->text[used + length] = '|';
        fixture->text[used + length + 1] = '\0';
    }
}

static void line(int x0, int y0, int x1, int y1, uint32_t color, void *context)
{
    (void)x0; (void)y0; (void)x1; (void)y1; (void)color;
    ((fixture_t *)context)->line_calls++;
}

static void cursor(int x, int y, int height, bool compact, void *context)
{
    (void)x; (void)y; (void)height; (void)compact;
    ((fixture_t *)context)->cursor_calls++;
}

static int check(const char *name, const char *expression, size_t cursor_position,
                 int minimum_lines, const char *expected_text)
{
    fixture_t fixture = {0};
    opencalc_math_layout_callbacks_t callbacks = {
        .measure = measure, .text = text, .line = line, .cursor = cursor,
        .context = &fixture,
    };
    opencalc_math_layout_result_t result = opencalc_math_layout(
        expression, cursor_position, 10, 30, false, 0xffffff, true, &callbacks);
    if (!result.complete || result.width <= 0 || result.ascent <= 0 || result.descent <= 0 ||
        fixture.line_calls < minimum_lines ||
        (cursor_position != SIZE_MAX && fixture.cursor_calls != 1) ||
        (expected_text != NULL && strstr(fixture.text, expected_text) == NULL)) {
        fprintf(stderr, "FAIL %s: complete=%d size=%dx%d lines=%d cursors=%d text=%s\n",
                name, result.complete, result.width, result.ascent + result.descent,
                fixture.line_calls, fixture.cursor_calls, fixture.text);
        return 1;
    }
    printf("PASS %s\n", name);
    return 0;
}

int main(void)
{
    int failed = 0;
    failed |= check("nested_fraction_radical", "frac(1,sqrt(x^2+1))", 16, 4, "x|");
    failed |= check("general_fraction", "(a+b)/(c+d)", SIZE_MAX, 1, "a|");
    failed |= check("matrix", "[[1,2],[3,4]]", SIZE_MAX, 6, "4|");
    failed |= check("piecewise", "piecewise(x^2,x<0,sqrt(x),x>=0)", SIZE_MAX, 6, "if|");
    failed |= check("limit", "limit(sin(x)/x,x,0)", SIZE_MAX, 1, "lim|");
    failed |= check("integral", "defint(x^2,x,0,1)", SIZE_MAX, 3, "d|");
    return failed;
}
