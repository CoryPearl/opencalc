#include "opencalc_math_layout.h"

#include <ctype.h>
#include <string.h>
#include <strings.h>

#define MATH_LAYOUT_MAX_DEPTH 14
#define MATH_LAYOUT_MAX_ARGS 16
#define MATH_LAYOUT_MAX_MATRIX 8
#define MATH_LAYOUT_GAP 2

typedef struct {
    size_t start;
    size_t end;
} span_t;

typedef struct {
    const char *source;
    size_t cursor;
    uint32_t color;
    bool draw;
    bool cursor_drawn;
    int cursor_x;
    int cursor_y;
    bool complete;
    const opencalc_math_layout_callbacks_t *cb;
} layout_context_t;

typedef struct {
    int width;
    int ascent;
    int descent;
} box_t;

static box_t layout_span(layout_context_t *ctx, span_t span, int x, int y,
                         bool compact, int precedence, int depth);

static box_t measure_span(layout_context_t *ctx, span_t span, bool compact,
                          int precedence, int depth)
{
    layout_context_t measure = *ctx;
    measure.draw = false;
    measure.cursor = SIZE_MAX;
    box_t box = layout_span(&measure, span, 0, 0, compact, precedence, depth);
    if (!measure.complete) ctx->complete = false;
    return box;
}

static box_t box_make(int width, int ascent, int descent)
{
    box_t box = {width, ascent, descent};
    return box;
}

static int box_height(box_t box)
{
    return box.ascent + box.descent;
}

static span_t span_trim(const char *source, span_t span)
{
    while (span.start < span.end && isspace((unsigned char)source[span.start])) span.start++;
    while (span.end > span.start && isspace((unsigned char)source[span.end - 1])) span.end--;
    return span;
}

static bool span_equals(const char *source, span_t span, const char *word)
{
    size_t length = span.end - span.start;
    return strlen(word) == length && strncasecmp(source + span.start, word, length) == 0;
}

static int measure_text(layout_context_t *ctx, const char *text, size_t length, bool compact)
{
    return ctx->cb->measure(text, length, compact, ctx->cb->context);
}

static void draw_text(layout_context_t *ctx, int x, int baseline, const char *text,
                      size_t length, bool compact)
{
    if (!ctx->draw || length == 0) return;
    int top = baseline - (compact ? 6 : 12);
    ctx->cb->text(x, top, text, length, compact, ctx->color, ctx->cb->context);
}

static box_t layout_text(layout_context_t *ctx, span_t span, int x, int y, bool compact)
{
    int width = measure_text(ctx, ctx->source + span.start, span.end - span.start, compact);
    draw_text(ctx, x, y, ctx->source + span.start, span.end - span.start, compact);
    if (!ctx->cursor_drawn && ctx->cursor != SIZE_MAX &&
        ctx->cursor >= span.start && ctx->cursor <= span.end) {
        size_t offset = ctx->cursor - span.start;
        int cursor_x = x + measure_text(ctx, ctx->source + span.start, offset, compact);
        int cursor_y = y - (compact ? 7 : 13);
        ctx->cursor_x = cursor_x;
        ctx->cursor_y = cursor_y;
        if (ctx->draw) {
            ctx->cb->cursor(cursor_x, cursor_y, compact ? 9 : 16,
                            compact, ctx->cb->context);
        }
        ctx->cursor_drawn = true;
    }
    return box_make(width, compact ? 6 : 12, compact ? 1 : 2);
}

static box_t layout_label(layout_context_t *ctx, const char *label, int x, int y, bool compact)
{
    size_t length = strlen(label);
    int width = measure_text(ctx, label, length, compact);
    draw_text(ctx, x, y, label, length, compact);
    return box_make(width, compact ? 6 : 12, compact ? 1 : 2);
}

static void draw_line(layout_context_t *ctx, int x0, int y0, int x1, int y1)
{
    if (ctx->draw) ctx->cb->line(x0, y0, x1, y1, ctx->color, ctx->cb->context);
}

