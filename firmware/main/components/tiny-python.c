/*
Cory Pearl
05/22/26

Single-file Python-like interpreter for ESP32.

This is a small embedded interpreter, not CPython. It supports simple
Python-like scripts with fixed buffers, nested indented blocks, functions,
print(...), input(...), dynamic lists, tuples, dictionaries, and common
integer/string operations.

Public API:
    #include "tiny-python.h"
    
    py_t py;
    py_init(&py);
    py_use_stdio(&py);
    py_run(&py, "x = 1", out, sizeof(out));
    py_run_source(&py, "x = 1\nprint(x)\n", out, sizeof(out));
    py_run_file(&py, "/spiffs/main.py", out, sizeof(out));
    py_deinit(&py);

------------------- Example Code -------------------

#include "tiny-python.h"
#include <stdio.h>

int main(void) {
    py_t py;

    py_init(&py);
    py_use_stdio(&py);

    if (!py_run_file(&py, "main.py", NULL, 0)) {
        printf("python error: %s\n", py.error);
        py_deinit(&py);
        return 1;
    }

    py_deinit(&py);
    return 0;
}

*/

#include "tiny-python.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

#ifndef PY_MAX_TOKENS
#ifdef ESP_PLATFORM
#define PY_MAX_TOKENS 128
#else
#define PY_MAX_TOKENS 512
#endif
#endif

#ifndef PY_MAX_LINE
#define PY_MAX_LINE 160
#endif

#ifndef PY_MAX_PROGRAM
#define PY_MAX_PROGRAM 2048
#endif

typedef enum {
    TOK_EOF = 0,
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,
    TOK_FSTRING,
    TOK_IDENT,
    TOK_PRINT,
    TOK_DEF,
    TOK_RETURN,
    TOK_IF,
    TOK_ELIF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_IN,
    TOK_RANGE,
    TOK_PASS,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_GLOBAL,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_IS,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NONE,
    TOK_ASSIGN,
    TOK_PLUS_ASSIGN,
    TOK_MINUS_ASSIGN,
    TOK_STAR_ASSIGN,
    TOK_SLASH_ASSIGN,
    TOK_PERCENT_ASSIGN,
    TOK_AMP_ASSIGN,
    TOK_PIPE_ASSIGN,
    TOK_CARET_ASSIGN,
    TOK_LSHIFT_ASSIGN,
    TOK_RSHIFT_ASSIGN,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_STARSTAR,
    TOK_SLASH,
    TOK_DSLASH,
    TOK_PERCENT,
    TOK_AMP,
    TOK_PIPE,
    TOK_CARET,
    TOK_TILDE,
    TOK_LSHIFT,
    TOK_RSHIFT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_DOT,
    TOK_COMMA,
    TOK_SEMI,
    TOK_COLON,
    TOK_EQ,
    TOK_NE,
    TOK_LT,
    TOK_LE,
    TOK_GT,
    TOK_GE
} token_type_t;

typedef struct {
    py_value_t key;
    py_value_t value;
} py_dict_entry_t;

struct py_object {
    py_value_type_t type;
    size_t count;
    size_t capacity;
    py_value_t *items;
    py_dict_entry_t *entries;
    py_object_t *next;
};

typedef struct {
    token_type_t type;
    int int_value;
    double float_value;
    size_t line;
    size_t col;
    char text[PY_MAX_STRING];
} token_t;

typedef struct {
    py_t *py;
    const char *source;
    token_t tokens[PY_MAX_TOKENS];
    size_t token_count;
    size_t pos;
    char *output;
    size_t output_size;
    size_t output_len;
    int exec_enabled;
    int loop_signal;
} parser_t;

