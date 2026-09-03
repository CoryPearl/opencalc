#include "opencalc_eigenmath.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

#define EIGENMATH_INPUT_MAX 768
#define EIGENMATH_LIMIT_DERIVATIVES 8

void run(char *buf);
void eigenmath_set_output_callback(void (*callback)(const char *, void *), void *user_data);

typedef struct {
    char *data;
    size_t size;
    size_t used;
    bool truncated;
    bool stopped;
} eigenmath_output_t;

static bool s_initialized;

#ifdef ESP_PLATFORM
static SemaphoreHandle_t s_mutex;

static bool lock_engine(void)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
    }
    return s_mutex != NULL && xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE;
}

static void unlock_engine(void)
{
    xSemaphoreGive(s_mutex);
}
#else
static bool lock_engine(void) { return true; }
static void unlock_engine(void) {}
#endif

static void capture_output(const char *text, void *user_data)
{
    eigenmath_output_t *output = user_data;
    if (text == NULL || output == NULL) return;
    if (strstr(text, "Stop:") != NULL) output->stopped = true;

    size_t length = strlen(text);
    size_t available = output->size > output->used ? output->size - output->used - 1 : 0;
    if (length > available) {
        length = available;
        output->truncated = true;
    }
    if (length > 0) {
        memcpy(output->data + output->used, text, length);
        output->used += length;
        output->data[output->used] = '\0';
    }
}

static void trim_output(char *text)
{
    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) text[--length] = '\0';
    size_t start = 0;
    while (isspace((unsigned char)text[start])) start++;
    if (start > 0) memmove(text, text + start, strlen(text + start) + 1);
}

static int find_top_level(const char *text, char target)
{
    int depth = 0;
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == '(' || text[i] == '[') depth++;
        else if (text[i] == ')' || text[i] == ']') depth--;
        else if (text[i] == target && depth == 0) return i;
    }
    return -1;
}

static bool outer_parentheses_wrap_expression(const char *text)
{
    size_t length = strlen(text);
    if (length < 2 || text[0] != '(' || text[length - 1] != ')') return false;
    int depth = 0;
    for (size_t i = 0; i < length; i++) {
        if (text[i] == '(' || text[i] == '[') depth++;
        else if (text[i] == ')' || text[i] == ']') depth--;
        if (depth == 0 && i + 1 < length) return false;
        if (depth < 0) return false;
    }
    return depth == 0;
}

static void strip_outer_parentheses(char *text)
{
    while (outer_parentheses_wrap_expression(text)) {
        size_t length = strlen(text);
        memmove(text, text + 1, length - 2);
        text[length - 2] = '\0';
    }
}

static bool output_contains_symbol(const char *text, char symbol)
{
    symbol = (char)tolower((unsigned char)symbol);
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (tolower((unsigned char)text[i]) != symbol) continue;
        bool left_name = i > 0 && (isalnum((unsigned char)text[i - 1]) || text[i - 1] == '_');
        bool right_name = isalnum((unsigned char)text[i + 1]) || text[i + 1] == '_';
        if (!left_name && !right_name) return true;
    }
    return false;
}

static bool take_wrapped(const char *expression, const char *name, char *inner, size_t inner_size)
{
    while (isspace((unsigned char)*expression)) expression++;
    size_t name_length = strlen(name);
    if (strlen(expression) < name_length) return false;
    for (size_t i = 0; i < name_length; i++) {
        if (tolower((unsigned char)expression[i]) != tolower((unsigned char)name[i])) return false;
    }
    const char *open = expression + name_length;
    while (isspace((unsigned char)*open)) open++;
    if (*open != '(') return false;
    const char *end = expression + strlen(expression);
    while (end > open && isspace((unsigned char)end[-1])) end--;
    if (end <= open + 1 || end[-1] != ')') return false;
    size_t length = (size_t)((end - 1) - (open + 1));
    if (length + 1 > inner_size) return false;
    memcpy(inner, open + 1, length);
    inner[length] = '\0';
    return true;
}

static bool expression_has_symbolic_variable(const char *expression)
{
    for (size_t i = 0; expression[i] != '\0';) {
        if (!isalpha((unsigned char)expression[i])) {
            i++;
            continue;
        }
        size_t start = i;
        while (isalnum((unsigned char)expression[i]) || expression[i] == '_') i++;
        size_t length = i - start;
        while (isspace((unsigned char)expression[i])) i++;
        if (expression[i] != '(' && length == 1) {
            char symbol = (char)tolower((unsigned char)expression[start]);
            if (symbol >= 'a' && symbol <= 'z' && symbol != 'i' && symbol != 'e') return true;
        }
    }
    return false;
}

