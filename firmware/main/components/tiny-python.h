#ifndef TINY_PYTHON_H
#define TINY_PYTHON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PY_MAX_VARS
#define PY_MAX_VARS 32
#endif

#ifndef PY_MAX_NAME
#define PY_MAX_NAME 16
#endif

#ifndef PY_MAX_STRING
#ifdef ESP_PLATFORM
#define PY_MAX_STRING 96
#else
#define PY_MAX_STRING 256
#endif
#endif

#ifndef PY_MAX_ERROR
#define PY_MAX_ERROR 96
#endif

#ifndef PY_MAX_FUNCS
#define PY_MAX_FUNCS 16
#endif

#ifndef PY_MAX_PARAMS
#define PY_MAX_PARAMS 8
#endif

#ifndef PY_MAX_FUNC_BODY
#define PY_MAX_FUNC_BODY 512
#endif

#ifndef PY_MAX_PROGRAM
#define PY_MAX_PROGRAM 2048
#endif

#ifndef PY_MAX_TRACE_DEPTH
#define PY_MAX_TRACE_DEPTH 12
#endif

#ifndef PY_MAX_OBJECTS
#define PY_MAX_OBJECTS 256
#endif

#ifndef PY_MAX_CONTAINER_ITEMS
#define PY_MAX_CONTAINER_ITEMS 1024
#endif

#ifndef PY_MAX_TOTAL_CONTAINER_ITEMS
#define PY_MAX_TOTAL_CONTAINER_ITEMS 4096
#endif

typedef enum {
    PY_VALUE_NONE = 0,
    PY_VALUE_INT,
    PY_VALUE_FLOAT,
    PY_VALUE_BOOL,
    PY_VALUE_STRING,
    PY_VALUE_LIST,
    PY_VALUE_TUPLE,
    PY_VALUE_DICT,
    PY_VALUE_MODULE
} py_value_type_t;

typedef struct py_object py_object_t;

typedef struct {
    py_value_type_t type;
    int64_t int_value;
    double float_value;
    char string_value[PY_MAX_STRING];
    py_object_t *object;
} py_value_t;

typedef struct {
    char name[PY_MAX_NAME];
    py_value_t value;
} py_var_t;

typedef struct {
    char name[PY_MAX_NAME];
    char params[PY_MAX_PARAMS][PY_MAX_NAME];
    size_t param_count;
    char body[PY_MAX_FUNC_BODY];
} py_func_t;

typedef struct py_runtime py_t;

typedef enum {
    PY_DEBUG_STATEMENT = 0,
    PY_DEBUG_CALL,
    PY_DEBUG_RETURN,
    PY_DEBUG_ERROR
} py_debug_event_t;

typedef struct {
    unsigned long statements;
    unsigned long function_calls;
    unsigned long max_call_depth;
} py_profile_t;

typedef int (*py_debug_callback_t)(py_t *py, py_debug_event_t event,
                                   size_t line, const char *function,
                                   void *user_data);
typedef int (*py_native_callback_t)(py_t *py, const char *module,
                                    const char *function,
                                    const py_value_t *args, size_t arg_count,
                                    py_value_t *result, void *user_data);

struct py_runtime {
    py_var_t vars[PY_MAX_VARS];
    py_func_t funcs[PY_MAX_FUNCS];
    size_t var_count;
    size_t func_count;
    char error[PY_MAX_ERROR];
    size_t error_line;
    size_t error_col;
    size_t current_line;
    size_t current_col;
    void (*output_callback)(const char *text, void *user_data);
    void *output_user_data;
    int (*input_callback)(char *buffer, size_t buffer_size, void *user_data);
    void *input_user_data;
    int (*gpio_mode_callback)(int pin, int mode, void *user_data);
    int (*gpio_write_callback)(int pin, int value, void *user_data);
    int (*gpio_read_callback)(int pin, int *value, void *user_data);
    void *gpio_user_data;
    py_debug_callback_t debug_callback;
    void *debug_user_data;
    py_native_callback_t native_callback;
    void *native_user_data;
    unsigned long statement_limit;
    unsigned long call_depth_limit;
    volatile int abort_requested;
    py_profile_t profile;
    size_t call_depth;
    char call_stack[PY_MAX_TRACE_DEPTH][PY_MAX_NAME];
    size_t call_lines[PY_MAX_TRACE_DEPTH];
    char traceback[PY_MAX_TRACE_DEPTH * (PY_MAX_NAME + 24)];
    py_object_t *objects;
    size_t object_count;
    size_t container_item_capacity;
};

void py_init(py_t *py); // Initializes the interpreter state. You must call this once before running code. It clears variables and errors.
void py_deinit(py_t *py); // Frees heap-backed lists, tuples, and dictionaries owned by the interpreter.
void py_set_output_callback(py_t *py, void (*callback)(const char *text, void *user_data), void *user_data); // Streams print output immediately when callback is not NULL.
void py_set_input_callback(py_t *py, int (*callback)(char *buffer, size_t buffer_size, void *user_data), void *user_data); // Provides text for input(). Callback returns 1 on success, 0 on failure.
void py_use_stdio(py_t *py); // Convenience helper: routes print() to stdout and input() to stdin.
void py_set_gpio_callbacks(py_t *py, int (*mode_callback)(int pin, int mode, void *user_data), int (*write_callback)(int pin, int value, void *user_data), int (*read_callback)(int pin, int *value, void *user_data), void *user_data); // Enables pinMode(), digitalWrite(), and digitalRead().
void py_set_debug_callback(py_t *py, py_debug_callback_t callback, void *user_data); // Called before executed statements and on call/return/error events. Return zero to abort safely.
void py_set_native_callback(py_t *py, py_native_callback_t callback, void *user_data); // Backs graphics.*, keys.*, storage.*, audio.*, and math.* module calls.
void py_set_execution_limits(py_t *py, unsigned long statement_limit, unsigned long call_depth_limit); // Zero keeps the corresponding default.
void py_request_abort(py_t *py); // Cooperatively stops execution at the next statement or function boundary.
void py_runtime_error(py_t *py, const char *message); // Reports a host/module error through the normal traceback path.
const py_profile_t *py_get_profile(const py_t *py);
size_t py_format_variables(const py_t *py, char *out, size_t out_size);
const char *py_get_traceback(const py_t *py);
int py_run(py_t *py, const char *line, char *output, size_t output_size); //  Runs one line of Python-like code. Good for a REPL or manually running one statement at a time.
int py_run_source(py_t *py, const char *source, char *output, size_t output_size); // Runs multiple lines from a C string. Variables stay alive between lines.
int py_run_file(py_t *py, const char *path, char *output, size_t output_size); // Runs a script from a file path, like /spiffs/main.py. You need to mount SPIFFS, LittleFS, or SD first.


#ifdef __cplusplus
}
#endif

#endif
