#ifndef TINY_PYTHON_H
#define TINY_PYTHON_H

#include <stddef.h>

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
#define PY_MAX_STRING 256
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

typedef enum {
    PY_VALUE_NONE = 0,
    PY_VALUE_INT,
    PY_VALUE_FLOAT,
    PY_VALUE_BOOL,
    PY_VALUE_STRING,
    PY_VALUE_LIST,
    PY_VALUE_TUPLE,
    PY_VALUE_DICT
} py_value_type_t;

typedef struct py_object py_object_t;

typedef struct {
    py_value_type_t type;
    int int_value;
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

typedef struct {
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
    py_object_t *objects;
} py_t;

void py_init(py_t *py); // Initializes the interpreter state. You must call this once before running code. It clears variables and errors.
void py_deinit(py_t *py); // Frees heap-backed lists, tuples, and dictionaries owned by the interpreter.
void py_set_output_callback(py_t *py, void (*callback)(const char *text, void *user_data), void *user_data); // Streams print output immediately when callback is not NULL.
void py_set_input_callback(py_t *py, int (*callback)(char *buffer, size_t buffer_size, void *user_data), void *user_data); // Provides text for input(). Callback returns 1 on success, 0 on failure.
void py_use_stdio(py_t *py); // Convenience helper: routes print() to stdout and input() to stdin.
void py_set_gpio_callbacks(py_t *py, int (*mode_callback)(int pin, int mode, void *user_data), int (*write_callback)(int pin, int value, void *user_data), int (*read_callback)(int pin, int *value, void *user_data), void *user_data); // Enables pinMode(), digitalWrite(), and digitalRead().
int py_run(py_t *py, const char *line, char *output, size_t output_size); //  Runs one line of Python-like code. Good for a REPL or manually running one statement at a time.
int py_run_source(py_t *py, const char *source, char *output, size_t output_size); // Runs multiple lines from a C string. Variables stay alive between lines.
int py_run_file(py_t *py, const char *path, char *output, size_t output_size); // Runs a script from a file path, like /spiffs/main.py. You need to mount SPIFFS, LittleFS, or SD first.


#ifdef __cplusplus
}
#endif

#endif
