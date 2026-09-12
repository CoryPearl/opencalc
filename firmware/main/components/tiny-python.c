#ifndef ESP_PLATFORM
#define _POSIX_C_SOURCE 200809L
#endif

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
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__GNUC__)
#define PY_NOINLINE __attribute__((noinline))
#else
#define PY_NOINLINE
#endif

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "opencalc_config.h"
#endif

static void *py_heap_calloc(size_t count, size_t size) {
#ifdef ESP_PLATFORM
    if (size != 0 && count > SIZE_MAX / size) return NULL;
    size_t bytes = count * size;
    size_t free_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (bytes > free_bytes || free_bytes - bytes < OPENCALC_PSRAM_RESERVE_BYTES) return NULL;
    return heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return calloc(count, size);
#endif
}

static void *py_heap_realloc(void *memory, size_t size) {
#ifdef ESP_PLATFORM
    size_t free_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (size > free_bytes || free_bytes - size < OPENCALC_PSRAM_RESERVE_BYTES) return NULL;
    return heap_caps_realloc(memory, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return realloc(memory, size);
#endif
}

static void py_heap_free(void *memory) {
#ifdef ESP_PLATFORM
    heap_caps_free(memory);
#else
    free(memory);
#endif
}

#ifndef PY_MAX_TOKENS
#define PY_MAX_TOKENS 8192
#endif

#ifndef PY_MAX_LINE
#define PY_MAX_LINE 512
#endif

#ifndef PY_MAX_PROGRAM
#define PY_MAX_PROGRAM 65536
#endif

typedef enum {
    TOK_EOF = 0,
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,
    TOK_FSTRING,
    TOK_BYTES,
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
    TOK_NONLOCAL,
    TOK_IMPORT,
    TOK_FROM,
    TOK_TRY,
    TOK_EXCEPT,
    TOK_FINALLY,
    TOK_RAISE,
    TOK_WITH,
    TOK_AS,
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
    uint8_t marked;
    char tag[PY_MAX_NAME];
    size_t count;
    size_t capacity;
    py_value_t *items;
    py_dict_entry_t *entries;
    py_object_t *next;
};

struct py_frame {
    py_var_t vars[PY_MAX_LOCALS];
    size_t var_count;
    char globals[PY_MAX_GLOBAL_DECLS][PY_MAX_NAME];
    size_t global_count;
    char nonlocals[PY_MAX_GLOBAL_DECLS][PY_MAX_NAME];
    size_t nonlocal_count;
    py_func_t *owner;
    py_frame_t *parent;
};

typedef struct {
    token_type_t type;
    int64_t int_value;
    double float_value;
    size_t line;
    size_t col;
    char text[PY_MAX_STRING];
} token_t;

typedef struct {
    py_t *py;
    const char *source;
    token_t *tokens;
    size_t token_count;
    size_t token_capacity;
    size_t pos;
    char *output;
    size_t output_size;
    size_t output_len;
    int exec_enabled;
    unsigned eval_suppressed;
    int loop_signal;
    int return_signal;
    py_value_t return_value;
    unsigned expression_depth;
} parser_t;

#define PY_MAX_EXPRESSION_DEPTH 12U

#ifdef ESP_PLATFORM
#ifndef PY_PARSER_POOL_SIZE
#define PY_PARSER_POOL_SIZE 6
#endif
static EXT_RAM_BSS_ATTR parser_t s_parser_pool[PY_PARSER_POOL_SIZE];
static int s_parser_pool_used[PY_PARSER_POOL_SIZE];
#endif

static parser_t *parser_alloc(void) {
#ifdef ESP_PLATFORM
    for (int i = 0; i < PY_PARSER_POOL_SIZE; i++) {
        if (s_parser_pool_used[i]) {
            continue;
        }
        memset(&s_parser_pool[i], 0, sizeof(s_parser_pool[i]));
        s_parser_pool_used[i] = 1;
        return &s_parser_pool[i];
    }
    return NULL;
#else
    return (parser_t *)calloc(1, sizeof(parser_t));
#endif
}

static void parser_free(parser_t *parser) {
    if (parser == NULL) {
        return;
    }
#ifdef ESP_PLATFORM
    for (int i = 0; i < PY_PARSER_POOL_SIZE; i++) {
        if (&s_parser_pool[i] == parser) {
            py_heap_free(parser->tokens);
            parser->tokens = NULL;
            parser->token_capacity = 0;
            s_parser_pool_used[i] = 0;
            return;
        }
    }
    return;
#else
    py_heap_free(parser->tokens);
    free(parser);
#endif
}

static char *program_buffer_alloc(void) {
    return (char *)py_heap_calloc(1, PY_MAX_PROGRAM);
}

static void program_buffer_free(char *program) {
    py_heap_free(program);
}

static void py_build_traceback(py_t *py) {
    size_t used = 0;
    if (py == NULL) return;
    py->traceback[0] = '\0';
    for (size_t i = 0; i < py->call_depth && i < PY_MAX_TRACE_DEPTH; i++) {
        int written = snprintf(py->traceback + used, sizeof(py->traceback) - used,
                               "  at %s line %u\n", py->call_stack[i],
                               (unsigned)py->call_lines[i]);
        if (written < 0 || (size_t)written >= sizeof(py->traceback) - used) break;
        used += (size_t)written;
    }
    if (used < sizeof(py->traceback)) {
        snprintf(py->traceback + used, sizeof(py->traceback) - used,
                 "  at <script> line %u", (unsigned)py->current_line);
    }
}

static void py_error(py_t *py, const char *message) {
    if (py->exception_type[0] == '\0') {
        const char *type = "RuntimeError";
        if (strstr(message, "division by zero") != NULL || strstr(message, "modulo by zero") != NULL) {
            type = "ZeroDivisionError";
        } else if (strstr(message, "overflow") != NULL) {
            type = "OverflowError";
        } else if (strstr(message, "index") != NULL || strstr(message, "subscript") != NULL) {
            type = "IndexError";
        } else if (strstr(message, "dictionary key") != NULL) {
            type = "KeyError";
        } else if (strstr(message, "module") != NULL || strstr(message, "import") != NULL) {
            type = "ImportError";
        }
        snprintf(py->exception_type, sizeof(py->exception_type), "%s", type);
    }
    if (py->current_line > 0) {
        py->error_line = py->current_line;
        py->error_col = py->current_col;
        snprintf(py->error, sizeof(py->error), "line %u, col %u: %s",
                 (unsigned)py->error_line,
                 (unsigned)py->error_col,
                 message);
    } else {
        py->error_line = 0;
        py->error_col = 0;
        snprintf(py->error, sizeof(py->error), "%s", message);
    }
    py_build_traceback(py);
    if (py->debug_callback != NULL) {
        (void)py->debug_callback(py, PY_DEBUG_ERROR, py->current_line,
                                 py->call_depth > 0 ? py->call_stack[py->call_depth - 1] : "<script>",
                                 py->debug_user_data);
    }
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

static py_value_t py_int(int64_t n) {
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
    if (isfinite(n) && n >= -9223372036854775808.0 && n < 9223372036854775808.0) {
        value.int_value = (int64_t)n;
    }
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

static py_value_t py_module(const char *name) {
    py_value_t value = py_none();
    value.type = PY_VALUE_MODULE;
    snprintf(value.string_value, sizeof(value.string_value), "%s", name);
    return value;
}

static py_value_t py_callable(const char *name) {
    py_value_t value = py_none();
    value.type = PY_VALUE_CALLABLE;
    snprintf(value.string_value, sizeof(value.string_value), "%s", name);
    return value;
}

static py_value_t py_container(py_t *py, py_value_type_t type);

static void py_gc_mark_value(py_value_t value);

static void py_gc_mark_object(py_object_t *object) {
    if (object == NULL || object->marked) return;
    object->marked = 1;
    if (object->type == PY_VALUE_DICT) {
        for (size_t i = 0; i < object->count; ++i) {
            py_gc_mark_value(object->entries[i].key);
            py_gc_mark_value(object->entries[i].value);
        }
    } else {
        for (size_t i = 0; i < object->count; ++i) {
            py_gc_mark_value(object->items[i]);
        }
    }
}

static void py_gc_mark_value(py_value_t value) {
    py_gc_mark_object(value.object);
}

static int py_gc_push_root(py_t *py, py_value_t value) {
    if (value.object == NULL) return 1;
    if (py->gc_root_count >= PY_MAX_GC_ROOTS) {
        py_error(py, "temporary root limit exceeded");
        return 0;
    }
    py->gc_roots[py->gc_root_count++] = value;
    return 1;
}

static void py_gc_pop_root(py_t *py, py_value_t value) {
    if (value.object == NULL) return;
    if (py->gc_root_count > 0) py->gc_root_count--;
}

size_t py_collect_garbage(py_t *py) {
    py_object_t **link;
    size_t reclaimed = 0;

    if (py == NULL) return 0;
    for (size_t i = 0; i < py->var_count; ++i) py_gc_mark_value(py->vars[i].value);
    for (size_t i = 0; i < py->func_count; ++i) {
        for (size_t j = 0; j < py->funcs[i].param_count; ++j) {
            if (py->funcs[i].has_default[j]) py_gc_mark_value(py->funcs[i].defaults[j]);
        }
        for (size_t j = 0; j < py->funcs[i].closure_count; ++j) {
            py_gc_mark_value(py->funcs[i].closure[j].value);
        }
    }
    for (py_frame_t *frame = py->current_frame; frame != NULL; frame = frame->parent) {
        for (size_t i = 0; i < frame->var_count; ++i) py_gc_mark_value(frame->vars[i].value);
    }
    for (size_t i = 0; i < py->gc_root_count; ++i) py_gc_mark_value(py->gc_roots[i]);

    link = &py->objects;
    while (*link != NULL) {
        py_object_t *object = *link;
        if (object->marked) {
            object->marked = 0;
            link = &object->next;
            continue;
        }
        *link = object->next;
        if (object->capacity <= py->container_item_capacity) {
            py->container_item_capacity -= object->capacity;
        } else {
            py->container_item_capacity = 0;
        }
        py_heap_free(object->items);
        py_heap_free(object->entries);
        py_heap_free(object);
        py->object_count--;
        reclaimed++;
    }
    return reclaimed;
}

size_t py_object_count(const py_t *py) {
    return py != NULL ? py->object_count : 0;
}

static py_value_t py_exception(py_t *py, const char *type, const char *message) {
    py_value_t value = py_container(py, PY_VALUE_EXCEPTION);
    snprintf(value.string_value, sizeof(value.string_value), "%s", message != NULL ? message : "");
    if (value.object != NULL) {
        snprintf(value.object->tag, sizeof(value.object->tag), "%s", type != NULL ? type : "Exception");
    }
    return value;
}

static void py_exception_type(const py_value_t *value, char *type, size_t size) {
    snprintf(type, size, "%s", value->object != NULL && value->object->tag[0] != '\0'
                                  ? value->object->tag : "Exception");
}

static py_value_t py_native(const char *kind, int handle) {
    py_value_t value = py_none();
    value.type = PY_VALUE_NATIVE;
    value.int_value = handle;
    snprintf(value.string_value, sizeof(value.string_value), "%s", kind);
    return value;
}

static py_value_t py_container(py_t *py, py_value_type_t type) {
    py_value_t value = py_none();
    py_object_t *object;

    if (py->object_count >= PY_MAX_OBJECTS) {
        py_error(py, "too many container objects");
        return value;
    }
    object = (py_object_t *)py_heap_calloc(1, sizeof(*object));

    if (object == NULL) {
        py_error(py, "out of memory");
        return value;
    }
    object->type = type;
    object->next = py->objects;
    py->objects = object;
    py->object_count++;
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

static py_value_t py_set(py_t *py) {
    return py_container(py, PY_VALUE_SET);
}

static py_value_t py_bytes(py_t *py, int mutable) {
    return py_container(py, mutable ? PY_VALUE_BYTEARRAY : PY_VALUE_BYTES);
}

static int py_sequence_append(py_t *py, py_value_t *sequence, py_value_t item) {
    py_value_t *items;
    size_t capacity;

    if ((sequence->type != PY_VALUE_LIST && sequence->type != PY_VALUE_TUPLE &&
         sequence->type != PY_VALUE_SET && sequence->type != PY_VALUE_BYTES &&
         sequence->type != PY_VALUE_BYTEARRAY) || sequence->object == NULL) {
        py_error(py, "append target is not a sequence");
        return 0;
    }
    if (sequence->object->count >= sequence->object->capacity) {
        if (sequence->object->count >= PY_MAX_CONTAINER_ITEMS) {
            py_error(py, "container too large");
            return 0;
        }
        capacity = sequence->object->capacity == 0 ? 4 : sequence->object->capacity * 2;
        if (capacity > PY_MAX_CONTAINER_ITEMS) capacity = PY_MAX_CONTAINER_ITEMS;
        size_t added_capacity = capacity - sequence->object->capacity;
        if (added_capacity > PY_MAX_TOTAL_CONTAINER_ITEMS - py->container_item_capacity) {
            py_error(py, "script container memory limit exceeded");
            return 0;
        }
        items = (py_value_t *)py_heap_realloc(sequence->object->items, capacity * sizeof(*items));
        if (items == NULL) {
            py_error(py, "out of memory");
            return 0;
        }
        sequence->object->items = items;
        py->container_item_capacity += added_capacity;
        sequence->object->capacity = capacity;
    }
    sequence->object->items[sequence->object->count++] = item;
    return 1;
}

static int py_set_add(py_t *py, py_value_t *set, py_value_t item) {
    if (set->type != PY_VALUE_SET || set->object == NULL) {
        py_error(py, "add target is not a set");
        return 0;
    }
    for (size_t i = 0; i < set->object->count; ++i) {
        if (py_values_equal(&set->object->items[i], &item)) return 1;
    }
    return py_sequence_append(py, set, item);
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
        if (dict->object->count >= PY_MAX_CONTAINER_ITEMS) {
            py_error(py, "dictionary too large");
            return 0;
        }
        capacity = dict->object->capacity == 0 ? 4 : dict->object->capacity * 2;
        if (capacity > PY_MAX_CONTAINER_ITEMS) capacity = PY_MAX_CONTAINER_ITEMS;
        size_t added_capacity = capacity - dict->object->capacity;
        if (added_capacity > PY_MAX_TOTAL_CONTAINER_ITEMS - py->container_item_capacity) {
            py_error(py, "script container memory limit exceeded");
            return 0;
        }
        entries = (py_dict_entry_t *)py_heap_realloc(dict->object->entries, capacity * sizeof(*entries));
        if (entries == NULL) {
            py_error(py, "out of memory");
            return 0;
        }
        dict->object->entries = entries;
        py->container_item_capacity += added_capacity;
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
    if (value.type == PY_VALUE_LIST || value.type == PY_VALUE_TUPLE || value.type == PY_VALUE_DICT ||
        value.type == PY_VALUE_SET || value.type == PY_VALUE_BYTES || value.type == PY_VALUE_BYTEARRAY) {
        return value.object != NULL && value.object->count != 0;
    }
    return value.type == PY_VALUE_MODULE || value.type == PY_VALUE_CALLABLE ||
           value.type == PY_VALUE_EXCEPTION || value.type == PY_VALUE_NATIVE;
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

static int py_copy_bounded(py_t *py, char *destination, size_t destination_size,
                           const char *source, const char *error) {
    size_t length = strlen(source);
    if (length >= destination_size) {
        py_error(py, error);
        return 0;
    }
    memcpy(destination, source, length + 1);
    return 1;
}

static py_var_t *py_find_stored_global(py_t *py, const char *name) {
    size_t i;
    for (i = 0; i < py->var_count; ++i) {
        if (strncmp(py->vars[i].name, name, PY_MAX_NAME) == 0) {
            return &py->vars[i];
        }
    }
    return NULL;
}

static int py_qualified_name(py_t *py, const char *module, const char *name,
                             char *qualified, size_t qualified_size) {
    if (module == NULL || module[0] == '\0') {
        return py_copy_bounded(py, qualified, qualified_size, name, "variable name too long");
    }
    if (snprintf(qualified, qualified_size, "%s.%s", module, name) >= (int)qualified_size) {
        py_error(py, "qualified module name too long");
        return 0;
    }
    return 1;
}

static py_var_t *py_find_module_var(py_t *py, const char *module, const char *name) {
    char qualified[PY_MAX_NAME];
    size_t module_length = strlen(module);
    size_t name_length = strlen(name);
    if (module_length + name_length + 2 > sizeof(qualified)) return NULL;
    memcpy(qualified, module, module_length);
    qualified[module_length] = '.';
    memcpy(qualified + module_length + 1, name, name_length + 1);
    return py_find_stored_global(py, qualified);
}

static py_var_t *py_find_global_var(py_t *py, const char *name) {
    if (py->current_module[0] != '\0') {
        py_var_t *module_var = py_find_module_var(py, py->current_module, name);
        if (module_var != NULL) return module_var;
    }
    return py_find_stored_global(py, name);
}

static int py_frame_declares_global(const py_frame_t *frame, const char *name) {
    size_t i;
    if (frame == NULL) return 0;
    for (i = 0; i < frame->global_count; ++i) {
        if (strncmp(frame->globals[i], name, PY_MAX_NAME) == 0) return 1;
    }
    return 0;
}

static int py_frame_declares_nonlocal(const py_frame_t *frame, const char *name) {
    if (frame == NULL) return 0;
    for (size_t i = 0; i < frame->nonlocal_count; ++i) {
        if (strncmp(frame->nonlocals[i], name, PY_MAX_NAME) == 0) return 1;
    }
    return 0;
}

static py_var_t *py_find_frame_var(py_frame_t *frame, const char *name) {
    size_t i;
    if (frame == NULL) return NULL;
    for (i = 0; i < frame->var_count; ++i) {
        if (strncmp(frame->vars[i].name, name, PY_MAX_NAME) == 0) return &frame->vars[i];
    }
    return NULL;
}

static py_var_t *py_find_var(py_t *py, const char *name) {
    py_frame_t *frame = py->current_frame;
    if (frame != NULL && py_frame_declares_global(frame, name)) {
        return py_find_global_var(py, name);
    }
    if (frame != NULL && py_frame_declares_nonlocal(frame, name)) {
        for (py_frame_t *parent = frame->parent; parent != NULL; parent = parent->parent) {
            py_var_t *var = py_find_frame_var(parent, name);
            if (var != NULL) return var;
        }
        return py_find_frame_var(frame, name);
    }
    while (frame != NULL) {
        py_var_t *var = py_find_frame_var(frame, name);
        if (var != NULL) return var;
        frame = frame->parent;
    }
    return py_find_global_var(py, name);
}

static int py_set_global_var(py_t *py, const char *name, py_value_t value) {
    char stored_name[PY_MAX_NAME];
    const char *target_name = name;
    if (py->current_module[0] != '\0') {
        if (!py_qualified_name(py, py->current_module, name, stored_name, sizeof(stored_name))) return 0;
        target_name = stored_name;
    }
    py_var_t *var = py_find_stored_global(py, target_name);
    if (var != NULL) {
        var->value = value;
        return 1;
    }
    if (py->var_count >= PY_MAX_VARS) {
        py_error(py, "variable table full");
        return 0;
    }
    if (!py_copy_bounded(py, py->vars[py->var_count].name, PY_MAX_NAME,
                         target_name, "variable name too long")) return 0;
    py->vars[py->var_count].value = value;
    py->var_count++;
    return 1;
}

static int py_set_var(py_t *py, const char *name, py_value_t value) {
    py_frame_t *frame = py->current_frame;
    py_var_t *var;

    if (frame == NULL || py_frame_declares_global(frame, name)) {
        return py_set_global_var(py, name, value);
    }
    if (py_frame_declares_nonlocal(frame, name)) {
        for (py_frame_t *parent = frame->parent; parent != NULL; parent = parent->parent) {
            var = py_find_frame_var(parent, name);
            if (var != NULL) {
                var->value = value;
                return 1;
            }
        }
    }
    var = py_find_frame_var(frame, name);
    if (var != NULL) {
        var->value = value;
        return 1;
    }
    if (frame->var_count >= PY_MAX_LOCALS) {
        py_error(py, "local variable table full");
        return 0;
    }
    if (!py_copy_bounded(py, frame->vars[frame->var_count].name, PY_MAX_NAME,
                         name, "variable name too long")) return 0;
    frame->vars[frame->var_count].value = value;
    frame->var_count++;
    return 1;
}

static int py_declare_global(py_t *py, const char *name) {
    py_frame_t *frame = py->current_frame;
    if (frame == NULL || py_frame_declares_global(frame, name)) return 1;
    if (py_find_frame_var(frame, name) != NULL) {
        py_error(py, "name assigned before global declaration");
        return 0;
    }
    if (frame->global_count >= PY_MAX_GLOBAL_DECLS) {
        py_error(py, "too many global declarations");
        return 0;
    }
    return py_copy_bounded(py, frame->globals[frame->global_count++], PY_MAX_NAME,
                           name, "variable name too long");
}

static int py_declare_nonlocal(py_t *py, const char *name) {
    py_frame_t *frame = py->current_frame;
    int found = 0;
    if (frame == NULL) {
        py_error(py, "nonlocal declaration outside function");
        return 0;
    }
    if (py_frame_declares_global(frame, name)) {
        py_error(py, "name declared both global and nonlocal");
        return 0;
    }
    if (py_frame_declares_nonlocal(frame, name)) return 1;
    for (py_frame_t *parent = frame->parent; parent != NULL && !found; parent = parent->parent) {
        found = py_find_frame_var(parent, name) != NULL;
    }
    if (!found && frame->owner != NULL) {
        for (size_t i = 0; i < frame->owner->closure_count; ++i) {
            if (strncmp(frame->owner->closure[i].name, name, PY_MAX_NAME) == 0) {
                found = 1;
                break;
            }
        }
    }
    if (!found) {
        py_error(py, "no binding for nonlocal name");
        return 0;
    }
    if (frame->nonlocal_count >= PY_MAX_GLOBAL_DECLS) {
        py_error(py, "too many nonlocal declarations");
        return 0;
    }
    return py_copy_bounded(py, frame->nonlocals[frame->nonlocal_count++], PY_MAX_NAME,
                           name, "variable name too long");
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

static int py_set_func(py_t *py, const char *name, char params[][PY_MAX_NAME],
                       const py_value_t *defaults, const uint8_t *has_default,
                       size_t param_count, const char *vararg, const char *kwarg,
                       const char *body) {
    char stored_name[PY_MAX_NAME];
    py_func_t *func = NULL;
    size_t i;

    if (strlen(name) >= PY_MAX_NAME) {
        py_error(py, "function name too long");
        return 0;
    }
    if (py->current_frame == NULL && py->current_module[0] == '\0') {
        memcpy(stored_name, name, strlen(name) + 1);
        func = py_find_func(py, stored_name);
    } else {
        snprintf(stored_name, sizeof(stored_name), "@%lu", ++py->function_serial);
    }

    if (strlen(body) >= PY_MAX_FUNC_BODY) {
        py_error(py, "function body too long");
        return 0;
    }
    for (i = 0; i < param_count; ++i) {
        if (strlen(params[i]) >= PY_MAX_NAME) {
            py_error(py, "function parameter name too long");
            return 0;
        }
    }

    if (func == NULL) {
        if (py->func_count >= PY_MAX_FUNCS) {
            py_error(py, "function table full");
            return 0;
        }
        func = &py->funcs[py->func_count++];
    }

    if (!py_copy_bounded(py, func->name, sizeof(func->name),
                         stored_name, "function name too long")) return 0;
    if (!py_copy_bounded(py, func->module, sizeof(func->module), py->current_module,
                         "module name too long")) return 0;
    func->param_count = param_count;
    for (i = 0; i < param_count; ++i) {
        if (!py_copy_bounded(py, func->params[i], sizeof(func->params[i]),
                             params[i], "function parameter name too long")) return 0;
        func->defaults[i] = defaults[i];
        func->has_default[i] = has_default[i];
    }
    if (!py_copy_bounded(py, func->vararg, sizeof(func->vararg),
                         vararg, "variadic parameter name too long")) return 0;
    if (!py_copy_bounded(py, func->kwarg, sizeof(func->kwarg),
                         kwarg, "keyword parameter name too long")) return 0;
    func->closure_count = 0;
    for (py_frame_t *frame = py->current_frame;
         frame != NULL && func->closure_count < PY_MAX_CLOSURE_VARS;
         frame = frame->parent) {
        for (i = 0; i < frame->var_count && func->closure_count < PY_MAX_CLOSURE_VARS; ++i) {
            int duplicate = 0;
            for (size_t j = 0; j < func->closure_count; ++j) {
                if (strncmp(func->closure[j].name, frame->vars[i].name, PY_MAX_NAME) == 0) {
                    duplicate = 1;
                    break;
                }
            }
            if (!duplicate) func->closure[func->closure_count++] = frame->vars[i];
        }
    }
    if (!py_copy_bounded(py, func->body, sizeof(func->body),
                         body, "function body too long")) return 0;
    if (!py_set_var(py, name, py_callable(stored_name))) return 0;
    if (py->current_frame != NULL && func->closure_count < PY_MAX_CLOSURE_VARS) {
        py_var_t *self = &func->closure[func->closure_count++];
        snprintf(self->name, sizeof(self->name), "%s", name);
        self->value = py_callable(stored_name);
    }
    return 1;
}

static int py_value_repr(const py_value_t *value, char *buffer, size_t size, int quote_strings) {
    size_t used;
    size_t i;

    if (value->type == PY_VALUE_BOOL) {
        return snprintf(buffer, size, "%s", value->int_value ? "True" : "False") > 0;
    }
    if (value->type == PY_VALUE_INT) {
        return snprintf(buffer, size, "%" PRId64, value->int_value) > 0;
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
    if (value->type == PY_VALUE_MODULE) {
        return snprintf(buffer, size, "<module '%s'>", value->string_value) >= 0;
    }
    if (value->type == PY_VALUE_CALLABLE) {
        return snprintf(buffer, size, "<function %s>", value->string_value) >= 0;
    }
    if (value->type == PY_VALUE_NATIVE) {
        return snprintf(buffer, size, "<%s %" PRId64 ">", value->string_value, value->int_value) >= 0;
    }
    if (value->type == PY_VALUE_EXCEPTION) {
        char type[PY_MAX_NAME];
        py_exception_type(value, type, sizeof(type));
        return snprintf(buffer, size, "%s('%s')", type, value->string_value) >= 0;
    }
    if (value->type == PY_VALUE_LIST || value->type == PY_VALUE_TUPLE || value->type == PY_VALUE_SET ||
        value->type == PY_VALUE_BYTES || value->type == PY_VALUE_BYTEARRAY) {
        char item[PY_MAX_STRING];
        char open = value->type == PY_VALUE_LIST ? '[' : (value->type == PY_VALUE_TUPLE ? '(' : '{');
        char close = value->type == PY_VALUE_LIST ? ']' : (value->type == PY_VALUE_TUPLE ? ')' : '}');

        if (size == 0) {
            return 0;
        }
        if (value->type == PY_VALUE_SET &&
            (value->object == NULL || value->object->count == 0)) {
            return snprintf(buffer, size, "set()") >= 0;
        }
        if (value->type == PY_VALUE_BYTES || value->type == PY_VALUE_BYTEARRAY) {
            int written = snprintf(buffer, size, "%s([", value->type == PY_VALUE_BYTES ? "bytes" : "bytearray");
            if (written < 0 || (size_t)written >= size) return 0;
            used = (size_t)written;
            close = ']';
        } else {
            buffer[0] = open;
            buffer[1] = '\0';
            used = 1;
        }
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
        if ((value->type == PY_VALUE_BYTES || value->type == PY_VALUE_BYTEARRAY) && used + 1 < size) {
            buffer[used++] = ')';
        }
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
    if (left->type == PY_VALUE_SET && right->type == PY_VALUE_SET) {
        size_t left_count = left->object == NULL ? 0 : left->object->count;
        size_t right_count = right->object == NULL ? 0 : right->object->count;
        if (left_count != right_count) return 0;
        for (size_t i = 0; i < left_count; ++i) {
            int found = 0;
            for (size_t j = 0; j < right_count; ++j) {
                if (py_values_equal(&left->object->items[i], &right->object->items[j])) {
                    found = 1;
                    break;
                }
            }
            if (!found) return 0;
        }
        return 1;
    }
    if ((left->type == PY_VALUE_BYTES || left->type == PY_VALUE_BYTEARRAY) &&
        (right->type == PY_VALUE_BYTES || right->type == PY_VALUE_BYTEARRAY)) {
        size_t left_count = left->object == NULL ? 0 : left->object->count;
        size_t right_count = right->object == NULL ? 0 : right->object->count;
        if (left_count != right_count) return 0;
        for (size_t i = 0; i < left_count; ++i) {
            if (left->object->items[i].int_value != right->object->items[i].int_value) return 0;
        }
        return 1;
    }
    if (left->type == PY_VALUE_LIST || right->type == PY_VALUE_LIST ||
        left->type == PY_VALUE_TUPLE || right->type == PY_VALUE_TUPLE ||
        left->type == PY_VALUE_DICT || right->type == PY_VALUE_DICT ||
        left->type == PY_VALUE_SET || right->type == PY_VALUE_SET ||
        left->type == PY_VALUE_BYTES || right->type == PY_VALUE_BYTES ||
        left->type == PY_VALUE_BYTEARRAY || right->type == PY_VALUE_BYTEARRAY) {
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

static int py_value_compare(py_t *py, py_value_t left, py_value_t right, int *comparison) {
    if (py_is_number(left) && py_is_number(right)) {
        double a = py_number_as_double(left);
        double b = py_number_as_double(right);
        *comparison = (a > b) - (a < b);
        return 1;
    }
    if (left.type == PY_VALUE_STRING && right.type == PY_VALUE_STRING) {
        *comparison = strcmp(left.string_value, right.string_value);
        return 1;
    }
    py_error(py, "values are not orderable");
    return 0;
}

static int py_iterable_count(py_value_t value) {
    if (value.type == PY_VALUE_STRING) {
        return (int)strlen(value.string_value);
    }
    if ((value.type == PY_VALUE_LIST || value.type == PY_VALUE_TUPLE || value.type == PY_VALUE_DICT ||
         value.type == PY_VALUE_SET || value.type == PY_VALUE_BYTES || value.type == PY_VALUE_BYTEARRAY) &&
        value.object != NULL) {
        return (int)value.object->count;
    }
    if (value.type == PY_VALUE_LIST || value.type == PY_VALUE_TUPLE || value.type == PY_VALUE_DICT ||
        value.type == PY_VALUE_SET || value.type == PY_VALUE_BYTES || value.type == PY_VALUE_BYTEARRAY) {
        return 0;
    }
    return -1;
}

static py_value_t py_iterable_item(py_t *py, py_value_t value, int index) {
    if (value.type == PY_VALUE_STRING) {
        char character[2] = {value.string_value[index], '\0'};
        return py_string(character);
    }
    if ((value.type == PY_VALUE_LIST || value.type == PY_VALUE_TUPLE || value.type == PY_VALUE_SET ||
         value.type == PY_VALUE_BYTES || value.type == PY_VALUE_BYTEARRAY) && value.object != NULL) {
        return value.object->items[index];
    }
    if (value.type == PY_VALUE_DICT && value.object != NULL) {
        return value.object->entries[index].key;
    }
    py_error(py, "object is not iterable");
    return py_none();
}

static int py_contains(py_t *py, py_value_t container, py_value_t needle) {
    int count = py_iterable_count(container);
    int i;

    if (count < 0) {
        py_error(py, "right operand of 'in' is not iterable");
        return 0;
    }
    if (container.type == PY_VALUE_STRING) {
        if (needle.type != PY_VALUE_STRING) {
            py_error(py, "string membership requires a string");
            return 0;
        }
        return strstr(container.string_value, needle.string_value) != NULL;
    }
    for (i = 0; i < count; ++i) {
        py_value_t item = py_iterable_item(py, container, i);
        if (py_values_equal(&item, &needle)) {
            return 1;
        }
    }
    return 0;
}

static py_value_t py_sequence_copy(py_t *py, py_value_t source, py_value_type_t type) {
    py_value_t result = type == PY_VALUE_TUPLE ? py_tuple(py) : py_list(py);
    int count = py_iterable_count(source);
    int i;

    if (count < 0) {
        py_error(py, "object is not iterable");
        return py_none();
    }
    for (i = 0; i < count && !py_has_error(py); ++i) {
        py_value_t item = py_iterable_item(py, source, i);
        if (!py_sequence_append(py, &result, item)) {
            return py_none();
        }
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
    if (p->token_count == p->token_capacity) {
        size_t capacity = p->token_capacity == 0 ? 128U : p->token_capacity * 2U;
        token_t *tokens;

        if (capacity > PY_MAX_TOKENS) capacity = PY_MAX_TOKENS;
        tokens = (token_t *)py_heap_realloc(p->tokens, capacity * sizeof(*tokens));
        if (tokens == NULL) {
            py_error(p->py, "out of memory while parsing");
            return 0;
        }
        p->tokens = tokens;
        p->token_capacity = capacity;
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
        if ((s[0] == 'f' || s[0] == 'F' || s[0] == 'b' || s[0] == 'B') &&
            (s[1] == '"' || s[1] == '\'')) {
            char quote = s[1];
            size_t len = 0;
            int bytes_literal = s[0] == 'b' || s[0] == 'B';
            s += 2;
            token.type = bytes_literal ? TOK_BYTES : TOK_FSTRING;
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
                py_error(p->py, bytes_literal ? "unterminated bytes literal" : "unterminated f-string");
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
                int64_t value;
                errno = 0;
                value = strtoll(s, &end, 10);
                if (errno == ERANGE) {
                    py_error(p->py, "integer literal out of range");
                    return 0;
                }
                token.type = TOK_INT;
                token.int_value = value;
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
            } else if (strcmp(token.text, "nonlocal") == 0) {
                token.type = TOK_NONLOCAL;
            } else if (strcmp(token.text, "import") == 0) {
                token.type = TOK_IMPORT;
            } else if (strcmp(token.text, "from") == 0) {
                token.type = TOK_FROM;
            } else if (strcmp(token.text, "try") == 0) {
                token.type = TOK_TRY;
            } else if (strcmp(token.text, "except") == 0) {
                token.type = TOK_EXCEPT;
            } else if (strcmp(token.text, "finally") == 0) {
                token.type = TOK_FINALLY;
            } else if (strcmp(token.text, "raise") == 0) {
                token.type = TOK_RAISE;
            } else if (strcmp(token.text, "with") == 0) {
                token.type = TOK_WITH;
            } else if (strcmp(token.text, "as") == 0) {
                token.type = TOK_AS;
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
    if (p->token_count == 0) {
        py_error(p->py, "parser has no tokens");
        return NULL;
    }
    if (p->pos >= p->token_count) {
        p->pos = p->token_count - 1;
    }
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
    token_t *token = current(p);
    if (token != NULL && token->type == type) {
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
static int py_run_function_body(py_t *py, const char *source, char *output, size_t output_size, py_value_t *return_value);
static py_value_t py_eval_fstring(parser_t *p, const char *source);
static int append_body_token(char *body, size_t body_size, token_t *token);

static int read_int_arg(parser_t *p, py_value_t value, const char *name) {
    if (!py_is_integer_value(value)) {
        py_error(p->py, name);
        return 0;
    }
    if (value.int_value < INT_MIN || value.int_value > INT_MAX) {
        py_error(p->py, "integer argument out of range");
        return 0;
    }
    return (int)value.int_value;
}

static py_value_t py_integer_string(int64_t value, unsigned base, const char *prefix) {
    static const char digits[] = "0123456789abcdef";
    char reversed[65];
    char output[70];
    size_t count = 0;
    size_t pos = 0;
    uint64_t magnitude = value < 0
                             ? (uint64_t)(-(value + 1)) + 1
                             : (uint64_t)value;

    do {
        reversed[count++] = digits[magnitude % base];
        magnitude /= base;
    } while (magnitude != 0 && count < sizeof(reversed));
    if (value < 0) output[pos++] = '-';
    while (*prefix != '\0' && pos + 1 < sizeof(output)) output[pos++] = *prefix++;
    while (count > 0 && pos + 1 < sizeof(output)) output[pos++] = reversed[--count];
    output[pos] = '\0';
    return py_string(output);
}

static int py_debug_event(py_t *py, py_debug_event_t event, size_t line, const char *function) {
    if (py->abort_requested) {
        py_error(py, "execution stopped by sandbox");
        return 0;
    }
    if (py->debug_callback != NULL &&
        !py->debug_callback(py, event, line, function, py->debug_user_data)) {
        py->abort_requested = 1;
        if (!py_has_error(py)) py_error(py, "execution stopped by debugger");
        return 0;
    }
    return 1;
}

static int py_enter_call(py_t *py, const char *function) {
    unsigned long limit = py->call_depth_limit != 0 ? py->call_depth_limit : PY_MAX_TRACE_DEPTH;
    if (py->call_depth >= limit || py->call_depth >= PY_MAX_TRACE_DEPTH) {
        py_error(py, "maximum call depth exceeded");
        return 0;
    }
    snprintf(py->call_stack[py->call_depth], PY_MAX_NAME, "%s", function);
    py->call_lines[py->call_depth] = py->current_line;
    py->call_depth++;
    py->profile.function_calls++;
    if (py->call_depth > py->profile.max_call_depth) {
        py->profile.max_call_depth = (unsigned long)py->call_depth;
    }
    return py_debug_event(py, PY_DEBUG_CALL, py->current_line, function);
}

static void py_leave_call(py_t *py, const char *function) {
    (void)py_debug_event(py, PY_DEBUG_RETURN, py->current_line, function);
    if (py->call_depth > 0) py->call_depth--;
}

typedef struct {
    py_value_t value;
    char keyword[PY_MAX_NAME];
} py_call_arg_t;

static py_value_t call_user_function(parser_t *p, py_func_t *func,
                                     py_call_arg_t *args, int argc) {
    py_frame_t *frame = NULL;
    py_frame_t *parent_frame = p->py->current_frame;
    py_value_t result = py_none();
    py_value_t varargs = py_none();
    py_value_t kwargs = py_none();
    py_value_t bound_values[PY_MAX_PARAMS];
    uint8_t bound[PY_MAX_PARAMS] = {0};
    const char *body = func->body;
    char scratch_output[PY_MAX_LINE];
    char previous_module[PY_MAX_NAME];
    size_t i;

    if (!py_enter_call(p->py, func->name)) {
        return py_none();
    }
    snprintf(previous_module, sizeof(previous_module), "%s", p->py->current_module);
    snprintf(p->py->current_module, sizeof(p->py->current_module), "%s", func->module);

    frame = (py_frame_t *)py_heap_calloc(1, sizeof(*frame));
    if (frame == NULL) {
        py_error(p->py, "out of memory");
        goto finish;
    }
    frame->parent = parent_frame;
    frame->owner = func;

    for (i = 0; i < func->closure_count && frame->var_count < PY_MAX_LOCALS; ++i) {
        frame->vars[frame->var_count++] = func->closure[i];
    }

    if (func->vararg[0] != '\0') varargs = py_tuple(p->py);
    if (func->kwarg[0] != '\0') kwargs = py_dict(p->py);
    if (py_has_error(p->py)) goto finish;

    size_t next_positional = 0;
    for (i = 0; i < (size_t)argc; ++i) {
        if (args[i].keyword[0] == '\0') {
            while (next_positional < func->param_count && bound[next_positional]) next_positional++;
            if (next_positional < func->param_count) {
                bound_values[next_positional] = args[i].value;
                bound[next_positional++] = 1;
            } else if (func->vararg[0] != '\0') {
                if (!py_sequence_append(p->py, &varargs, args[i].value)) goto finish;
            } else {
                py_error(p->py, "too many positional arguments");
                goto finish;
            }
        } else {
            size_t parameter = func->param_count;
            for (size_t j = 0; j < func->param_count; ++j) {
                if (strncmp(func->params[j], args[i].keyword, PY_MAX_NAME) == 0) {
                    parameter = j;
                    break;
                }
            }
            if (parameter < func->param_count) {
                if (bound[parameter]) {
                    py_error(p->py, "multiple values for function argument");
                    goto finish;
                }
                bound_values[parameter] = args[i].value;
                bound[parameter] = 1;
            } else if (func->kwarg[0] != '\0') {
                if (!py_dict_set(p->py, &kwargs, py_string(args[i].keyword), args[i].value)) goto finish;
            } else {
                py_error(p->py, "unexpected keyword argument");
                goto finish;
            }
        }
    }

    for (i = 0; i < func->param_count; ++i) {
        if (!bound[i]) {
            if (!func->has_default[i]) {
                py_error(p->py, "missing required function argument");
                goto finish;
            }
            bound_values[i] = func->defaults[i];
        }
    }

    p->py->current_frame = frame;
    for (i = 0; i < func->param_count; ++i) {
        if (!py_set_var(p->py, func->params[i], bound_values[i])) goto finish;
    }
    if (func->vararg[0] != '\0' && !py_set_var(p->py, func->vararg, varargs)) goto finish;
    if (func->kwarg[0] != '\0' && !py_set_var(p->py, func->kwarg, kwargs)) goto finish;

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
            if (!py_run_function_body(p->py, body, scratch_output, sizeof(scratch_output), &result)) {
                result = py_none();
            } else {
                py_append(p, scratch_output);
            }
        }
    }

finish:
    if (frame != NULL) {
        for (size_t declaration = 0; declaration < frame->nonlocal_count; ++declaration) {
            const char *name = frame->nonlocals[declaration];
            py_var_t *updated = NULL;
            for (py_frame_t *parent = frame->parent; parent != NULL && updated == NULL;
                 parent = parent->parent) {
                updated = py_find_frame_var(parent, name);
            }
            if (updated == NULL) updated = py_find_frame_var(frame, name);
            if (updated != NULL) {
                for (size_t captured = 0; captured < func->closure_count; ++captured) {
                    if (strncmp(func->closure[captured].name, name, PY_MAX_NAME) == 0) {
                        func->closure[captured].value = updated->value;
                        break;
                    }
                }
            }
        }
    }
    p->py->current_frame = parent_frame;
    snprintf(p->py->current_module, sizeof(p->py->current_module), "%s", previous_module);
    py_heap_free(frame);
    py_leave_call(p->py, func->name);

    return result;
}

static py_value_t call_function_with_args(parser_t *p, const char *name,
                                          py_value_t *args, int argc);
static py_value_t call_method_with_args(parser_t *p, py_value_t target,
                                        const char *name, py_value_t *args, int argc);

static PY_NOINLINE py_value_t call_function(parser_t *p, const char *name) {
    py_call_arg_t *call_args = NULL;
    py_value_t positional[PY_MAX_CALL_ARGS];
    py_value_t result;
    int argc = 0;
    int saw_keyword = 0;

    if (!expect(p, TOK_LPAREN, "expected '(' after function name")) {
        return py_none();
    }

    if (!match(p, TOK_RPAREN)) {
        call_args = (py_call_arg_t *)py_heap_calloc(PY_MAX_CALL_ARGS, sizeof(*call_args));
        if (call_args == NULL) {
            py_error(p->py, "out of memory");
            return py_none();
        }
        do {
            if (argc >= PY_MAX_CALL_ARGS) {
                py_error(p->py, "too many function arguments");
                goto fail;
            }
            if (match(p, TOK_STARSTAR)) {
                py_value_t mapping = parse_expression(p);
                if (py_has_error(p->py)) goto fail;
                if (p->eval_suppressed) continue;
                if (mapping.type != PY_VALUE_DICT || mapping.object == NULL) {
                    py_error(p->py, "** argument must be a dictionary");
                    goto fail;
                }
                saw_keyword = 1;
                for (size_t i = 0; i < mapping.object->count; ++i) {
                    py_value_t key = mapping.object->entries[i].key;
                    if (key.type != PY_VALUE_STRING || argc >= PY_MAX_CALL_ARGS) {
                        py_error(p->py, "** keys must be strings and fit argument limit");
                        goto fail;
                    }
                    if (!py_copy_bounded(p->py, call_args[argc].keyword,
                                         sizeof(call_args[argc].keyword), key.string_value,
                                         "keyword name too long")) goto fail;
                    call_args[argc++].value = mapping.object->entries[i].value;
                }
            } else if (match(p, TOK_STAR)) {
                py_value_t sequence = parse_expression(p);
                int count;
                if (py_has_error(p->py)) goto fail;
                if (p->eval_suppressed) continue;
                count = py_iterable_count(sequence);
                if (count < 0 || argc + count > PY_MAX_CALL_ARGS) {
                    py_error(p->py, "* argument must be iterable and fit argument limit");
                    goto fail;
                }
                for (int i = 0; i < count; ++i) {
                    call_args[argc++].value = py_iterable_item(p->py, sequence, i);
                }
            } else if (current(p)->type == TOK_IDENT && peek(p, 1)->type == TOK_ASSIGN) {
                token_t keyword = *current(p);
                p->pos += 2;
                saw_keyword = 1;
                if (!py_copy_bounded(p->py, call_args[argc].keyword,
                                     sizeof(call_args[argc].keyword), keyword.text,
                                     "keyword name too long")) goto fail;
                call_args[argc++].value = parse_expression(p);
            } else {
                if (saw_keyword) {
                    py_error(p->py, "positional argument follows keyword argument");
                    goto fail;
                }
                call_args[argc++].value = parse_expression(p);
            }
            if (py_has_error(p->py)) goto fail;
        } while (match(p, TOK_COMMA));

        if (!expect(p, TOK_RPAREN, "expected ')' after arguments")) {
            goto fail;
        }
    }

    if (p->eval_suppressed) {
        py_heap_free(call_args);
        return py_none();
    }

    py_var_t *call_target = py_find_var(p->py, name);
    py_func_t *func = NULL;
    if (call_target != NULL && call_target->value.type == PY_VALUE_CALLABLE) {
        const char *callable_name = call_target->value.string_value;
        const char *dot = strchr(callable_name, '.');
        if (dot != NULL) {
            char module[PY_MAX_NAME];
            size_t length = (size_t)(dot - callable_name);
            if (length == 0 || length >= sizeof(module)) {
                py_error(p->py, "invalid imported callable");
                goto fail;
            }
            memcpy(module, callable_name, length);
            module[length] = '\0';
            for (int i = 0; i < argc; ++i) {
                if (call_args[i].keyword[0] != '\0') {
                    py_error(p->py, "keyword arguments are not supported for module calls");
                    goto fail;
                }
                positional[i] = call_args[i].value;
            }
            result = call_method_with_args(p, py_module(module), dot + 1, positional, argc);
            py_heap_free(call_args);
            return result;
        }
        func = py_find_func(p->py, callable_name);
    } else {
        func = py_find_func(p->py, name);
    }
    if (func != NULL) {
        result = call_user_function(p, func, call_args, argc);
    } else {
        for (int i = 0; i < argc; ++i) {
            if (call_args[i].keyword[0] != '\0') {
                py_error(p->py, "keyword arguments are supported for user functions only");
                goto fail;
            }
            positional[i] = call_args[i].value;
        }
        result = call_function_with_args(p, name, positional, argc);
    }
    py_heap_free(call_args);
    return result;

fail:
    py_heap_free(call_args);
    return py_none();
}

static PY_NOINLINE py_value_t call_function_with_args(parser_t *p, const char *name,
                                                       py_value_t *args, int argc) {
    if (strcmp(name, "BaseException") == 0 || strcmp(name, "Exception") == 0 ||
        strcmp(name, "RuntimeError") == 0 || strcmp(name, "ValueError") == 0 ||
        strcmp(name, "TypeError") == 0 || strcmp(name, "LookupError") == 0 ||
        strcmp(name, "KeyError") == 0 || strcmp(name, "IndexError") == 0 ||
        strcmp(name, "ArithmeticError") == 0 || strcmp(name, "ZeroDivisionError") == 0 ||
        strcmp(name, "OverflowError") == 0 || strcmp(name, "ImportError") == 0 ||
        strcmp(name, "AssertionError") == 0 || strcmp(name, "StopIteration") == 0) {
        if (argc > 1) {
            py_error(p->py, "exception constructor expects zero or one argument");
            return py_none();
        }
        char message[PY_MAX_STRING] = "";
        if (argc == 1) py_value_to_string(&args[0], message, sizeof(message));
        return py_exception(p->py, name, message);
    }
    if (strcmp(name, "set") == 0 || strcmp(name, "bytes") == 0 || strcmp(name, "bytearray") == 0) {
        py_value_t result = strcmp(name, "set") == 0 ? py_set(p->py)
                            : py_bytes(p->py, strcmp(name, "bytearray") == 0);
        if (argc > 1) {
            py_error(p->py, "container constructor expects at most one argument");
            return py_none();
        }
        if (argc == 1) {
            int count = py_iterable_count(args[0]);
            if (count < 0) {
                py_error(p->py, "container constructor expects an iterable");
                return py_none();
            }
            for (int i = 0; i < count; ++i) {
                py_value_t item = py_iterable_item(p->py, args[0], i);
                if (result.type == PY_VALUE_SET) {
                    if (!py_set_add(p->py, &result, item)) return py_none();
                } else {
                    if (!py_is_integer_value(item) || item.int_value < 0 || item.int_value > 255) {
                        py_error(p->py, "byte value must be in range(0, 256)");
                        return py_none();
                    }
                    if (!py_sequence_append(p->py, &result, py_int(item.int_value))) return py_none();
                }
            }
        }
        return result;
    }
    if (strcmp(name, "len") == 0) {
        if (argc != 1 ||
            (args[0].type != PY_VALUE_STRING &&
             args[0].type != PY_VALUE_LIST &&
             args[0].type != PY_VALUE_TUPLE &&
             args[0].type != PY_VALUE_DICT &&
             args[0].type != PY_VALUE_SET &&
             args[0].type != PY_VALUE_BYTES &&
             args[0].type != PY_VALUE_BYTEARRAY)) {
            py_error(p->py, "len() expects one container");
            return py_none();
        }
        if (args[0].type == PY_VALUE_LIST || args[0].type == PY_VALUE_TUPLE ||
            args[0].type == PY_VALUE_DICT || args[0].type == PY_VALUE_SET ||
            args[0].type == PY_VALUE_BYTES || args[0].type == PY_VALUE_BYTEARRAY) {
            return py_int(args[0].object == NULL ? 0 : (int)args[0].object->count);
        }
        return py_int((int)strlen(args[0].string_value));
    }
    if (strcmp(name, "int") == 0) {
        char *end;
        int64_t converted;
        if (argc != 1) {
            py_error(p->py, "int() expects one argument");
            return py_none();
        }
        if (py_is_number(args[0])) {
            return py_int(args[0].int_value);
        }
        if (args[0].type == PY_VALUE_STRING) {
            errno = 0;
            converted = strtoll(args[0].string_value, &end, 10);
            while (isspace((unsigned char)*end)) {
                end++;
            }
            if (errno != 0 || end == args[0].string_value || *end != '\0') {
                py_error(p->py, "invalid literal for int()");
                return py_none();
            }
            return py_int(converted);
        }
        py_error(p->py, "int() argument must be a number or string");
        return py_none();
    }
    if (strcmp(name, "float") == 0) {
        char *end;
        double converted;
        if (argc != 1) {
            py_error(p->py, "float() expects one argument");
            return py_none();
        }
        if (py_is_number(args[0])) {
            return py_float(py_number_as_double(args[0]));
        }
        if (args[0].type == PY_VALUE_STRING) {
            errno = 0;
            converted = strtod(args[0].string_value, &end);
            while (isspace((unsigned char)*end)) {
                end++;
            }
            if (errno != 0 || end == args[0].string_value || *end != '\0') {
                py_error(p->py, "could not convert string to float");
                return py_none();
            }
            return py_float(converted);
        }
        py_error(p->py, "float() argument must be a number or string");
        return py_none();
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
        if (argc != 1 || !py_is_number(args[0])) {
            py_error(p->py, "abs() expects one argument");
            return py_none();
        }
        if (args[0].type == PY_VALUE_FLOAT) {
            return py_float(fabs(args[0].float_value));
        }
        if (args[0].int_value == INT64_MIN) {
            py_error(p->py, "integer overflow");
            return py_none();
        }
        return py_int(args[0].int_value < 0 ? -args[0].int_value : args[0].int_value);
    }
    if (strcmp(name, "min") == 0 || strcmp(name, "max") == 0) {
        py_value_t best;
        int index;
        int count;
        int comparison;
        int want_min = strcmp(name, "min") == 0;
        if (argc == 0) {
            py_error(p->py, "min()/max() expect at least one argument");
            return py_none();
        }
        if (argc == 1) {
            count = py_iterable_count(args[0]);
            if (count <= 0) {
                py_error(p->py, count == 0 ? "min()/max() argument is empty" : "min()/max() argument is not iterable");
                return py_none();
            }
            best = py_iterable_item(p->py, args[0], 0);
            for (index = 1; index < count; ++index) {
                py_value_t candidate = py_iterable_item(p->py, args[0], index);
                if (!py_value_compare(p->py, candidate, best, &comparison)) {
                    return py_none();
                }
                if ((want_min && comparison < 0) || (!want_min && comparison > 0)) {
                    best = candidate;
                }
            }
            return best;
        }
        best = args[0];
        for (index = 1; index < argc; ++index) {
            if (!py_value_compare(p->py, args[index], best, &comparison)) {
                return py_none();
            }
            if ((want_min && comparison < 0) || (!want_min && comparison > 0)) {
                best = args[index];
            }
        }
        return best;
    }
    if (strcmp(name, "pow") == 0) {
        double result;
        if (argc != 2 || !py_is_number(args[0]) || !py_is_number(args[1])) {
            py_error(p->py, "pow() expects two arguments");
            return py_none();
        }
        result = pow(py_number_as_double(args[0]), py_number_as_double(args[1]));
        if (!isfinite(result)) {
            py_error(p->py, "pow() math domain error");
            return py_none();
        }
        if (py_is_integer_value(args[0]) && py_is_integer_value(args[1]) &&
            args[1].int_value >= 0) {
            if (result < -9223372036854775808.0 || result >= 9223372036854775808.0) {
                py_error(p->py, "integer overflow");
                return py_none();
            }
            return py_int((int64_t)result);
        }
        return py_float(result);
    }
    if (strcmp(name, "sum") == 0) {
        py_value_t total;
        int count;
        int i;
        if (argc < 1 || argc > 2) {
            py_error(p->py, "sum() expects an iterable and optional start");
            return py_none();
        }
        count = py_iterable_count(args[0]);
        if (count < 0) {
            py_error(p->py, "sum() argument is not iterable");
            return py_none();
        }
        total = argc == 2 ? args[1] : py_int(0);
        if (!py_is_number(total)) {
            py_error(p->py, "sum() start must be numeric");
            return py_none();
        }
        for (i = 0; i < count; ++i) {
            py_value_t item = py_iterable_item(p->py, args[0], i);
            if (!py_is_number(item)) {
                py_error(p->py, "sum() items must be numeric");
                return py_none();
            }
            if (total.type == PY_VALUE_FLOAT || item.type == PY_VALUE_FLOAT) {
                total = py_float(py_number_as_double(total) + py_number_as_double(item));
            } else {
                int64_t sum = 0;
                if (__builtin_add_overflow(total.int_value, item.int_value, &sum)) {
                    py_error(p->py, "integer overflow");
                    return py_none();
                }
                total = py_int(sum);
            }
        }
        return total;
    }
    if (strcmp(name, "round") == 0) {
        int digits = 0;
        double scale;
        double result;
        if (argc < 1 || argc > 2 || !py_is_number(args[0]) ||
            (argc == 2 && !py_is_integer_value(args[1]))) {
            py_error(p->py, "round() expects a number and optional integer digits");
            return py_none();
        }
        if (argc == 1) {
            return py_int((int)round(py_number_as_double(args[0])));
        }
        digits = read_int_arg(p, args[1], "round() digits out of range");
        if (py_has_error(p->py)) return py_none();
        scale = pow(10.0, (double)digits);
        result = round(py_number_as_double(args[0]) * scale) / scale;
        return py_float(result);
    }
    if (strcmp(name, "list") == 0 || strcmp(name, "tuple") == 0) {
        py_value_type_t type = strcmp(name, "tuple") == 0 ? PY_VALUE_TUPLE : PY_VALUE_LIST;
        if (argc == 0) {
            return type == PY_VALUE_TUPLE ? py_tuple(p->py) : py_list(p->py);
        }
        if (argc != 1) {
            py_error(p->py, "list()/tuple() expect at most one iterable");
            return py_none();
        }
        return py_sequence_copy(p->py, args[0], type);
    }
    if (strcmp(name, "range") == 0) {
        py_value_t result = py_list(p->py);
        int start = 0;
        int stop;
        int step = 1;
        int value;
        int guard = 0;
        if (argc < 1 || argc > 3) {
            py_error(p->py, "range() expects one to three arguments");
            return py_none();
        }
        for (int i = 0; i < argc; ++i) {
            if (!py_is_integer_value(args[i])) {
                py_error(p->py, "range() arguments must be integers");
                return py_none();
            }
        }
        if (argc == 1) stop = read_int_arg(p, args[0], "range() argument out of range");
        else {
            start = read_int_arg(p, args[0], "range() argument out of range");
            stop = read_int_arg(p, args[1], "range() argument out of range");
            if (argc == 3) step = read_int_arg(p, args[2], "range() argument out of range");
        }
        if (py_has_error(p->py)) return py_none();
        if (step == 0) {
            py_error(p->py, "range() step cannot be zero");
            return py_none();
        }
        for (value = start; (step > 0) ? value < stop : value > stop; value += step) {
            if (++guard > 10000) {
                py_error(p->py, "range() result too large");
                return py_none();
            }
            if (!py_list_append(p->py, &result, py_int(value))) return py_none();
        }
        return result;
    }
    if (strcmp(name, "enumerate") == 0) {
        py_value_t result = py_list(p->py);
        int count;
        int64_t start = 0;
        int i;
        if (argc < 1 || argc > 2 || (count = py_iterable_count(args[0])) < 0 ||
            (argc == 2 && !py_is_integer_value(args[1]))) {
            py_error(p->py, "enumerate() expects an iterable and optional integer start");
            return py_none();
        }
        if (argc == 2) start = args[1].int_value;
        for (i = 0; i < count; ++i) {
            py_value_t pair = py_tuple(p->py);
            if (!py_sequence_append(p->py, &pair, py_int(start + i)) ||
                !py_sequence_append(p->py, &pair, py_iterable_item(p->py, args[0], i)) ||
                !py_list_append(p->py, &result, pair)) return py_none();
        }
        return result;
    }
    if (strcmp(name, "reversed") == 0) {
        py_value_t result = py_list(p->py);
        int count;
        if (argc != 1 || (count = py_iterable_count(args[0])) < 0) {
            py_error(p->py, "reversed() expects one iterable");
            return py_none();
        }
        for (int i = count - 1; i >= 0; --i) {
            if (!py_list_append(p->py, &result, py_iterable_item(p->py, args[0], i))) return py_none();
        }
        return result;
    }
    if (strcmp(name, "zip") == 0) {
        py_value_t result = py_list(p->py);
        int count = INT_MAX;
        if (argc < 1) return result;
        for (int arg = 0; arg < argc; ++arg) {
            int item_count = py_iterable_count(args[arg]);
            if (item_count < 0) {
                py_error(p->py, "zip() arguments must be iterable");
                return py_none();
            }
            if (item_count < count) count = item_count;
        }
        for (int i = 0; i < count; ++i) {
            py_value_t tuple = py_tuple(p->py);
            for (int arg = 0; arg < argc; ++arg) {
                if (!py_sequence_append(p->py, &tuple, py_iterable_item(p->py, args[arg], i))) {
                    return py_none();
                }
            }
            if (!py_list_append(p->py, &result, tuple)) return py_none();
        }
        return result;
    }
    if (strcmp(name, "divmod") == 0) {
        py_value_t result = py_tuple(p->py);
        if (argc != 2 || !py_is_number(args[0]) || !py_is_number(args[1]) ||
            py_number_as_double(args[1]) == 0.0) {
            py_error(p->py, "divmod() expects two numbers with a nonzero divisor");
            return py_none();
        }
        if (py_is_integer_value(args[0]) && py_is_integer_value(args[1])) {
            int64_t a = args[0].int_value;
            int64_t b = args[1].int_value;
            if (a == INT64_MIN && b == -1) {
                py_error(p->py, "integer overflow");
                return py_none();
            }
            int64_t quotient = a / b;
            int64_t remainder = a % b;
            if (remainder != 0 && ((remainder < 0) != (b < 0))) {
                quotient--;
                remainder += b;
            }
            if (!py_sequence_append(p->py, &result, py_int(quotient)) ||
                !py_sequence_append(p->py, &result, py_int(remainder))) return py_none();
        } else {
            double a = py_number_as_double(args[0]);
            double b = py_number_as_double(args[1]);
            double quotient = floor(a / b);
            if (!py_sequence_append(p->py, &result, py_float(quotient)) ||
                !py_sequence_append(p->py, &result, py_float(a - quotient * b))) return py_none();
        }
        return result;
    }
    if (strcmp(name, "bin") == 0 || strcmp(name, "oct") == 0 || strcmp(name, "hex") == 0) {
        if (argc != 1 || !py_is_integer_value(args[0])) {
            py_error(p->py, "bin()/oct()/hex() expect one integer");
            return py_none();
        }
        if (strcmp(name, "bin") == 0) return py_integer_string(args[0].int_value, 2, "0b");
        if (strcmp(name, "oct") == 0) return py_integer_string(args[0].int_value, 8, "0o");
        return py_integer_string(args[0].int_value, 16, "0x");
    }
    if (strcmp(name, "sorted") == 0) {
        py_value_t result;
        int count;
        int i;
        if (argc != 1 || (count = py_iterable_count(args[0])) < 0) {
            py_error(p->py, "sorted() expects one iterable");
            return py_none();
        }
        result = py_sequence_copy(p->py, args[0], PY_VALUE_LIST);
        for (i = 1; i < count; ++i) {
            py_value_t item = result.object->items[i];
            int j = i - 1;
            while (j >= 0) {
                int comparison;
                if (!py_value_compare(p->py, result.object->items[j], item, &comparison)) return py_none();
                if (comparison <= 0) break;
                result.object->items[j + 1] = result.object->items[j];
                j--;
            }
            result.object->items[j + 1] = item;
        }
        return result;
    }
    if (strcmp(name, "any") == 0 || strcmp(name, "all") == 0) {
        int count;
        int i;
        int want_all = strcmp(name, "all") == 0;
        if (argc != 1 || (count = py_iterable_count(args[0])) < 0) {
            py_error(p->py, "any()/all() expect one iterable");
            return py_none();
        }
        for (i = 0; i < count; ++i) {
            int truth = py_truthy(py_iterable_item(p->py, args[0], i));
            if ((!want_all && truth) || (want_all && !truth)) {
                return py_bool(!want_all);
            }
        }
        return py_bool(want_all);
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
        if (args[0].type == PY_VALUE_SET) return py_string("set");
        if (args[0].type == PY_VALUE_BYTES) return py_string("bytes");
        if (args[0].type == PY_VALUE_BYTEARRAY) return py_string("bytearray");
        if (args[0].type == PY_VALUE_CALLABLE) return py_string("function");
        if (args[0].type == PY_VALUE_EXCEPTION) {
            char exception_name[PY_MAX_NAME];
            py_exception_type(&args[0], exception_name, sizeof(exception_name));
            return py_string(exception_name);
        }
        if (args[0].type == PY_VALUE_NATIVE) return py_string(args[0].string_value);
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

    py_error(p->py, "unknown function");
    return py_none();
}

static py_value_t eval_binary(parser_t *p, py_value_t left, token_type_t op, py_value_t right) {
    char concat[PY_MAX_STRING];
    int64_t integer_result = 0;

    if (p->eval_suppressed) return py_none();
    if (py_has_error(p->py)) {
        return py_none();
    }

    switch (op) {
        case TOK_PLUS:
            if (py_is_number(left) && py_is_number(right)) {
                if (left.type == PY_VALUE_FLOAT || right.type == PY_VALUE_FLOAT) {
                    return py_float(py_number_as_double(left) + py_number_as_double(right));
                }
                if (__builtin_add_overflow(left.int_value, right.int_value, &integer_result)) {
                    py_error(p->py, "integer overflow");
                    return py_none();
                }
                return py_int(integer_result);
            }
            if (left.type == PY_VALUE_STRING && right.type == PY_VALUE_STRING) {
                if (strlen(left.string_value) + strlen(right.string_value) >= sizeof(concat)) {
                    py_error(p->py, "string too long");
                    return py_none();
                }
                strcpy(concat, left.string_value);
                strcat(concat, right.string_value);
                return py_string(concat);
            }
            if ((left.type == PY_VALUE_LIST && right.type == PY_VALUE_LIST) ||
                (left.type == PY_VALUE_TUPLE && right.type == PY_VALUE_TUPLE)) {
                py_value_t result = left.type == PY_VALUE_LIST ? py_list(p->py) : py_tuple(p->py);
                int left_count = py_iterable_count(left);
                int right_count = py_iterable_count(right);
                int i;
                for (i = 0; i < left_count; ++i) {
                    if (!py_sequence_append(p->py, &result, py_iterable_item(p->py, left, i))) {
                        return py_none();
                    }
                }
                for (i = 0; i < right_count; ++i) {
                    if (!py_sequence_append(p->py, &result, py_iterable_item(p->py, right, i))) {
                        return py_none();
                    }
                }
                return result;
            }
            py_error(p->py, "unsupported operands for +");
            return py_none();
        case TOK_MINUS:
            if (!py_is_number(left) || !py_is_number(right)) {
                py_error(p->py, "subtraction requires numbers");
                return py_none();
            }
            if (left.type == PY_VALUE_FLOAT || right.type == PY_VALUE_FLOAT) {
                return py_float(py_number_as_double(left) - py_number_as_double(right));
            }
            if (__builtin_sub_overflow(left.int_value, right.int_value, &integer_result)) {
                py_error(p->py, "integer overflow");
                return py_none();
            }
            return py_int(integer_result);
        case TOK_STAR:
            if ((left.type == PY_VALUE_STRING || left.type == PY_VALUE_LIST || left.type == PY_VALUE_TUPLE) &&
                py_is_integer_value(right)) {
                py_value_t result;
                int count = py_iterable_count(left);
                int repetitions;
                int i;
                int j;
                if (right.int_value <= 0) repetitions = 0;
                else if (right.int_value > INT_MAX) {
                    py_error(p->py, "sequence repetition too large");
                    return py_none();
                } else repetitions = (int)right.int_value;
                if (left.type == PY_VALUE_STRING) {
                    size_t length = strlen(left.string_value);
                    size_t used = 0;
                    if (length > 0 && (size_t)repetitions > (sizeof(concat) - 1) / length) {
                        py_error(p->py, "string too long");
                        return py_none();
                    }
                    concat[0] = '\0';
                    for (i = 0; i < repetitions; ++i) {
                        memcpy(concat + used, left.string_value, length);
                        used += length;
                    }
                    concat[used] = '\0';
                    return py_string(concat);
                }
                result = left.type == PY_VALUE_LIST ? py_list(p->py) : py_tuple(p->py);
                for (i = 0; i < repetitions; ++i) {
                    for (j = 0; j < count; ++j) {
                        if (!py_sequence_append(p->py, &result, py_iterable_item(p->py, left, j))) {
                            return py_none();
                        }
                    }
                }
                return result;
            }
            if (py_is_integer_value(left) &&
                (right.type == PY_VALUE_STRING || right.type == PY_VALUE_LIST || right.type == PY_VALUE_TUPLE)) {
                return eval_binary(p, right, TOK_STAR, left);
            }
            if (!py_is_number(left) || !py_is_number(right)) {
                py_error(p->py, "multiplication requires numbers or a sequence and integer");
                return py_none();
            }
            if (left.type == PY_VALUE_FLOAT || right.type == PY_VALUE_FLOAT) {
                return py_float(py_number_as_double(left) * py_number_as_double(right));
            }
            if (__builtin_mul_overflow(left.int_value, right.int_value, &integer_result)) {
                py_error(p->py, "integer overflow");
                return py_none();
            }
            return py_int(integer_result);
        case TOK_STARSTAR: {
            double result;
            if (!py_is_number(left) || !py_is_number(right)) {
                py_error(p->py, "exponent requires numbers");
                return py_none();
            }
            result = pow(py_number_as_double(left), py_number_as_double(right));
            if (!isfinite(result)) {
                py_error(p->py, "math domain error");
                return py_none();
            }
            if (py_is_integer_value(left) && py_is_integer_value(right) && right.int_value >= 0) {
                if (result < -9223372036854775808.0 || result >= 9223372036854775808.0) {
                    py_error(p->py, "integer overflow");
                    return py_none();
                }
                return py_int((int64_t)result);
            }
            return py_float(result);
        }
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
        case TOK_DSLASH: {
            int64_t quotient;
            int64_t remainder;
            if (!py_is_number(left) || !py_is_number(right)) {
                py_error(p->py, "division requires numbers");
                return py_none();
            }
            if (py_number_as_double(right) == 0.0) {
                py_error(p->py, "division by zero");
                return py_none();
            }
            if (left.type == PY_VALUE_FLOAT || right.type == PY_VALUE_FLOAT) {
                return py_float(floor(py_number_as_double(left) / py_number_as_double(right)));
            }
            if (left.int_value == INT64_MIN && right.int_value == -1) {
                py_error(p->py, "integer overflow");
                return py_none();
            }
            quotient = left.int_value / right.int_value;
            remainder = left.int_value % right.int_value;
            if (remainder != 0 && ((remainder < 0) != (right.int_value < 0))) {
                quotient--;
            }
            return py_int(quotient);
        }
        case TOK_PERCENT: {
            double divisor;
            double result;
            if (!py_is_number(left) || !py_is_number(right)) {
                py_error(p->py, "modulo requires numbers");
                return py_none();
            }
            divisor = py_number_as_double(right);
            if (divisor == 0.0) {
                py_error(p->py, "modulo by zero");
                return py_none();
            }
            if (left.type == PY_VALUE_FLOAT || right.type == PY_VALUE_FLOAT) {
                result = py_number_as_double(left) - floor(py_number_as_double(left) / divisor) * divisor;
                return py_float(result);
            }
            if (left.int_value == INT64_MIN && right.int_value == -1) {
                return py_int(0);
            }
            {
                int64_t remainder = left.int_value % right.int_value;
                if (remainder != 0 && ((remainder < 0) != (right.int_value < 0))) {
                    remainder += right.int_value;
                }
                return py_int(remainder);
            }
        }
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
            if (right.int_value < 0 || right.int_value >= 64 || left.int_value < 0 ||
                (right.int_value > 0 &&
                 (uint64_t)left.int_value > (uint64_t)INT64_MAX >> (unsigned)right.int_value)) {
                py_error(p->py, "invalid or overflowing shift");
                return py_none();
            }
            return py_int((int64_t)((uint64_t)left.int_value << (unsigned)right.int_value));
        case TOK_RSHIFT:
            if (!py_is_integer_value(left) || !py_is_integer_value(right)) {
                py_error(p->py, "right shift requires integers");
                return py_none();
            }
            if (right.int_value < 0 || right.int_value >= 64) {
                py_error(p->py, "invalid shift count");
                return py_none();
            }
            return py_int(left.int_value >> (unsigned)right.int_value);
        case TOK_EQ:
            return py_bool(py_values_equal(&left, &right));
        case TOK_NE:
            return py_bool(!py_values_equal(&left, &right));
        case TOK_LT:
        case TOK_LE:
        case TOK_GT:
        case TOK_GE:
            {
            int comparison;
            if (!py_value_compare(p->py, left, right, &comparison)) {
                return py_none();
            }
            if (op == TOK_LT) {
                return py_bool(comparison < 0);
            }
            if (op == TOK_LE) {
                return py_bool(comparison <= 0);
            }
            if (op == TOK_GT) {
                return py_bool(comparison > 0);
            }
            return py_bool(comparison >= 0);
            }
        default:
            py_error(p->py, "unknown operator");
            return py_none();
    }
}

static int py_sequence_len(py_value_t value) {
    if (value.type == PY_VALUE_STRING) {
        return (int)strlen(value.string_value);
    }
    if ((value.type == PY_VALUE_LIST || value.type == PY_VALUE_TUPLE ||
         value.type == PY_VALUE_BYTES || value.type == PY_VALUE_BYTEARRAY) && value.object != NULL) {
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
        if (!py_is_integer_value(start_value)) {
            py_error(p->py, "slice start must be integer");
            return py_none();
        }
        start = py_normalize_index(read_int_arg(p, start_value, "slice start out of range"), len);
        if (py_has_error(p->py)) return py_none();
    }
    if (has_stop) {
        if (!py_is_integer_value(stop_value)) {
            py_error(p->py, "slice stop must be integer");
            return py_none();
        }
        stop = py_normalize_index(read_int_arg(p, stop_value, "slice stop out of range"), len);
        if (py_has_error(p->py)) return py_none();
    }
    if (has_step) {
        if (!py_is_integer_value(step_value)) {
            py_error(p->py, "slice step must be integer");
            return py_none();
        }
        step = read_int_arg(p, step_value, "slice step out of range");
        if (py_has_error(p->py)) return py_none();
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

    if (!py_is_integer_value(index)) {
        py_error(p->py, "index must be integer");
        return py_none();
    }
    len = py_sequence_len(target);
    if (len < 0) {
        py_error(p->py, "target is not subscriptable");
        return py_none();
    }
    i = py_normalize_index(read_int_arg(p, index, "index out of range"), len);
    if (py_has_error(p->py)) return py_none();
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
    if (target->type == PY_VALUE_TUPLE || target->type == PY_VALUE_STRING || target->type == PY_VALUE_BYTES) {
        py_error(p->py, "target does not support item assignment");
        return 0;
    }
    if ((target->type != PY_VALUE_LIST && target->type != PY_VALUE_BYTEARRAY) || target->object == NULL) {
        py_error(p->py, "target is not subscriptable");
        return 0;
    }
    if (!py_is_integer_value(index)) {
        py_error(p->py, "index must be integer");
        return 0;
    }
    len = py_sequence_len(*target);
    i = py_normalize_index(read_int_arg(p, index, "index out of range"), len);
    if (py_has_error(p->py)) return 0;
    if (i < 0 || i >= len) {
        py_error(p->py, "index out of range");
        return 0;
    }
    if (target->type == PY_VALUE_BYTEARRAY &&
        (!py_is_integer_value(value) || value.int_value < 0 || value.int_value > 255)) {
        py_error(p->py, "byte value must be in range(0, 256)");
        return 0;
    }
    target->object->items[i] = target->type == PY_VALUE_BYTEARRAY ? py_int(value.int_value) : value;
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

static size_t find_comprehension_for(parser_t *p, size_t start, token_type_t closing) {
    int paren = 0, bracket = 0, brace = 0;
    for (size_t i = start; i < p->token_count; ++i) {
        token_type_t type = p->tokens[i].type;
        if (type == closing && paren == 0 && bracket == 0 && brace == 0) return p->token_count;
        if (type == TOK_FOR && paren == 0 && bracket == 0 && brace == 0) return i;
        if (type == TOK_LPAREN) paren++;
        else if (type == TOK_RPAREN) paren--;
        else if (type == TOK_LBRACKET) bracket++;
        else if (type == TOK_RBRACKET) bracket--;
        else if (type == TOK_LBRACE) brace++;
        else if (type == TOK_RBRACE) brace--;
    }
    return p->token_count;
}

static int tokens_to_expression(parser_t *p, size_t start, size_t end,
                                char *out, size_t out_size) {
    out[0] = '\0';
    for (size_t i = start; i < end; ++i) {
        if (!append_body_token(out, out_size, &p->tokens[i])) {
            py_error(p->py, "comprehension expression too long");
            return 0;
        }
    }
    return 1;
}

#define PY_MAX_COMPREHENSION_CLAUSES 3

typedef struct {
    char variable[PY_MAX_NAME];
    char iterable[PY_MAX_LINE];
    char condition[PY_MAX_LINE];
} py_comprehension_clause_t;

static size_t comprehension_clause_end(parser_t *p, size_t start, token_type_t closing,
                                       int stop_at_if) {
    int paren = 0, bracket = 0, brace = 0;
    for (size_t i = start; i < p->token_count; ++i) {
        token_type_t type = p->tokens[i].type;
        if (paren == 0 && bracket == 0 && brace == 0 &&
            (type == closing || type == TOK_FOR || (stop_at_if && type == TOK_IF))) return i;
        if (type == TOK_LPAREN) paren++;
        else if (type == TOK_RPAREN) paren--;
        else if (type == TOK_LBRACKET) bracket++;
        else if (type == TOK_RBRACKET) bracket--;
        else if (type == TOK_LBRACE) brace++;
        else if (type == TOK_RBRACE) brace--;
    }
    return p->token_count;
}

static int parse_comprehension_clauses(parser_t *p, size_t for_position,
                                       token_type_t closing,
                                       py_comprehension_clause_t *clauses,
                                       size_t *clause_count) {
    p->pos = for_position;
    *clause_count = 0;
    while (match(p, TOK_FOR)) {
        if (*clause_count >= PY_MAX_COMPREHENSION_CLAUSES) {
            py_error(p->py, "too many comprehension clauses");
            return 0;
        }
        py_comprehension_clause_t *clause = &clauses[(*clause_count)++];
        memset(clause, 0, sizeof(*clause));
        token_t variable = *current(p);
        if (!expect(p, TOK_IDENT, "expected comprehension variable") ||
            !expect(p, TOK_IN, "expected 'in' in comprehension")) return 0;
        if (!py_copy_bounded(p->py, clause->variable, sizeof(clause->variable),
                             variable.text, "comprehension variable name too long")) return 0;
        size_t end = comprehension_clause_end(p, p->pos, closing, 1);
        if (end == p->pos || !tokens_to_expression(p, p->pos, end, clause->iterable,
                                                   sizeof(clause->iterable))) return 0;
        p->pos = end;
        if (match(p, TOK_IF)) {
            end = comprehension_clause_end(p, p->pos, closing, 0);
            if (end == p->pos || !tokens_to_expression(p, p->pos, end, clause->condition,
                                                       sizeof(clause->condition))) return 0;
            p->pos = end;
        }
    }
    return expect(p, closing, closing == TOK_RBRACKET
                                   ? "expected ']' after comprehension"
                                   : "expected '}' after comprehension");
}

static int evaluate_comprehension(py_t *py, py_comprehension_clause_t *clauses,
                                  size_t clause_count, size_t clause_index,
                                  const char *key_expression, const char *value_expression,
                                  py_value_t *result) {
    if (clause_index == clause_count) {
        py_value_t key = py_eval_expression(py, key_expression);
        if (py_has_error(py)) return 0;
        if (result->type == PY_VALUE_DICT) {
            py_value_t value = py_eval_expression(py, value_expression);
            return !py_has_error(py) && py_dict_set(py, result, key, value);
        }
        return result->type == PY_VALUE_SET ? py_set_add(py, result, key)
                                            : py_list_append(py, result, key);
    }

    py_comprehension_clause_t *clause = &clauses[clause_index];
    py_value_t iterable = py_eval_expression(py, clause->iterable);
    int count = py_iterable_count(iterable);
    if (py_has_error(py)) return 0;
    if (count < 0) {
        py_error(py, "comprehension source is not iterable");
        return 0;
    }
    for (int i = 0; i < count && !py_has_error(py); ++i) {
        if (!py_set_var(py, clause->variable, py_iterable_item(py, iterable, i))) return 0;
        if (clause->condition[0] != '\0') {
            py_value_t include = py_eval_expression(py, clause->condition);
            if (py_has_error(py)) return 0;
            if (!py_truthy(include)) continue;
        }
        if (!evaluate_comprehension(py, clauses, clause_count, clause_index + 1,
                                    key_expression, value_expression, result)) return 0;
    }
    return 1;
}

static py_value_t parse_list_comprehension(parser_t *p, size_t expression_start,
                                           size_t for_position) {
    char expression[PY_MAX_LINE];
    py_comprehension_clause_t clauses[PY_MAX_COMPREHENSION_CLAUSES];
    size_t clause_count;
    py_value_t result = py_list(p->py);
    py_frame_t *frame;
    py_frame_t *parent = p->py->current_frame;

    if (!tokens_to_expression(p, expression_start, for_position, expression, sizeof(expression)) ||
        !parse_comprehension_clauses(p, for_position, TOK_RBRACKET, clauses, &clause_count)) {
        return py_none();
    }
    frame = (py_frame_t *)py_heap_calloc(1, sizeof(*frame));
    if (frame == NULL) {
        py_error(p->py, "out of memory");
        return py_none();
    }
    frame->parent = parent;
    p->py->current_frame = frame;
    int ok = evaluate_comprehension(p->py, clauses, clause_count, 0, expression, "", &result);
    p->py->current_frame = parent;
    py_heap_free(frame);
    return ok ? result : py_none();
}

static size_t find_top_level_token(parser_t *p, size_t start, size_t end,
                                   token_type_t wanted) {
    int paren = 0, bracket = 0, brace = 0;
    for (size_t i = start; i < end; ++i) {
        token_type_t type = p->tokens[i].type;
        if (type == wanted && paren == 0 && bracket == 0 && brace == 0) return i;
        if (type == TOK_LPAREN) paren++;
        else if (type == TOK_RPAREN) paren--;
        else if (type == TOK_LBRACKET) bracket++;
        else if (type == TOK_RBRACKET) bracket--;
        else if (type == TOK_LBRACE) brace++;
        else if (type == TOK_RBRACE) brace--;
    }
    return end;
}

static py_value_t parse_brace_comprehension(parser_t *p, size_t expression_start,
                                            size_t for_position) {
    char key_expression[PY_MAX_LINE];
    char value_expression[PY_MAX_LINE] = "";
    size_t colon = find_top_level_token(p, expression_start, for_position, TOK_COLON);
    int dictionary = colon < for_position;
    py_value_t result = dictionary ? py_dict(p->py) : py_set(p->py);
    py_comprehension_clause_t clauses[PY_MAX_COMPREHENSION_CLAUSES];
    size_t clause_count;
    py_frame_t *frame;
    py_frame_t *parent = p->py->current_frame;

    if (!tokens_to_expression(p, expression_start, dictionary ? colon : for_position,
                              key_expression, sizeof(key_expression))) return py_none();
    if (dictionary &&
        !tokens_to_expression(p, colon + 1, for_position, value_expression,
                              sizeof(value_expression))) return py_none();
    if (!parse_comprehension_clauses(p, for_position, TOK_RBRACE, clauses, &clause_count)) return py_none();
    frame = (py_frame_t *)py_heap_calloc(1, sizeof(*frame));
    if (frame == NULL) {
        py_error(p->py, "out of memory");
        return py_none();
    }
    frame->parent = parent;
    p->py->current_frame = frame;
    int ok = evaluate_comprehension(p->py, clauses, clause_count, 0,
                                    key_expression, value_expression, &result);
    p->py->current_frame = parent;
    py_heap_free(frame);
    return ok ? result : py_none();
}

static PY_NOINLINE py_value_t parse_atom(parser_t *p) {
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
    if (match(p, TOK_BYTES)) {
        py_value_t bytes = py_bytes(p->py, 0);
        for (size_t i = 0; token->text[i] != '\0'; ++i) {
            if (!py_sequence_append(p->py, &bytes, py_int((unsigned char)token->text[i]))) return py_none();
        }
        return bytes;
    }
    if (match(p, TOK_FSTRING)) {
        if (p->eval_suppressed) return py_none();
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
    if (match(p, TOK_RANGE)) {
        return call_function(p, "range");
    }
    if (match(p, TOK_LBRACKET)) {
        size_t expression_start = p->pos;
        size_t for_position = find_comprehension_for(p, expression_start, TOK_RBRACKET);
        if (for_position < p->token_count) {
            return parse_list_comprehension(p, expression_start, for_position);
        }
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
        size_t expression_start = p->pos;
        size_t for_position = find_comprehension_for(p, expression_start, TOK_RBRACE);
        if (for_position < p->token_count) {
            return parse_brace_comprehension(p, expression_start, for_position);
        }
        py_value_t dict = py_dict(p->py);

        if (!match(p, TOK_RBRACE)) {
            py_value_t key = parse_expression(p);
            if (py_has_error(p->py)) return py_none();
            if (!match(p, TOK_COLON)) {
                py_value_t set = py_set(p->py);
                if (!py_set_add(p->py, &set, key)) return py_none();
                while (match(p, TOK_COMMA) && current(p)->type != TOK_RBRACE) {
                    key = parse_expression(p);
                    if (py_has_error(p->py) || !py_set_add(p->py, &set, key)) return py_none();
                }
                if (!expect(p, TOK_RBRACE, "expected '}' after set")) return py_none();
                return set;
            }
            do {
                py_value_t value;
                value = parse_expression(p);
                if (py_has_error(p->py)) {
                    return py_none();
                }
                if (!py_dict_set(p->py, &dict, key, value)) {
                    return py_none();
                }
                if (!match(p, TOK_COMMA)) break;
                if (current(p)->type == TOK_RBRACE) break;
                key = parse_expression(p);
                if (py_has_error(p->py) || !expect(p, TOK_COLON, "expected ':' in dict")) return py_none();
            } while (1);

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
        if (p->eval_suppressed) return py_none();
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

static py_value_t call_method_with_args(parser_t *p, py_value_t target,
                                        const char *name, py_value_t *args, int argc);

static int py_standard_module(const char *name) {
    return strcmp(name, "math") == 0 || strcmp(name, "random") == 0 ||
           strcmp(name, "time") == 0 || strcmp(name, "statistics") == 0;
}

static int py_known_module(const char *name) {
    return py_standard_module(name) || strcmp(name, "graphics") == 0 ||
           strcmp(name, "keys") == 0 || strcmp(name, "storage") == 0 ||
           strcmp(name, "audio") == 0 || strcmp(name, "sensors") == 0 ||
           strcmp(name, "board") == 0 || strcmp(name, "digitalio") == 0 ||
           strcmp(name, "analogio") == 0 || strcmp(name, "busio") == 0;
}

static int py_exception_matches(const char *raised, const char *caught) {
    if (strcmp(caught, "BaseException") == 0 || strcmp(caught, "Exception") == 0 ||
        strcmp(raised, caught) == 0) return 1;
    if (strcmp(caught, "ArithmeticError") == 0) {
        return strcmp(raised, "ZeroDivisionError") == 0 || strcmp(raised, "OverflowError") == 0;
    }
    if (strcmp(caught, "LookupError") == 0) {
        return strcmp(raised, "IndexError") == 0 || strcmp(raised, "KeyError") == 0;
    }
    return 0;
}

static uint64_t py_random_next(py_t *py) {
    uint64_t state = py->random_state;
    if (state == 0) state = UINT64_C(0x9e3779b97f4a7c15);
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    py->random_state = state;
    return state * UINT64_C(2685821657736338717);
}

static double py_random_unit(py_t *py) {
    return (double)(py_random_next(py) >> 11) * (1.0 / 9007199254740992.0);
}

static double py_time_seconds(int monotonic) {
#ifdef ESP_PLATFORM
    if (monotonic) return (double)esp_timer_get_time() / 1000000.0;
    return (double)time(NULL);
#else
    struct timespec now;
    clock_gettime(monotonic ? CLOCK_MONOTONIC : CLOCK_REALTIME, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
#endif
}

static int py_sleep_cancellable(py_t *py, double seconds) {
    if (!isfinite(seconds) || seconds < 0.0) {
        py_error(py, "sleep length must be a non-negative finite number");
        return 0;
    }
    while (seconds > 0.0) {
        double slice = seconds > 0.01 ? 0.01 : seconds;
        if (py->abort_requested) {
            py_error(py, "execution stopped by sandbox");
            return 0;
        }
#ifdef ESP_PLATFORM
        if (slice < 0.001) {
            esp_rom_delay_us((uint32_t)ceil(slice * 1000000.0));
        } else {
            TickType_t ticks = pdMS_TO_TICKS((uint32_t)ceil(slice * 1000.0));
            vTaskDelay(ticks > 0 ? ticks : 1);
        }
#else
        struct timespec delay;
        delay.tv_sec = (time_t)slice;
        delay.tv_nsec = (long)((slice - (double)delay.tv_sec) * 1000000000.0);
        nanosleep(&delay, NULL);
#endif
        seconds -= slice;
    }
#ifdef ESP_PLATFORM
    taskYIELD();
#endif
    return 1;
}

static int py_module_attribute(parser_t *p, py_value_t target, const char *name,
                               py_value_t *result) {
    if (target.type == PY_VALUE_NATIVE) {
        if (strcmp(name, "value") == 0 &&
            (strcmp(target.string_value, "digitalio.DigitalInOut") == 0 ||
             strcmp(target.string_value, "analogio.AnalogIn") == 0)) {
            *result = call_method_with_args(p, target, "value", NULL, 0);
            return !py_has_error(p->py);
        }
        py_error(p->py, "unknown hardware object property");
        return 0;
    }
    if (target.type != PY_VALUE_MODULE) return 0;
    if (strcmp(target.string_value, "math") == 0) {
        if (strcmp(name, "pi") == 0) *result = py_float(3.14159265358979323846);
        else if (strcmp(name, "e") == 0) *result = py_float(2.71828182845904523536);
        else if (strcmp(name, "tau") == 0) *result = py_float(6.28318530717958647692);
        else if (strcmp(name, "inf") == 0) *result = py_float(INFINITY);
        else if (strcmp(name, "nan") == 0) *result = py_float(NAN);
        else {
            py_error(p->py, "unknown module attribute");
            return 0;
        }
        return 1;
    }
    if (strcmp(target.string_value, "board") == 0) {
        if (((name[0] == 'D' && name[1] >= '0' && name[1] <= '9' && name[2] == '\0') ||
             (name[0] == 'D' && name[1] == '1' && name[2] >= '0' && name[2] <= '1' && name[3] == '\0'))) {
            int pin = name[1] - '0';
            if (name[2] != '\0') pin = 10 + name[2] - '0';
            *result = py_int(pin);
            return 1;
        }
        if (name[0] == 'A' && name[1] >= '0' && name[1] <= '3' && name[2] == '\0') {
            *result = py_int(name[1] - '0');
            return 1;
        }
        py_error(p->py, "unknown OpenCalc board pin");
        return 0;
    }
    if (strcmp(target.string_value, "digitalio") == 0) {
        if (strcmp(name, "INPUT") == 0) *result = py_int(0);
        else if (strcmp(name, "OUTPUT") == 0) *result = py_int(1);
        else if (strcmp(name, "PULL_UP") == 0) *result = py_int(2);
        else if (strcmp(name, "LOW") == 0) *result = py_int(0);
        else if (strcmp(name, "HIGH") == 0) *result = py_int(1);
        else {
            py_error(p->py, "unknown digitalio constant");
            return 0;
        }
        return 1;
    }
    if (!py_known_module(target.string_value)) {
        py_var_t *var = py_find_module_var(p->py, target.string_value, name);
        if (var != NULL) {
            *result = var->value;
            return 1;
        }
    }
    py_error(p->py, "module attributes are not available");
    return 0;
}

/* Returns 1 when handled, 0 on a handled error, and -1 for host fallback. */
static int py_call_standard_module(parser_t *p, const char *module, const char *name,
                                   py_value_t *args, int argc, py_value_t *result) {
    if (strcmp(module, "math") == 0) {
        double a = argc > 0 && py_is_number(args[0]) ? py_number_as_double(args[0]) : 0.0;
        double b = argc > 1 && py_is_number(args[1]) ? py_number_as_double(args[1]) : 0.0;
        double value = 0.0;
        int numeric_args = argc == 1 && py_is_number(args[0]);

        if (strcmp(name, "isfinite") == 0 && numeric_args) { *result = py_bool(isfinite(a)); return 1; }
        if (strcmp(name, "isinf") == 0 && numeric_args) { *result = py_bool(isinf(a)); return 1; }
        if (strcmp(name, "isnan") == 0 && numeric_args) { *result = py_bool(isnan(a)); return 1; }
        if (strcmp(name, "isclose") == 0 && argc >= 2 && argc <= 4 &&
            py_is_number(args[0]) && py_is_number(args[1]) &&
            (argc < 3 || py_is_number(args[2])) && (argc < 4 || py_is_number(args[3]))) {
            double rel_tol = argc >= 3 ? py_number_as_double(args[2]) : 1e-9;
            double abs_tol = argc >= 4 ? py_number_as_double(args[3]) : 0.0;
            if (rel_tol < 0.0 || abs_tol < 0.0) { py_error(p->py, "tolerances must be non-negative"); return 0; }
            double difference = fabs(a - b);
            *result = py_bool(a == b || difference <= fmax(rel_tol * fmax(fabs(a), fabs(b)), abs_tol));
            return 1;
        }
        if (strcmp(name, "prod") == 0 && (argc == 1 || argc == 2)) {
            int count = py_iterable_count(args[0]);
            py_value_t product = argc == 2 ? args[1] : py_int(1);
            if (count < 0 || !py_is_number(product)) { py_error(p->py, "prod() expects numeric data"); return 0; }
            for (int i = 0; i < count; ++i) {
                py_value_t item = py_iterable_item(p->py, args[0], i);
                if (!py_is_number(item)) { py_error(p->py, "prod() data must be numeric"); return 0; }
                if (product.type == PY_VALUE_FLOAT || item.type == PY_VALUE_FLOAT) {
                    product = py_float(py_number_as_double(product) * py_number_as_double(item));
                    if (!isfinite(product.float_value)) { py_error(p->py, "prod() overflow"); return 0; }
                } else {
                    int64_t multiplied;
                    if (__builtin_mul_overflow(product.int_value, item.int_value, &multiplied)) {
                        py_error(p->py, "integer overflow");
                        return 0;
                    }
                    product = py_int(multiplied);
                }
            }
            *result = product;
            return 1;
        }
        if (strcmp(name, "atan2") == 0 && argc == 2 && py_is_number(args[0]) && py_is_number(args[1])) value = atan2(a, b);
        else if (strcmp(name, "pow") == 0 && argc == 2 && py_is_number(args[0]) && py_is_number(args[1])) value = pow(a, b);
        else if (strcmp(name, "hypot") == 0 && argc == 2 && py_is_number(args[0]) && py_is_number(args[1])) value = hypot(a, b);
        else if (strcmp(name, "fmod") == 0 && argc == 2 && py_is_number(args[0]) && py_is_number(args[1]) && b != 0.0) value = fmod(a, b);
        else if (strcmp(name, "copysign") == 0 && argc == 2 && py_is_number(args[0]) && py_is_number(args[1])) value = copysign(a, b);
        else if (strcmp(name, "log") == 0 && (argc == 1 || argc == 2) && py_is_number(args[0]) && a > 0.0 &&
                 (argc == 1 || (py_is_number(args[1]) && b > 0.0 && b != 1.0))) value = argc == 1 ? log(a) : log(a) / log(b);
        else if (strcmp(name, "gcd") == 0 && argc == 2 && py_is_integer_value(args[0]) && py_is_integer_value(args[1])) {
            uint64_t x = args[0].int_value < 0 ? (uint64_t)(-(args[0].int_value + 1)) + 1 : (uint64_t)args[0].int_value;
            uint64_t y = args[1].int_value < 0 ? (uint64_t)(-(args[1].int_value + 1)) + 1 : (uint64_t)args[1].int_value;
            while (y != 0) { uint64_t remainder = x % y; x = y; y = remainder; }
            if (x > INT64_MAX) { py_error(p->py, "gcd result is out of range"); return 0; }
            *result = py_int((int64_t)x);
            return 1;
        } else if (strcmp(name, "lcm") == 0 && argc == 2 &&
                   py_is_integer_value(args[0]) && py_is_integer_value(args[1])) {
            uint64_t x = args[0].int_value < 0 ? (uint64_t)(-(args[0].int_value + 1)) + 1 : (uint64_t)args[0].int_value;
            uint64_t y = args[1].int_value < 0 ? (uint64_t)(-(args[1].int_value + 1)) + 1 : (uint64_t)args[1].int_value;
            uint64_t gcd = x;
            uint64_t remainder_source = y;
            while (remainder_source != 0) {
                uint64_t remainder = gcd % remainder_source;
                gcd = remainder_source;
                remainder_source = remainder;
            }
            if (x == 0 || y == 0) { *result = py_int(0); return 1; }
            if (x / gcd > (uint64_t)INT64_MAX / y) { py_error(p->py, "lcm result is out of range"); return 0; }
            *result = py_int((int64_t)((x / gcd) * y));
            return 1;
        } else if (strcmp(name, "factorial") == 0 && argc == 1 && py_is_integer_value(args[0]) &&
                   args[0].int_value >= 0 && args[0].int_value <= 20) {
            int64_t factorial = 1;
            for (int64_t i = 2; i <= args[0].int_value; ++i) factorial *= i;
            *result = py_int(factorial);
            return 1;
        } else if (numeric_args) {
            if (strcmp(name, "sin") == 0) value = sin(a);
            else if (strcmp(name, "cos") == 0) value = cos(a);
            else if (strcmp(name, "tan") == 0) value = tan(a);
            else if (strcmp(name, "asin") == 0 && a >= -1.0 && a <= 1.0) value = asin(a);
            else if (strcmp(name, "acos") == 0 && a >= -1.0 && a <= 1.0) value = acos(a);
            else if (strcmp(name, "atan") == 0) value = atan(a);
            else if (strcmp(name, "sinh") == 0) value = sinh(a);
            else if (strcmp(name, "cosh") == 0) value = cosh(a);
            else if (strcmp(name, "tanh") == 0) value = tanh(a);
            else if (strcmp(name, "sqrt") == 0 && a >= 0.0) value = sqrt(a);
            else if (strcmp(name, "exp") == 0) value = exp(a);
            else if (strcmp(name, "log10") == 0 && a > 0.0) value = log10(a);
            else if (strcmp(name, "log2") == 0 && a > 0.0) value = log2(a);
            else if (strcmp(name, "floor") == 0) { *result = py_int((int64_t)floor(a)); return 1; }
            else if (strcmp(name, "ceil") == 0) { *result = py_int((int64_t)ceil(a)); return 1; }
            else if (strcmp(name, "trunc") == 0) { *result = py_int((int64_t)trunc(a)); return 1; }
            else if (strcmp(name, "fabs") == 0) value = fabs(a);
            else if (strcmp(name, "degrees") == 0) value = a * (180.0 / 3.14159265358979323846);
            else if (strcmp(name, "radians") == 0) value = a * (3.14159265358979323846 / 180.0);
            else return -1;
        } else return -1;

        if (!isfinite(value)) { py_error(p->py, "math domain or range error"); return 0; }
        *result = py_float(value);
        return 1;
    }

    if (strcmp(module, "random") == 0) {
        if (strcmp(name, "seed") == 0 && argc <= 1) {
            if (argc == 1 && args[0].type != PY_VALUE_NONE && !py_is_integer_value(args[0])) return -1;
            uint64_t seed = argc == 0 || args[0].type == PY_VALUE_NONE
                                ? (uint64_t)(py_time_seconds(0) * 1000000.0)
                                : (uint64_t)args[0].int_value;
            p->py->random_state = seed != 0 ? seed : UINT64_C(0x9e3779b97f4a7c15);
            *result = py_none();
            return 1;
        }
        if (strcmp(name, "random") == 0 && argc == 0) { *result = py_float(py_random_unit(p->py)); return 1; }
        if (strcmp(name, "getrandbits") == 0 && argc == 1 && py_is_integer_value(args[0])) {
            int bits = read_int_arg(p, args[0], "getrandbits() expects 0..63 bits");
            if (py_has_error(p->py)) return 0;
            if (bits < 0 || bits > 63) { py_error(p->py, "getrandbits() expects 0..63 bits"); return 0; }
            *result = py_int(bits == 0 ? 0 : (int64_t)(py_random_next(p->py) >> (64 - bits)));
            return 1;
        }
        if (strcmp(name, "shuffle") == 0 && argc == 1 && args[0].type == PY_VALUE_LIST &&
            args[0].object != NULL) {
            for (size_t i = args[0].object->count; i > 1; --i) {
                size_t j = (size_t)(py_random_next(p->py) % i);
                py_value_t swap = args[0].object->items[i - 1];
                args[0].object->items[i - 1] = args[0].object->items[j];
                args[0].object->items[j] = swap;
            }
            *result = py_none();
            return 1;
        }
        if ((strcmp(name, "gauss") == 0 || strcmp(name, "normalvariate") == 0) &&
            argc == 2 && py_is_number(args[0]) && py_is_number(args[1])) {
            double u1 = py_random_unit(p->py);
            double u2 = py_random_unit(p->py);
            if (u1 <= 0.0) u1 = 1.0 / 9007199254740992.0;
            *result = py_float(py_number_as_double(args[0]) + py_number_as_double(args[1]) *
                               sqrt(-2.0 * log(u1)) * cos(6.28318530717958647692 * u2));
            return 1;
        }
        if (strcmp(name, "uniform") == 0 && argc == 2 && py_is_number(args[0]) && py_is_number(args[1])) {
            double a = py_number_as_double(args[0]);
            double b = py_number_as_double(args[1]);
            *result = py_float(a + (b - a) * py_random_unit(p->py));
            return 1;
        }
        if (strcmp(name, "choice") == 0 && argc == 1) {
            int count = py_iterable_count(args[0]);
            if (count <= 0) { py_error(p->py, "cannot choose from an empty sequence"); return 0; }
            *result = py_iterable_item(p->py, args[0], (int)(py_random_next(p->py) % (uint64_t)count));
            return !py_has_error(p->py);
        }
        if ((strcmp(name, "randrange") == 0 && argc >= 1 && argc <= 3) ||
            (strcmp(name, "randint") == 0 && argc == 2)) {
            int start = 0, stop = 0, step = 1;
            if (strcmp(name, "randint") == 0) {
                start = read_int_arg(p, args[0], "randint() expects integers");
                stop = read_int_arg(p, args[1], "randint() expects integers");
                if (stop == INT_MAX) { py_error(p->py, "randint() range is too large"); return 0; }
                stop++;
            } else if (argc == 1) {
                stop = read_int_arg(p, args[0], "randrange() expects integers");
            } else {
                start = read_int_arg(p, args[0], "randrange() expects integers");
                stop = read_int_arg(p, args[1], "randrange() expects integers");
                if (argc == 3) step = read_int_arg(p, args[2], "randrange() expects integers");
            }
            if (py_has_error(p->py)) return 0;
            if (step == 0) { py_error(p->py, "empty range for random selection"); return 0; }
            int64_t distance = step > 0 ? (int64_t)stop - start : (int64_t)start - stop;
            int64_t magnitude = step > 0 ? step : -(int64_t)step;
            int64_t count = distance > 0 ? (distance + magnitude - 1) / magnitude : 0;
            if (count <= 0) { py_error(p->py, "empty range for random selection"); return 0; }
            *result = py_int((int64_t)start + (int64_t)step * (int64_t)(py_random_next(p->py) % (uint64_t)count));
            return 1;
        }
        return -1;
    }

    if (strcmp(module, "time") == 0) {
        if (strcmp(name, "time") == 0 && argc == 0) { *result = py_float(py_time_seconds(0)); return 1; }
        if (strcmp(name, "monotonic") == 0 && argc == 0) { *result = py_float(py_time_seconds(1)); return 1; }
        if (strcmp(name, "monotonic_ns") == 0 && argc == 0) {
            *result = py_int((int64_t)(py_time_seconds(1) * 1000000000.0));
            return 1;
        }
        if (strcmp(name, "sleep") == 0 && argc == 1 && py_is_number(args[0])) {
            if (!py_sleep_cancellable(p->py, py_number_as_double(args[0]))) return 0;
            *result = py_none();
            return 1;
        }
        if ((strcmp(name, "sleep_ms") == 0 || strcmp(name, "sleep_us") == 0) &&
            argc == 1 && py_is_number(args[0])) {
            double divisor = strcmp(name, "sleep_ms") == 0 ? 1000.0 : 1000000.0;
            if (!py_sleep_cancellable(p->py, py_number_as_double(args[0]) / divisor)) return 0;
            *result = py_none();
            return 1;
        }
        if ((strcmp(name, "ticks_ms") == 0 || strcmp(name, "ticks_us") == 0) && argc == 0) {
            double multiplier = strcmp(name, "ticks_ms") == 0 ? 1000.0 : 1000000.0;
            *result = py_int((int64_t)(py_time_seconds(1) * multiplier));
            return 1;
        }
        if (strcmp(name, "ticks_diff") == 0 && argc == 2 &&
            py_is_integer_value(args[0]) && py_is_integer_value(args[1])) {
            int64_t difference;
            if (__builtin_sub_overflow(args[0].int_value, args[1].int_value, &difference)) {
                py_error(p->py, "ticks_diff() overflow");
                return 0;
            }
            *result = py_int(difference);
            return 1;
        }
        return -1;
    }
    if (strcmp(module, "statistics") == 0 && argc == 1) {
        int count = py_iterable_count(args[0]);
        if (count <= 0) {
            py_error(p->py, count == 0 ? "statistics requires data" : "statistics expects an iterable");
            return 0;
        }
        double *values = (double *)py_heap_calloc((size_t)count, sizeof(*values));
        if (values == NULL) {
            py_error(p->py, "out of memory");
            return 0;
        }
        double sum = 0.0;
        for (int i = 0; i < count; ++i) {
            py_value_t item = py_iterable_item(p->py, args[0], i);
            if (!py_is_number(item)) {
                py_heap_free(values);
                py_error(p->py, "statistics data must be numeric");
                return 0;
            }
            values[i] = py_number_as_double(item);
            sum += values[i];
        }
        if (strcmp(name, "mean") == 0 || strcmp(name, "fmean") == 0) {
            *result = py_float(sum / count);
        } else if (strcmp(name, "median") == 0) {
            for (int i = 1; i < count; ++i) {
                double value = values[i];
                int j = i - 1;
                while (j >= 0 && values[j] > value) { values[j + 1] = values[j]; --j; }
                values[j + 1] = value;
            }
            *result = py_float((count & 1) ? values[count / 2]
                                           : (values[count / 2 - 1] + values[count / 2]) / 2.0);
        } else if (strcmp(name, "pvariance") == 0 || strcmp(name, "variance") == 0 ||
                   strcmp(name, "pstdev") == 0 || strcmp(name, "stdev") == 0) {
            int sample = strcmp(name, "variance") == 0 || strcmp(name, "stdev") == 0;
            if (sample && count < 2) {
                py_heap_free(values);
                py_error(p->py, "sample statistics require two data points");
                return 0;
            }
            double mean = sum / count;
            double squared = 0.0;
            for (int i = 0; i < count; ++i) {
                double delta = values[i] - mean;
                squared += delta * delta;
            }
            double variance = squared / (sample ? count - 1 : count);
            *result = py_float((strcmp(name, "pstdev") == 0 || strcmp(name, "stdev") == 0)
                                   ? sqrt(variance) : variance);
        } else {
            py_heap_free(values);
            return -1;
        }
        py_heap_free(values);
        return 1;
    }
    return -1;
}

static PY_NOINLINE py_value_t call_method(parser_t *p, py_value_t target, const char *name) {
    py_value_t *args = NULL;
    py_value_t result;
    int argc = 0;

    if (!expect(p, TOK_LPAREN, "expected '(' after method name")) {
        return py_none();
    }
    if (!match(p, TOK_RPAREN)) {
        args = (py_value_t *)py_heap_calloc(PY_MAX_PARAMS, sizeof(*args));
        if (args == NULL) {
            py_error(p->py, "out of memory");
            return py_none();
        }
        do {
            if (argc >= PY_MAX_PARAMS) {
                py_error(p->py, "too many method arguments");
                goto fail;
            }
            args[argc++] = parse_expression(p);
            if (py_has_error(p->py)) {
                goto fail;
            }
        } while (match(p, TOK_COMMA));
        if (!expect(p, TOK_RPAREN, "expected ')' after method arguments")) {
            goto fail;
        }
    }

    result = p->eval_suppressed ? py_none() : call_method_with_args(p, target, name, args, argc);
    py_heap_free(args);
    return result;

fail:
    py_heap_free(args);
    return py_none();
}

static PY_NOINLINE py_value_t call_method_with_args(parser_t *p, py_value_t target,
                                                     const char *name, py_value_t *args, int argc) {
    size_t i;

    if (target.type == PY_VALUE_MODULE) {
        if ((strcmp(target.string_value, "digitalio") == 0 && strcmp(name, "DigitalInOut") == 0) ||
            (strcmp(target.string_value, "analogio") == 0 && strcmp(name, "AnalogIn") == 0)) {
            if (argc != 1 || !py_is_integer_value(args[0])) {
                py_error(p->py, "hardware object expects one board pin");
                return py_none();
            }
            return py_native(strcmp(target.string_value, "digitalio") == 0
                                 ? "digitalio.DigitalInOut" : "analogio.AnalogIn",
                             read_int_arg(p, args[0], "invalid board pin"));
        }
        if (strcmp(target.string_value, "busio") == 0 && strcmp(name, "I2C") == 0) {
            if (argc != 0) {
                py_error(p->py, "busio.I2C() uses the fixed expansion bus");
                return py_none();
            }
            return py_native("busio.I2C", 0);
        }
        if (!py_known_module(target.string_value)) {
            py_var_t *member = py_find_module_var(p->py, target.string_value, name);
            py_func_t *func = member != NULL && member->value.type == PY_VALUE_CALLABLE
                                ? py_find_func(p->py, member->value.string_value) : NULL;
            py_call_arg_t call_args[PY_MAX_PARAMS];
            if (func == NULL) {
                py_error(p->py, "unknown user-module function");
                return py_none();
            }
            if (argc > PY_MAX_PARAMS) {
                py_error(p->py, "too many module function arguments");
                return py_none();
            }
            memset(call_args, 0, sizeof(call_args));
            for (int arg = 0; arg < argc; ++arg) call_args[arg].value = args[arg];
            return call_user_function(p, func, call_args, argc);
        }
        py_value_t result = py_none();
        int standard = py_call_standard_module(p, target.string_value, name, args, argc, &result);
        if (standard >= 0) return standard ? result : py_none();
        if (p->py->native_callback == NULL) {
            py_error(p->py, "unknown module function");
            return py_none();
        }
        if (!p->py->native_callback(p->py, target.string_value, name, args,
                                    (size_t)argc, &result, p->py->native_user_data)) {
            if (!py_has_error(p->py)) py_error(p->py, "module call failed");
            return py_none();
        }
        return result;
    }

    if (target.type == PY_VALUE_NATIVE) {
        py_value_t result = py_none();
        py_value_t forwarded[PY_MAX_PARAMS + 1];
        const char *module = NULL;
        const char *function = name;
        if (p->py->native_callback == NULL || argc + 1 > PY_MAX_PARAMS + 1) {
            py_error(p->py, "hardware service is unavailable");
            return py_none();
        }
        forwarded[0] = py_int(target.int_value);
        for (int arg = 0; arg < argc; ++arg) forwarded[arg + 1] = args[arg];
        if (strcmp(target.string_value, "digitalio.DigitalInOut") == 0) {
            int value = 0;
            if (strcmp(name, "switch_to_input") == 0 && argc <= 1) {
                int mode = argc == 1 && py_truthy(args[0]) ? 2 : 0;
                if (p->py->gpio_mode_callback == NULL ||
                    !p->py->gpio_mode_callback((int)target.int_value, mode, p->py->gpio_user_data)) {
                    py_error(p->py, "digital input configuration failed");
                }
                return py_none();
            }
            if (strcmp(name, "switch_to_output") == 0 && argc <= 1) {
                if (p->py->gpio_mode_callback == NULL ||
                    !p->py->gpio_mode_callback((int)target.int_value, 1, p->py->gpio_user_data) ||
                    (argc == 1 && (p->py->gpio_write_callback == NULL ||
                     !p->py->gpio_write_callback((int)target.int_value, py_truthy(args[0]), p->py->gpio_user_data)))) {
                    py_error(p->py, "digital output configuration failed");
                }
                return py_none();
            }
            if ((strcmp(name, "value") == 0 || strcmp(name, "read") == 0) && argc == 0) {
                if (p->py->gpio_read_callback == NULL ||
                    !p->py->gpio_read_callback((int)target.int_value, &value, p->py->gpio_user_data)) {
                    py_error(p->py, "digital read failed");
                    return py_none();
                }
                return py_bool(value);
            }
            if (strcmp(name, "write") == 0 && argc == 1) {
                if (p->py->gpio_write_callback == NULL ||
                    !p->py->gpio_write_callback((int)target.int_value, py_truthy(args[0]), p->py->gpio_user_data)) {
                    py_error(p->py, "digital write failed");
                }
                return py_none();
            }
            if (strcmp(name, "deinit") == 0 && argc == 0) return py_none();
            py_error(p->py, "unknown DigitalInOut method");
            return py_none();
        } else if (strcmp(target.string_value, "analogio.AnalogIn") == 0) {
            module = "analogio";
            if (strcmp(name, "value") == 0) function = "raw";
            else if (strcmp(name, "voltage") == 0 || strcmp(name, "read") == 0) function = "read";
            else if (strcmp(name, "deinit") == 0 && argc == 0) return py_none();
        } else if (strcmp(target.string_value, "busio.I2C") == 0) {
            module = "busio";
            if (strcmp(name, "try_lock") == 0 && argc == 0) return py_bool(1);
            if ((strcmp(name, "unlock") == 0 || strcmp(name, "deinit") == 0) && argc == 0) return py_none();
            /* I2C calls do not take a pin handle. */
            for (int arg = 0; arg < argc; ++arg) forwarded[arg] = args[arg];
            if (!p->py->native_callback(p->py, module, function, forwarded, (size_t)argc,
                                        &result, p->py->native_user_data)) {
                if (!py_has_error(p->py)) py_error(p->py, "I2C call failed");
                return py_none();
            }
            return result;
        }
        if (module == NULL || !p->py->native_callback(p->py, module, function, forwarded,
                                                       (size_t)argc + 1, &result,
                                                       p->py->native_user_data)) {
            if (!py_has_error(p->py)) py_error(p->py, "hardware object method failed");
            return py_none();
        }
        return result;
    }

    if (target.type == PY_VALUE_SET || target.type == PY_VALUE_BYTEARRAY) {
        if (target.object == NULL) {
            py_error(p->py, "invalid mutable container");
            return py_none();
        }
        if ((strcmp(name, "add") == 0 || strcmp(name, "append") == 0) && argc == 1) {
            if (target.type == PY_VALUE_SET) {
                if (!py_set_add(p->py, &target, args[0])) return py_none();
            } else {
                if (!py_is_integer_value(args[0]) || args[0].int_value < 0 || args[0].int_value > 255) {
                    py_error(p->py, "byte value must be in range(0, 256)");
                    return py_none();
                }
                if (!py_sequence_append(p->py, &target, py_int(args[0].int_value))) return py_none();
            }
            return py_none();
        }
        if (strcmp(name, "clear") == 0 && argc == 0) {
            target.object->count = 0;
            return py_none();
        }
        if (target.type == PY_VALUE_SET && (strcmp(name, "discard") == 0 || strcmp(name, "remove") == 0) && argc == 1) {
            for (i = 0; i < target.object->count; ++i) {
                if (py_values_equal(&target.object->items[i], &args[0])) {
                    memmove(&target.object->items[i], &target.object->items[i + 1],
                            (target.object->count - i - 1) * sizeof(*target.object->items));
                    target.object->count--;
                    return py_none();
                }
            }
            if (strcmp(name, "remove") == 0) py_error(p->py, "value not in set");
            return py_none();
        }
    }

    if (target.type == PY_VALUE_LIST) {
        if (target.object == NULL) {
            py_error(p->py, "invalid list");
            return py_none();
        }
        if (strcmp(name, "append") == 0) {
            if (argc != 1) {
                py_error(p->py, "append() expects one argument");
                return py_none();
            }
            py_list_append(p->py, &target, args[0]);
            return py_none();
        }
        if (strcmp(name, "extend") == 0) {
            int count;
            if (argc != 1 || (count = py_iterable_count(args[0])) < 0) {
                py_error(p->py, "extend() expects one iterable");
                return py_none();
            }
            for (i = 0; i < (size_t)count; ++i) {
                if (!py_list_append(p->py, &target, py_iterable_item(p->py, args[0], (int)i))) {
                    return py_none();
                }
            }
            return py_none();
        }
        if (strcmp(name, "insert") == 0) {
            int index;
            if (argc != 2 || !py_is_integer_value(args[0])) {
                py_error(p->py, "insert() expects integer index and value");
                return py_none();
            }
            index = read_int_arg(p, args[0], "insert() index out of range");
            if (py_has_error(p->py)) return py_none();
            if (index < 0) {
                index += (int)target.object->count;
            }
            if (index < 0) index = 0;
            if (index > (int)target.object->count) index = (int)target.object->count;
            if (!py_list_append(p->py, &target, py_none())) {
                return py_none();
            }
            memmove(&target.object->items[index + 1], &target.object->items[index],
                    (target.object->count - (size_t)index - 1) * sizeof(*target.object->items));
            target.object->items[index] = args[1];
            return py_none();
        }
        if (strcmp(name, "pop") == 0) {
            int index = (int)target.object->count - 1;
            py_value_t result;
            if (argc > 1 || (argc == 1 && !py_is_integer_value(args[0]))) {
                py_error(p->py, "pop() expects an optional integer index");
                return py_none();
            }
            if (argc == 1) {
                index = read_int_arg(p, args[0], "pop() index out of range");
                if (py_has_error(p->py)) return py_none();
                index = py_normalize_index(index, (int)target.object->count);
            }
            if (index < 0 || index >= (int)target.object->count) {
                py_error(p->py, target.object->count == 0 ? "pop from empty list" : "pop index out of range");
                return py_none();
            }
            result = target.object->items[index];
            memmove(&target.object->items[index], &target.object->items[index + 1],
                    (target.object->count - (size_t)index - 1) * sizeof(*target.object->items));
            target.object->count--;
            return result;
        }
        if (strcmp(name, "remove") == 0) {
            if (argc != 1) {
                py_error(p->py, "remove() expects one value");
                return py_none();
            }
            for (i = 0; i < target.object->count; ++i) {
                if (py_values_equal(&target.object->items[i], &args[0])) {
                    memmove(&target.object->items[i], &target.object->items[i + 1],
                            (target.object->count - i - 1) * sizeof(*target.object->items));
                    target.object->count--;
                    return py_none();
                }
            }
            py_error(p->py, "value not in list");
            return py_none();
        }
        if (strcmp(name, "clear") == 0) {
            if (argc != 0) {
                py_error(p->py, "clear() expects no arguments");
                return py_none();
            }
            target.object->count = 0;
            return py_none();
        }
        if (strcmp(name, "reverse") == 0) {
            if (argc != 0) {
                py_error(p->py, "reverse() expects no arguments");
                return py_none();
            }
            for (i = 0; i < target.object->count / 2; ++i) {
                py_value_t swap = target.object->items[i];
                target.object->items[i] = target.object->items[target.object->count - i - 1];
                target.object->items[target.object->count - i - 1] = swap;
            }
            return py_none();
        }
        if (strcmp(name, "copy") == 0) {
            if (argc != 0) {
                py_error(p->py, "copy() expects no arguments");
                return py_none();
            }
            return py_sequence_copy(p->py, target, PY_VALUE_LIST);
        }
        if (strcmp(name, "count") == 0 || strcmp(name, "index") == 0) {
            int count = 0;
            if (argc != 1) {
                py_error(p->py, "count()/index() expect one value");
                return py_none();
            }
            for (i = 0; i < target.object->count; ++i) {
                if (py_values_equal(&target.object->items[i], &args[0])) {
                    if (strcmp(name, "index") == 0) return py_int((int)i);
                    count++;
                }
            }
            if (strcmp(name, "index") == 0) {
                py_error(p->py, "value is not in list");
                return py_none();
            }
            return py_int(count);
        }
    }

    if (target.type == PY_VALUE_DICT && target.object != NULL) {
        if (strcmp(name, "get") == 0) {
            if (argc < 1 || argc > 2) {
                py_error(p->py, "get() expects key and optional default");
                return py_none();
            }
            for (i = 0; i < target.object->count; ++i) {
                if (py_values_equal(&target.object->entries[i].key, &args[0])) {
                    return target.object->entries[i].value;
                }
            }
            return argc == 2 ? args[1] : py_none();
        }
        if (strcmp(name, "keys") == 0 || strcmp(name, "values") == 0 || strcmp(name, "items") == 0) {
            py_value_t result = py_list(p->py);
            if (argc != 0) {
                py_error(p->py, "keys()/values()/items() expect no arguments");
                return py_none();
            }
            for (i = 0; i < target.object->count; ++i) {
                py_value_t item;
                if (strcmp(name, "keys") == 0) item = target.object->entries[i].key;
                else if (strcmp(name, "values") == 0) item = target.object->entries[i].value;
                else {
                    item = py_tuple(p->py);
                    if (!py_sequence_append(p->py, &item, target.object->entries[i].key) ||
                        !py_sequence_append(p->py, &item, target.object->entries[i].value)) return py_none();
                }
                if (!py_list_append(p->py, &result, item)) return py_none();
            }
            return result;
        }
    }

    if (target.type == PY_VALUE_STRING) {
        char result[PY_MAX_STRING];
        size_t length = strlen(target.string_value);
        if (strcmp(name, "lower") == 0 || strcmp(name, "upper") == 0) {
            if (argc != 0) {
                py_error(p->py, "lower()/upper() expect no arguments");
                return py_none();
            }
            for (i = 0; i < length; ++i) {
                result[i] = (char)(strcmp(name, "lower") == 0
                    ? tolower((unsigned char)target.string_value[i])
                    : toupper((unsigned char)target.string_value[i]));
            }
            result[length] = '\0';
            return py_string(result);
        }
        if (strcmp(name, "strip") == 0) {
            size_t start = 0;
            size_t end = length;
            if (argc != 0) {
                py_error(p->py, "strip() currently expects no arguments");
                return py_none();
            }
            while (start < end && isspace((unsigned char)target.string_value[start])) start++;
            while (end > start && isspace((unsigned char)target.string_value[end - 1])) end--;
            memcpy(result, target.string_value + start, end - start);
            result[end - start] = '\0';
            return py_string(result);
        }
        if (strcmp(name, "startswith") == 0 || strcmp(name, "endswith") == 0) {
            size_t needle_length;
            if (argc != 1 || args[0].type != PY_VALUE_STRING) {
                py_error(p->py, "startswith()/endswith() expect one string");
                return py_none();
            }
            needle_length = strlen(args[0].string_value);
            if (needle_length > length) return py_bool(0);
            if (strcmp(name, "startswith") == 0) {
                return py_bool(strncmp(target.string_value, args[0].string_value, needle_length) == 0);
            }
            return py_bool(strcmp(target.string_value + length - needle_length, args[0].string_value) == 0);
        }
        if (strcmp(name, "find") == 0) {
            char *found;
            if (argc != 1 || args[0].type != PY_VALUE_STRING) {
                py_error(p->py, "find() expects one string");
                return py_none();
            }
            found = strstr(target.string_value, args[0].string_value);
            return py_int(found == NULL ? -1 : (int)(found - target.string_value));
        }
    }

    py_error(p->py, "unknown method for value");
    return py_none();
}

static PY_NOINLINE py_value_t parse_primary(parser_t *p) {
    py_value_t value = parse_atom(p);

    while (!py_has_error(p->py)) {
        if (match(p, TOK_DOT)) {
            token_t attribute = *current(p);
            if (!expect(p, TOK_IDENT, "expected attribute name")) {
                return py_none();
            }
            if (current(p)->type == TOK_LPAREN) {
                value = call_method(p, value, attribute.text);
            } else if (p->eval_suppressed) {
                value = py_none();
            } else if (!py_module_attribute(p, value, attribute.text, &value)) {
                return py_none();
            }
        } else if (match(p, TOK_LBRACKET)) {
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
                value = p->eval_suppressed ? py_none() : py_subscript(p, value, index);
            }
          }
        } else {
            break;
        }
    }
    return value;
}

static py_value_t parse_unary(parser_t *p);
static py_value_t parse_unary_impl(parser_t *p);

static PY_NOINLINE py_value_t parse_power(parser_t *p) {
    py_value_t left = parse_primary(p);
    if (!py_has_error(p->py) && match(p, TOK_STARSTAR)) {
        left = eval_binary(p, left, TOK_STARSTAR, parse_unary(p));
    }
    return left;
}

static py_value_t parse_unary(parser_t *p) {
    if (p->expression_depth >= PY_MAX_EXPRESSION_DEPTH) {
        py_error(p->py, "expression nesting too deep");
        return py_none();
    }
    p->expression_depth++;
    py_value_t value = parse_unary_impl(p);
    p->expression_depth--;
    return value;
}

static py_value_t parse_unary_impl(parser_t *p) {
    if (match(p, TOK_NOT)) {
        py_value_t value = parse_unary(p);
        return p->eval_suppressed ? py_none() : py_bool(!py_truthy(value));
    }
    if (match(p, TOK_PLUS)) {
        py_value_t value = parse_unary(p);
        if (p->eval_suppressed) return py_none();
        if (!py_is_number(value)) {
            py_error(p->py, "unary plus requires number");
            return py_none();
        }
        return value.type == PY_VALUE_FLOAT ? py_float(value.float_value) : py_int(value.int_value);
    }
    if (match(p, TOK_MINUS)) {
        py_value_t value = parse_unary(p);
        if (p->eval_suppressed) return py_none();
        if (!py_is_number(value)) {
            py_error(p->py, "unary minus requires number");
            return py_none();
        }
        if (value.type == PY_VALUE_FLOAT) {
            return py_float(-value.float_value);
        }
        if (value.int_value == INT64_MIN) {
            py_error(p->py, "integer overflow");
            return py_none();
        }
        return py_int(-value.int_value);
    }
    if (match(p, TOK_TILDE)) {
        py_value_t value = parse_unary(p);
        if (p->eval_suppressed) return py_none();
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
        int negate_membership = 0;
        if (op == TOK_NOT && peek(p, 1)->type == TOK_IN) {
            negate_membership = 1;
            p->pos += 2;
            op = TOK_IN;
        } else if (op == TOK_IN) {
            p->pos++;
        } else if (op == TOK_LT || op == TOK_LE || op == TOK_GT || op == TOK_GE) {
            p->pos++;
        } else {
            break;
        }
        if (op == TOK_IN) {
            py_value_t right = parse_bit_or(p);
            if (p->eval_suppressed) {
                left = py_none();
                continue;
            }
            int contained = py_contains(p->py, right, left);
            if (!py_has_error(p->py)) {
                left = py_bool(negate_membership ? !contained : contained);
            }
        } else {
            left = eval_binary(p, left, op, parse_bit_or(p));
        }
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
            left = p->eval_suppressed ? py_none() : py_bool(!py_values_equal(&left, &right));
        } else if (op == TOK_IS) {
            py_value_t right = parse_comparison(p);
            left = p->eval_suppressed ? py_none() : py_bool(py_values_equal(&left, &right));
        } else {
            left = eval_binary(p, left, op, parse_comparison(p));
        }
    }
    return left;
}

static py_value_t parse_and(parser_t *p) {
    py_value_t left = parse_equality(p);
    while (!py_has_error(p->py) && match(p, TOK_AND)) {
        if (!p->eval_suppressed && !py_truthy(left)) {
            p->eval_suppressed++;
            (void)parse_equality(p);
            p->eval_suppressed--;
        } else {
            py_value_t right = parse_equality(p);
            if (!p->eval_suppressed) left = right;
        }
    }
    return left;
}

static py_value_t parse_expression(parser_t *p) {
    if (p->expression_depth >= PY_MAX_EXPRESSION_DEPTH) {
        py_error(p->py, "expression nesting too deep");
        return py_none();
    }
    p->expression_depth++;
    py_value_t left = parse_and(p);
    while (!py_has_error(p->py) && match(p, TOK_OR)) {
        if (!p->eval_suppressed && py_truthy(left)) {
            p->eval_suppressed++;
            (void)parse_and(p);
            p->eval_suppressed--;
        } else {
            py_value_t right = parse_and(p);
            if (!p->eval_suppressed) left = right;
        }
    }
    p->expression_depth--;
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
    static char int_text[32];

    switch (token->type) {
        case TOK_INT:
            snprintf(int_text, sizeof(int_text), "%" PRId64, token->int_value);
            return int_text;
        case TOK_FLOAT:
            snprintf(int_text, sizeof(int_text), "%g", token->float_value);
            return int_text;
        case TOK_STRING:
        case TOK_FSTRING:
        case TOK_BYTES:
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
        case TOK_NONLOCAL: return "nonlocal";
        case TOK_IMPORT: return "import";
        case TOK_FROM: return "from";
        case TOK_TRY: return "try";
        case TOK_EXCEPT: return "except";
        case TOK_FINALLY: return "finally";
        case TOK_RAISE: return "raise";
        case TOK_WITH: return "with";
        case TOK_AS: return "as";
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

    if (token->type == TOK_STRING || token->type == TOK_FSTRING || token->type == TOK_BYTES) {
        if (used + len + 4 > body_size) {
            return 0;
        }
        if (token->type == TOK_FSTRING) body[used++] = 'f';
        else if (token->type == TOK_BYTES) body[used++] = 'b';
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
    py_value_t defaults[PY_MAX_PARAMS];
    uint8_t has_default[PY_MAX_PARAMS] = {0};
    char vararg[PY_MAX_NAME] = "";
    char kwarg[PY_MAX_NAME] = "";
    size_t param_count = 0;
    int saw_default = 0;
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
        for (;;) {
            token_t param = *current(p);
            if (match(p, TOK_STARSTAR)) {
                param = *current(p);
                if (!expect(p, TOK_IDENT, "expected name after '**'")) return 0;
                if (!py_copy_bounded(p->py, kwarg, sizeof(kwarg), param.text,
                                     "keyword parameter name too long")) return 0;
                if (match(p, TOK_COMMA) && current(p)->type != TOK_RPAREN) {
                    py_error(p->py, "**kwargs must be the final parameter");
                    return 0;
                }
                break;
            }
            if (match(p, TOK_STAR)) {
                param = *current(p);
                if (!expect(p, TOK_IDENT, "expected name after '*'")) return 0;
                if (!py_copy_bounded(p->py, vararg, sizeof(vararg), param.text,
                                     "variadic parameter name too long")) return 0;
                if (!match(p, TOK_COMMA)) break;
                if (!match(p, TOK_STARSTAR)) {
                    py_error(p->py, "only **kwargs may follow *args");
                    return 0;
                }
                param = *current(p);
                if (!expect(p, TOK_IDENT, "expected name after '**'")) return 0;
                if (!py_copy_bounded(p->py, kwarg, sizeof(kwarg), param.text,
                                     "keyword parameter name too long")) return 0;
                if (match(p, TOK_COMMA) && current(p)->type != TOK_RPAREN) {
                    py_error(p->py, "**kwargs must be the final parameter");
                    return 0;
                }
                break;
            }
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
            defaults[param_count] = py_none();
            if (match(p, TOK_ASSIGN)) {
                if (!p->exec_enabled) p->eval_suppressed++;
                defaults[param_count] = parse_expression(p);
                if (!p->exec_enabled) p->eval_suppressed--;
                if (py_has_error(p->py)) return 0;
                has_default[param_count] = 1;
                saw_default = 1;
            } else if (saw_default) {
                py_error(p->py, "non-default parameter follows default parameter");
                return 0;
            }
            param_count++;
            if (!match(p, TOK_COMMA)) break;
            if (current(p)->type == TOK_RPAREN) break;
        }

        if (!expect(p, TOK_RPAREN, "expected ')' after parameters")) {
            return 0;
        }
    }
    if (!expect(p, TOK_COLON, "expected ':' after function definition")) {
        return 0;
    }

    body[0] = '\0';
    if (current(p)->type == TOK_LBRACE) {
        int depth = 1;
        p->pos++;
        while (depth > 0 && current(p)->type != TOK_EOF) {
            if (current(p)->type == TOK_LBRACE) {
                depth++;
            } else if (current(p)->type == TOK_RBRACE) {
                depth--;
            }
            if (depth == 0) {
                p->pos++;
                break;
            }
            if (!append_body_token(body, sizeof(body), current(p))) {
                py_error(p->py, "function body too long");
                return 0;
            }
            p->pos++;
        }

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
    return py_set_func(p->py, name.text, params, defaults, has_default,
                       param_count, vararg, kwarg, body);
}

static int parse_return(parser_t *p) {
    if (!expect(p, TOK_RETURN, "expected 'return'")) {
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

    if (current(p)->type == TOK_SEMI ||
        current(p)->type == TOK_RBRACE ||
        current(p)->type == TOK_EOF ||
        current(p)->type == TOK_ELIF ||
        current(p)->type == TOK_ELSE) {
        p->return_value = py_none();
    } else {
        p->return_value = parse_expression(p);
        if (py_has_error(p->py)) {
            return 0;
        }
    }
    p->return_signal = 1;
    return 1;
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

static PY_NOINLINE int parse_assignment(parser_t *p) {
    token_t name = *current(p);
    token_type_t assign_type;
    py_value_t value;
    py_value_t *indices = NULL;
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
            py_heap_free(indices);
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
            py_heap_free(indices);
            return 1;
        }
        if (indices == NULL) {
            indices = (py_value_t *)py_heap_calloc(PY_MAX_PARAMS, sizeof(*indices));
            if (indices == NULL) {
                py_error(p->py, "out of memory");
                return 0;
            }
        }
        indices[index_count++] = parse_expression(p);
        if (py_has_error(p->py)) {
            py_heap_free(indices);
            return 0;
        }
        if (!expect(p, TOK_RBRACKET, "expected ']' after assignment target")) {
            py_heap_free(indices);
            return 0;
        }
    }
    assign_type = current(p)->type;
    if (!is_assignment_operator(assign_type)) {
        py_error(p->py, "expected assignment operator");
        py_heap_free(indices);
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
        py_heap_free(indices);
        return 1;
    }
    value = parse_expression(p);
    if (py_has_error(p->py)) {
        py_heap_free(indices);
        return 0;
    }
    if (assign_type != TOK_ASSIGN) {
        if (index_count > 0) {
            target_var = py_find_var(p->py, name.text);
            if (target_var == NULL) {
                py_error(p->py, "undefined variable");
                py_heap_free(indices);
                return 0;
            }
            target = target_var->value;
            for (i = 0; i + 1 < index_count; ++i) {
                target = py_subscript(p, target, indices[i]);
                if (py_has_error(p->py)) {
                    py_heap_free(indices);
                    return 0;
                }
            }
            value = eval_binary(p, py_subscript(p, target, indices[index_count - 1]), assignment_to_binary(assign_type), value);
        } else {
            value = eval_binary(p, py_get_var(p->py, name.text), assignment_to_binary(assign_type), value);
        }
        if (py_has_error(p->py)) {
            py_heap_free(indices);
            return 0;
        }
    }
    if (!p->exec_enabled) {
        py_heap_free(indices);
        return 1;
    }
    if (index_count > 0) {
        target_var = py_find_var(p->py, name.text);
        if (target_var == NULL) {
            py_error(p->py, "undefined variable");
            py_heap_free(indices);
            return 0;
        }
        target = target_var->value;
        for (i = 0; i + 1 < index_count; ++i) {
            target = py_subscript(p, target, indices[i]);
            if (py_has_error(p->py)) {
                py_heap_free(indices);
                return 0;
            }
        }
        int assigned = py_assign_subscript(p, &target, indices[index_count - 1], value);
        py_heap_free(indices);
        return assigned;
    }
    py_heap_free(indices);
    return py_set_var(p->py, name.text, value);
}

static int parse_attribute_assignment(parser_t *p) {
    token_t target_name = *current(p);
    token_t attribute;
    py_value_t target;
    py_value_t value;

    if (!expect(p, TOK_IDENT, "expected object name") ||
        !expect(p, TOK_DOT, "expected '.' in property assignment")) return 0;
    attribute = *current(p);
    if (!expect(p, TOK_IDENT, "expected property name") ||
        !expect(p, TOK_ASSIGN, "expected '=' after property name")) return 0;
    if (!p->exec_enabled) {
        p->eval_suppressed++;
        (void)parse_expression(p);
        p->eval_suppressed--;
        return !py_has_error(p->py);
    }
    target = py_get_var(p->py, target_name.text);
    value = parse_expression(p);
    if (py_has_error(p->py)) return 0;
    if (target.type == PY_VALUE_NATIVE &&
        strcmp(target.string_value, "digitalio.DigitalInOut") == 0 &&
        strcmp(attribute.text, "value") == 0) {
        (void)call_method_with_args(p, target, "write", &value, 1);
        return !py_has_error(p->py);
    }
    py_error(p->py, "property is read-only or unsupported");
    return 0;
}

static PY_NOINLINE int parse_multi_assignment(parser_t *p) {
    token_t *names = (token_t *)py_heap_calloc(PY_MAX_PARAMS, sizeof(*names));
    py_value_t *values = (py_value_t *)py_heap_calloc(PY_MAX_PARAMS, sizeof(*values));
    size_t name_count = 0;
    size_t value_count = 0;

    if (names == NULL || values == NULL) {
        py_heap_free(names);
        py_heap_free(values);
        py_error(p->py, "out of memory");
        return 0;
    }

    do {
        if (name_count >= PY_MAX_PARAMS) {
            py_error(p->py, "too many assignment targets");
            goto fail;
        }
        names[name_count] = *current(p);
        if (!expect(p, TOK_IDENT, "expected assignment target")) {
            goto fail;
        }
        name_count++;
    } while (match(p, TOK_COMMA));

    if (!expect(p, TOK_ASSIGN, "expected '='")) {
        goto fail;
    }
    if (!p->exec_enabled) {
        while (current(p)->type != TOK_EOF &&
               current(p)->type != TOK_SEMI &&
               current(p)->type != TOK_RBRACE &&
               current(p)->type != TOK_ELIF &&
               current(p)->type != TOK_ELSE) {
            p->pos++;
        }
        goto success;
    }

    do {
        if (value_count >= PY_MAX_PARAMS) {
            py_error(p->py, "too many assignment values");
            goto fail;
        }
        values[value_count++] = parse_expression(p);
        if (py_has_error(p->py)) {
            goto fail;
        }
    } while (match(p, TOK_COMMA));

    if (name_count != value_count) {
        py_error(p->py, "assignment count mismatch");
        goto fail;
    }
    if (!p->exec_enabled) {
        goto success;
    }
    for (value_count = 0; value_count < name_count; ++value_count) {
        if (!py_set_var(p->py, names[value_count].text, values[value_count])) {
            goto fail;
        }
    }
success:
    py_heap_free(names);
    py_heap_free(values);
    return 1;

fail:
    py_heap_free(names);
    py_heap_free(values);
    return 0;
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
    if (p->return_signal) {
        p->exec_enabled = previous_exec_enabled;
        return 1;
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
        if (p->return_signal) {
            p->exec_enabled = previous_exec_enabled;
            return 1;
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
        if (p->return_signal) {
            p->exec_enabled = previous_exec_enabled;
            return 1;
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
        p->pos = body_start;
        p->exec_enabled = 1;
        if (!parse_block(p)) {
            p->exec_enabled = previous_exec_enabled;
            return 0;
        }
        p->exec_enabled = previous_exec_enabled;
        if (p->return_signal) {
            return 1;
        }
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
    py_value_t iterable = py_none();
    int range_loop = 0;
    int iterable_count = 0;
    int argc = 0;
    int start = 0;
    int stop = 0;
    int step = 1;
    int i;
    int previous_exec_enabled;
    size_t body_start;
    size_t after_loop;

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
    if (match(p, TOK_RANGE)) {
        range_loop = 1;
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
                if (py_has_error(p->py)) return 0;
            } while (match(p, TOK_COMMA));
            if (!expect(p, TOK_RPAREN, "expected ')' after range arguments")) return 0;
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
        if (py_has_error(p->py)) return 0;
        if (step == 0) {
            py_error(p->py, "range() step cannot be zero");
            return 0;
        }
    } else {
        iterable = parse_expression(p);
        if (py_has_error(p->py)) return 0;
        iterable_count = py_iterable_count(iterable);
        if (iterable_count < 0) {
            py_error(p->py, "for loop object is not iterable");
            return 0;
        }
    }
    if (!expect(p, TOK_COLON, "expected ':' after for iterable")) {
        return 0;
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

    if (!range_loop && !py_gc_push_root(p->py, iterable)) return 0;
    for (i = range_loop ? start : 0;
         !py_has_error(p->py) && previous_exec_enabled &&
             (range_loop ? ((step > 0) ? (i < stop) : (i > stop))
                         : (i < py_iterable_count(iterable)));
         i += range_loop ? step : 1) {
        py_value_t item = range_loop ? py_int(i) : py_iterable_item(p->py, iterable, i);
        if (!py_set_var(p->py, var_name.text, item)) {
            if (!range_loop) py_gc_pop_root(p->py, iterable);
            return 0;
        }
        p->pos = body_start;
        p->exec_enabled = 1;
        if (!parse_block(p)) {
            p->exec_enabled = previous_exec_enabled;
            if (!range_loop) py_gc_pop_root(p->py, iterable);
            return 0;
        }
        p->exec_enabled = previous_exec_enabled;
        if (p->return_signal) {
            if (!range_loop) py_gc_pop_root(p->py, iterable);
            return 1;
        }
        if (p->loop_signal == 1) {
            p->loop_signal = 0;
            break;
        }
        if (p->loop_signal == 2) {
            p->loop_signal = 0;
        }
    }

    if (!range_loop) py_gc_pop_root(p->py, iterable);
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

static int parse_global(parser_t *p) {
    if (!expect(p, TOK_GLOBAL, "expected 'global'")) {
        return 0;
    }
    do {
        token_t name = *current(p);
        if (!expect(p, TOK_IDENT, "expected global name")) {
            return 0;
        }
        if (p->exec_enabled && !py_declare_global(p->py, name.text)) return 0;
    } while (match(p, TOK_COMMA));
    return 1;
}

static int parse_nonlocal(parser_t *p) {
    if (!expect(p, TOK_NONLOCAL, "expected 'nonlocal'")) return 0;
    do {
        token_t name = *current(p);
        if (!expect(p, TOK_IDENT, "expected nonlocal name")) return 0;
        if (p->exec_enabled && !py_declare_nonlocal(p->py, name.text)) return 0;
    } while (match(p, TOK_COMMA));
    return 1;
}

static int py_import_user_module(parser_t *p, const char *module) {
    char path[PY_MAX_IMPORT_PATH + PY_MAX_NAME + 5];
    char output[PY_MAX_LINE];
    char previous_module[PY_MAX_NAME];

    for (size_t i = 0; i < p->py->imported_module_count; ++i) {
        if (strncmp(p->py->imported_modules[i], module, PY_MAX_NAME) == 0) return 1;
    }
    if (p->py->import_depth >= PY_MAX_IMPORTED_MODULES) {
        py_error(p->py, "module import depth exceeded");
        return 0;
    }
    if (p->py->imported_module_count >= PY_MAX_IMPORTED_MODULES) {
        py_error(p->py, "too many imported modules");
        return 0;
    }
    for (const char *c = module; *c != '\0'; ++c) {
        if (!isalnum((unsigned char)*c) && *c != '_') {
            py_error(p->py, "invalid module name");
            return 0;
        }
    }
    if (snprintf(path, sizeof(path), "%s/%s.py",
                 p->py->import_root[0] ? p->py->import_root : ".", module) >= (int)sizeof(path)) {
        py_error(p->py, "module path too long");
        return 0;
    }
    if (!py_copy_bounded(p->py, p->py->imported_modules[p->py->imported_module_count],
                         PY_MAX_NAME, module, "module name too long")) return 0;
    p->py->imported_module_count++;
    p->py->import_depth++;
    snprintf(previous_module, sizeof(previous_module), "%s", p->py->current_module);
    snprintf(p->py->current_module, sizeof(p->py->current_module), "%s", module);
    int ok = py_run_file(p->py, path, output, sizeof(output));
    snprintf(p->py->current_module, sizeof(p->py->current_module), "%s", previous_module);
    p->py->import_depth--;
    if (!ok) {
        if (strstr(p->py->error, "could not open file") != NULL) {
            p->py->current_line = 0;
            p->py->current_col = 0;
            p->py->exception_type[0] = '\0';
            py_error(p->py, "module is not available");
        }
        return 0;
    }
    py_append(p, output);
    return 1;
}

static int parse_raise(parser_t *p) {
    py_value_t exception;
    char type[PY_MAX_NAME] = "RuntimeError";
    char message[PY_MAX_STRING] = "exception raised";

    if (!expect(p, TOK_RAISE, "expected 'raise'")) return 0;
    if (!p->exec_enabled) {
        while (current(p)->type != TOK_EOF && current(p)->type != TOK_SEMI &&
               current(p)->type != TOK_RBRACE) p->pos++;
        return 1;
    }
    if (current(p)->type == TOK_EOF || current(p)->type == TOK_SEMI ||
        current(p)->type == TOK_RBRACE) {
        if (p->py->last_exception_type[0] == '\0') {
            py_error(p->py, "no active exception to reraise");
            return 0;
        }
        snprintf(type, sizeof(type), "%s", p->py->last_exception_type);
        snprintf(message, sizeof(message), "%s", p->py->last_exception_message);
    } else {
        exception = parse_expression(p);
        if (py_has_error(p->py)) return 0;
        if (exception.type == PY_VALUE_EXCEPTION) {
            py_exception_type(&exception, type, sizeof(type));
            snprintf(message, sizeof(message), "%s", exception.string_value);
        } else if (exception.type == PY_VALUE_STRING) {
            snprintf(message, sizeof(message), "%s", exception.string_value);
        } else {
            py_error(p->py, "exceptions must derive from Exception");
            return 0;
        }
    }
    snprintf(p->py->exception_type, sizeof(p->py->exception_type), "%s", type);
    py_error(p->py, message);
    return 0;
}

static size_t block_end_position(parser_t *p, size_t start) {
    if (p->tokens[start].type != TOK_LBRACE) return start;
    int depth = 0;
    for (size_t i = start; i < p->token_count; ++i) {
        if (p->tokens[i].type == TOK_LBRACE) depth++;
        else if (p->tokens[i].type == TOK_RBRACE && --depth == 0) return i + 1;
    }
    return p->token_count;
}

static int parse_try(parser_t *p) {
    char saved_error[PY_MAX_ERROR] = "";
    char saved_type[PY_MAX_NAME] = "";
    size_t saved_line = 0, saved_col = 0;
    int previous_exec = p->exec_enabled;
    int failed = 0;
    int handled = 0;

    if (!expect(p, TOK_TRY, "expected 'try'") ||
        !expect(p, TOK_COLON, "expected ':' after try")) return 0;
    size_t after_try = block_end_position(p, p->pos);
    if (!parse_block(p)) {
        if (!previous_exec || !py_has_error(p->py)) return 0;
        failed = 1;
        snprintf(saved_error, sizeof(saved_error), "%s", p->py->error);
        snprintf(saved_type, sizeof(saved_type), "%s",
                 p->py->exception_type[0] ? p->py->exception_type : "RuntimeError");
        saved_line = p->py->error_line;
        saved_col = p->py->error_col;
        snprintf(p->py->last_exception_type, sizeof(p->py->last_exception_type), "%s", saved_type);
        snprintf(p->py->last_exception_message, sizeof(p->py->last_exception_message), "%s", saved_error);
        p->py->error[0] = '\0';
        p->py->exception_type[0] = '\0';
        p->pos = after_try;
    }

    while (match(p, TOK_EXCEPT)) {
        char catch_type[PY_MAX_NAME] = "Exception";
        char alias[PY_MAX_NAME] = "";
        if (current(p)->type == TOK_IDENT) {
            if (!py_copy_bounded(p->py, catch_type, sizeof(catch_type), current(p)->text,
                                 "exception type name too long")) return 0;
            p->pos++;
            if (match(p, TOK_AS)) {
                if (current(p)->type != TOK_IDENT) {
                    py_error(p->py, "expected exception alias");
                    return 0;
                }
                if (!py_copy_bounded(p->py, alias, sizeof(alias), current(p)->text,
                                     "exception alias too long")) return 0;
                p->pos++;
            }
        }
        if (!expect(p, TOK_COLON, "expected ':' after except")) return 0;
        int selected = previous_exec && failed && !handled &&
                       py_exception_matches(saved_type, catch_type);
        p->exec_enabled = selected;
        if (selected && alias[0] != '\0' &&
            !py_set_var(p->py, alias, py_exception(p->py, saved_type, saved_error))) {
            p->exec_enabled = previous_exec;
            return 0;
        }
        if (!parse_block(p)) {
            p->exec_enabled = previous_exec;
            return 0;
        }
        if (selected) handled = 1;
    }
    if (match(p, TOK_ELSE)) {
        if (!expect(p, TOK_COLON, "expected ':' after else")) return 0;
        p->exec_enabled = previous_exec && !failed;
        if (!parse_block(p)) {
            p->exec_enabled = previous_exec;
            return 0;
        }
    }
    if (match(p, TOK_FINALLY)) {
        if (!expect(p, TOK_COLON, "expected ':' after finally")) return 0;
        p->exec_enabled = previous_exec;
        if (!parse_block(p)) return 0;
    }
    p->exec_enabled = previous_exec;
    if (previous_exec && failed && !handled && !py_has_error(p->py)) {
        snprintf(p->py->error, sizeof(p->py->error), "%s", saved_error);
        snprintf(p->py->exception_type, sizeof(p->py->exception_type), "%s", saved_type);
        p->py->error_line = saved_line;
        p->py->error_col = saved_col;
        return 0;
    }
    return !py_has_error(p->py);
}

static int parse_import(parser_t *p) {
    if (!expect(p, TOK_IMPORT, "expected 'import'")) return 0;
    do {
        token_t module = *current(p);
        token_t alias = module;
        if (!expect(p, TOK_IDENT, "expected module name")) return 0;
        if (!py_known_module(module.text) && p->exec_enabled &&
            !py_import_user_module(p, module.text)) return 0;
        if (match(p, TOK_AS)) {
            alias = *current(p);
            if (!expect(p, TOK_IDENT, "expected import alias")) return 0;
        }
        if (p->exec_enabled && !py_set_var(p->py, alias.text, py_module(module.text))) return 0;
    } while (match(p, TOK_COMMA));
    return 1;
}

static int py_import_constant(parser_t *p, const char *module, const char *name,
                              py_value_t *value) {
    int is_constant =
        (strcmp(module, "math") == 0 &&
         (strcmp(name, "pi") == 0 || strcmp(name, "e") == 0 ||
          strcmp(name, "tau") == 0 || strcmp(name, "inf") == 0 || strcmp(name, "nan") == 0)) ||
        (strcmp(module, "board") == 0 && (name[0] == 'D' || name[0] == 'A')) ||
        (strcmp(module, "digitalio") == 0 &&
         (strcmp(name, "INPUT") == 0 || strcmp(name, "OUTPUT") == 0 ||
          strcmp(name, "PULL_UP") == 0 || strcmp(name, "LOW") == 0 || strcmp(name, "HIGH") == 0));
    if (!is_constant) return 0;
    return py_module_attribute(p, py_module(module), name, value);
}

static int parse_from_import(parser_t *p) {
    token_t module;

    if (!expect(p, TOK_FROM, "expected 'from'")) return 0;
    module = *current(p);
    if (!expect(p, TOK_IDENT, "expected module name") ||
        !expect(p, TOK_IMPORT, "expected 'import' after module name")) return 0;
    if (!py_known_module(module.text) && p->exec_enabled &&
        !py_import_user_module(p, module.text)) return 0;

    do {
        token_t member = *current(p);
        token_t alias = member;
        py_value_t value = py_none();
        if (!expect(p, TOK_IDENT, "expected imported name")) return 0;
        if (match(p, TOK_AS)) {
            alias = *current(p);
            if (!expect(p, TOK_IDENT, "expected import alias")) return 0;
        }
        if (!p->exec_enabled) continue;

        if (py_known_module(module.text)) {
            if (!py_import_constant(p, module.text, member.text, &value)) {
                char qualified[PY_MAX_STRING];
                if (snprintf(qualified, sizeof(qualified), "%s.%s", module.text, member.text) >=
                    (int)sizeof(qualified)) {
                    py_error(p->py, "imported callable name too long");
                    return 0;
                }
                value = py_callable(qualified);
            }
        } else {
            py_var_t *variable = py_find_module_var(p->py, module.text, member.text);
            py_func_t *function = variable != NULL && variable->value.type == PY_VALUE_CALLABLE
                                    ? py_find_func(p->py, variable->value.string_value) : NULL;
            if (variable != NULL) value = variable->value;
            else if (function != NULL) value = py_callable(function->name);
            else {
                py_error(p->py, "name is not exported by module");
                return 0;
            }
        }
        if (!py_set_var(p->py, alias.text, value)) return 0;
    } while (match(p, TOK_COMMA));
    return 1;
}

static int parse_with(parser_t *p) {
    int execute = p->exec_enabled;
    token_t alias;
    py_value_t resource;

    if (!expect(p, TOK_WITH, "expected 'with'")) return 0;
    if (!execute) p->eval_suppressed++;
    resource = parse_expression(p);
    if (!execute) p->eval_suppressed--;
    if (py_has_error(p->py) || !expect(p, TOK_AS, "expected 'as' in with statement")) return 0;
    alias = *current(p);
    if (!expect(p, TOK_IDENT, "expected context variable") ||
        !expect(p, TOK_COLON, "expected ':' after with statement")) return 0;
    if (execute) {
        if (resource.type != PY_VALUE_NATIVE) {
            py_error(p->py, "object does not support the context manager protocol");
            return 0;
        }
        if (!py_set_var(p->py, alias.text, resource)) return 0;
    }
    int ok = parse_block(p);
    if (execute) {
        char saved_error[PY_MAX_ERROR];
        char saved_type[PY_MAX_NAME];
        snprintf(saved_error, sizeof(saved_error), "%s", p->py->error);
        snprintf(saved_type, sizeof(saved_type), "%s", p->py->exception_type);
        p->py->error[0] = '\0';
        p->py->exception_type[0] = '\0';
        (void)call_method_with_args(p, resource, "deinit", NULL, 0);
        if (saved_error[0] != '\0') {
            snprintf(p->py->error, sizeof(p->py->error), "%s", saved_error);
            snprintf(p->py->exception_type, sizeof(p->py->exception_type), "%s", saved_type);
            return 0;
        }
    }
    return ok && !py_has_error(p->py);
}

static int py_before_statement(parser_t *p, const token_t *token) {
    unsigned long limit;
    if (!p->exec_enabled) return 1;
    if (p->py->call_depth == 0 && p->py->object_count >= (PY_MAX_OBJECTS * 3U) / 4U) {
        (void)py_collect_garbage(p->py);
    }
    p->py->current_line = token->line;
    p->py->current_col = token->col;
    p->py->profile.statements++;
    limit = p->py->statement_limit != 0 ? p->py->statement_limit : 100000UL;
    if (limit != PY_EXECUTION_UNLIMITED && p->py->profile.statements > limit) {
        py_error(p->py, "statement limit exceeded");
        return 0;
    }
    return py_debug_event(p->py, PY_DEBUG_STATEMENT, token->line,
                          p->py->call_depth > 0
                              ? p->py->call_stack[p->py->call_depth - 1]
                              : "<script>");
}

static int parse_statement(parser_t *p) {
    token_t *token = current(p);

    if (token->type == TOK_EOF) {
        return 1;
    }
    if (!py_before_statement(p, token)) {
        return 0;
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
    if (token->type == TOK_RETURN) {
        return parse_return(p);
    }
    if (token->type == TOK_GLOBAL) {
        return parse_global(p);
    }
    if (token->type == TOK_NONLOCAL) {
        return parse_nonlocal(p);
    }
    if (token->type == TOK_IMPORT) {
        return parse_import(p);
    }
    if (token->type == TOK_FROM) {
        return parse_from_import(p);
    }
    if (token->type == TOK_WITH) {
        return parse_with(p);
    }
    if (token->type == TOK_RAISE) {
        return parse_raise(p);
    }
    if (token->type == TOK_TRY) {
        return parse_try(p);
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
    if (token->type == TOK_IDENT && peek(p, 1)->type == TOK_DOT &&
        peek(p, 2)->type == TOK_IDENT && peek(p, 3)->type == TOK_ASSIGN) {
        return parse_attribute_assignment(p);
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
           current(p)->type != TOK_EXCEPT &&
           current(p)->type != TOK_FINALLY &&
           current(p)->type != TOK_RBRACE) {
        if (current(p)->type == TOK_SEMI) {
            p->pos++;
            continue;
        }
        if (!parse_statement(p)) {
            return 0;
        }
        if (p->loop_signal || p->return_signal) {
            return 1;
        }
        if (current(p)->type == TOK_SEMI) {
            p->pos++;
        } else if (current(p)->type != TOK_EOF &&
                   current(p)->type != TOK_ELIF &&
                   current(p)->type != TOK_ELSE &&
                   current(p)->type != TOK_EXCEPT &&
                   current(p)->type != TOK_FINALLY &&
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
        if (p->loop_signal || p->return_signal) {
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
    py_set_var(py, "INPUT_PULLUP", py_int(2));
    py_set_var(py, "graphics", py_module("graphics"));
    py_set_var(py, "keys", py_module("keys"));
    py_set_var(py, "storage", py_module("storage"));
    py_set_var(py, "audio", py_module("audio"));
    py_set_var(py, "math", py_module("math"));
    py_set_var(py, "random", py_module("random"));
    py_set_var(py, "time", py_module("time"));
    py_set_var(py, "statistics", py_module("statistics"));
    py_set_var(py, "sensors", py_module("sensors"));
    py_set_var(py, "board", py_module("board"));
    py_set_var(py, "digitalio", py_module("digitalio"));
    py_set_var(py, "analogio", py_module("analogio"));
    py_set_var(py, "busio", py_module("busio"));
    py->statement_limit = 100000UL;
    py->call_depth_limit = PY_MAX_TRACE_DEPTH;
    py->random_state = UINT64_C(0x6f70656e63616c63);
}

void py_deinit(py_t *py) {
    py_object_t *object;

    if (py == NULL) {
        return;
    }
    object = py->objects;
    while (object != NULL) {
        py_object_t *next = object->next;
        py_heap_free(object->items);
        py_heap_free(object->entries);
        py_heap_free(object);
        object = next;
    }
    py->objects = NULL;
    py->object_count = 0;
    py->container_item_capacity = 0;
    py->gc_root_count = 0;
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

void py_set_debug_callback(py_t *py, py_debug_callback_t callback, void *user_data) {
    if (py == NULL) return;
    py->debug_callback = callback;
    py->debug_user_data = user_data;
}

void py_set_native_callback(py_t *py, py_native_callback_t callback, void *user_data) {
    if (py == NULL) return;
    py->native_callback = callback;
    py->native_user_data = user_data;
}

void py_set_execution_limits(py_t *py, unsigned long statement_limit, unsigned long call_depth_limit) {
    if (py == NULL) return;
    py->statement_limit = statement_limit != 0 ? statement_limit : 100000UL;
    py->call_depth_limit = call_depth_limit != 0 ? call_depth_limit : PY_MAX_TRACE_DEPTH;
}

void py_request_abort(py_t *py) {
    if (py != NULL) py->abort_requested = 1;
}

void py_runtime_error(py_t *py, const char *message) {
    if (py == NULL || message == NULL || message[0] == '\0') return;
    py_error(py, message);
}

const py_profile_t *py_get_profile(const py_t *py) {
    return py != NULL ? &py->profile : NULL;
}

size_t py_format_variables(const py_t *py, char *out, size_t out_size) {
    size_t used = 0;
    if (out == NULL || out_size == 0) return 0;
    out[0] = '\0';
    if (py == NULL) return 0;
    for (size_t i = 0; i < py->var_count; i++) {
        char value[PY_MAX_STRING];
        int written;
        if (py->vars[i].value.type == PY_VALUE_MODULE) continue;
        if (!py_value_repr(&py->vars[i].value, value, sizeof(value), 1)) {
            snprintf(value, sizeof(value), "<value too large>");
        }
        written = snprintf(out + used, out_size - used, "%s%s = %s",
                           used == 0 ? "" : "\n", py->vars[i].name, value);
        if (written < 0 || (size_t)written >= out_size - used) {
            if (out_size >= 4) memcpy(out + out_size - 4, "...", 4);
            return out_size - 1;
        }
        used += (size_t)written;
    }
    return used;
}

const char *py_get_traceback(const py_t *py) {
    return py != NULL ? py->traceback : "";
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
        parser_free(parser);
        return py_none();
    }
    value = parse_expression(parser);
    if (py_has_error(py)) {
        parser_free(parser);
        return py_none();
    }
    if (!expect(parser, TOK_EOF, "unexpected trailing input")) {
        parser_free(parser);
        return py_none();
    }
    parser_free(parser);
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
    if (py->import_depth == 0) {
        memset(&py->profile, 0, sizeof(py->profile));
        py->abort_requested = 0;
        py->call_depth = 0;
        py->traceback[0] = '\0';
        py->error[0] = '\0';
        py->exception_type[0] = '\0';
        py->last_exception_type[0] = '\0';
        py->last_exception_message[0] = '\0';
        py->error_line = 0;
        py->error_col = 0;
        py->current_line = 0;
        py->current_col = 0;
    }

    if (output != NULL && output_size > 0) {
        output[0] = '\0';
    }

    if (!lex(parser)) {
        parser_free(parser);
        return 0;
    }
    if (!parse_statement_list(parser)) {
        parser_free(parser);
        return 0;
    }
    if (!expect(parser, TOK_EOF, "unexpected trailing input")) {
        parser_free(parser);
        return 0;
    }
    ok = !py_has_error(py);
    parser_free(parser);
    if (py->call_depth == 0 && py->import_depth == 0) (void)py_collect_garbage(py);
    return ok;
}

static int py_run_function_body(py_t *py, const char *source, char *output, size_t output_size, py_value_t *return_value) {
    parser_t *parser;
    int ok;

    if (py == NULL || source == NULL || return_value == NULL) {
        return 0;
    }

    *return_value = py_none();
    if (output != NULL && output_size > 0) {
        output[0] = '\0';
    }

    parser = parser_alloc();
    if (parser == NULL) {
        py_error(py, "out of memory");
        return 0;
    }

    parser->py = py;
    parser->source = source;
    parser->output = output;
    parser->output_size = output_size;
    parser->exec_enabled = 1;
    parser->return_value = py_none();
    py->error[0] = '\0';

    if (!lex(parser)) {
        parser_free(parser);
        return 0;
    }
    if (!parse_statement_list(parser)) {
        parser_free(parser);
        return 0;
    }
    if (!parser->return_signal && !expect(parser, TOK_EOF, "unexpected trailing input")) {
        parser_free(parser);
        return 0;
    }

    *return_value = parser->return_signal ? parser->return_value : py_none();
    ok = !py_has_error(py);
    parser_free(parser);
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
    size_t line_len;

    while (*line == ' ' || *line == '\t') {
        line++;
    }
    len = strlen(word);
    line_len = strlen(line);
    if (line_len < len) return 0;
    if (strncmp(line, word, len) != 0) return 0;
    if (line_len == len) return 1;
    return line[len] == ' ' || line[len] == '\t' || line[len] == ':';
}

static int continues_if_chain(const char *line) {
    return starts_with_word(line, "elif") || starts_with_word(line, "else") ||
           starts_with_word(line, "except") || starts_with_word(line, "finally");
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
    char clean[PY_MAX_LINE] = {0};
    char *trimmed;
    size_t len;
    int indent;

    trimmed = skip_indent((char *)line);
    if (trimmed[0] == '\0' || trimmed[0] == '\n' || trimmed[0] == '\r' || trimmed[0] == '#') {
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
    char *program;
    int indents[16];
    int depth = 0;
    int previous_colon = 0;
    size_t line_len = 0;
    size_t source_line = 1;
    const char *s;
    int ok = 0;

    if (py == NULL || source == NULL) {
        return 0;
    }
    if (output != NULL && output_size > 0) {
        output[0] = '\0';
    }

    if (py->import_depth == 0) {
        py->error[0] = '\0';
        py->error_line = 0;
        py->error_col = 0;
        py->current_line = 0;
        py->current_col = 0;
    }

    program = program_buffer_alloc();
    if (program == NULL) {
        py_error(py, "out of memory");
        return 0;
    }
    program[0] = '\0';
    indents[0] = 0;
    for (s = source; ; ++s) {
        if (*s == '\n' || *s == '\0') {
            line[line_len] = '\0';
            py->current_line = source_line;
            py->current_col = 1;
            if (!append_source_line(py, program, PY_MAX_PROGRAM, line, indents, &depth, &previous_colon)) {
                goto done;
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
            goto done;
        }
        line[line_len++] = *s;
    }

    while (depth > 0) {
        if (!append_program_text(py, program, PY_MAX_PROGRAM, " }")) {
            goto done;
        }
        depth--;
    }
    ok = py_run(py, program, output, output_size);

done:
    program_buffer_free(program);
    return ok;
}

int py_run_file(py_t *py, const char *path, char *output, size_t output_size) {
    FILE *file;
    char line[PY_MAX_LINE];
    char *program;
    int indents[16];
    int depth = 0;
    int previous_colon = 0;
    size_t source_line = 1;
    int ok = 0;

    if (py == NULL || path == NULL) {
        return 0;
    }
    if (py->import_depth == 0) {
        const char *slash = strrchr(path, '/');
        size_t root_length = slash == NULL ? 1 : (size_t)(slash - path);
        py->imported_module_count = 0;
        if (slash == NULL) {
            snprintf(py->import_root, sizeof(py->import_root), ".");
        } else if (root_length < sizeof(py->import_root)) {
            memcpy(py->import_root, path, root_length);
            py->import_root[root_length] = '\0';
        } else {
            py_error(py, "script path is too long");
            return 0;
        }
    }
    if (output != NULL && output_size > 0) {
        output[0] = '\0';
    }
    if (py->import_depth == 0) {
        py->error[0] = '\0';
        py->error_line = 0;
        py->error_col = 0;
        py->current_line = 0;
        py->current_col = 0;
    }

    program = program_buffer_alloc();
    if (program == NULL) {
        py_error(py, "out of memory");
        return 0;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        py_error(py, "could not open file");
        program_buffer_free(program);
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
            program_buffer_free(program);
            return 0;
        }
        if (!append_source_line(py, program, PY_MAX_PROGRAM, line, indents, &depth, &previous_colon)) {
            fclose(file);
            program_buffer_free(program);
            return 0;
        }
        source_line++;
    }

    while (depth > 0) {
        if (!append_program_text(py, program, PY_MAX_PROGRAM, " }")) {
            fclose(file);
            program_buffer_free(program);
            return 0;
        }
        depth--;
    }

    fclose(file);
    ok = py_run(py, program, output, output_size);
    program_buffer_free(program);
    return ok;
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
