#include "opencalc_graph_series.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEQUENCE_MAX_INDEX 4096

static void trim_copy(char *out, size_t out_size, const char *start, size_t length)
{
    while (length > 0 && isspace((unsigned char)*start)) {
        start++;
        length--;
    }
    while (length > 0 && isspace((unsigned char)start[length - 1])) length--;
    if (length >= out_size) length = out_size - 1;
    memcpy(out, start, length);
    out[length] = '\0';
}

bool opencalc_graph_sequence_parse(const char *text, opencalc_graph_sequence_t *sequence)
{
    if (text == NULL || sequence == NULL) return false;
    memset(sequence, 0, sizeof(*sequence));
    snprintf(sequence->expression, sizeof(sequence->expression), "%s", text);
    sequence->kind = OPENCALC_GRAPH_SEQUENCE_DIRECT;

    while (isspace((unsigned char)*text)) text++;
    if (strncmp(text, "rec(", 4) != 0) return sequence->expression[0] != '\0';

    const char *body = text + 4;
    const char *comma[2] = {NULL, NULL};
    int depth = 0;
    for (const char *cursor = body; *cursor != '\0'; cursor++) {
        if (*cursor == '(' || *cursor == '[' || *cursor == '{') depth++;
        else if (*cursor == ')' || *cursor == ']' || *cursor == '}') {
            if (*cursor == ')' && depth == 0) {
                if (comma[0] == NULL || comma[1] == NULL) return false;
                const char *tail = cursor + 1;
                while (isspace((unsigned char)*tail)) tail++;
                if (*tail != '\0') return false;
                trim_copy(sequence->expression, sizeof(sequence->expression), body,
                          (size_t)(comma[0] - body));
                trim_copy(sequence->initial0, sizeof(sequence->initial0), comma[0] + 1,
                          (size_t)(comma[1] - comma[0] - 1));
                trim_copy(sequence->initial1, sizeof(sequence->initial1), comma[1] + 1,
                          (size_t)(cursor - comma[1] - 1));
                sequence->kind = OPENCALC_GRAPH_SEQUENCE_RECURRENCE;
                return sequence->expression[0] != '\0' && sequence->initial0[0] != '\0' &&
                    sequence->initial1[0] != '\0';
            }
            if (depth <= 0) return false;
            depth--;
        } else if (*cursor == ',' && depth == 0) {
            if (comma[0] == NULL) comma[0] = cursor;
            else if (comma[1] == NULL) comma[1] = cursor;
            else return false;
        }
    }
    return false;
}

static bool append_text(char *out, size_t out_size, size_t *used, const char *text)
{
    size_t length = strlen(text);
    if (length >= out_size - *used) return false;
    memcpy(out + *used, text, length);
    *used += length;
    out[*used] = '\0';
    return true;
}

static bool recurrence_expand(const char *expression, double previous, double previous2,
                              char *out, size_t out_size)
{
    char prev[32];
    char prev2[32];
    snprintf(prev, sizeof(prev), "(%.17g)", previous);
    snprintf(prev2, sizeof(prev2), "(%.17g)", previous2);
    size_t used = 0;
    out[0] = '\0';
    for (const char *cursor = expression; *cursor != '\0';) {
        const char *replacement = NULL;
        size_t token_length = 0;
        if (strncmp(cursor, "u(n-1)", 6) == 0) {
            replacement = prev;
            token_length = 6;
        } else if (strncmp(cursor, "u(n-2)", 6) == 0) {
            replacement = prev2;
            token_length = 6;
        }
        if (replacement != NULL) {
            if (!append_text(out, out_size, &used, replacement)) return false;
            cursor += token_length;
        } else {
            char one[2] = {*cursor++, '\0'};
            if (!append_text(out, out_size, &used, one)) return false;
        }
    }
    return true;
}

bool opencalc_graph_sequence_eval(const opencalc_graph_sequence_t *sequence, int n,
                                  opencalc_graph_scalar_eval_fn evaluate, double *value)
{
    if (sequence == NULL || evaluate == NULL || value == NULL || n < 0 || n > SEQUENCE_MAX_INDEX) {
        return false;
    }
    if (sequence->kind == OPENCALC_GRAPH_SEQUENCE_DIRECT) {
        return evaluate(sequence->expression, 'n', (double)n, value) && isfinite(*value);
    }

    double previous2 = 0.0;
    double previous = 0.0;
    if (!evaluate(sequence->initial0, 'n', 0.0, &previous2) || !isfinite(previous2)) return false;
    if (n == 0) {
        *value = previous2;
        return true;
    }
    if (!evaluate(sequence->initial1, 'n', 1.0, &previous) || !isfinite(previous)) return false;
    if (n == 1) {
        *value = previous;
        return true;
    }

    char expanded[256];
    for (int index = 2; index <= n; index++) {
        if (!recurrence_expand(sequence->expression, previous, previous2,
                               expanded, sizeof(expanded))) return false;
        double next = 0.0;
        if (!evaluate(expanded, 'n', (double)index, &next) || !isfinite(next)) return false;
        previous2 = previous;
        previous = next;
    }
    *value = previous;
    return true;
}

const char *opencalc_graph_sequence_end_name(opencalc_graph_sequence_end_t behavior)
{
    switch (behavior) {
    case OPENCALC_GRAPH_SEQUENCE_END_INCREASING: return "increasing";
    case OPENCALC_GRAPH_SEQUENCE_END_DECREASING: return "decreasing";
    case OPENCALC_GRAPH_SEQUENCE_END_OSCILLATING: return "oscillating";
    case OPENCALC_GRAPH_SEQUENCE_END_DIVERGING: return "diverging";
    default: return "steady";
    }
}