static void draw_cursor(layout_context_t *ctx, int x, int y, bool compact)
{
    if (ctx->cursor_drawn || ctx->cursor == SIZE_MAX) return;
    ctx->cursor_x = x;
    ctx->cursor_y = y - (compact ? 7 : 13);
    if (ctx->draw) {
        ctx->cb->cursor(ctx->cursor_x, ctx->cursor_y, compact ? 9 : 16,
                        compact, ctx->cb->context);
    }
    ctx->cursor_drawn = true;
}

static bool matching_delimiter(const char *source, span_t span, char open, char close)
{
    if (span.end - span.start < 2 || source[span.start] != open || source[span.end - 1] != close) {
        return false;
    }
    int depth = 0;
    for (size_t i = span.start; i < span.end; i++) {
        if (source[i] == open) depth++;
        else if (source[i] == close && --depth == 0) return i == span.end - 1;
    }
    return false;
}

static int split_arguments(const char *source, span_t span, span_t *args, int capacity)
{
    int paren = 0, bracket = 0, brace = 0;
    int count = 0;
    size_t start = span.start;
    for (size_t i = span.start; i <= span.end; i++) {
        char c = i < span.end ? source[i] : ',';
        if (c == '(') paren++;
        else if (c == ')') paren--;
        else if (c == '[') bracket++;
        else if (c == ']') bracket--;
        else if (c == '{') brace++;
        else if (c == '}') brace--;
        if (c == ',' && paren == 0 && bracket == 0 && brace == 0) {
            if (count >= capacity) return -1;
            args[count++] = span_trim(source, (span_t){start, i});
            start = i + 1;
        }
    }
    return count == 1 && args[0].start == args[0].end ? 0 : count;
}

static bool function_span(const char *source, span_t span, span_t *name, span_t *arguments)
{
    size_t open = span.start;
    while (open < span.end && (isalnum((unsigned char)source[open]) || source[open] == '_')) open++;
    if (open == span.start || open >= span.end || source[open] != '(' || source[span.end - 1] != ')' ||
        !matching_delimiter(source, (span_t){open, span.end}, '(', ')')) return false;
    *name = (span_t){span.start, open};
    *arguments = (span_t){open + 1, span.end - 1};
    return true;
}

static size_t find_operator(const char *source, span_t span, const char *operators)
{
    int paren = 0, bracket = 0, brace = 0;
    for (size_t i = span.end; i-- > span.start;) {
        char c = source[i];
        if (c == ')') paren++;
        else if (c == '(') paren--;
        else if (c == ']') bracket++;
        else if (c == '[') bracket--;
        else if (c == '}') brace++;
        else if (c == '{') brace--;
        if (paren || bracket || brace || strchr(operators, c) == NULL) continue;
        if ((c == '+' || c == '-') && (i == span.start || strchr("+-*/^(=,<>", source[i - 1]) != NULL)) continue;
        return i;
    }
    return SIZE_MAX;
}

static box_t layout_row(layout_context_t *ctx, span_t left_span, span_t op_span,
                        span_t right_span, int x, int y, bool compact, int precedence,
                        int depth)
{
    box_t left = layout_span(ctx, left_span, x, y, compact,
                             precedence > 0 ? precedence - 1 : 0, depth + 1);
    box_t op = layout_text(ctx, op_span, x + left.width + MATH_LAYOUT_GAP, y, compact);
    box_t right = layout_span(ctx, right_span,
        x + left.width + op.width + MATH_LAYOUT_GAP * 2, y, compact, precedence, depth + 1);
    int ascent = left.ascent > op.ascent ? left.ascent : op.ascent;
    if (right.ascent > ascent) ascent = right.ascent;
    int descent = left.descent > op.descent ? left.descent : op.descent;
    if (right.descent > descent) descent = right.descent;
    return box_make(left.width + op.width + right.width + MATH_LAYOUT_GAP * 2, ascent, descent);
}