static bool contains_named_call(const char *expression, const char *name)
{
    size_t name_length = strlen(name);
    for (size_t i = 0; expression[i] != '\0';) {
        if (!isalpha((unsigned char)expression[i]) && expression[i] != '_') {
            i++;
            continue;
        }
        size_t start = i++;
        while (isalnum((unsigned char)expression[i]) || expression[i] == '_') i++;
        size_t length = i - start;
        size_t next = i;
        while (isspace((unsigned char)expression[next])) next++;
        if (expression[next] != '(' || length != name_length) continue;
        bool matches = true;
        for (size_t j = 0; j < length; j++) {
            if (tolower((unsigned char)expression[start + j]) !=
                tolower((unsigned char)name[j])) {
                matches = false;
                break;
            }
        }
        if (matches) return true;
    }
    return false;
}

static bool normalize_alias_calls(const char *expression, char *out, size_t out_size)
{
    size_t used = 0;
    for (size_t i = 0; expression[i] != '\0';) {
        if (!isalpha((unsigned char)expression[i]) && expression[i] != '_') {
            if (used + 1 >= out_size) return false;
            out[used++] = expression[i++];
            continue;
        }

        size_t start = i++;
        while (isalnum((unsigned char)expression[i]) || expression[i] == '_') i++;
        size_t length = i - start;
        size_t next = i;
        while (isspace((unsigned char)expression[next])) next++;
        const char *replacement = NULL;
        if (expression[next] == '(') {
            if (length == 5 && strncasecmp(expression + start, "gamma", length) == 0) replacement = "tgamma";
            else if (length == 5 && strncasecmp(expression + start, "deriv", length) == 0) replacement = "d";
            else if (length == 4 && strncasecmp(expression + start, "diff", length) == 0) replacement = "d";
            else if (length == 3 && strncasecmp(expression + start, "int", length) == 0) replacement = "integral";
            else if (length == 9 && strncasecmp(expression + start, "integrate", length) == 0) replacement = "integral";
        }
        size_t write_length = replacement != NULL ? strlen(replacement) : length;
        if (used + write_length >= out_size) return false;
        memcpy(out + used, replacement != NULL ? replacement : expression + start, write_length);
        used += write_length;
    }
    if (used >= out_size) return false;
    out[used] = '\0';
    return true;
}

static bool translate_solve(const char *inner, char *translated, size_t translated_size)
{
    char work[EIGENMATH_INPUT_MAX];
    if (snprintf(work, sizeof(work), "%s", inner) >= (int)sizeof(work)) return false;

    char variable = 'x';
    int comma = find_top_level(work, ',');
    if (comma >= 0) {
        char *argument = work + comma + 1;
        while (isspace((unsigned char)*argument)) argument++;
        if (!isalpha((unsigned char)argument[0])) return false;
        variable = (char)tolower((unsigned char)argument[0]);
        argument++;
        while (isspace((unsigned char)*argument)) argument++;
        if (*argument != '\0') return false;
        work[comma] = '\0';
    }

    int equal = find_top_level(work, '=');
    int written;
    if (equal >= 0) {
        work[equal] = '\0';
        written = snprintf(translated, translated_size, "roots((%s)-(%s),%c)", work, work + equal + 1, variable);
    } else {
        written = snprintf(translated, translated_size, "roots(%s,%c)", work, variable);
    }
    return written > 0 && (size_t)written < translated_size;
}