static parser_t *parser_alloc(void) {
#ifdef ESP_PLATFORM
    parser_t *parser = (parser_t *)heap_caps_calloc(1, sizeof(parser_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (parser != NULL) {
        return parser;
    }
#endif
    return (parser_t *)calloc(1, sizeof(parser_t));
}

static void py_error(py_t *py, const char *message) {
    if (py->current_line > 0) {
        py->error_line = py->current_line;
        py->error_col = py->current_col;
        snprintf(py->error, sizeof(py->error), "line %u, col %u: %s",
                 (unsigned)py->error_line,
                 (unsigned)py->error_col,
                 message);
        return;
    }
    py->error_line = 0;
    py->error_col = 0;
    snprintf(py->error, sizeof(py->error), "%s", message);
}

static int py_has_error(const py_t *py) {
    return py->error[0] != '\0';
}

static int py_values_equal(const py_value_t *left, const py_value_t *right);

static py_value_t py_none(void) {
    py_value_t value;

    value.type = PY_VALUE_NONE;
    value.int_value = 0;
    value.float_value = 0.0;
    value.string_value[0] = '\0';
    value.object = NULL;
    return value;
}

static py_value_t py_int(int n) {
    py_value_t value = py_none();
    value.type = PY_VALUE_INT;
    value.int_value = n;
    value.float_value = (double)n;
    return value;
}

static py_value_t py_float(double n) {
    py_value_t value = py_none();
    value.type = PY_VALUE_FLOAT;
    value.float_value = n;
    value.int_value = (int)n;
    return value;
}

static py_value_t py_string(const char *s) {
    py_value_t value = py_none();
    value.type = PY_VALUE_STRING;
    snprintf(value.string_value, sizeof(value.string_value), "%s", s);
    return value;
}

static py_value_t py_bool(int truth) {
    py_value_t value = py_none();
    value.type = PY_VALUE_BOOL;
    value.int_value = truth ? 1 : 0;
    value.float_value = (double)value.int_value;
    return value;
}

static py_value_t py_container(py_t *py, py_value_type_t type) {
    py_value_t value = py_none();
    py_object_t *object = (py_object_t *)calloc(1, sizeof(*object));

    if (object == NULL) {
        py_error(py, "out of memory");
        return value;
    }
    object->type = type;
    object->next = py->objects;
    py->objects = object;
    value.type = type;
    value.object = object;
    return value;
}

static py_value_t py_list(py_t *py) {
    return py_container(py, PY_VALUE_LIST);
}

static py_value_t py_tuple(py_t *py) {
    return py_container(py, PY_VALUE_TUPLE);
}

static py_value_t py_dict(py_t *py) {
    return py_container(py, PY_VALUE_DICT);
}

static int py_sequence_append(py_t *py, py_value_t *sequence, py_value_t item) {
    py_value_t *items;
    size_t capacity;

    if ((sequence->type != PY_VALUE_LIST && sequence->type != PY_VALUE_TUPLE) || sequence->object == NULL) {
        py_error(py, "append target is not a sequence");
        return 0;
    }
    if (sequence->object->count >= sequence->object->capacity) {
        capacity = sequence->object->capacity == 0 ? 4 : sequence->object->capacity * 2;
        items = (py_value_t *)realloc(sequence->object->items, capacity * sizeof(*items));
        if (items == NULL) {
            py_error(py, "out of memory");
            return 0;
        }
        sequence->object->items = items;
        sequence->object->capacity = capacity;
    }
    sequence->object->items[sequence->object->count++] = item;
    return 1;
}

static int py_list_append(py_t *py, py_value_t *list, py_value_t item) {
    if (list->type != PY_VALUE_LIST) {
        py_error(py, "append target is not a list");
        return 0;
    }
    return py_sequence_append(py, list, item);
}

static int py_dict_set(py_t *py, py_value_t *dict, py_value_t key, py_value_t value) {
    py_dict_entry_t *entries;
    size_t capacity;
    size_t i;

    if (dict->type != PY_VALUE_DICT || dict->object == NULL) {
        py_error(py, "dict target is not a dict");
        return 0;
    }
    for (i = 0; i < dict->object->count; ++i) {
        if (py_values_equal(&dict->object->entries[i].key, &key)) {
            dict->object->entries[i].value = value;
            return 1;
        }
    }
    if (dict->object->count >= dict->object->capacity) {
        capacity = dict->object->capacity == 0 ? 4 : dict->object->capacity * 2;
        entries = (py_dict_entry_t *)realloc(dict->object->entries, capacity * sizeof(*entries));
        if (entries == NULL) {
            py_error(py, "out of memory");
            return 0;
        }
        dict->object->entries = entries;
        dict->object->capacity = capacity;
    }
    dict->object->entries[dict->object->count].key = key;
    dict->object->entries[dict->object->count].value = value;
    dict->object->count++;
    return 1;
}

static int py_truthy(py_value_t value) {
    if (value.type == PY_VALUE_INT || value.type == PY_VALUE_BOOL) {
        return value.int_value != 0;
    }
    if (value.type == PY_VALUE_FLOAT) {
        return value.float_value != 0.0;
    }
    if (value.type == PY_VALUE_STRING) {
        return value.string_value[0] != '\0';
    }
    if (value.type == PY_VALUE_LIST || value.type == PY_VALUE_TUPLE || value.type == PY_VALUE_DICT) {
        return value.object != NULL && value.object->count != 0;
    }
    return 0;
}

static int py_is_number(py_value_t value) {
    return value.type == PY_VALUE_INT || value.type == PY_VALUE_FLOAT || value.type == PY_VALUE_BOOL;
}

static int py_is_integer_value(py_value_t value) {
    return value.type == PY_VALUE_INT || value.type == PY_VALUE_BOOL;
}

static double py_number_as_double(py_value_t value) {
    if (value.type == PY_VALUE_FLOAT) {
        return value.float_value;
    }
    return (double)value.int_value;
}

static py_var_t *py_find_var(py_t *py, const char *name) {
    size_t i;
    for (i = 0; i < py->var_count; ++i) {
        if (strncmp(py->vars[i].name, name, PY_MAX_NAME) == 0) {
            return &py->vars[i];
        }
    }
    return NULL;
}

static int py_set_var(py_t *py, const char *name, py_value_t value) {
    py_var_t *var = py_find_var(py, name);
    if (var != NULL) {
        var->value = value;
        return 1;
    }
    if (py->var_count >= PY_MAX_VARS) {
        py_error(py, "variable table full");
        return 0;
    }
    snprintf(py->vars[py->var_count].name, PY_MAX_NAME, "%s", name);
    py->vars[py->var_count].value = value;
    py->var_count++;
    return 1;
}

static py_value_t py_get_var(py_t *py, const char *name) {
    py_var_t *var = py_find_var(py, name);
    if (var == NULL) {
        py_error(py, "undefined variable");
        return py_none();
    }
    return var->value;
}

static py_func_t *py_find_func(py_t *py, const char *name) {
    size_t i;
    for (i = 0; i < py->func_count; ++i) {
        if (strncmp(py->funcs[i].name, name, PY_MAX_NAME) == 0) {
            return &py->funcs[i];
        }
    }
    return NULL;
}

static int py_set_func(py_t *py, const char *name, char params[][PY_MAX_NAME], size_t param_count, const char *body) {
    py_func_t *func = py_find_func(py, name);
    size_t i;

    if (func == NULL) {
        if (py->func_count >= PY_MAX_FUNCS) {
            py_error(py, "function table full");
            return 0;
        }
        func = &py->funcs[py->func_count++];
    }

    snprintf(func->name, sizeof(func->name), "%s", name);
    func->param_count = param_count;
    for (i = 0; i < param_count; ++i) {
        snprintf(func->params[i], sizeof(func->params[i]), "%s", params[i]);
    }
    snprintf(func->body, sizeof(func->body), "%s", body);
    return 1;
}

static int py_value_repr(const py_value_t *value, char *buffer, size_t size, int quote_strings) {
    size_t used;
    size_t i;

    if (value->type == PY_VALUE_BOOL) {
        return snprintf(buffer, size, "%s", value->int_value ? "True" : "False") > 0;
    }
    if (value->type == PY_VALUE_INT) {
        return snprintf(buffer, size, "%d", value->int_value) > 0;
    }
    if (value->type == PY_VALUE_FLOAT) {
        return snprintf(buffer, size, "%g", value->float_value) > 0;
    }
    if (value->type == PY_VALUE_STRING) {
        if (quote_strings) {
            return snprintf(buffer, size, "'%s'", value->string_value) >= 0;
        }
        return snprintf(buffer, size, "%s", value->string_value) >= 0;
    }
    if (value->type == PY_VALUE_LIST || value->type == PY_VALUE_TUPLE) {
        char item[PY_MAX_STRING];
        char open = value->type == PY_VALUE_LIST ? '[' : '(';
        char close = value->type == PY_VALUE_LIST ? ']' : ')';

        if (size == 0) {
            return 0;
        }
        buffer[0] = open;
        buffer[1] = '\0';
        used = 1;
        if (value->object == NULL) {
            if (used + 2 > size) {
                return 0;
            }
            buffer[used++] = close;
            buffer[used] = '\0';
            return 1;
        }
        for (i = 0; i < value->object->count; ++i) {
            int written;
            const char *separator = i == 0 ? "" : ", ";

            if (!py_value_repr(&value->object->items[i], item, sizeof(item), 1)) {
                return 0;
            }
            written = snprintf(buffer + used, size - used, "%s%s", separator, item);
            if (written < 0 || (size_t)written >= size - used) {
                return 0;
            }
            used += (size_t)written;
        }
        if (value->type == PY_VALUE_TUPLE && value->object->count == 1) {
            if (used + 2 > size) {
                return 0;
            }
            buffer[used++] = ',';
            buffer[used] = '\0';
        }
        if (used + 2 > size) {
            return 0;
        }
        buffer[used++] = close;
        buffer[used] = '\0';
        return 1;
    }
    if (value->type == PY_VALUE_DICT) {
        char key[PY_MAX_STRING];
        char item[PY_MAX_STRING];

        if (size == 0) {
            return 0;
        }
        buffer[0] = '{';
        buffer[1] = '\0';
        used = 1;
        if (value->object != NULL) {
            for (i = 0; i < value->object->count; ++i) {
                int written;
                const char *separator = i == 0 ? "" : ", ";

                if (!py_value_repr(&value->object->entries[i].key, key, sizeof(key), 1) ||
                    !py_value_repr(&value->object->entries[i].value, item, sizeof(item), 1)) {
                    return 0;
                }
                written = snprintf(buffer + used, size - used, "%s%s: %s", separator, key, item);
                if (written < 0 || (size_t)written >= size - used) {
                    return 0;
                }
                used += (size_t)written;
            }
        }
        if (used + 2 > size) {
            return 0;
        }
        buffer[used++] = '}';
        buffer[used] = '\0';
        return 1;
    }
    return snprintf(buffer, size, "None") > 0;
}

static int py_value_to_string(const py_value_t *value, char *buffer, size_t size) {
    return py_value_repr(value, buffer, size, 0);
}

static int py_values_equal(const py_value_t *left, const py_value_t *right) {
    char left_buf[PY_MAX_STRING];
    char right_buf[PY_MAX_STRING];

    if (left->type == PY_VALUE_NONE || right->type == PY_VALUE_NONE) {
        return left->type == right->type;
    }
    if (left->type == PY_VALUE_LIST || right->type == PY_VALUE_LIST ||
        left->type == PY_VALUE_TUPLE || right->type == PY_VALUE_TUPLE ||
        left->type == PY_VALUE_DICT || right->type == PY_VALUE_DICT) {
        py_value_to_string(left, left_buf, sizeof(left_buf));
        py_value_to_string(right, right_buf, sizeof(right_buf));
        return strcmp(left_buf, right_buf) == 0;
    }
    if (left->type == PY_VALUE_STRING || right->type == PY_VALUE_STRING) {
        py_value_to_string(left, left_buf, sizeof(left_buf));
        py_value_to_string(right, right_buf, sizeof(right_buf));
        return strcmp(left_buf, right_buf) == 0;
    }
    if (py_is_number(*left) && py_is_number(*right)) {
        return py_number_as_double(*left) == py_number_as_double(*right);
    }
    return left->int_value == right->int_value;
}

static int py_pow_int(py_t *py, int base, int exp) {
    int result = 1;
    int i;

    if (exp < 0) {
        py_error(py, "negative exponent not supported");
        return 0;
    }
    for (i = 0; i < exp; ++i) {
        result *= base;
    }
    return result;
}

static void py_append(parser_t *p, const char *text) {
    int written;
    size_t remaining;

    if (!p->exec_enabled || text[0] == '\0' || py_has_error(p->py)) {
        return;
    }
    if (p->py->output_callback != NULL) {
        p->py->output_callback(text, p->py->output_user_data);
        return;
    }
    if (p->output == NULL || p->output_size == 0) {
        return;
    }
    if (p->output_len >= p->output_size) {
        py_error(p->py, "output buffer full");
        return;
    }

    remaining = p->output_size - p->output_len;
    written = snprintf(p->output + p->output_len, remaining, "%s", text);
    if (written < 0 || (size_t)written >= remaining) {
        py_error(p->py, "output buffer full");
        return;
    }
    p->output_len += (size_t)written;
}

static int push_token(parser_t *p, token_t token) {
    if (p->token_count >= PY_MAX_TOKENS) {
        py_error(p->py, "too many tokens");
        return 0;
    }
    p->tokens[p->token_count++] = token;
    return 1;
}

static int lex(parser_t *p) {
    const char *s = p->source;
    size_t line = 1;
    size_t col = 1;

    while (*s != '\0' && !py_has_error(p->py)) {
        token_t token;
        const char *start;
        memset(&token, 0, sizeof(token));

        if (isspace((unsigned char)*s)) {
            if (*s == '\n') {
                line++;
                col = 1;
            } else {
                col++;
            }
            s++;
            continue;
        }
        if (*s == '#') {
            while (*s != '\0' && *s != '\n') {
                s++;
                col++;
            }
            continue;
        }
        start = s;
        token.line = line;
        token.col = col;
        p->py->current_line = line;
        p->py->current_col = col;
        if ((s[0] == 'f' || s[0] == 'F') && (s[1] == '"' || s[1] == '\'')) {
            char quote = s[1];
            size_t len = 0;
            s += 2;
            token.type = TOK_FSTRING;
            while (*s != '\0' && *s != quote && len < sizeof(token.text) - 1) {
                if (*s == '\\' && s[1] != '\0') {
                    s++;
                    if (*s == 'n') {
                        token.text[len++] = '\n';
                    } else if (*s == 't') {
                        token.text[len++] = '\t';
                    } else if (*s == 'r') {
                        token.text[len++] = '\r';
                    } else {
                        token.text[len++] = *s;
                    }
                    s++;
                    continue;
                }
                token.text[len++] = *s++;
            }
            token.text[len] = '\0';
            if (*s != quote) {
                py_error(p->py, "unterminated f-string");
                return 0;
            }
            s++;
            if (!push_token(p, token)) {
                return 0;
            }
            col += (size_t)(s - start);
            continue;
        }
        if (isdigit((unsigned char)*s)) {
            char *end = NULL;
            const char *scan = s;
            int is_float = 0;
            while (isdigit((unsigned char)*scan)) {
                scan++;
            }
            if (*scan == '.' && isdigit((unsigned char)scan[1])) {
                is_float = 1;
            }
            if (is_float) {
                token.type = TOK_FLOAT;
                token.float_value = strtod(s, &end);
            } else {
                long value = strtol(s, &end, 10);
                token.type = TOK_INT;
                token.int_value = (int)value;
                token.float_value = (double)token.int_value;
            }
            if (!push_token(p, token)) {
                return 0;
            }
            col += (size_t)(end - start);
            s = end;
            continue;
        }
        if (isalpha((unsigned char)*s) || *s == '_') {
            size_t len = 0;
            token.type = TOK_IDENT;
            while ((isalnum((unsigned char)s[len]) || s[len] == '_') && len < sizeof(token.text) - 1) {
                token.text[len] = s[len];
                len++;
            }
            token.text[len] = '\0';
            if (strcmp(token.text, "print") == 0) {
                token.type = TOK_PRINT;
            } else if (strcmp(token.text, "def") == 0) {
                token.type = TOK_DEF;
            } else if (strcmp(token.text, "return") == 0) {
                token.type = TOK_RETURN;
            } else if (strcmp(token.text, "if") == 0) {
                token.type = TOK_IF;
            } else if (strcmp(token.text, "elif") == 0) {
                token.type = TOK_ELIF;
            } else if (strcmp(token.text, "else") == 0) {
                token.type = TOK_ELSE;
            } else if (strcmp(token.text, "while") == 0) {
                token.type = TOK_WHILE;
            } else if (strcmp(token.text, "for") == 0) {
                token.type = TOK_FOR;
            } else if (strcmp(token.text, "in") == 0) {
                token.type = TOK_IN;
            } else if (strcmp(token.text, "range") == 0) {
                token.type = TOK_RANGE;
            } else if (strcmp(token.text, "pass") == 0) {
                token.type = TOK_PASS;
            } else if (strcmp(token.text, "break") == 0) {
                token.type = TOK_BREAK;
            } else if (strcmp(token.text, "continue") == 0) {
                token.type = TOK_CONTINUE;
            } else if (strcmp(token.text, "global") == 0) {
                token.type = TOK_GLOBAL;
            } else if (strcmp(token.text, "and") == 0) {
                token.type = TOK_AND;
            } else if (strcmp(token.text, "or") == 0) {
                token.type = TOK_OR;
            } else if (strcmp(token.text, "not") == 0) {
                token.type = TOK_NOT;
            } else if (strcmp(token.text, "is") == 0) {
                token.type = TOK_IS;
            } else if (strcmp(token.text, "True") == 0) {
                token.type = TOK_TRUE;
            } else if (strcmp(token.text, "False") == 0) {
                token.type = TOK_FALSE;
            } else if (strcmp(token.text, "None") == 0) {
                token.type = TOK_NONE;
            }
            if (!push_token(p, token)) {
                return 0;
            }
            s += len;
            while (isalnum((unsigned char)*s) || *s == '_') {
                s++;
            }
            col += (size_t)(s - start);
            continue;
        }
        if (*s == '"' || *s == '\'') {
            char quote = *s;
            size_t len = 0;
            s++;
            token.type = TOK_STRING;
            while (*s != '\0' && *s != quote && len < sizeof(token.text) - 1) {
                if (*s == '\\' && s[1] != '\0') {
                    s++;
                    if (*s == 'n') {
                        token.text[len++] = '\n';
                    } else if (*s == 't') {
                        token.text[len++] = '\t';
                    } else if (*s == 'r') {
                        token.text[len++] = '\r';
                    } else {
                        token.text[len++] = *s;
                    }
                    s++;
                    continue;
                }
                token.text[len++] = *s++;
            }
            token.text[len] = '\0';
            if (*s != quote) {
                py_error(p->py, "unterminated string");
                return 0;
            }
            s++;
            if (!push_token(p, token)) {
                return 0;
            }
            col += (size_t)(s - start);
            continue;
        }

        switch (*s) {
            case '=':
                if (s[1] == '=') {
                    token.type = TOK_EQ;
                    s += 2;
                } else {
                    token.type = TOK_ASSIGN;
                    s++;
                }
                break;
            case '+':
                if (s[1] == '=') {
                    token.type = TOK_PLUS_ASSIGN;
                    s += 2;
                } else {
                    token.type = TOK_PLUS;
                    s++;
                }
                break;
            case '-':
                if (s[1] == '=') {
                    token.type = TOK_MINUS_ASSIGN;
                    s += 2;
                } else {
                    token.type = TOK_MINUS;
                    s++;
                }
                break;
            case '*':
                if (s[1] == '=') {
                    token.type = TOK_STAR_ASSIGN;
                    s += 2;
                } else if (s[1] == '*') {
                    token.type = TOK_STARSTAR;
                    s += 2;
                } else {
                    token.type = TOK_STAR;
                    s++;
                }
                break;
            case '/':
                if (s[1] == '=') {
                    token.type = TOK_SLASH_ASSIGN;
                    s += 2;
                } else if (s[1] == '/') {
                    token.type = TOK_DSLASH;
                    s += 2;
                } else {
                    token.type = TOK_SLASH;
                    s++;
                }
                break;
            case '%':
                if (s[1] == '=') {
                    token.type = TOK_PERCENT_ASSIGN;
                    s += 2;
                } else {
                    token.type = TOK_PERCENT;
                    s++;
                }
                break;
            case '!':
                if (s[1] != '=') {
                    py_error(p->py, "unexpected '!'");
                    return 0;
                }
                token.type = TOK_NE;
                s += 2;
                break;
            case '<':
                if (s[1] == '<' && s[2] == '=') {
                    token.type = TOK_LSHIFT_ASSIGN;
                    s += 3;
                } else if (s[1] == '<') {
                    token.type = TOK_LSHIFT;
                    s += 2;
                } else if (s[1] == '=') {
                    token.type = TOK_LE;
                    s += 2;
                } else {
                    token.type = TOK_LT;
                    s++;
                }
                break;
            case '>':
                if (s[1] == '>' && s[2] == '=') {
                    token.type = TOK_RSHIFT_ASSIGN;
                    s += 3;
                } else if (s[1] == '>') {
                    token.type = TOK_RSHIFT;
                    s += 2;
                } else if (s[1] == '=') {
                    token.type = TOK_GE;
                    s += 2;
                } else {
                    token.type = TOK_GT;
                    s++;
                }
                break;
            case '&':
                if (s[1] == '=') {
                    token.type = TOK_AMP_ASSIGN;
                    s += 2;
                } else {
                    token.type = TOK_AMP;
                    s++;
                }
                break;
            case '|':
                if (s[1] == '=') {
                    token.type = TOK_PIPE_ASSIGN;
                    s += 2;
                } else {
                    token.type = TOK_PIPE;
                    s++;
                }
                break;
            case '^':
                if (s[1] == '=') {
                    token.type = TOK_CARET_ASSIGN;
                    s += 2;
                } else {
                    token.type = TOK_CARET;
                    s++;
                }
                break;
            case '~': token.type = TOK_TILDE; s++; break;
            case '(': token.type = TOK_LPAREN; s++; break;
            case ')': token.type = TOK_RPAREN; s++; break;
            case '{': token.type = TOK_LBRACE; s++; break;
            case '}': token.type = TOK_RBRACE; s++; break;
            case '[': token.type = TOK_LBRACKET; s++; break;
            case ']': token.type = TOK_RBRACKET; s++; break;
            case '.': token.type = TOK_DOT; s++; break;
            case ',': token.type = TOK_COMMA; s++; break;
            case ';': token.type = TOK_SEMI; s++; break;
            case ':': token.type = TOK_COLON; s++; break;
            default:
                py_error(p->py, "invalid character");
                return 0;
        }

        if (!push_token(p, token)) {
            return 0;
        }
        col += (size_t)(s - start);
    }

    {
        token_t eof_token;
        memset(&eof_token, 0, sizeof(eof_token));
        eof_token.type = TOK_EOF;
        eof_token.line = line;
        eof_token.col = col;
        return push_token(p, eof_token);
    }
}

static token_t *current(parser_t *p) {
    if (p->pos < p->token_count) {
        p->py->current_line = p->tokens[p->pos].line;
        p->py->current_col = p->tokens[p->pos].col;
    }
    return &p->tokens[p->pos];
}

static token_t *peek(parser_t *p, size_t ahead) {
    size_t index = p->pos + ahead;
    if (index >= p->token_count) {
        return &p->tokens[p->token_count - 1];
    }
    return &p->tokens[index];
}

static int match(parser_t *p, token_type_t type) {
    if (current(p)->type == type) {
        p->pos++;
        return 1;
    }
    return 0;
}

static int expect(parser_t *p, token_type_t type, const char *message) {
    if (match(p, type)) {
        return 1;
    }
    py_error(p->py, message);
    return 0;
}

static py_value_t parse_expression(parser_t *p);
static int parse_statement(parser_t *p);
static int parse_statement_list(parser_t *p);
static int parse_block(parser_t *p);
static py_value_t py_eval_expression(py_t *py, const char *source);
static py_value_t py_eval_fstring(parser_t *p, const char *source);

static int read_int_arg(parser_t *p, py_value_t value, const char *name) {
    if (!py_is_integer_value(value)) {
        py_error(p->py, name);
        return 0;
    }
    return value.int_value;
}

static void py_remove_var(py_t *py, const char *name) {
    size_t i;
    for (i = 0; i < py->var_count; ++i) {
        if (strncmp(py->vars[i].name, name, PY_MAX_NAME) == 0) {
            size_t j;
            for (j = i + 1; j < py->var_count; ++j) {
                py->vars[j - 1] = py->vars[j];
            }
            py->var_count--;
            return;
        }
    }
}

static py_value_t call_user_function(parser_t *p, py_func_t *func, py_value_t *args, int argc) {
    py_var_t saved[PY_MAX_PARAMS];
    int had_saved[PY_MAX_PARAMS];
    py_value_t result = py_none();
    const char *body = func->body;
    char scratch_output[PY_MAX_LINE];
    size_t i;

    if ((size_t)argc != func->param_count) {
        py_error(p->py, "wrong number of function arguments");
        return py_none();
    }

    for (i = 0; i < func->param_count; ++i) {
        py_var_t *existing = py_find_var(p->py, func->params[i]);
        had_saved[i] = existing != NULL;
        if (existing != NULL) {
            saved[i] = *existing;
        }
        if (!py_set_var(p->py, func->params[i], args[i])) {
            return py_none();
        }
    }

    while (isspace((unsigned char)*body)) {
        body++;
    }
    if (strncmp(body, "return", 6) == 0 && isspace((unsigned char)body[6])) {
        body += 6;
        while (isspace((unsigned char)*body)) {
            body++;
        }
        result = py_eval_expression(p->py, body);
    } else {
        result = py_eval_expression(p->py, body);
        if (py_has_error(p->py)) {
            p->py->error[0] = '\0';
            if (!py_run(p->py, body, scratch_output, sizeof(scratch_output))) {
                result = py_none();
            } else {
                py_append(p, scratch_output);
                result = py_none();
            }
        }
    }

    for (i = 0; i < func->param_count; ++i) {
        if (had_saved[i]) {
            py_var_t *existing = py_find_var(p->py, func->params[i]);
            if (existing != NULL) {
                *existing = saved[i];
            }
        } else {
            py_remove_var(p->py, func->params[i]);
        }
    }

    return result;
}

static py_value_t call_function(parser_t *p, const char *name) {
    py_value_t args[PY_MAX_PARAMS];
    int argc = 0;
    py_func_t *func;

    if (!expect(p, TOK_LPAREN, "expected '(' after function name")) {
        return py_none();
    }
    if (!match(p, TOK_RPAREN)) {
        do {
            if (argc >= PY_MAX_PARAMS) {
                py_error(p->py, "too many function arguments");
                return py_none();
            }
            args[argc++] = parse_expression(p);
            if (py_has_error(p->py)) {
                return py_none();
            }
        } while (match(p, TOK_COMMA));

        if (!expect(p, TOK_RPAREN, "expected ')' after arguments")) {
            return py_none();
        }
    }

    if (strcmp(name, "len") == 0) {
        if (argc != 1 ||
            (args[0].type != PY_VALUE_STRING &&
             args[0].type != PY_VALUE_LIST &&
             args[0].type != PY_VALUE_TUPLE &&
             args[0].type != PY_VALUE_DICT)) {
            py_error(p->py, "len() expects one container");
            return py_none();
        }
        if (args[0].type == PY_VALUE_LIST || args[0].type == PY_VALUE_TUPLE || args[0].type == PY_VALUE_DICT) {
            return py_int(args[0].object == NULL ? 0 : (int)args[0].object->count);
        }
        return py_int((int)strlen(args[0].string_value));
    }
    if (strcmp(name, "int") == 0) {
        if (argc != 1) {
            py_error(p->py, "int() expects one argument");
            return py_none();
        }
        if (py_is_number(args[0])) {
            return py_int(args[0].int_value);
        }
        if (args[0].type == PY_VALUE_STRING) {
            return py_int(atoi(args[0].string_value));
        }
        return py_int(0);
    }
    if (strcmp(name, "float") == 0) {
        if (argc != 1) {
            py_error(p->py, "float() expects one argument");
            return py_none();
        }
        if (py_is_number(args[0])) {
            return py_float(py_number_as_double(args[0]));
        }
        if (args[0].type == PY_VALUE_STRING) {
            return py_float(strtod(args[0].string_value, NULL));
        }
        return py_float(0.0);
    }
    if (strcmp(name, "str") == 0) {
        char buffer[PY_MAX_STRING];
        if (argc != 1) {
            py_error(p->py, "str() expects one argument");
            return py_none();
        }
        py_value_to_string(&args[0], buffer, sizeof(buffer));
        return py_string(buffer);
    }
    if (strcmp(name, "bool") == 0) {
        if (argc != 1) {
            py_error(p->py, "bool() expects one argument");
            return py_none();
        }
        return py_bool(py_truthy(args[0]));
    }
    if (strcmp(name, "abs") == 0) {
        int n;
        if (argc != 1) {
            py_error(p->py, "abs() expects one argument");
            return py_none();
        }
        n = read_int_arg(p, args[0], "abs() expects integer");
        return py_int(n < 0 ? -n : n);
    }
    if (strcmp(name, "min") == 0 || strcmp(name, "max") == 0) {
        int a;
        int b;
        if (argc != 2) {
            py_error(p->py, "min()/max() expect two arguments");
            return py_none();
        }
        a = read_int_arg(p, args[0], "min()/max() expect integers");
        b = read_int_arg(p, args[1], "min()/max() expect integers");
        if (py_has_error(p->py)) {
            return py_none();
        }
        if (strcmp(name, "min") == 0) {
            return py_int(a < b ? a : b);
        }
        return py_int(a > b ? a : b);
    }
    if (strcmp(name, "pow") == 0) {
        int base;
        int exp;
        if (argc != 2) {
            py_error(p->py, "pow() expects two arguments");
            return py_none();
        }
        base = read_int_arg(p, args[0], "pow() expects integers");
        exp = read_int_arg(p, args[1], "pow() expects integers");
        if (py_has_error(p->py)) {
            return py_none();
        }
        return py_int(py_pow_int(p->py, base, exp));
    }
    if (strcmp(name, "ord") == 0) {
        if (argc != 1 || args[0].type != PY_VALUE_STRING || strlen(args[0].string_value) != 1) {
            py_error(p->py, "ord() expects one character");
            return py_none();
        }
        return py_int((unsigned char)args[0].string_value[0]);
    }
    if (strcmp(name, "chr") == 0) {
        char s[2];
        int n;
        if (argc != 1) {
            py_error(p->py, "chr() expects one integer");
            return py_none();
        }
        n = read_int_arg(p, args[0], "chr() expects integer");
        if (py_has_error(p->py)) {
            return py_none();
        }
        if (n < 0 || n > 255) {
            py_error(p->py, "chr() out of range");
            return py_none();
        }
        s[0] = (char)n;
        s[1] = '\0';
        return py_string(s);
    }
    if (strcmp(name, "type") == 0) {
        if (argc != 1) {
            py_error(p->py, "type() expects one argument");
            return py_none();
        }
        if (args[0].type == PY_VALUE_INT) {
            return py_string("int");
        }
        if (args[0].type == PY_VALUE_FLOAT) {
            return py_string("float");
        }
        if (args[0].type == PY_VALUE_BOOL) {
            return py_string("bool");
        }
        if (args[0].type == PY_VALUE_STRING) {
            return py_string("str");
        }
        if (args[0].type == PY_VALUE_LIST) {
            return py_string("list");
        }
        if (args[0].type == PY_VALUE_TUPLE) {
            return py_string("tuple");
        }
        if (args[0].type == PY_VALUE_DICT) {
            return py_string("dict");
        }
        return py_string("NoneType");
    }
    if (strcmp(name, "input") == 0) {
        char input[PY_MAX_STRING];
        size_t len;

        if (argc > 1) {
            py_error(p->py, "input() expects zero or one argument");
            return py_none();
        }
        if (argc == 1) {
            char prompt[PY_MAX_STRING];
            py_value_to_string(&args[0], prompt, sizeof(prompt));
            py_append(p, prompt);
            if (py_has_error(p->py)) {
                return py_none();
            }
        }
        if (p->py->input_callback == NULL) {
            py_error(p->py, "input callback not set");
            return py_none();
        }

        input[0] = '\0';
        if (!p->py->input_callback(input, sizeof(input), p->py->input_user_data)) {
            py_error(p->py, "input failed");
            return py_none();
        }
        input[sizeof(input) - 1] = '\0';
        len = strlen(input);
        while (len > 0 && (input[len - 1] == '\n' || input[len - 1] == '\r')) {
            input[--len] = '\0';
        }
        return py_string(input);
    }
    if (strcmp(name, "pinMode") == 0) {
        int pin;
        int mode;
        if (argc != 2) {
            py_error(p->py, "pinMode() expects pin and mode");
            return py_none();
        }
        if (p->py->gpio_mode_callback == NULL) {
            py_error(p->py, "gpio mode callback not set");
            return py_none();
        }
        pin = read_int_arg(p, args[0], "pinMode() expects integer pin");
        mode = read_int_arg(p, args[1], "pinMode() expects integer mode");
        if (py_has_error(p->py)) {
            return py_none();
        }
        if (!p->py->gpio_mode_callback(pin, mode, p->py->gpio_user_data)) {
            py_error(p->py, "gpio mode failed");
            return py_none();
        }
        return py_none();
    }
    if (strcmp(name, "digitalWrite") == 0) {
        int pin;
        int value;
        if (argc != 2) {
            py_error(p->py, "digitalWrite() expects pin and value");
            return py_none();
        }
        if (p->py->gpio_write_callback == NULL) {
            py_error(p->py, "gpio write callback not set");
            return py_none();
        }
        pin = read_int_arg(p, args[0], "digitalWrite() expects integer pin");
        value = read_int_arg(p, args[1], "digitalWrite() expects integer value");
        if (py_has_error(p->py)) {
            return py_none();
        }
        if (!p->py->gpio_write_callback(pin, value ? 1 : 0, p->py->gpio_user_data)) {
            py_error(p->py, "gpio write failed");
            return py_none();
        }
        return py_none();
    }
    if (strcmp(name, "digitalRead") == 0) {
        int pin;
        int value = 0;
        if (argc != 1) {
            py_error(p->py, "digitalRead() expects pin");
            return py_none();
        }
        if (p->py->gpio_read_callback == NULL) {
            py_error(p->py, "gpio read callback not set");
            return py_none();
        }
        pin = read_int_arg(p, args[0], "digitalRead() expects integer pin");
        if (py_has_error(p->py)) {
            return py_none();
        }
        if (!p->py->gpio_read_callback(pin, &value, p->py->gpio_user_data)) {
            py_error(p->py, "gpio read failed");
            return py_none();
        }
        return py_int(value ? 1 : 0);
    }
    if (strcmp(name, "printable") == 0) {
        if (argc != 1) {
            py_error(p->py, "printable() expects one argument");
            return py_none();
        }
        return py_bool(args[0].type == PY_VALUE_STRING && args[0].string_value[0] != '\0');
    }

    func = py_find_func(p->py, name);
    if (func != NULL) {
        return call_user_function(p, func, args, argc);
    }

    py_error(p->py, "unknown function");
    return py_none();
}

static py_value_t eval_binary(parser_t *p, py_value_t left, token_type_t op, py_value_t right) {
    char left_buf[PY_MAX_STRING];
    char right_buf[PY_MAX_STRING];
    char concat[PY_MAX_STRING];

    if (py_has_error(p->py)) {
        return py_none();
    }

    switch (op) {
        case TOK_PLUS:
            if (py_is_number(left) && py_is_number(right)) {
                if (left.type == PY_VALUE_FLOAT || right.type == PY_VALUE_FLOAT) {
                    return py_float(py_number_as_double(left) + py_number_as_double(right));
                }
                return py_int(left.int_value + right.int_value);
            }
            py_value_to_string(&left, left_buf, sizeof(left_buf));
            py_value_to_string(&right, right_buf, sizeof(right_buf));
            if (strlen(left_buf) + strlen(right_buf) >= sizeof(concat)) {
                py_error(p->py, "string too long");
                return py_none();
            }
            strcpy(concat, left_buf);
            strcat(concat, right_buf);
            return py_string(concat);
        case TOK_MINUS:
            if (!py_is_number(left) || !py_is_number(right)) {
                py_error(p->py, "subtraction requires numbers");
                return py_none();
            }
            if (left.type == PY_VALUE_FLOAT || right.type == PY_VALUE_FLOAT) {
                return py_float(py_number_as_double(left) - py_number_as_double(right));
            }
            return py_int(left.int_value - right.int_value);
        case TOK_STAR:
            if (!py_is_number(left) || !py_is_number(right)) {
                py_error(p->py, "multiplication requires numbers");
                return py_none();
            }
            if (left.type == PY_VALUE_FLOAT || right.type == PY_VALUE_FLOAT) {
                return py_float(py_number_as_double(left) * py_number_as_double(right));
            }
            return py_int(left.int_value * right.int_value);
        case TOK_STARSTAR:
            if (!py_is_number(left) || !py_is_number(right)) {
                py_error(p->py, "exponent requires numbers");
                return py_none();
            }
            if (left.type == PY_VALUE_FLOAT || right.type == PY_VALUE_FLOAT) {
                int exp = right.type == PY_VALUE_FLOAT ? (int)right.float_value : right.int_value;
                double result = 1.0;
                int i;
                if (exp < 0) {
                    py_error(p->py, "negative exponent not supported");
                    return py_none();
                }
                for (i = 0; i < exp; ++i) {
                    result *= py_number_as_double(left);
                }
                return py_float(result);
            }
            return py_int(py_pow_int(p->py, left.int_value, right.int_value));
        case TOK_SLASH:
            if (!py_is_number(left) || !py_is_number(right)) {
                py_error(p->py, "division requires numbers");
                return py_none();
            }
            if (py_number_as_double(right) == 0.0) {
                py_error(p->py, "division by zero");
                return py_none();
            }
            return py_float(py_number_as_double(left) / py_number_as_double(right));
        case TOK_DSLASH:
            if (!py_is_number(left) || !py_is_number(right)) {
                py_error(p->py, "division requires numbers");
                return py_none();
            }
            if (py_number_as_double(right) == 0.0) {
                py_error(p->py, "division by zero");
                return py_none();
            }
            return py_int((int)(py_number_as_double(left) / py_number_as_double(right)));
        case TOK_PERCENT:
            if (!py_is_integer_value(left) || !py_is_integer_value(right)) {
                py_error(p->py, "modulo requires integers");
                return py_none();
            }
            if (right.int_value == 0) {
                py_error(p->py, "modulo by zero");
                return py_none();
            }
            return py_int(left.int_value % right.int_value);
        case TOK_AMP:
            if (!py_is_integer_value(left) || !py_is_integer_value(right)) {
                py_error(p->py, "bitwise and requires integers");
                return py_none();
            }
            return py_int(left.int_value & right.int_value);
        case TOK_PIPE:
            if (!py_is_integer_value(left) || !py_is_integer_value(right)) {
                py_error(p->py, "bitwise or requires integers");
                return py_none();
            }
            return py_int(left.int_value | right.int_value);
        case TOK_CARET:
            if (!py_is_integer_value(left) || !py_is_integer_value(right)) {
                py_error(p->py, "bitwise xor requires integers");
                return py_none();
            }
            return py_int(left.int_value ^ right.int_value);
        case TOK_LSHIFT:
            if (!py_is_integer_value(left) || !py_is_integer_value(right)) {
                py_error(p->py, "left shift requires integers");
                return py_none();
            }
            if (right.int_value < 0) {
                py_error(p->py, "negative shift count");
                return py_none();
            }
            return py_int(left.int_value << right.int_value);
        case TOK_RSHIFT:
            if (!py_is_integer_value(left) || !py_is_integer_value(right)) {
                py_error(p->py, "right shift requires integers");
                return py_none();
            }
            if (right.int_value < 0) {
                py_error(p->py, "negative shift count");
                return py_none();
            }
            return py_int(left.int_value >> right.int_value);
        case TOK_EQ:
            return py_bool(py_values_equal(&left, &right));
        case TOK_NE:
            return py_bool(!py_values_equal(&left, &right));
        case TOK_LT:
        case TOK_LE:
        case TOK_GT:
        case TOK_GE:
            if (!py_is_number(left) || !py_is_number(right)) {
                py_error(p->py, "ordering comparison requires numbers");
                return py_none();
            }
            if (op == TOK_LT) {
                return py_int(py_number_as_double(left) < py_number_as_double(right));
            }
            if (op == TOK_LE) {
                return py_int(py_number_as_double(left) <= py_number_as_double(right));
            }
            if (op == TOK_GT) {
                return py_int(py_number_as_double(left) > py_number_as_double(right));
            }
            return py_int(py_number_as_double(left) >= py_number_as_double(right));
        default:
            py_error(p->py, "unknown operator");
            return py_none();
    }
}

static int py_sequence_len(py_value_t value) {
    if (value.type == PY_VALUE_STRING) {
        return (int)strlen(value.string_value);
    }
    if ((value.type == PY_VALUE_LIST || value.type == PY_VALUE_TUPLE) && value.object != NULL) {
        return (int)value.object->count;
    }
    return -1;
}

static int py_normalize_index(int index, int len) {
    if (index < 0) {
        index += len;
    }
    return index;
}

static py_value_t py_sequence_slice(parser_t *p, py_value_t sequence, py_value_t start_value, int has_start, py_value_t stop_value, int has_stop, py_value_t step_value, int has_step) {
    int len = py_sequence_len(sequence);
    int start = 0;
    int stop = len;
    int step = 1;
    int i;
    py_value_t result;

    if (len < 0) {
        py_error(p->py, "slice target is not a sequence");
        return py_none();
    }
    if (has_start) {
        if (!py_is_number(start_value)) {
            py_error(p->py, "slice start must be integer");
            return py_none();
        }
        start = py_normalize_index(start_value.int_value, len);
    }
    if (has_stop) {
        if (!py_is_number(stop_value)) {
            py_error(p->py, "slice stop must be integer");
            return py_none();
        }
        stop = py_normalize_index(stop_value.int_value, len);
    }
    if (has_step) {
        if (!py_is_number(step_value)) {
            py_error(p->py, "slice step must be integer");
            return py_none();
        }
        step = step_value.int_value;
        if (step == 0) {
            py_error(p->py, "slice step cannot be zero");
            return py_none();
        }
    }
    if (!has_start && step < 0) {
        start = len - 1;
    }
    if (!has_stop && step < 0) {
        stop = -1;
    }
    if (start < 0) {
        start = step < 0 ? -1 : 0;
    }
    if (stop < 0 && step > 0) {
        stop = 0;
    }
    if (start > len) {
        start = step < 0 ? len - 1 : len;
    }
    if (stop > len) {
        stop = len;
    }
    if (step > 0 && stop < start) {
        stop = start;
    }
    if (step < 0 && stop > start) {
        stop = start;
    }

    if (sequence.type == PY_VALUE_STRING) {
        char text[PY_MAX_STRING];
        size_t count = 0;
        for (i = start; (step > 0) ? (i < stop) : (i > stop); i += step) {
            if (i >= 0 && i < len && count + 1 < sizeof(text)) {
                text[count++] = sequence.string_value[i];
            }
        }
        text[count] = '\0';
        return py_string(text);
    }

    result = sequence.type == PY_VALUE_TUPLE ? py_tuple(p->py) : py_list(p->py);
    if (py_has_error(p->py)) {
        return py_none();
    }
    for (i = start; (step > 0) ? (i < stop) : (i > stop); i += step) {
        if (i >= 0 && i < len && !py_sequence_append(p->py, &result, sequence.object->items[i])) {
            return py_none();
        }
    }
    return result;
}

static py_value_t py_subscript(parser_t *p, py_value_t target, py_value_t index) {
    int len;
    int i;

    if (target.type == PY_VALUE_DICT) {
        if (target.object == NULL) {
            py_error(p->py, "key not found");
            return py_none();
        }
        for (i = 0; i < (int)target.object->count; ++i) {
            if (py_values_equal(&target.object->entries[i].key, &index)) {
                return target.object->entries[i].value;
            }
        }
        py_error(p->py, "key not found");
        return py_none();
    }

    if (!py_is_number(index)) {
        py_error(p->py, "index must be integer");
        return py_none();
    }
    len = py_sequence_len(target);
    if (len < 0) {
        py_error(p->py, "target is not subscriptable");
        return py_none();
    }
    i = py_normalize_index(index.int_value, len);
    if (i < 0 || i >= len) {
        py_error(p->py, "index out of range");
        return py_none();
    }
    if (target.type == PY_VALUE_STRING) {
        char text[2];
        text[0] = target.string_value[i];
        text[1] = '\0';
        return py_string(text);
    }
    return target.object->items[i];
}

static int py_assign_subscript(parser_t *p, py_value_t *target, py_value_t index, py_value_t value) {
    int len;
    int i;

    if (target->type == PY_VALUE_DICT) {
        return py_dict_set(p->py, target, index, value);
    }
    if (target->type == PY_VALUE_TUPLE || target->type == PY_VALUE_STRING) {
        py_error(p->py, "target does not support item assignment");
        return 0;
    }
    if (target->type != PY_VALUE_LIST || target->object == NULL) {
        py_error(p->py, "target is not subscriptable");
        return 0;
    }
    if (!py_is_number(index)) {
        py_error(p->py, "index must be integer");
        return 0;
    }
    len = py_sequence_len(*target);
    i = py_normalize_index(index.int_value, len);
    if (i < 0 || i >= len) {
        py_error(p->py, "index out of range");
        return 0;
    }
    target->object->items[i] = value;
    return 1;
}

static int parse_slice_tail(parser_t *p, py_value_t base, int has_start, py_value_t start, py_value_t *out) {
    py_value_t stop = py_none();
    py_value_t step = py_none();
    int has_stop = 0;
    int has_step = 0;
    int closed = 0;

    if (match(p, TOK_RBRACKET)) {
        closed = 1;
    } else if (current(p)->type != TOK_COLON) {
        stop = parse_expression(p);
        has_stop = 1;
        if (py_has_error(p->py)) {
            return 0;
        }
    }
    if (!closed && match(p, TOK_COLON)) {
        has_step = 1;
        if (!match(p, TOK_RBRACKET)) {
            step = parse_expression(p);
            if (py_has_error(p->py)) {
                return 0;
            }
            if (!expect(p, TOK_RBRACKET, "expected ']' after slice")) {
                return 0;
            }
        }
    } else if (!closed && !expect(p, TOK_RBRACKET, "expected ']' after slice")) {
        return 0;
    }
    *out = py_sequence_slice(p, base, start, has_start, stop, has_stop, step, has_step);
    return !py_has_error(p->py);
}

static py_value_t parse_atom(parser_t *p) {
    token_t *token = current(p);

    if (match(p, TOK_INT)) {
        return py_int(token->int_value);
    }
    if (match(p, TOK_FLOAT)) {
        return py_float(token->float_value);
    }
    if (match(p, TOK_STRING)) {
        return py_string(token->text);
    }
    if (match(p, TOK_FSTRING)) {
        return py_eval_fstring(p, token->text);
    }
    if (match(p, TOK_TRUE)) {
        return py_bool(1);
    }
    if (match(p, TOK_FALSE)) {
        return py_bool(0);
    }
    if (match(p, TOK_NONE)) {
        return py_none();
    }
    if (match(p, TOK_LBRACKET)) {
        py_value_t list = py_list(p->py);

        if (!match(p, TOK_RBRACKET)) {
            do {
                py_value_t item = parse_expression(p);
                if (py_has_error(p->py)) {
                    return py_none();
                }
                if (!py_list_append(p->py, &list, item)) {
                    return py_none();
                }
            } while (match(p, TOK_COMMA));

            if (!expect(p, TOK_RBRACKET, "expected ']' after list")) {
                return py_none();
            }
        }
        return list;
    }
    if (match(p, TOK_LBRACE)) {
        py_value_t dict = py_dict(p->py);

        if (!match(p, TOK_RBRACE)) {
            do {
                py_value_t key = parse_expression(p);
                py_value_t value;
                if (py_has_error(p->py)) {
                    return py_none();
                }
                if (!expect(p, TOK_COLON, "expected ':' in dict")) {
                    return py_none();
                }
                value = parse_expression(p);
                if (py_has_error(p->py)) {
                    return py_none();
                }
                if (!py_dict_set(p->py, &dict, key, value)) {
                    return py_none();
                }
            } while (match(p, TOK_COMMA));

            if (!expect(p, TOK_RBRACE, "expected '}' after dict")) {
                return py_none();
            }
        }
        return dict;
    }
    if (match(p, TOK_IDENT)) {
        if (current(p)->type == TOK_LPAREN) {
            return call_function(p, token->text);
        }
        return py_get_var(p->py, token->text);
    }
    if (match(p, TOK_LPAREN)) {
        py_value_t first;
        py_value_t tuple;

        if (match(p, TOK_RPAREN)) {
            return py_tuple(p->py);
        }
        first = parse_expression(p);
        if (py_has_error(p->py)) {
            return py_none();
        }
        if (!match(p, TOK_COMMA)) {
            expect(p, TOK_RPAREN, "expected ')'");
            return first;
        }
        tuple = py_tuple(p->py);
        if (py_has_error(p->py) || !py_sequence_append(p->py, &tuple, first)) {
            return py_none();
        }
        if (!match(p, TOK_RPAREN)) {
            do {
                py_value_t item = parse_expression(p);
                if (py_has_error(p->py)) {
                    return py_none();
                }
                if (!py_sequence_append(p->py, &tuple, item)) {
                    return py_none();
                }
            } while (match(p, TOK_COMMA));
            if (!expect(p, TOK_RPAREN, "expected ')' after tuple")) {
                return py_none();
            }
        }
        return tuple;
    }

    py_error(p->py, "expected expression");
    return py_none();
}

static py_value_t parse_primary(parser_t *p) {
    py_value_t value = parse_atom(p);

    while (!py_has_error(p->py) && match(p, TOK_LBRACKET)) {
        if (match(p, TOK_COLON)) {
            if (!parse_slice_tail(p, value, 0, py_none(), &value)) {
                return py_none();
            }
        } else {
            py_value_t index = parse_expression(p);
            if (py_has_error(p->py)) {
                return py_none();
            }
            if (match(p, TOK_COLON)) {
                if (!parse_slice_tail(p, value, 1, index, &value)) {
                    return py_none();
                }
            } else {
                if (!expect(p, TOK_RBRACKET, "expected ']' after index")) {
                    return py_none();
                }
                value = py_subscript(p, value, index);
            }
        }
    }
    return value;
}

static py_value_t parse_power(parser_t *p) {
    py_value_t left = parse_primary(p);
    if (!py_has_error(p->py) && match(p, TOK_STARSTAR)) {
        left = eval_binary(p, left, TOK_STARSTAR, parse_power(p));
    }
    return left;
}

static py_value_t parse_unary(parser_t *p) {
    if (match(p, TOK_NOT)) {
        return py_bool(!py_truthy(parse_unary(p)));
    }
    if (match(p, TOK_PLUS)) {
        py_value_t value = parse_unary(p);
        if (!py_is_number(value)) {
            py_error(p->py, "unary plus requires number");
            return py_none();
        }
        return value.type == PY_VALUE_FLOAT ? py_float(value.float_value) : py_int(value.int_value);
    }
    if (match(p, TOK_MINUS)) {
        py_value_t value = parse_unary(p);
        if (!py_is_number(value)) {
            py_error(p->py, "unary minus requires number");
            return py_none();
        }
        if (value.type == PY_VALUE_FLOAT) {
            return py_float(-value.float_value);
        }
        return py_int(-value.int_value);
    }
    if (match(p, TOK_TILDE)) {
        py_value_t value = parse_unary(p);
        if (!py_is_number(value)) {
            py_error(p->py, "bitwise invert requires integer");
            return py_none();
        }
        return py_int(~value.int_value);
    }
    return parse_power(p);
}

static py_value_t parse_factor(parser_t *p) {
    py_value_t left = parse_unary(p);
    while (!py_has_error(p->py)) {
        token_type_t op = current(p)->type;
        if (op != TOK_STAR && op != TOK_SLASH && op != TOK_DSLASH && op != TOK_PERCENT) {
            break;
        }
        p->pos++;
        left = eval_binary(p, left, op, parse_unary(p));
    }
    return left;
}

static py_value_t parse_term(parser_t *p) {
    py_value_t left = parse_factor(p);
    while (!py_has_error(p->py)) {
        token_type_t op = current(p)->type;
        if (op != TOK_PLUS && op != TOK_MINUS) {
            break;
        }
        p->pos++;
        left = eval_binary(p, left, op, parse_factor(p));
    }
    return left;
}

static py_value_t parse_shift(parser_t *p) {
    py_value_t left = parse_term(p);
    while (!py_has_error(p->py)) {
        token_type_t op = current(p)->type;
        if (op != TOK_LSHIFT && op != TOK_RSHIFT) {
            break;
        }
        p->pos++;
        left = eval_binary(p, left, op, parse_term(p));
    }
    return left;
}

static py_value_t parse_bit_and(parser_t *p) {
    py_value_t left = parse_shift(p);
    while (!py_has_error(p->py) && match(p, TOK_AMP)) {
        left = eval_binary(p, left, TOK_AMP, parse_shift(p));
    }
    return left;
}

static py_value_t parse_bit_xor(parser_t *p) {
    py_value_t left = parse_bit_and(p);
    while (!py_has_error(p->py) && match(p, TOK_CARET)) {
        left = eval_binary(p, left, TOK_CARET, parse_bit_and(p));
    }
    return left;
}

static py_value_t parse_bit_or(parser_t *p) {
    py_value_t left = parse_bit_xor(p);
    while (!py_has_error(p->py) && match(p, TOK_PIPE)) {
        left = eval_binary(p, left, TOK_PIPE, parse_bit_xor(p));
    }
    return left;
}

static py_value_t parse_comparison(parser_t *p) {
    py_value_t left = parse_bit_or(p);
    while (!py_has_error(p->py)) {
        token_type_t op = current(p)->type;
        if (op != TOK_LT && op != TOK_LE && op != TOK_GT && op != TOK_GE) {
            break;
        }
        p->pos++;
        left = eval_binary(p, left, op, parse_bit_or(p));
    }
    return left;
}

static py_value_t parse_equality(parser_t *p) {
    py_value_t left = parse_comparison(p);
    while (!py_has_error(p->py)) {
        token_type_t op = current(p)->type;
        if (op != TOK_EQ && op != TOK_NE && op != TOK_IS) {
            break;
        }
        p->pos++;
        if (op == TOK_IS && match(p, TOK_NOT)) {
            py_value_t right = parse_comparison(p);
            left = py_bool(!py_values_equal(&left, &right));
        } else if (op == TOK_IS) {
            py_value_t right = parse_comparison(p);
            left = py_bool(py_values_equal(&left, &right));
        } else {
            left = eval_binary(p, left, op, parse_comparison(p));
        }
    }
    return left;
}

static py_value_t parse_and(parser_t *p) {
    py_value_t left = parse_equality(p);
    while (!py_has_error(p->py) && match(p, TOK_AND)) {
        py_value_t right = parse_equality(p);
        left = py_bool(py_truthy(left) && py_truthy(right));
    }
    return left;
}

static py_value_t parse_expression(parser_t *p) {
    py_value_t left = parse_and(p);
    while (!py_has_error(p->py) && match(p, TOK_OR)) {
        py_value_t right = parse_and(p);
        left = py_bool(py_truthy(left) || py_truthy(right));
    }
    return left;
}

static int emit_value_line(parser_t *p, py_value_t value) {
    char value_buf[PY_MAX_STRING];
    char line[PY_MAX_STRING + 4];

    py_value_to_string(&value, value_buf, sizeof(value_buf));
    snprintf(line, sizeof(line), "%s\n", value_buf);
    py_append(p, line);
    return !py_has_error(p->py);
}

static int parse_print(parser_t *p) {
    py_value_t value;
    char value_buf[PY_MAX_STRING];
    char end_text[PY_MAX_STRING];
    int depth;

    snprintf(end_text, sizeof(end_text), "\n");
    if (!expect(p, TOK_PRINT, "expected 'print'")) {
        return 0;
    }
    if (!expect(p, TOK_LPAREN, "expected '(' after print")) {
        return 0;
    }
    if (!p->exec_enabled) {
        depth = 1;
        while (current(p)->type != TOK_EOF && depth > 0) {
            if (current(p)->type == TOK_LPAREN) {
                depth++;
            } else if (current(p)->type == TOK_RPAREN) {
                depth--;
            }
            p->pos++;
        }
        return depth == 0;
    }
    if (!match(p, TOK_RPAREN)) {
        do {
            if (current(p)->type == TOK_IDENT &&
                strncmp(current(p)->text, "end", PY_MAX_NAME) == 0 &&
                peek(p, 1)->type == TOK_ASSIGN) {
                p->pos += 2;
                value = parse_expression(p);
                if (py_has_error(p->py)) {
                    return 0;
                }
                py_value_to_string(&value, end_text, sizeof(end_text));
                break;
            }

            value = parse_expression(p);
            if (py_has_error(p->py)) {
                return 0;
            }
            py_value_to_string(&value, value_buf, sizeof(value_buf));
            py_append(p, value_buf);
            if (current(p)->type == TOK_COMMA &&
                !(peek(p, 1)->type == TOK_IDENT &&
                  strncmp(peek(p, 1)->text, "end", PY_MAX_NAME) == 0 &&
                  peek(p, 2)->type == TOK_ASSIGN)) {
                py_append(p, " ");
            }
        } while (match(p, TOK_COMMA));

        if (!expect(p, TOK_RPAREN, "expected ')' after print argument")) {
            return 0;
        }
    }
    py_append(p, end_text);
    return !py_has_error(p->py);
}

static const char *token_source(token_t *token) {
    static char int_text[16];

    switch (token->type) {
        case TOK_INT:
            snprintf(int_text, sizeof(int_text), "%d", token->int_value);
            return int_text;
        case TOK_FLOAT:
            snprintf(int_text, sizeof(int_text), "%g", token->float_value);
            return int_text;
        case TOK_STRING:
        case TOK_FSTRING:
        case TOK_IDENT:
            return token->text;
        case TOK_PRINT: return "print";
        case TOK_DEF: return "def";
        case TOK_RETURN: return "return";
        case TOK_IF: return "if";
        case TOK_ELIF: return "elif";
        case TOK_ELSE: return "else";
        case TOK_WHILE: return "while";
        case TOK_FOR: return "for";
        case TOK_IN: return "in";
        case TOK_RANGE: return "range";
        case TOK_PASS: return "pass";
        case TOK_BREAK: return "break";
        case TOK_CONTINUE: return "continue";
        case TOK_GLOBAL: return "global";
        case TOK_AND: return "and";
        case TOK_OR: return "or";
        case TOK_NOT: return "not";
        case TOK_IS: return "is";
        case TOK_TRUE: return "True";
        case TOK_FALSE: return "False";
        case TOK_NONE: return "None";
        case TOK_ASSIGN: return "=";
        case TOK_PLUS_ASSIGN: return "+=";
        case TOK_MINUS_ASSIGN: return "-=";
        case TOK_STAR_ASSIGN: return "*=";
        case TOK_SLASH_ASSIGN: return "/=";
        case TOK_PERCENT_ASSIGN: return "%=";
        case TOK_AMP_ASSIGN: return "&=";
        case TOK_PIPE_ASSIGN: return "|=";
        case TOK_CARET_ASSIGN: return "^=";
        case TOK_LSHIFT_ASSIGN: return "<<=";
        case TOK_RSHIFT_ASSIGN: return ">>=";
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_STARSTAR: return "**";
        case TOK_SLASH: return "/";
        case TOK_DSLASH: return "//";
        case TOK_PERCENT: return "%";
        case TOK_AMP: return "&";
        case TOK_PIPE: return "|";
        case TOK_CARET: return "^";
        case TOK_TILDE: return "~";
        case TOK_LSHIFT: return "<<";
        case TOK_RSHIFT: return ">>";
        case TOK_LPAREN: return "(";
        case TOK_RPAREN: return ")";
        case TOK_LBRACE: return "{";
        case TOK_RBRACE: return "}";
        case TOK_LBRACKET: return "[";
        case TOK_RBRACKET: return "]";
        case TOK_DOT: return ".";
        case TOK_COMMA: return ",";
        case TOK_SEMI: return ";";
        case TOK_COLON: return ":";
        case TOK_EQ: return "==";
        case TOK_NE: return "!=";
        case TOK_LT: return "<";
        case TOK_LE: return "<=";
        case TOK_GT: return ">";
        case TOK_GE: return ">=";
        default: return "";
    }
}

static int append_body_token(char *body, size_t body_size, token_t *token) {
    const char *text = token_source(token);
    size_t used = strlen(body);
    size_t len = strlen(text);

    if (token->type == TOK_STRING || token->type == TOK_FSTRING) {
        if (used + len + 4 > body_size) {
            return 0;
        }
        if (token->type == TOK_FSTRING) {
            body[used++] = 'f';
        }
        body[used++] = '"';
        memcpy(body + used, text, len);
        used += len;
        body[used++] = '"';
        body[used] = '\0';
        return 1;
    }

    if (used > 0 &&
        token->type != TOK_COMMA &&
        token->type != TOK_RPAREN &&
        token->type != TOK_RBRACE &&
        token->type != TOK_COLON) {
        if (used + 1 >= body_size) {
            return 0;
        }
        body[used++] = ' ';
    }
    if (used + len + 1 > body_size) {
        return 0;
    }
    memcpy(body + used, text, len);
    body[used + len] = '\0';
    return 1;
}

static int parse_def(parser_t *p) {
    token_t name;
    char params[PY_MAX_PARAMS][PY_MAX_NAME];
    size_t param_count = 0;
    char body[PY_MAX_FUNC_BODY];

    if (!expect(p, TOK_DEF, "expected 'def'")) {
        return 0;
    }
    name = *current(p);
    if (!expect(p, TOK_IDENT, "expected function name")) {
        return 0;
    }
    if (!expect(p, TOK_LPAREN, "expected '(' after function name")) {
        return 0;
    }
    if (!match(p, TOK_RPAREN)) {
        do {
            token_t param = *current(p);
            if (param_count >= PY_MAX_PARAMS) {
                py_error(p->py, "too many function parameters");
                return 0;
            }
            if (!expect(p, TOK_IDENT, "expected parameter name")) {
                return 0;
            }
            size_t param_len = strlen(param.text);
            if (param_len >= sizeof(params[param_count])) {
                py_error(p->py, "function parameter name too long");
                return 0;
            }
            memcpy(params[param_count], param.text, param_len + 1);
            param_count++;
        } while (match(p, TOK_COMMA));

        if (!expect(p, TOK_RPAREN, "expected ')' after parameters")) {
            return 0;
        }
    }
    if (!expect(p, TOK_COLON, "expected ':' after function definition")) {
        return 0;
    }

    body[0] = '\0';
    if (current(p)->type == TOK_LBRACE) {
        int depth = 0;
        do {
            if (current(p)->type == TOK_LBRACE) {
                depth++;
            } else if (current(p)->type == TOK_RBRACE) {
                depth--;
            }
            if (!append_body_token(body, sizeof(body), current(p))) {
                py_error(p->py, "function body too long");
                return 0;
            }
            p->pos++;
        } while (depth > 0 && current(p)->type != TOK_EOF);

        if (depth != 0) {
            py_error(p->py, "unterminated function body");
            return 0;
        }
    } else {
        while (current(p)->type != TOK_EOF && current(p)->type != TOK_SEMI) {
            if (!append_body_token(body, sizeof(body), current(p))) {
                py_error(p->py, "function body too long");
                return 0;
            }
            p->pos++;
        }
    }
    while (body[0] != '\0' && body[strlen(body) - 1] == ' ') {
        body[strlen(body) - 1] = '\0';
    }
    if (body[0] == '\0') {
        py_error(p->py, "expected function body");
        return 0;
    }
    if (!p->exec_enabled) {
        return 1;
    }
    return py_set_func(p->py, name.text, params, param_count, body);
}

static int is_assignment_operator(token_type_t type) {
    return type == TOK_ASSIGN ||
        type == TOK_PLUS_ASSIGN ||
        type == TOK_MINUS_ASSIGN ||
        type == TOK_STAR_ASSIGN ||
        type == TOK_SLASH_ASSIGN ||
        type == TOK_PERCENT_ASSIGN ||
        type == TOK_AMP_ASSIGN ||
        type == TOK_PIPE_ASSIGN ||
        type == TOK_CARET_ASSIGN ||
        type == TOK_LSHIFT_ASSIGN ||
        type == TOK_RSHIFT_ASSIGN;
}

static token_type_t assignment_to_binary(token_type_t type) {
    if (type == TOK_PLUS_ASSIGN) {
        return TOK_PLUS;
    }
    if (type == TOK_MINUS_ASSIGN) {
        return TOK_MINUS;
    }
    if (type == TOK_STAR_ASSIGN) {
        return TOK_STAR;
    }
    if (type == TOK_SLASH_ASSIGN) {
        return TOK_SLASH;
    }
    if (type == TOK_PERCENT_ASSIGN) {
        return TOK_PERCENT;
    }
    if (type == TOK_AMP_ASSIGN) {
        return TOK_AMP;
    }
    if (type == TOK_PIPE_ASSIGN) {
        return TOK_PIPE;
    }
    if (type == TOK_CARET_ASSIGN) {
        return TOK_CARET;
    }
    if (type == TOK_LSHIFT_ASSIGN) {
        return TOK_LSHIFT;
    }
    if (type == TOK_RSHIFT_ASSIGN) {
        return TOK_RSHIFT;
    }
    return TOK_ASSIGN;
}

static int parse_assignment(parser_t *p) {
    token_t name = *current(p);
    token_type_t assign_type;
    py_value_t value;
    py_value_t indices[PY_MAX_PARAMS];
    size_t index_count = 0;
    size_t i;
    py_var_t *target_var;
    py_value_t target;

    if (!expect(p, TOK_IDENT, "expected variable name")) {
        return 0;
    }
    while (match(p, TOK_LBRACKET)) {
        if (index_count >= PY_MAX_PARAMS) {
            py_error(p->py, "too many subscript levels");
            return 0;
        }
        if (!p->exec_enabled) {
            while (current(p)->type != TOK_EOF &&
                   current(p)->type != TOK_SEMI &&
                   current(p)->type != TOK_RBRACE &&
                   current(p)->type != TOK_ELIF &&
                   current(p)->type != TOK_ELSE) {
                p->pos++;
            }
            return 1;
        }
        indices[index_count++] = parse_expression(p);
        if (py_has_error(p->py)) {
            return 0;
        }
        if (!expect(p, TOK_RBRACKET, "expected ']' after assignment target")) {
            return 0;
        }
    }
    assign_type = current(p)->type;
    if (!is_assignment_operator(assign_type)) {
        py_error(p->py, "expected assignment operator");
        return 0;
    }
    p->pos++;
    if (!p->exec_enabled) {
        while (current(p)->type != TOK_EOF &&
               current(p)->type != TOK_SEMI &&
               current(p)->type != TOK_RBRACE &&
               current(p)->type != TOK_ELIF &&
               current(p)->type != TOK_ELSE) {
            p->pos++;
        }
        return 1;
    }
    value = parse_expression(p);
    if (py_has_error(p->py)) {
        return 0;
    }
    if (assign_type != TOK_ASSIGN) {
        if (index_count > 0) {
            target_var = py_find_var(p->py, name.text);
            if (target_var == NULL) {
                py_error(p->py, "undefined variable");
                return 0;
            }
            target = target_var->value;
            for (i = 0; i + 1 < index_count; ++i) {
                target = py_subscript(p, target, indices[i]);
                if (py_has_error(p->py)) {
                    return 0;
                }
            }
            value = eval_binary(p, py_subscript(p, target, indices[index_count - 1]), assignment_to_binary(assign_type), value);
        } else {
            value = eval_binary(p, py_get_var(p->py, name.text), assignment_to_binary(assign_type), value);
        }
        if (py_has_error(p->py)) {
            return 0;
        }
    }
    if (!p->exec_enabled) {
        return 1;
    }
    if (index_count > 0) {
        target_var = py_find_var(p->py, name.text);
        if (target_var == NULL) {
            py_error(p->py, "undefined variable");
            return 0;
        }
        target = target_var->value;
        for (i = 0; i + 1 < index_count; ++i) {
            target = py_subscript(p, target, indices[i]);
            if (py_has_error(p->py)) {
                return 0;
            }
        }
        return py_assign_subscript(p, &target, indices[index_count - 1], value);
    }
    return py_set_var(p->py, name.text, value);
}

static int parse_multi_assignment(parser_t *p) {
    token_t names[PY_MAX_PARAMS];
    py_value_t values[PY_MAX_PARAMS];
    size_t name_count = 0;
    size_t value_count = 0;

    do {
        if (name_count >= PY_MAX_PARAMS) {
            py_error(p->py, "too many assignment targets");
            return 0;
        }
        names[name_count] = *current(p);
        if (!expect(p, TOK_IDENT, "expected assignment target")) {
            return 0;
        }
        name_count++;
    } while (match(p, TOK_COMMA));

    if (!expect(p, TOK_ASSIGN, "expected '='")) {
        return 0;
    }
    if (!p->exec_enabled) {
        while (current(p)->type != TOK_EOF &&
               current(p)->type != TOK_SEMI &&
               current(p)->type != TOK_RBRACE &&
               current(p)->type != TOK_ELIF &&
               current(p)->type != TOK_ELSE) {
            p->pos++;
        }
        return 1;
    }

    do {
        if (value_count >= PY_MAX_PARAMS) {
            py_error(p->py, "too many assignment values");
            return 0;
        }
        values[value_count++] = parse_expression(p);
        if (py_has_error(p->py)) {
            return 0;
        }
    } while (match(p, TOK_COMMA));

    if (name_count != value_count) {
        py_error(p->py, "assignment count mismatch");
        return 0;
    }
    if (!p->exec_enabled) {
        return 1;
    }
    for (value_count = 0; value_count < name_count; ++value_count) {
        if (!py_set_var(p->py, names[value_count].text, values[value_count])) {
            return 0;
        }
    }
    return 1;
}

static int parse_if(parser_t *p) {
    py_value_t condition;
    py_value_t elif_condition;
    int previous_exec_enabled;
    int condition_true;
    int branch_taken;

    if (!expect(p, TOK_IF, "expected 'if'")) {
        return 0;
    }
    if (!p->exec_enabled) {
        while (current(p)->type != TOK_COLON && current(p)->type != TOK_EOF) {
            p->pos++;
        }
        if (!expect(p, TOK_COLON, "expected ':' after if condition")) {
            return 0;
        }
        if (!parse_block(p)) {
            return 0;
        }
        while (match(p, TOK_ELIF)) {
            while (current(p)->type != TOK_COLON && current(p)->type != TOK_EOF) {
                p->pos++;
            }
            if (!expect(p, TOK_COLON, "expected ':' after elif condition")) {
                return 0;
            }
            if (!parse_block(p)) {
                return 0;
            }
        }
        if (match(p, TOK_ELSE)) {
            if (!expect(p, TOK_COLON, "expected ':' after else")) {
                return 0;
            }
            if (!parse_block(p)) {
                return 0;
            }
        }
        return 1;
    }
    condition = parse_expression(p);
    if (!expect(p, TOK_COLON, "expected ':' after if condition")) {
        return 0;
    }

    condition_true = py_truthy(condition);
    branch_taken = condition_true;
    previous_exec_enabled = p->exec_enabled;
    p->exec_enabled = previous_exec_enabled && condition_true;
    if (!parse_block(p)) {
        p->exec_enabled = previous_exec_enabled;
        return 0;
    }

    while (match(p, TOK_ELIF)) {
        elif_condition = parse_expression(p);
        if (!expect(p, TOK_COLON, "expected ':' after elif condition")) {
            p->exec_enabled = previous_exec_enabled;
            return 0;
        }

        condition_true = !branch_taken && py_truthy(elif_condition);
        p->exec_enabled = previous_exec_enabled && condition_true;
        if (!parse_block(p)) {
            p->exec_enabled = previous_exec_enabled;
            return 0;
        }
        if (condition_true) {
            branch_taken = 1;
        }
    }

    if (match(p, TOK_ELSE)) {
        if (!expect(p, TOK_COLON, "expected ':' after else")) {
            p->exec_enabled = previous_exec_enabled;
            return 0;
        }
        p->exec_enabled = previous_exec_enabled && !branch_taken;
        if (!parse_block(p)) {
            p->exec_enabled = previous_exec_enabled;
            return 0;
        }
    }
    p->exec_enabled = previous_exec_enabled;
    return 1;
}

static int parse_while(parser_t *p) {
    size_t condition_start;
    size_t condition_end;
    size_t body_start;
    size_t after_loop;
    char condition_src[PY_MAX_LINE];
    py_value_t condition;
    int previous_exec_enabled;
    int guard = 0;

    if (!expect(p, TOK_WHILE, "expected 'while'")) {
        return 0;
    }
    if (!p->exec_enabled) {
        while (current(p)->type != TOK_COLON && current(p)->type != TOK_EOF) {
            p->pos++;
        }
        if (!expect(p, TOK_COLON, "expected ':' after while condition")) {
            return 0;
        }
        return parse_block(p);
    }
    condition_start = p->pos;
    condition = parse_expression(p);
    condition_end = p->pos;
    if (!expect(p, TOK_COLON, "expected ':' after while condition")) {
        return 0;
    }
    condition_src[0] = '\0';
    while (condition_start < condition_end) {
        if (!append_body_token(condition_src, sizeof(condition_src), &p->tokens[condition_start])) {
            py_error(p->py, "while condition too long");
            return 0;
        }
        condition_start++;
    }
    body_start = p->pos;

    previous_exec_enabled = p->exec_enabled;
    p->exec_enabled = 0;
    if (!parse_block(p)) {
        p->exec_enabled = previous_exec_enabled;
        return 0;
    }
    after_loop = p->pos;
    p->exec_enabled = previous_exec_enabled;

    while (!py_has_error(p->py) && previous_exec_enabled && py_truthy(condition)) {
        if (++guard > 10000) {
            py_error(p->py, "while loop limit reached");
            return 0;
        }
        p->pos = body_start;
        p->exec_enabled = 1;
        if (!parse_block(p)) {
            p->exec_enabled = previous_exec_enabled;
            return 0;
        }
        p->exec_enabled = previous_exec_enabled;
        if (p->loop_signal == 1) {
            p->loop_signal = 0;
            break;
        }
        if (p->loop_signal == 2) {
            p->loop_signal = 0;
        }
        condition = py_eval_expression(p->py, condition_src);
    }

    p->pos = after_loop;
    return !py_has_error(p->py);
}

static int parse_for(parser_t *p) {
    token_t var_name;
    py_value_t args[3];
    py_var_t saved_var;
    py_var_t *existing_var;
    int had_var = 0;
    int argc = 0;
    int start = 0;
    int stop = 0;
    int step = 1;
    int i;
    int previous_exec_enabled;
    size_t body_start;
    size_t after_loop;
    int guard = 0;

    if (!expect(p, TOK_FOR, "expected 'for'")) {
        return 0;
    }
    if (!p->exec_enabled) {
        while (current(p)->type != TOK_COLON && current(p)->type != TOK_EOF) {
            p->pos++;
        }
        if (!expect(p, TOK_COLON, "expected ':' after for range")) {
            return 0;
        }
        return parse_block(p);
    }
    var_name = *current(p);
    if (!expect(p, TOK_IDENT, "expected loop variable")) {
        return 0;
    }
    if (!expect(p, TOK_IN, "expected 'in' after loop variable")) {
        return 0;
    }
    if (!expect(p, TOK_RANGE, "expected range(...)")) {
        return 0;
    }
    if (!expect(p, TOK_LPAREN, "expected '(' after range")) {
        return 0;
    }

    if (!match(p, TOK_RPAREN)) {
        do {
            if (argc >= 3) {
                py_error(p->py, "range() takes at most three arguments");
                return 0;
            }
            args[argc++] = parse_expression(p);
            if (py_has_error(p->py)) {
                return 0;
            }
        } while (match(p, TOK_COMMA));

        if (!expect(p, TOK_RPAREN, "expected ')' after range arguments")) {
            return 0;
        }
    }

    if (argc == 0) {
        py_error(p->py, "range() expects at least one argument");
        return 0;
    }
    if (argc == 1) {
        stop = read_int_arg(p, args[0], "range() expects integers");
    } else if (argc == 2) {
        start = read_int_arg(p, args[0], "range() expects integers");
        stop = read_int_arg(p, args[1], "range() expects integers");
    } else {
        start = read_int_arg(p, args[0], "range() expects integers");
        stop = read_int_arg(p, args[1], "range() expects integers");
        step = read_int_arg(p, args[2], "range() expects integers");
    }
    if (py_has_error(p->py)) {
        return 0;
    }
    if (step == 0) {
        py_error(p->py, "range() step cannot be zero");
        return 0;
    }
    if (!expect(p, TOK_COLON, "expected ':' after for range")) {
        return 0;
    }

    body_start = p->pos;
    previous_exec_enabled = p->exec_enabled;

    existing_var = py_find_var(p->py, var_name.text);
    if (existing_var != NULL) {
        had_var = 1;
        saved_var = *existing_var;
    }
    if (!py_set_var(p->py, var_name.text, py_int(start))) {
        return 0;
    }

    p->exec_enabled = 0;
    if (!parse_block(p)) {
        p->exec_enabled = previous_exec_enabled;
        return 0;
    }
    after_loop = p->pos;
    p->exec_enabled = previous_exec_enabled;

    if (had_var) {
        existing_var = py_find_var(p->py, var_name.text);
        if (existing_var != NULL) {
            *existing_var = saved_var;
        }
    } else {
        size_t index;
        for (index = 0; index < p->py->var_count; ++index) {
            if (strncmp(p->py->vars[index].name, var_name.text, PY_MAX_NAME) == 0) {
                size_t j;
                for (j = index + 1; j < p->py->var_count; ++j) {
                    p->py->vars[j - 1] = p->py->vars[j];
                }
                p->py->var_count--;
                break;
            }
        }
    }

    for (i = start;
         !py_has_error(p->py) && previous_exec_enabled && ((step > 0) ? (i < stop) : (i > stop));
         i += step) {
        if (++guard > 10000) {
            py_error(p->py, "for loop limit reached");
            return 0;
        }
        if (!py_set_var(p->py, var_name.text, py_int(i))) {
            return 0;
        }
        p->pos = body_start;
        p->exec_enabled = 1;
        if (!parse_block(p)) {
            p->exec_enabled = previous_exec_enabled;
            return 0;
        }
        p->exec_enabled = previous_exec_enabled;
        if (p->loop_signal == 1) {
            p->loop_signal = 0;
            break;
        }
        if (p->loop_signal == 2) {
            p->loop_signal = 0;
        }
    }

    p->pos = after_loop;
    return !py_has_error(p->py);
}

static int parse_expression_statement(parser_t *p) {
    if (!p->exec_enabled) {
        while (current(p)->type != TOK_EOF &&
               current(p)->type != TOK_SEMI &&
               current(p)->type != TOK_RBRACE &&
               current(p)->type != TOK_ELIF &&
               current(p)->type != TOK_ELSE) {
            p->pos++;
        }
        return 1;
    }
    py_value_t value = parse_expression(p);
    if (py_has_error(p->py)) {
        return 0;
    }
    if (value.type == PY_VALUE_NONE) {
        return 1;
    }
    return emit_value_line(p, value);
}

static int parse_method_call(parser_t *p) {
    token_t target = *current(p);
    token_t method;
    py_value_t args[PY_MAX_PARAMS];
    int argc = 0;
    py_var_t *var;

    if (!expect(p, TOK_IDENT, "expected method target")) {
        return 0;
    }
    if (!expect(p, TOK_DOT, "expected '.'")) {
        return 0;
    }
    method = *current(p);
    if (!expect(p, TOK_IDENT, "expected method name")) {
        return 0;
    }
    if (!expect(p, TOK_LPAREN, "expected '(' after method name")) {
        return 0;
    }

    if (!p->exec_enabled) {
        int depth = 1;
        while (current(p)->type != TOK_EOF && depth > 0) {
            if (current(p)->type == TOK_LPAREN) {
                depth++;
            } else if (current(p)->type == TOK_RPAREN) {
                depth--;
            }
            p->pos++;
        }
        return depth == 0;
    }

    if (!match(p, TOK_RPAREN)) {
        do {
            if (argc >= PY_MAX_PARAMS) {
                py_error(p->py, "too many method arguments");
                return 0;
            }
            args[argc++] = parse_expression(p);
            if (py_has_error(p->py)) {
                return 0;
            }
        } while (match(p, TOK_COMMA));

        if (!expect(p, TOK_RPAREN, "expected ')' after method arguments")) {
            return 0;
        }
    }

    var = py_find_var(p->py, target.text);
    if (var == NULL) {
        py_error(p->py, "undefined variable");
        return 0;
    }
    if (strcmp(method.text, "append") == 0) {
        if (argc != 1) {
            py_error(p->py, "append() expects one argument");
            return 0;
        }
        return py_list_append(p->py, &var->value, args[0]);
    }

    py_error(p->py, "unknown method");
    return 0;
}

static int parse_global(parser_t *p) {
    if (!expect(p, TOK_GLOBAL, "expected 'global'")) {
        return 0;
    }
    do {
        if (!expect(p, TOK_IDENT, "expected global name")) {
            return 0;
        }
    } while (match(p, TOK_COMMA));
    return 1;
}

static int parse_statement(parser_t *p) {
    token_t *token = current(p);

    if (token->type == TOK_EOF) {
        return 1;
    }
    if (token->type == TOK_PASS) {
        p->pos++;
        return 1;
    }
    if (token->type == TOK_BREAK) {
        p->pos++;
        if (p->exec_enabled) {
            p->loop_signal = 1;
        }
        return 1;
    }
    if (token->type == TOK_CONTINUE) {
        p->pos++;
        if (p->exec_enabled) {
            p->loop_signal = 2;
        }
        return 1;
    }
    if (token->type == TOK_GLOBAL) {
        return parse_global(p);
    }
    if (token->type == TOK_LBRACE) {
        return parse_block(p);
    }
    if (token->type == TOK_PRINT) {
        return parse_print(p);
    }
    if (token->type == TOK_DEF) {
        return parse_def(p);
    }
    if (token->type == TOK_IF) {
        return parse_if(p);
    }
    if (token->type == TOK_WHILE) {
        return parse_while(p);
    }
    if (token->type == TOK_FOR) {
        return parse_for(p);
    }
    if (token->type == TOK_IDENT && peek(p, 1)->type == TOK_DOT) {
        return parse_method_call(p);
    }
    if (token->type == TOK_IDENT && peek(p, 1)->type == TOK_COMMA) {
        return parse_multi_assignment(p);
    }
    if (token->type == TOK_IDENT &&
        (is_assignment_operator(peek(p, 1)->type) || peek(p, 1)->type == TOK_LBRACKET)) {
        return parse_assignment(p);
    }
    return parse_expression_statement(p);
}

static int parse_statement_list(parser_t *p) {
    while (current(p)->type != TOK_EOF &&
           current(p)->type != TOK_ELIF &&
           current(p)->type != TOK_ELSE &&
           current(p)->type != TOK_RBRACE) {
        if (current(p)->type == TOK_SEMI) {
            p->pos++;
            continue;
        }
        if (!parse_statement(p)) {
            return 0;
        }
        if (p->loop_signal) {
            return 1;
        }
        if (current(p)->type == TOK_SEMI) {
            p->pos++;
        } else if (current(p)->type != TOK_EOF &&
                   current(p)->type != TOK_ELIF &&
                   current(p)->type != TOK_ELSE &&
                   current(p)->type != TOK_RBRACE) {
            py_error(p->py, "expected ';'");
            return 0;
        }
    }
    return 1;
}

static int parse_block(parser_t *p) {
    if (match(p, TOK_LBRACE)) {
        if (!parse_statement_list(p)) {
            return 0;
        }
        if (p->loop_signal) {
            int depth = 1;
            while (current(p)->type != TOK_EOF && depth > 0) {
                if (current(p)->type == TOK_LBRACE) {
                    depth++;
                } else if (current(p)->type == TOK_RBRACE) {
                    depth--;
                }
                p->pos++;
            }
            return depth == 0;
        }
        return expect(p, TOK_RBRACE, "expected '}' after block");
    }
    return parse_statement(p);
}

void py_init(py_t *py) {
    if (py == NULL) {
        return;
    }
    memset(py, 0, sizeof(*py));
    py_set_var(py, "LOW", py_int(0));
    py_set_var(py, "HIGH", py_int(1));
    py_set_var(py, "INPUT", py_int(0));
    py_set_var(py, "OUTPUT", py_int(1));
}

void py_deinit(py_t *py) {
    py_object_t *object;

    if (py == NULL) {
        return;
    }
    object = py->objects;
    while (object != NULL) {
        py_object_t *next = object->next;
        free(object->items);
        free(object->entries);
        free(object);
        object = next;
    }
    py->objects = NULL;
}

void py_set_output_callback(py_t *py, void (*callback)(const char *text, void *user_data), void *user_data) {
    if (py == NULL) {
        return;
    }
    py->output_callback = callback;
    py->output_user_data = user_data;
}

void py_set_input_callback(py_t *py, int (*callback)(char *buffer, size_t buffer_size, void *user_data), void *user_data) {
    if (py == NULL) {
        return;
    }
    py->input_callback = callback;
    py->input_user_data = user_data;
}

void py_set_gpio_callbacks(py_t *py,
                           int (*mode_callback)(int pin, int mode, void *user_data),
                           int (*write_callback)(int pin, int value, void *user_data),
                           int (*read_callback)(int pin, int *value, void *user_data),
                           void *user_data) {
    if (py == NULL) {
        return;
    }
    py->gpio_mode_callback = mode_callback;
    py->gpio_write_callback = write_callback;
    py->gpio_read_callback = read_callback;
    py->gpio_user_data = user_data;
}

static void py_stdio_output(const char *text, void *user_data) {
    FILE *stream = user_data == NULL ? stdout : (FILE *)user_data;
    fputs(text, stream);
    fflush(stream);
}

static int py_stdio_input(char *buffer, size_t buffer_size, void *user_data) {
    FILE *stream = user_data == NULL ? stdin : (FILE *)user_data;
    if (buffer == NULL || buffer_size == 0) {
        return 0;
    }
    if (fgets(buffer, (int)buffer_size, stream) == NULL) {
        buffer[0] = '\0';
        return 0;
    }
    return 1;
}

void py_use_stdio(py_t *py) {
    if (py == NULL) {
        return;
    }
    py_set_output_callback(py, py_stdio_output, stdout);
    py_set_input_callback(py, py_stdio_input, stdin);
}

static py_value_t py_eval_expression(py_t *py, const char *source) {
    parser_t *parser;
    py_value_t value;

    parser = parser_alloc();
    if (parser == NULL) {
        py_error(py, "out of memory");
        return py_none();
    }

    parser->py = py;
    parser->source = source;
    parser->exec_enabled = 1;
    py->error[0] = '\0';

    if (!lex(parser)) {
        free(parser);
        return py_none();
    }
    value = parse_expression(parser);
    if (py_has_error(py)) {
        free(parser);
        return py_none();
    }
    if (!expect(parser, TOK_EOF, "unexpected trailing input")) {
        free(parser);
        return py_none();
    }
    free(parser);
    return value;
}

static py_value_t py_eval_fstring(parser_t *p, const char *source) {
    char result[PY_MAX_STRING];
    size_t out = 0;
    size_t i = 0;

    result[0] = '\0';
    while (source[i] != '\0') {
        if (source[i] == '{' && source[i + 1] == '{') {
            if (out + 1 >= sizeof(result)) {
                py_error(p->py, "f-string too long");
                return py_none();
            }
            result[out++] = '{';
            i += 2;
            continue;
        }
        if (source[i] == '}' && source[i + 1] == '}') {
            if (out + 1 >= sizeof(result)) {
                py_error(p->py, "f-string too long");
                return py_none();
            }
            result[out++] = '}';
            i += 2;
            continue;
        }
        if (source[i] == '{') {
            char expr[PY_MAX_LINE];
            char value_text[PY_MAX_STRING];
            py_value_t value;
            size_t expr_len = 0;

            i++;
            while (source[i] != '\0' && source[i] != '}' && expr_len + 1 < sizeof(expr)) {
                expr[expr_len++] = source[i++];
            }
            expr[expr_len] = '\0';
            if (source[i] != '}') {
                py_error(p->py, "unterminated f-string expression");
                return py_none();
            }
            i++;

            value = py_eval_expression(p->py, expr);
            if (py_has_error(p->py)) {
                return py_none();
            }
            py_value_to_string(&value, value_text, sizeof(value_text));
            if (out + strlen(value_text) >= sizeof(result)) {
                py_error(p->py, "f-string too long");
                return py_none();
            }
            memcpy(result + out, value_text, strlen(value_text));
            out += strlen(value_text);
            continue;
        }
        if (source[i] == '}') {
            py_error(p->py, "single '}' in f-string");
            return py_none();
        }
        if (out + 1 >= sizeof(result)) {
            py_error(p->py, "f-string too long");
            return py_none();
        }
        result[out++] = source[i++];
    }
    result[out] = '\0';
    return py_string(result);
}

int py_run(py_t *py, const char *line, char *output, size_t output_size) {
    parser_t *parser;
    int ok;

    if (py == NULL || line == NULL) {
        return 0;
    }

    parser = parser_alloc();
    if (parser == NULL) {
        py_error(py, "out of memory");
        return 0;
    }

    parser->py = py;
    parser->source = line;
    parser->output = output;
    parser->output_size = output_size;
    parser->exec_enabled = 1;
    py->error[0] = '\0';
    py->error_line = 0;
    py->error_col = 0;
    py->current_line = 0;
    py->current_col = 0;

    if (output != NULL && output_size > 0) {
        output[0] = '\0';
    }

    if (!lex(parser)) {
        free(parser);
        return 0;
    }
    if (!parse_statement_list(parser)) {
        free(parser);
        return 0;
    }
    if (!expect(parser, TOK_EOF, "unexpected trailing input")) {
        free(parser);
        return 0;
    }
    ok = !py_has_error(py);
    free(parser);
    return ok;
}

static char *skip_indent(char *line) {
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    return line;
}

static int starts_with_word(const char *line, const char *word) {
    size_t len;

    while (*line == ' ' || *line == '\t') {
        line++;
    }
    len = strlen(word);
    return strncmp(line, word, len) == 0 &&
        (line[len] == ' ' || line[len] == '\t' || line[len] == ':' || line[len] == '\0');
}

static int continues_if_chain(const char *line) {
    return starts_with_word(line, "elif") || starts_with_word(line, "else");
}

static int line_ends_colon(const char *line) {
    const char *end = line + strlen(line);
    while (end > line && isspace((unsigned char)end[-1])) {
        end--;
    }
    return end > line && end[-1] == ':';
}

static int count_indent(const char *line) {
    int indent = 0;
    while (*line == ' ' || *line == '\t') {
        indent += (*line == '\t') ? 4 : 1;
        line++;
    }
    return indent;
}

static int append_program_text(py_t *py, char *dest, size_t dest_size, const char *text) {
    size_t used;
    size_t len = strlen(text);

    used = strlen(dest);
    if (used + len + 1 > dest_size) {
        py_error(py, "program too long");
        return 0;
    }
    memcpy(dest + used, text, len);
    dest[used + len] = '\0';
    return 1;
}

static char last_non_space(const char *text) {
    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        len--;
    }
    return len == 0 ? '\0' : text[len - 1];
}

static int append_source_line(py_t *py, char *program, size_t program_size, const char *line, int *indents, int *depth, int *previous_colon) {
    char clean[PY_MAX_LINE];
    char *trimmed;
    size_t len;
    int indent;

    trimmed = skip_indent((char *)line);
    if (trimmed[0] == '\0' || trimmed[0] == '#') {
        if (!append_program_text(py, program, program_size, "\n")) {
            return 0;
        }
        return 1;
    }

    indent = count_indent(line);
    while (*depth > 0 && indent < indents[*depth]) {
        if (!append_program_text(py, program, program_size, " }")) {
            return 0;
        }
        (*depth)--;
        *previous_colon = 0;
    }
    if (indent > indents[*depth]) {
        if (!*previous_colon) {
            py->current_col = (size_t)indent + 1;
            py_error(py, "unexpected indent");
            return 0;
        }
        if (*depth + 1 >= 16) {
            py_error(py, "too many nested blocks");
            return 0;
        }
        if (!append_program_text(py, program, program_size, " {\n")) {
            return 0;
        }
        (*depth)++;
        indents[*depth] = indent;
    } else if (indent != indents[*depth]) {
        py->current_col = (size_t)indent + 1;
        py_error(py, "bad indentation");
        return 0;
    }

    len = strlen(trimmed);
    while (len > 0 && isspace((unsigned char)trimmed[len - 1])) {
        len--;
    }
    if (len + 1 > sizeof(clean)) {
        py_error(py, "line too long");
        return 0;
    }
    memcpy(clean, trimmed, len);
    clean[len] = '\0';

    if (program[0] != '\0') {
        char last = last_non_space(program);
        if (last == '{' || continues_if_chain(clean)) {
            if (!append_program_text(py, program, program_size, " ")) {
                return 0;
            }
        } else {
            if (!append_program_text(py, program, program_size, ";\n")) {
                return 0;
            }
        }
    }
    if (!append_program_text(py, program, program_size, clean)) {
        return 0;
    }
    *previous_colon = line_ends_colon(clean);
    return 1;
}

int py_run_source(py_t *py, const char *source, char *output, size_t output_size) {
    char line[PY_MAX_LINE];
    char program[PY_MAX_PROGRAM];
    int indents[16];
    int depth = 0;
    int previous_colon = 0;
    size_t line_len = 0;
    size_t source_line = 1;
    const char *s;

    if (py == NULL || source == NULL) {
        return 0;
    }
    if (output != NULL && output_size > 0) {
        output[0] = '\0';
    }

    py->error[0] = '\0';
    py->error_line = 0;
    py->error_col = 0;
    py->current_line = 0;
    py->current_col = 0;
    program[0] = '\0';
    indents[0] = 0;
    for (s = source; ; ++s) {
        if (*s == '\n' || *s == '\0') {
            line[line_len] = '\0';
            py->current_line = source_line;
            py->current_col = 1;
            if (!append_source_line(py, program, sizeof(program), line, indents, &depth, &previous_colon)) {
                return 0;
            }

            line_len = 0;
            if (*s == '\0') {
                break;
            }
            source_line++;
            continue;
        }

        if (line_len + 1 >= sizeof(line)) {
            py->current_line = source_line;
            py->current_col = line_len + 1;
            py_error(py, "line too long");
            return 0;
        }
        line[line_len++] = *s;
    }

    while (depth > 0) {
        if (!append_program_text(py, program, sizeof(program), " }")) {
            return 0;
        }
        depth--;
    }
    return py_run(py, program, output, output_size);
}

int py_run_file(py_t *py, const char *path, char *output, size_t output_size) {
    FILE *file;
    char line[PY_MAX_LINE];
    char program[PY_MAX_PROGRAM];
    int indents[16];
    int depth = 0;
    int previous_colon = 0;
    size_t source_line = 1;

    if (py == NULL || path == NULL) {
        return 0;
    }
    if (output != NULL && output_size > 0) {
        output[0] = '\0';
    }
    py->error[0] = '\0';
    py->error_line = 0;
    py->error_col = 0;
    py->current_line = 0;
    py->current_col = 0;

    file = fopen(path, "r");
    if (file == NULL) {
        py_error(py, "could not open file");
        return 0;
    }
    program[0] = '\0';
    indents[0] = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        py->current_line = source_line;
        py->current_col = 1;
        if (strchr(line, '\n') == NULL && !feof(file)) {
            fclose(file);
            py->current_col = strlen(line);
            py_error(py, "line too long");
            return 0;
        }
        if (!append_source_line(py, program, sizeof(program), line, indents, &depth, &previous_colon)) {
            fclose(file);
            return 0;
        }
        source_line++;
    }

    while (depth > 0) {
        if (!append_program_text(py, program, sizeof(program), " }")) {
            fclose(file);
            return 0;
        }
        depth--;
    }

    fclose(file);
    return py_run(py, program, output, output_size);
}

#ifdef PY_DESKTOP_MAIN
int main(int argc, char **argv) {
    py_t py;
    char output[512];

    py_init(&py);
    py_use_stdio(&py);
    if (argc > 1) {
        if (!py_run_file(&py, argv[1], NULL, 0)) {
            printf("error: %s\n", py.error);
            py_deinit(&py);
            return 1;
        }
        py_deinit(&py);
        return 0;
    }

    while (fgets(output, sizeof(output), stdin) != NULL) {
        char result[512];
        if (!py_run(&py, output, result, sizeof(result))) {
            printf("error: %s\n", py.error);
        } else {
            printf("%s", result);
        }
    }
    py_deinit(&py);
    return 0;
}
#endif