static box_t layout_fraction(layout_context_t *ctx, span_t numerator, span_t denominator,
                             int x, int y, int depth)
{
    box_t num = measure_span(ctx, numerator, true, 0, depth + 1);
    box_t den = measure_span(ctx, denominator, true, 0, depth + 1);
    int width = (num.width > den.width ? num.width : den.width) + 4;
    int num_y = y - 3 - num.descent;
    int den_y = y + 3 + den.ascent;
    layout_span(ctx, numerator, x + (width - num.width) / 2, num_y, true, 0, depth + 1);
    layout_span(ctx, denominator, x + (width - den.width) / 2, den_y, true, 0, depth + 1);
    draw_line(ctx, x, y, x + width, y);
    if (ctx->cursor < numerator.start) draw_cursor(ctx, x + (width - num.width) / 2, num_y, true);
    else if (ctx->cursor > numerator.end && ctx->cursor < denominator.start) {
        draw_cursor(ctx, x + (width - den.width) / 2, den_y, true);
    } else if (ctx->cursor > denominator.end) draw_cursor(ctx, x + width, den_y, true);
    return box_make(width + 1, box_height(num) + 3, box_height(den) + 3);
}

static box_t layout_power(layout_context_t *ctx, span_t base_span, span_t exponent_span,
                          int x, int y, bool compact, int depth)
{
    box_t base = layout_span(ctx, base_span, x, y, compact, 3, depth + 1);
    box_t exponent = measure_span(ctx, exponent_span, true, 0, depth + 1);
    int exponent_y = y - base.ascent + exponent.descent;
    layout_span(ctx, exponent_span, x + base.width + 1, exponent_y, true, 0, depth + 1);
    if (ctx->cursor > base_span.end && ctx->cursor < exponent_span.start) {
        draw_cursor(ctx, x + base.width + 1, exponent_y, true);
    }
    return box_make(base.width + exponent.width + 1,
                    base.ascent + box_height(exponent) - 2, base.descent);
}

static box_t layout_root(layout_context_t *ctx, span_t value_span, const span_t *index_span,
                         int x, int y, bool compact, int depth)
{
    box_t value = measure_span(ctx, value_span, compact, 0, depth + 1);
    box_t index = box_make(0, 0, 0);
    if (index_span != NULL) index = measure_span(ctx, *index_span, true, 0, depth + 1);
    int index_width = index_span != NULL ? index.width + 1 : 0;
    int radical_x = x + index_width;
    int value_x = radical_x + 10;
    layout_span(ctx, value_span, value_x, y, compact, 0, depth + 1);
    if (index_span != NULL) {
        layout_span(ctx, *index_span, x, y - value.ascent + 2, true, 0, depth + 1);
    }
    int top = y - value.ascent - 2;
    draw_line(ctx, radical_x, y, radical_x + 3, y + value.descent);
    draw_line(ctx, radical_x + 3, y + value.descent, radical_x + 7, top);
    draw_line(ctx, radical_x + 7, top, value_x + value.width + 1, top);
    size_t first = index_span != NULL ? index_span->start : value_span.start;
    if (ctx->cursor < first) draw_cursor(ctx, value_x, y, compact);
    else if (ctx->cursor > value_span.end) draw_cursor(ctx, value_x + value.width, y, compact);
    return box_make(index_width + 11 + value.width, value.ascent + 2,
                    value.descent + 1);
}

static box_t layout_delimited(layout_context_t *ctx, span_t inner, int x, int y,
                              bool compact, char left, char right, int depth)
{
    char left_text[2] = {left, '\0'};
    char right_text[2] = {right, '\0'};
    box_t child = measure_span(ctx, inner, compact, 0, depth + 1);
    int side = measure_text(ctx, left_text, 1, compact);
    draw_text(ctx, x, y, left_text, 1, compact);
    layout_span(ctx, inner, x + side, y, compact, 0, depth + 1);
    draw_text(ctx, x + side + child.width, y, right_text, 1, compact);
    return box_make(child.width + side * 2, child.ascent, child.descent);
}