static void sequence_analysis_add(opencalc_graph_sequence_analysis_t *analysis,
                                  int index, double value, double tail[8], int *tail_count)
{
    if (analysis->evaluated == 0) analysis->first = value;
    analysis->previous = analysis->last;
    analysis->last = value;
    analysis->sum += value;
    analysis->evaluated++;
    if (fabs(value) <= 1e-9 &&
        analysis->zero_count < OPENCALC_GRAPH_SEQUENCE_ZERO_LIMIT) {
        analysis->zero_indices[analysis->zero_count++] = index;
    }
    if (*tail_count < 8) tail[(*tail_count)++] = value;
    else {
        memmove(tail, tail + 1, 7 * sizeof(double));
        tail[7] = value;
    }
}

bool opencalc_graph_sequence_analyze(const opencalc_graph_sequence_t *sequence,
                                     int first_index, int last_index,
                                     opencalc_graph_scalar_eval_fn evaluate,
                                     opencalc_graph_sequence_analysis_t *analysis)
{
    if (sequence == NULL || evaluate == NULL || analysis == NULL ||
        first_index < 0 || first_index > last_index || last_index > SEQUENCE_MAX_INDEX) {
        return false;
    }
    memset(analysis, 0, sizeof(*analysis));
    analysis->first_index = first_index;
    analysis->last_index = last_index;
    double tail[8] = {0};
    int tail_count = 0;

    if (sequence->kind == OPENCALC_GRAPH_SEQUENCE_DIRECT) {
        for (int index = first_index; index <= last_index; index++) {
            double value = 0.0;
            if (!evaluate(sequence->expression, 'n', (double)index, &value) ||
                !isfinite(value)) return false;
            sequence_analysis_add(analysis, index, value, tail, &tail_count);
        }
    } else {
        double previous2 = 0.0;
        double previous = 0.0;
        if (!evaluate(sequence->initial0, 'n', 0.0, &previous2) || !isfinite(previous2) ||
            !evaluate(sequence->initial1, 'n', 1.0, &previous) || !isfinite(previous)) return false;
        char expanded[256];
        for (int index = 0; index <= last_index; index++) {
            double value = 0.0;
            if (index == 0) value = previous2;
            else if (index == 1) value = previous;
            else {
                if (!recurrence_expand(sequence->expression, previous, previous2,
                                       expanded, sizeof(expanded)) ||
                    !evaluate(expanded, 'n', (double)index, &value) || !isfinite(value)) {
                    return false;
                }
                previous2 = previous;
                previous = value;
            }
            if (index >= first_index) {
                sequence_analysis_add(analysis, index, value, tail, &tail_count);
            }
        }
    }

    if (analysis->evaluated == 0) return false;
    analysis->delta = analysis->evaluated > 1 ? analysis->last - analysis->previous : 0.0;
    double scale = fmax(1.0, fabs(analysis->last));
    bool steady = tail_count >= 4;
    bool magnitude_rising = tail_count >= 4;
    int sign_changes = 0;
    for (int i = 1; i < tail_count; i++) {
        steady &= fabs(tail[i] - tail[i - 1]) <= 1e-7 * scale;
        magnitude_rising &= fabs(tail[i]) >= fabs(tail[i - 1]) * (1.0 - 1e-9);
        if ((tail[i] < 0.0) != (tail[i - 1] < 0.0) &&
            fabs(tail[i]) > 1e-12 && fabs(tail[i - 1]) > 1e-12) sign_changes++;
    }
    if (steady) analysis->end_behavior = OPENCALC_GRAPH_SEQUENCE_END_STEADY;
    else if (magnitude_rising && fabs(analysis->last) >
             fmax(1e6, 8.0 * fmax(1.0, fabs(analysis->first)))) {
        analysis->end_behavior = OPENCALC_GRAPH_SEQUENCE_END_DIVERGING;
    } else if (sign_changes >= 3) {
        analysis->end_behavior = OPENCALC_GRAPH_SEQUENCE_END_OSCILLATING;
    } else if (analysis->delta > 1e-9 * scale) {
        analysis->end_behavior = OPENCALC_GRAPH_SEQUENCE_END_INCREASING;
    } else if (analysis->delta < -1e-9 * scale) {
        analysis->end_behavior = OPENCALC_GRAPH_SEQUENCE_END_DECREASING;
    } else {
        analysis->end_behavior = OPENCALC_GRAPH_SEQUENCE_END_STEADY;
    }
    return true;
}

bool opencalc_graph_segment_is_continuous(double previous_x, double previous_y,
                                          double midpoint_x, double midpoint_y,
                                          double current_x, double current_y,
                                          double visible_x_span, double visible_y_span)
{
    if (!isfinite(previous_x) || !isfinite(previous_y) || !isfinite(midpoint_x) ||
        !isfinite(midpoint_y) || !isfinite(current_x) || !isfinite(current_y) ||
        visible_x_span <= 0.0 || visible_y_span <= 0.0) return false;
    double chord_x = (previous_x + current_x) * 0.5;
    double chord_y = (previous_y + current_y) * 0.5;
    double normalized_error = fabs(midpoint_x - chord_x) / visible_x_span +
        fabs(midpoint_y - chord_y) / visible_y_span;
    double normalized_jump = fabs(current_x - previous_x) / visible_x_span +
        fabs(current_y - previous_y) / visible_y_span;
    return normalized_error < 0.35 && normalized_jump < 1.5;
}