static bool translate_expression(const char *expression, char *translated, size_t translated_size)
{
    char normalized[EIGENMATH_INPUT_MAX];
    char inner[EIGENMATH_INPUT_MAX];
    if (!normalize_alias_calls(expression, normalized, sizeof(normalized))) return false;
    expression = normalized;
    if (take_wrapped(expression, "expand", inner, sizeof(inner))) {
        int written = snprintf(translated, translated_size, "%s", inner);
        return written > 0 && (size_t)written < translated_size;
    }
    if (take_wrapped(expression, "deriv", inner, sizeof(inner))) {
        int written = snprintf(translated, translated_size, "d(%s)", inner);
        return written > 0 && (size_t)written < translated_size;
    }
    if (take_wrapped(expression, "diff", inner, sizeof(inner))) {
        int written = snprintf(translated, translated_size, "d(%s)", inner);
        return written > 0 && (size_t)written < translated_size;
    }
    if (take_wrapped(expression, "int", inner, sizeof(inner))) {
        int written = snprintf(translated, translated_size, "integral(%s)", inner);
        return written > 0 && (size_t)written < translated_size;
    }
    if (take_wrapped(expression, "integrate", inner, sizeof(inner))) {
        int written = snprintf(translated, translated_size, "integral(%s)", inner);
        return written > 0 && (size_t)written < translated_size;
    }
    if (take_wrapped(expression, "gamma", inner, sizeof(inner))) {
        int written = snprintf(translated, translated_size, "tgamma(%s)", inner);
        return written > 0 && (size_t)written < translated_size;
    }
    if (take_wrapped(expression, "solve", inner, sizeof(inner))) {
        return translate_solve(inner, translated, translated_size);
    }

    static const char *const symbolic_functions[] = {
        "abs", "adj", "arccos", "arccosh", "arcsin", "arcsinh", "arctan", "arctanh",
        "arg", "binding", "ceiling", "check", "circexp", "clock", "cofactor", "conj",
        "contract", "cos", "cosh", "d", "defint", "denominator", "derivative", "det",
        "dim", "dot", "eigenvec", "erf", "erfc", "eval", "exp", "expcos", "expcosh",
        "expform", "expsin", "expsinh", "exptan", "exptanh", "factorial", "fdist",
        "float", "floor", "hadamard", "imag", "incbeta", "infixform", "inner",
        "integral", "inv", "kronecker", "lgamma", "log", "logform", "mag", "minor",
        "minormatrix", "mod", "noexpand", "nroots", "number", "numerator", "outer",
        "polar", "prefixform", "product", "quote", "rank", "rationalize", "real", "rect",
        "roots", "rotate", "sgn", "simplify", "sin", "sinh", "sqrt", "sum", "tan",
        "tanh", "taylor", "tdist", "tdistinv", "test", "testeq", "testge", "testgt",
        "testle", "testlt", "tgamma", "transpose", "unit", "zero"
    };
    for (size_t i = 0; i < sizeof(symbolic_functions) / sizeof(symbolic_functions[0]); i++) {
        if (take_wrapped(expression, symbolic_functions[i], inner, sizeof(inner))) {
            int written = snprintf(translated, translated_size, "%s(%s)", symbolic_functions[i], inner);
            return written > 0 && (size_t)written < translated_size;
        }
    }

    for (size_t i = 0; i < sizeof(symbolic_functions) / sizeof(symbolic_functions[0]); i++) {
        if (contains_named_call(expression, symbolic_functions[i])) {
            int written = snprintf(translated, translated_size, "%s", expression);
            return written > 0 && (size_t)written < translated_size;
        }
    }

    if (!expression_has_symbolic_variable(expression)) return false;

    int equal = find_top_level(expression, '=');
    if (equal > 0) {
        size_t start = 0;
        while (isspace((unsigned char)expression[start])) start++;
        size_t end = (size_t)equal;
        while (end > start && isspace((unsigned char)expression[end - 1])) end--;
        bool simple_name = end > start && (isalpha((unsigned char)expression[start]) || expression[start] == '_');
        for (size_t i = start + 1; simple_name && i < end; i++) {
            simple_name = isalnum((unsigned char)expression[i]) || expression[i] == '_';
        }
        if (simple_name) {
            int written = snprintf(translated, translated_size, "%s\n%.*s", expression,
                                   (int)(end - start), expression + start);
            return written > 0 && (size_t)written < translated_size;
        }
    }

    int written = snprintf(translated, translated_size, "%s", expression);
    return written > 0 && (size_t)written < translated_size;
}

static bool run_capture_locked(const char *command, char *out, size_t out_size,
                               bool *stopped)
{
    char mutable_command[EIGENMATH_INPUT_MAX];
    if (snprintf(mutable_command, sizeof(mutable_command), "%s", command) >=
        (int)sizeof(mutable_command)) {
        snprintf(out, out_size, "CAS input too long");
        if (stopped != NULL) *stopped = true;
        return true;
    }

    out[0] = '\0';
    eigenmath_output_t output = {.data = out, .size = out_size};
    eigenmath_set_output_callback(capture_output, &output);
    run(mutable_command);
    eigenmath_set_output_callback(NULL, NULL);
    trim_output(out);

    if (stopped != NULL) *stopped = output.stopped;
    if (output.stopped) {
        char *message = strstr(out, "Stop:");
        if (message != NULL && message != out) memmove(out, message, strlen(message) + 1);
    } else if (output.truncated) {
        snprintf(out, out_size, "CAS output too long");
    }
    return out[0] != '\0';
}