static box_t layout_matrix(layout_context_t *ctx, span_t body, int x, int y,
                           int depth)
{
    span_t rows[MATH_LAYOUT_MAX_ARGS];
    int row_count = split_arguments(ctx->source, body, rows, MATH_LAYOUT_MAX_ARGS);
    if (row_count <= 0) return layout_delimited(ctx, body, x, y, true, '[', ']', depth);
    int cols = 0;
    if (row_count > MATH_LAYOUT_MAX_MATRIX) {
        ctx->complete = false;
        return layout_delimited(ctx, body, x, y, true, '[', ']', depth);
    }
    span_t cells[MATH_LAYOUT_MAX_MATRIX][MATH_LAYOUT_MAX_MATRIX];
    int col_width[MATH_LAYOUT_MAX_MATRIX] = {0};
    int row_height[MATH_LAYOUT_MAX_MATRIX] = {0};
    memset(cells, 0, sizeof(cells));
    for (int r = 0; r < row_count; r++) {
        span_t row = span_trim(ctx->source, rows[r]);
        if (!matching_delimiter(ctx->source, row, '[', ']')) return layout_delimited(ctx, body, x, y, true, '[', ']', depth);
        int count = split_arguments(ctx->source, (span_t){row.start + 1, row.end - 1},
                                    cells[r], MATH_LAYOUT_MAX_MATRIX);
        if (count <= 0) return layout_delimited(ctx, body, x, y, true, '[', ']', depth);
        if (r == 0) cols = count;
        else if (count != cols) return layout_delimited(ctx, body, x, y, true, '[', ']', depth);
        for (int c = 0; c < cols; c++) {
            box_t cell = measure_span(ctx, cells[r][c], true, 0, depth + 1);
            if (cell.width > col_width[c]) col_width[c] = cell.width;
            if (box_height(cell) > row_height[r]) row_height[r] = box_height(cell);
        }
    }
    int width = 8;
    int height = 4;
    for (int c = 0; c < cols; c++) width += col_width[c] + (c ? 5 : 0);
    for (int r = 0; r < row_count; r++) height += row_height[r] + (r ? 3 : 0);
    int top = y - height / 2;
    int cy = top + 2;
    for (int r = 0; r < row_count; r++) {
        int cx = x + 4;
        int baseline = cy + row_height[r] - 1;
        for (int c = 0; c < cols; c++) {
            box_t cell = measure_span(ctx, cells[r][c], true, 0, depth + 1);
            layout_span(ctx, cells[r][c], cx + (col_width[c] - cell.width) / 2,
                        baseline, true, 0, depth + 1);
            cx += col_width[c] + 5;
        }
        cy += row_height[r] + 3;
    }
    draw_line(ctx, x + 3, top, x, top);
    draw_line(ctx, x, top, x, top + height);
    draw_line(ctx, x, top + height, x + 3, top + height);
    draw_line(ctx, x + width - 4, top, x + width - 1, top);
    draw_line(ctx, x + width - 1, top, x + width - 1, top + height);
    draw_line(ctx, x + width - 1, top + height, x + width - 4, top + height);
    if (ctx->cursor < cells[0][0].start) draw_cursor(ctx, x + 4, top + 8, true);
    else if (ctx->cursor > cells[row_count - 1][cols - 1].end) {
        draw_cursor(ctx, x + width - 5, top + height - 2, true);
    }
    return box_make(width, height / 2, height - height / 2);
}

static box_t layout_piecewise(layout_context_t *ctx, span_t *args, int count,
                              int x, int y, int depth)
{
    int rows = (count + 1) / 2;
    int value_width = 0, condition_width = 0;
    for (int r = 0; r < rows; r++) {
        box_t value = measure_span(ctx, args[r * 2], true, 0, depth + 1);
        if (value.width > value_width) value_width = value.width;
        if (r * 2 + 1 < count) {
            box_t condition = measure_span(ctx, args[r * 2 + 1], true, 0, depth + 1);
            if (condition.width > condition_width) condition_width = condition.width;
        }
    }
    int row_height = 10;
    int height = rows * row_height;
    int top = y - height / 2;
    draw_line(ctx, x + 5, top, x + 2, top + 3);
    draw_line(ctx, x + 2, top + 3, x + 2, y - 2);
    draw_line(ctx, x + 2, y - 2, x, y);
    draw_line(ctx, x, y, x + 2, y + 2);
    draw_line(ctx, x + 2, y + 2, x + 2, top + height - 3);
    draw_line(ctx, x + 2, top + height - 3, x + 5, top + height);
    for (int r = 0; r < rows; r++) {
        int baseline = top + r * row_height + 7;
        layout_span(ctx, args[r * 2], x + 9, baseline, true, 0, depth + 1);
        if (r * 2 + 1 < count) {
            layout_label(ctx, "if", x + 13 + value_width, baseline, true);
            layout_span(ctx, args[r * 2 + 1], x + 28 + value_width, baseline,
                        true, 0, depth + 1);
        }
    }
    if (ctx->cursor < args[0].start) draw_cursor(ctx, x + 9, top + 7, true);
    else if (ctx->cursor > args[count - 1].end) {
        draw_cursor(ctx, x + 9 + value_width + condition_width, top + height - 3, true);
    }
    return box_make(30 + value_width + condition_width, height / 2, height - height / 2);
}

static box_t layout_limit(layout_context_t *ctx, span_t *args, int count,
                          int x, int y, int depth)
{
    box_t expression = measure_span(ctx, args[0], true, 0, depth + 1);
    int lim_width = measure_text(ctx, "lim", 3, true);
    int label_width = lim_width;
    if (count >= 3) {
        box_t variable = measure_span(ctx, args[1], true, 0, depth + 1);
        box_t point = measure_span(ctx, args[2], true, 0, depth + 1);
        int under_width = variable.width + point.width + 10;
        if (under_width > label_width) label_width = under_width;
        layout_span(ctx, args[1], x, y + 8, true, 0, depth + 1);
        layout_label(ctx, "->", x + variable.width + 1, y + 8, true);
        layout_span(ctx, args[2], x + variable.width + 10, y + 8, true, 0, depth + 1);
    }
    layout_label(ctx, "lim", x + (label_width - lim_width) / 2, y, true);
    layout_span(ctx, args[0], x + label_width + 4, y, true, 0, depth + 1);
    if (ctx->cursor < args[0].start) draw_cursor(ctx, x + label_width + 4, y, true);
    else if (ctx->cursor > args[count - 1].end) {
        draw_cursor(ctx, x + label_width + 4 + expression.width, y, true);
    }
    return box_make(label_width + 4 + expression.width, expression.ascent,
                    count >= 3 ? 10 : expression.descent);
}

static box_t layout_integral(layout_context_t *ctx, span_t *args, int count,
                             int x, int y, int depth, bool definite)
{
    box_t expression = measure_span(ctx, args[0], true, 0, depth + 1);
    int symbol_width = definite ? 16 : 10;
    int top = y - expression.ascent - 3;
    int bottom = y + expression.descent + 3;
    draw_line(ctx, x + 7, top, x + 4, top + 2);
    draw_line(ctx, x + 4, top + 2, x + 4, bottom - 2);
    draw_line(ctx, x + 4, bottom - 2, x + 1, bottom);
    if (definite && count >= 4) {
        layout_span(ctx, args[3], x + 8, top + 5, true, 0, depth + 1);
        layout_span(ctx, args[2], x, bottom - 1, true, 0, depth + 1);
    }
    layout_span(ctx, args[0], x + symbol_width, y, true, 0, depth + 1);
    int dx = x + symbol_width + expression.width + 3;
    box_t d = layout_label(ctx, "d", dx, y, true);
    int variable_width = 0;
    if (count >= 2) {
        box_t variable = layout_span(ctx, args[1], dx + d.width, y, true, 0, depth + 1);
        variable_width = variable.width;
    }
    if (ctx->cursor < args[0].start) draw_cursor(ctx, x + symbol_width, y, true);
    else if (ctx->cursor > args[count - 1].end) {
        draw_cursor(ctx, dx + d.width + variable_width, y, true);
    }
    return box_make(symbol_width + expression.width + 3 + d.width + variable_width,
                    expression.ascent + 3, expression.descent + 3);
}