static bool initialize_engine_locked(void)
{
    if (s_initialized) return true;
    char ignored[16];
    bool stopped = false;
    (void)run_capture_locked("tty=1", ignored, sizeof(ignored), &stopped);
    if (stopped) return false;
    s_initialized = true;
    return true;
}

static bool evaluate_limit_locked(const char *expression, char *out, size_t out_size)
{
    char inner[EIGENMATH_INPUT_MAX];
    if (!take_wrapped(expression, "limit", inner, sizeof(inner))) return false;

    int first_comma = find_top_level(inner, ',');
    if (first_comma < 0) {
        snprintf(out, out_size, "limit needs expr,var,point");
        return true;
    }
    inner[first_comma] = '\0';
    char *variable_text = inner + first_comma + 1;
    int second_comma = find_top_level(variable_text, ',');
    if (second_comma < 0) {
        snprintf(out, out_size, "limit needs expr,var,point");
        return true;
    }
    variable_text[second_comma] = '\0';
    char *point = variable_text + second_comma + 1;
    int direction_comma = find_top_level(point, ',');
    if (direction_comma >= 0) {
        snprintf(out, out_size, "one-sided limit unsupported");
        return true;
    }
    while (isspace((unsigned char)*variable_text)) variable_text++;
    while (isspace((unsigned char)*point)) point++;
    size_t variable_length = strlen(variable_text);
    while (variable_length > 0 && isspace((unsigned char)variable_text[variable_length - 1])) {
        variable_text[--variable_length] = '\0';
    }
    if (variable_length != 1 || !isalpha((unsigned char)variable_text[0]) || *point == '\0') {
        snprintf(out, out_size, "invalid limit variable/point");
        return true;
    }
    char variable = (char)tolower((unsigned char)variable_text[0]);

    char normalized_inner[EIGENMATH_INPUT_MAX];
    if (!normalize_alias_calls(inner, normalized_inner, sizeof(normalized_inner))) {
        snprintf(out, out_size, "CAS input too long");
        return true;
    }
    snprintf(inner, sizeof(inner), "%s", normalized_inner);

    char command[EIGENMATH_INPUT_MAX];
    int written = snprintf(command, sizeof(command), "eval(simplify(%s),%c,%s)",
                           inner, variable, point);
    if (written <= 0 || (size_t)written >= sizeof(command)) {
        snprintf(out, out_size, "CAS input too long");
        return true;
    }

    bool stopped = false;
    if (run_capture_locked(command, out, out_size, &stopped) &&
        !stopped && !output_contains_symbol(out, variable)) {
        return true;
    }

    strip_outer_parentheses(inner);
    int slash = find_top_level(inner, '/');
    if (slash < 0) {
        snprintf(out, out_size, "limit unresolved");
        return true;
    }
    inner[slash] = '\0';
    const char *numerator = inner;
    const char *denominator = inner + slash + 1;
    for (int order = 1; order <= EIGENMATH_LIMIT_DERIVATIVES; order++) {
        written = snprintf(command, sizeof(command),
                           "eval(simplify(d((%s),%c,%d)/d((%s),%c,%d)),%c,%s)",
                           numerator, variable, order, denominator, variable, order,
                           variable, point);
        if (written <= 0 || (size_t)written >= sizeof(command)) break;
        if (run_capture_locked(command, out, out_size, &stopped) &&
            !stopped && !output_contains_symbol(out, variable)) {
            return true;
        }
    }

    snprintf(out, out_size, "limit unresolved");
    return true;
}

bool opencalc_eigenmath_eval(const char *expression, char *out, size_t out_size)
{
    char translated[EIGENMATH_INPUT_MAX];
    if (expression == NULL || out == NULL || out_size < 2) return false;
    out[0] = '\0';
    if (!lock_engine()) return false;
    bool handled = false;
    if (initialize_engine_locked()) {
        handled = evaluate_limit_locked(expression, out, out_size);
        if (!handled && translate_expression(expression, translated, sizeof(translated))) {
            handled = run_capture_locked(translated, out, out_size, NULL);
        }
    }
    unlock_engine();
    return handled;
}