static box_t layout_function(layout_context_t *ctx, span_t span, span_t name,
                             span_t argument_span, int x, int y, bool compact, int depth)
{
    span_t args[MATH_LAYOUT_MAX_ARGS];
    int count = split_arguments(ctx->source, argument_span, args, MATH_LAYOUT_MAX_ARGS);
    if (count < 0) { ctx->complete = false; return layout_text(ctx, span, x, y, compact); }
    if (span_equals(ctx->source, name, "frac") && count == 2) {
        return layout_fraction(ctx, args[0], args[1], x, y, depth);
    }
    if (span_equals(ctx->source, name, "sqrt") && count == 1) {
        return layout_root(ctx, args[0], NULL, x, y, compact, depth);
    }
    if ((span_equals(ctx->source, name, "nroot") || span_equals(ctx->source, name, "root")) && count == 2) {
        return layout_root(ctx, args[1], &args[0], x, y, compact, depth);
    }
    if ((span_equals(ctx->source, name, "matrix") || span_equals(ctx->source, name, "mat")) &&
        count == 1 && matching_delimiter(ctx->source, args[0], '[', ']')) {
        return layout_matrix(ctx, (span_t){args[0].start + 1, args[0].end - 1}, x, y, depth);
    }
    if ((span_equals(ctx->source, name, "piecewise") || span_equals(ctx->source, name, "when")) && count > 0) {
        return layout_piecewise(ctx, args, count, x, y, depth);
    }
    if (span_equals(ctx->source, name, "limit") && count >= 1) {
        return layout_limit(ctx, args, count, x, y, depth);
    }
    bool definite = span_equals(ctx->source, name, "defint");
    if ((definite || span_equals(ctx->source, name, "int") ||
         span_equals(ctx->source, name, "integrate")) && count >= 1) {
        return layout_integral(ctx, args, count, x, y, depth, definite);
    }

    box_t name_box = layout_text(ctx, name, x, y, compact);
    int side = measure_text(ctx, "(", 1, compact);
    draw_text(ctx, x + name_box.width, y, "(", 1, compact);
    int cx = x + name_box.width + side;
    int ascent = name_box.ascent, descent = name_box.descent;
    for (int i = 0; i < count; i++) {
        box_t arg = layout_span(ctx, args[i], cx, y, compact, 0, depth + 1);
        cx += arg.width;
        if (arg.ascent > ascent) ascent = arg.ascent;
        if (arg.descent > descent) descent = arg.descent;
        if (i + 1 < count) {
            draw_text(ctx, cx, y, ",", 1, compact);
            cx += measure_text(ctx, ",", 1, compact) + 1;
        }
    }
    draw_text(ctx, cx, y, ")", 1, compact);
    cx += side;
    return box_make(cx - x, ascent, descent);
}

static box_t layout_span(layout_context_t *ctx, span_t span, int x, int y,
                         bool compact, int precedence, int depth)
{
    span = span_trim(ctx->source, span);
    if (span.start >= span.end) return layout_label(ctx, " ", x, y, compact);
    if (depth > MATH_LAYOUT_MAX_DEPTH) {
        ctx->complete = false;
        return layout_text(ctx, span, x, y, compact);
    }

    if (matching_delimiter(ctx->source, span, '(', ')')) {
        return layout_delimited(ctx, (span_t){span.start + 1, span.end - 1},
                                x, y, compact, '(', ')', depth);
    }
    if (matching_delimiter(ctx->source, span, '[', ']')) {
        span_t body = {span.start + 1, span.end - 1};
        span_t trimmed = span_trim(ctx->source, body);
        if (trimmed.start < trimmed.end && ctx->source[trimmed.start] == '[') {
            return layout_matrix(ctx, body, x, y, depth);
        }
        return layout_delimited(ctx, body, x, y, compact, '[', ']', depth);
    }
    if (matching_delimiter(ctx->source, span, '{', '}')) {
        return layout_delimited(ctx, (span_t){span.start + 1, span.end - 1},
                                x, y, compact, '{', '}', depth);
    }

    static const char *levels[] = {"=<>!", "+-", "*/", "^"};
    for (int level = precedence; level < 4; level++) {
        size_t op = find_operator(ctx->source, span, levels[level]);
        if (op == SIZE_MAX) continue;
        size_t operator_length = 1;
        if (level == 0 && ctx->source[op] == '=' && op > span.start &&
            strchr("<>!", ctx->source[op - 1]) != NULL) {
            op--;
            operator_length = 2;
        }
        span_t left = {span.start, op};
        span_t right = {op + operator_length, span.end};
        if (level == 2 && ctx->source[op] == '/') {
            return layout_fraction(ctx, left, right, x, y, depth);
        }
        if (level == 3) {
            if (matching_delimiter(ctx->source, right, '(', ')')) {
                right.start++;
                right.end--;
            }
            return layout_power(ctx, left, right, x, y, compact, depth);
        }
        return layout_row(ctx, left, (span_t){op, op + operator_length}, right, x, y,
                          compact, level + 1, depth);
    }

    span_t name, arguments;
    if (function_span(ctx->source, span, &name, &arguments)) {
        return layout_function(ctx, span, name, arguments, x, y, compact, depth);
    }
    return layout_text(ctx, span, x, y, compact);
}

opencalc_math_layout_result_t opencalc_math_layout(
    const char *expression, size_t cursor, int x, int y, bool compact,
    uint32_t color, bool draw, const opencalc_math_layout_callbacks_t *callbacks)
{
    opencalc_math_layout_result_t result = {
        .width = 0, .ascent = compact ? 6 : 12, .descent = compact ? 1 : 2,
        .cursor_x = 0, .cursor_y = 0, .cursor_valid = false, .complete = false,
    };
    if (expression == NULL || callbacks == NULL || callbacks->measure == NULL ||
        callbacks->text == NULL || callbacks->line == NULL || callbacks->cursor == NULL) return result;

    layout_context_t context = {
        .source = expression,
        .cursor = cursor,
        .color = color,
        .draw = draw,
        .cursor_drawn = false,
        .cursor_x = 0,
        .cursor_y = 0,
        .complete = true,
        .cb = callbacks,
    };
    span_t span = {0, strlen(expression)};
    box_t box = layout_span(&context, span, x, y, compact, 0, 0);
    if (cursor != SIZE_MAX && !context.cursor_drawn) {
        int cursor_x = cursor == 0 ? x : x + box.width;
        context.cursor_x = cursor_x;
        context.cursor_y = y - (compact ? 7 : 13);
        context.cursor_drawn = true;
        if (draw) {
            callbacks->cursor(cursor_x, context.cursor_y, compact ? 9 : 16,
                              compact, callbacks->context);
        }
    }
    result.width = box.width;
    result.ascent = box.ascent;
    result.descent = box.descent;
    result.cursor_x = context.cursor_x;
    result.cursor_y = context.cursor_y;
    result.cursor_valid = context.cursor_drawn;
    result.complete = context.complete;
    return result;
}
