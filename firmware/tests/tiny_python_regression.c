#include "tiny-python.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char **values;
    size_t count;
    size_t index;
} input_fixture_t;

typedef struct {
    unsigned long statements;
    size_t last_line;
} debug_fixture_t;

typedef struct {
    char text[1024];
    size_t length;
} output_fixture_t;

typedef struct {
    int mode;
    int value;
} gpio_fixture_t;

static int test_gpio_mode(int pin, int mode, void *user_data)
{
    gpio_fixture_t *fixture = (gpio_fixture_t *)user_data;
    if (pin < 0 || pin > 11 || fixture == NULL) return 0;
    fixture->mode = mode;
    return 1;
}

static int test_gpio_write(int pin, int value, void *user_data)
{
    gpio_fixture_t *fixture = (gpio_fixture_t *)user_data;
    if (pin < 0 || pin > 11 || fixture == NULL) return 0;
    fixture->value = value != 0;
    return 1;
}

static int test_gpio_read(int pin, int *value, void *user_data)
{
    gpio_fixture_t *fixture = (gpio_fixture_t *)user_data;
    if (pin < 0 || pin > 11 || fixture == NULL || value == NULL) return 0;
    *value = fixture->value;
    return 1;
}

static int test_debug(py_t *py, py_debug_event_t event, size_t line,
                      const char *function, void *user_data)
{
    (void)py;
    (void)function;
    debug_fixture_t *fixture = (debug_fixture_t *)user_data;
    if (event == PY_DEBUG_STATEMENT) {
        fixture->statements++;
        fixture->last_line = line;
    }
    return 1;
}

static int stop_long_running_script(py_t *py, py_debug_event_t event, size_t line,
                                    const char *function, void *user_data)
{
    (void)py;
    (void)line;
    (void)function;
    unsigned long *statements = (unsigned long *)user_data;
    if (event == PY_DEBUG_STATEMENT && ++(*statements) > 12000UL) return 0;
    return 1;
}

static int test_native(py_t *py, const char *module, const char *function,
                       const py_value_t *args, size_t arg_count,
                       py_value_t *result, void *user_data)
{
    (void)py;
    (void)user_data;
    memset(result, 0, sizeof(*result));
    if (strcmp(module, "math") == 0 && strcmp(function, "double") == 0 &&
        arg_count == 1 && args[0].type == PY_VALUE_INT) {
        result->type = PY_VALUE_INT;
        result->int_value = args[0].int_value * 2;
        result->float_value = (double)result->int_value;
        return 1;
    }
    if (strcmp(module, "sensors") == 0 && strcmp(function, "available") == 0 &&
        arg_count == 0) {
        result->type = PY_VALUE_BOOL;
        result->int_value = 1;
        return 1;
    }
    if (strcmp(module, "sensors") == 0 && strcmp(function, "analog_read") == 0 &&
        arg_count == 1 && args[0].type == PY_VALUE_INT) {
        result->type = PY_VALUE_FLOAT;
        result->float_value = 1.25;
        return 1;
    }
    if (strcmp(module, "digitalio") == 0 && strcmp(function, "read") == 0 &&
        arg_count == 1 && args[0].type == PY_VALUE_INT) {
        result->type = PY_VALUE_BOOL;
        result->int_value = 1;
        return 1;
    }
    if (strcmp(module, "analogio") == 0 && strcmp(function, "read") == 0 &&
        arg_count == 1 && args[0].type == PY_VALUE_INT) {
        result->type = PY_VALUE_FLOAT;
        result->float_value = 2.5;
        return 1;
    }
    if (strcmp(module, "analogio") == 0 && strcmp(function, "raw") == 0 &&
        arg_count == 1 && args[0].type == PY_VALUE_INT) {
        result->type = PY_VALUE_INT;
        result->int_value = 1234;
        result->float_value = 1234.0;
        return 1;
    }
    if (strcmp(module, "busio") == 0 && strcmp(function, "present") == 0 &&
        arg_count == 1 && args[0].type == PY_VALUE_INT) {
        result->type = PY_VALUE_BOOL;
        result->int_value = 1;
        return 1;
    }
    return 0;
}

static int test_input(char *buffer, size_t buffer_size, void *user_data)
{
    input_fixture_t *fixture = (input_fixture_t *)user_data;
    if (buffer == NULL || buffer_size == 0 || fixture == NULL || fixture->index >= fixture->count) {
        return 0;
    }
    snprintf(buffer, buffer_size, "%s", fixture->values[fixture->index++]);
    return 1;
}

static void test_stream_output(const char *text, void *user_data)
{
    output_fixture_t *fixture = (output_fixture_t *)user_data;
    if (text == NULL || fixture == NULL) return;
    size_t available = sizeof(fixture->text) - fixture->length;
    if (available <= 1) return;
    int written = snprintf(fixture->text + fixture->length, available, "%s", text);
    if (written > 0) {
        fixture->length += (size_t)written < available ? (size_t)written : available - 1;
    }
}

static void normalize_output(char *text)
{
    if (text == NULL) {
        return;
    }
    for (char *p = text; *p != '\0'; p++) {
        if (*p == '\r') {
            *p = '\n';
        }
    }
}

static int run_case(const char *name, const char *source, const char *expected,
                    const char **inputs, size_t input_count)
{
    py_t py;
    char output[1024];
    input_fixture_t fixture = {
        .values = inputs,
        .count = input_count,
        .index = 0,
    };

    py_init(&py);
    py_set_input_callback(&py, test_input, &fixture);

    int ok = py_run_source(&py, source, output, sizeof(output));
    normalize_output(output);
    if (!ok) {
        fprintf(stderr, "FAIL %s: %s at line %zu col %zu\n",
                name, py.error, py.error_line, py.error_col);
        py_deinit(&py);
        return 1;
    }
    if (strcmp(output, expected) != 0) {
        fprintf(stderr, "FAIL %s: expected <%s> got <%s>\n", name, expected, output);
        py_deinit(&py);
        return 1;
    }
    if (fixture.index != input_count) {
        fprintf(stderr, "FAIL %s: used %zu/%zu inputs\n", name, fixture.index, input_count);
        py_deinit(&py);
        return 1;
    }

    py_deinit(&py);
    printf("PASS %s\n", name);
    return 0;
}

static int run_error_case(const char *name, const char *source, const char *expected_error)
{
    py_t py;
    char output[128];
    py_init(&py);
    int ok = py_run_source(&py, source, output, sizeof(output));
    if (ok || strstr(py.error, expected_error) == NULL) {
        fprintf(stderr, "FAIL %s: expected error containing <%s>, got <%s>\n",
                name, expected_error, py.error);
        py_deinit(&py);
        return 1;
    }
    if (!py_run_source(&py, "print(6 * 7)\n", output, sizeof(output)) ||
        strcmp(output, "42\n") != 0) {
        fprintf(stderr, "FAIL %s recovery: %s output <%s>\n", name, py.error, output);
        py_deinit(&py);
        return 1;
    }
    py_deinit(&py);
    printf("PASS %s\n", name);
    return 0;
}

static int run_file_case(void)
{
    const char *path = "/tmp/opencalc_tiny_python_file_test.py";
    const char *source =
        "a = 0\n"
        "b = 1\n"
        "i = int(input(\"Enter a number: \"))\n"
        "while i < 10:\n"
        "    print(a)\n"
        "    next_value = a + b\n"
        "    a = b\n"
        "    b = next_value\n"
        "    i = i + 1\n";
    const char *inputs[] = {"8"};
    py_t py;
    char output[1024];
    input_fixture_t fixture = {
        .values = inputs,
        .count = 1,
        .index = 0,
    };

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        perror(path);
        return 1;
    }
    fputs(source, file);
    fclose(file);

    py_init(&py);
    py_set_input_callback(&py, test_input, &fixture);
    int ok = py_run_file(&py, path, output, sizeof(output));
    remove(path);
    normalize_output(output);
    if (!ok) {
        fprintf(stderr, "FAIL file_fib_input: %s at line %zu col %zu\n",
                py.error, py.error_line, py.error_col);
        py_deinit(&py);
        return 1;
    }
    if (strcmp(output, "Enter a number: 0\n1\n") != 0) {
        fprintf(stderr, "FAIL file_fib_input: expected <Enter a number: 0\\n1\\n> got <%s>\n", output);
        py_deinit(&py);
        return 1;
    }
    py_deinit(&py);
    printf("PASS file_fib_input\n");
    return 0;
}

static int run_large_source_case(void)
{
    const size_t source_capacity = 24000;
    char *source = calloc(1, source_capacity);
    py_t py;
    char output[32];
    size_t used = 0;
    if (source == NULL) return 1;

    used += (size_t)snprintf(source + used, source_capacity - used, "x = 0\n");
    for (int i = 0; i < 1000; ++i) {
        used += (size_t)snprintf(source + used, source_capacity - used, "x = x + 1\n");
    }
    snprintf(source + used, source_capacity - used, "print(x)\n");

    if (strlen(source) <= 8192) {
        fprintf(stderr, "FAIL large_source: fixture did not exceed old 8 KB limit\n");
        free(source);
        return 1;
    }

    py_init(&py);
    int ok = py_run_source(&py, source, output, sizeof(output));
    free(source);
    if (!ok || strcmp(output, "1000\n") != 0) {
        fprintf(stderr, "FAIL large_source: %s output <%s>\n", py.error, output);
        py_deinit(&py);
        return 1;
    }
    py_deinit(&py);
    printf("PASS large_source\n");
    return 0;
}

static int run_user_module_case(void)
{
    const char *main_path = "/tmp/opencalc_tiny_import_main.py";
    const char *module_path = "/tmp/helpermod.py";
    FILE *module = fopen(module_path, "w");
    FILE *main_file = NULL;
    py_t py;
    char output[128];
    if (module == NULL) return 1;
    fputs("FACTOR = 3\ndef scale(value):\n    return value * FACTOR\n", module);
    fclose(module);
    main_file = fopen(main_path, "w");
    if (main_file == NULL) { remove(module_path); return 1; }
    fputs("FACTOR = 100\n"
          "import helpermod as helper\n"
          "from helpermod import scale as triple, FACTOR as HELPER_FACTOR\n"
          "callable_copy = triple\n"
          "print(helper.scale(7), helper.FACTOR, callable_copy(5), FACTOR, HELPER_FACTOR)\n",
          main_file);
    fclose(main_file);
    py_init(&py);
    int ok = py_run_file(&py, main_path, output, sizeof(output));
    remove(main_path);
    remove(module_path);
    if (!ok || strcmp(output, "21 3 15 100 3\n") != 0) {
        fprintf(stderr, "FAIL user_file_import: %s output <%s>\n", py.error, output);
        py_deinit(&py);
        return 1;
    }
    py_deinit(&py);
    printf("PASS user_file_import\n");
    return 0;
}

static int run_hardware_objects_case(void)
{
    py_t py;
    char output[128];
    gpio_fixture_t gpio = {0};
    py_init(&py);
    py_set_gpio_callbacks(&py, test_gpio_mode, test_gpio_write, test_gpio_read, &gpio);
    py_set_native_callback(&py, test_native, NULL);
    int ok = py_run_source(&py,
        "import board, digitalio, analogio, busio\n"
        "led = digitalio.DigitalInOut(board.D2)\n"
        "led.switch_to_output(False)\n"
        "led.value = True\n"
        "sensor = analogio.AnalogIn(board.A1)\n"
        "wire = busio.I2C()\n"
        "with busio.I2C() as managed:\n"
        "    locked = managed.try_lock()\n"
        "print(led.value, sensor.value, sensor.voltage(), wire.present(72), locked)\n",
        output, sizeof(output));
    if (!ok || strcmp(output, "True 1234 2.5 True True\n") != 0 || gpio.mode != 1 || gpio.value != 1) {
        fprintf(stderr, "FAIL hardware_objects: %s output <%s>\n", py.error, output);
        py_deinit(&py);
        return 1;
    }
    py_deinit(&py);
    printf("PASS hardware_objects\n");
    return 0;
}

static int run_streaming_input_case(void)
{
    const char *inputs[] = {"8"};
    input_fixture_t input = {.values = inputs, .count = 1, .index = 0};
    output_fixture_t output = {0};
    py_t py;

    py_init(&py);
    py_set_input_callback(&py, test_input, &input);
    py_set_output_callback(&py, test_stream_output, &output);
    int ok = py_run_file(&py, "storage_image/scripts/fib.py", NULL, 0);
    if (!ok || strcmp(output.text, "How many numbers: 0\n1\n1\n2\n3\n5\n8\n13\n") != 0) {
        fprintf(stderr, "FAIL streaming_input: %s output <%s>\n", py.error, output.text);
        py_deinit(&py);
        return 1;
    }
    py_deinit(&py);
    printf("PASS streaming_input\n");
    return 0;
}

static int run_repeated_lifecycle_case(void)
{
    const char *source =
        "def fib(n):\n"
        "    if n <= 1:\n"
        "        return n\n"
        "    return fib(n - 1) + fib(n - 2)\n"
        "values = [fib(6), fib(7)]\n"
        "print(values[0] + values[1])\n";

    for (int iteration = 0; iteration < 200; iteration++) {
        py_t py;
        char output[128];
        py_init(&py);
        int ok = py_run_source(&py, source, output, sizeof(output));
        if (!ok || strcmp(output, "21\n") != 0) {
            fprintf(stderr, "FAIL repeated_lifecycle iteration %d: %s output <%s>\n",
                    iteration, py.error, output);
            py_deinit(&py);
            return 1;
        }
        py_deinit(&py);
    }

    printf("PASS repeated_lifecycle\n");
    return 0;
}

static int run_same_runtime_case(void)
{
    py_t py;
    char output[256];

    py_init(&py);
    for (int iteration = 0; iteration < 100; iteration++) {
        if (!py_run_source(&py,
                           "items = [1, 2, 3]\n"
                           "items.append(4)\n"
                           "print(sum(items))\n",
                           output, sizeof(output)) || strcmp(output, "10\n") != 0) {
            fprintf(stderr, "FAIL same_runtime iteration %d: %s output <%s>\n",
                    iteration, py.error, output);
            py_deinit(&py);
            return 1;
        }
    }
    py_deinit(&py);
    printf("PASS same_runtime\n");
    return 0;
}

static int run_garbage_collection_case(void)
{
    py_t py;
    char output[256];

    py_init(&py);
    py_set_execution_limits(&py, 20000, 8);
    if (!py_run_source(&py,
                       "kept = [[1, 2], [3, 4]]\n"
                       "i = 0\n"
                       "while i < 1500:\n"
                       "    temporary = [i, [i + 1]]\n"
                       "    i = i + 1\n"
                       "cycle = []\n"
                       "cycle.append(cycle)\n"
                       "cycle = None\n"
                       "print(kept[1][0], i)\n",
                       output, sizeof(output)) || strcmp(output, "3 1500\n") != 0) {
        fprintf(stderr, "FAIL garbage_collection: %s output <%s> objects=%zu\n",
                py.error, output, py_object_count(&py));
        py_deinit(&py);
        return 1;
    }
    size_t before = py_object_count(&py);
    size_t reclaimed = py_collect_garbage(&py);
    if (before >= 16 || py_object_count(&py) > before) {
        fprintf(stderr, "FAIL garbage_collection accounting: before=%zu reclaimed=%zu after=%zu\n",
                before, reclaimed, py_object_count(&py));
        py_deinit(&py);
        return 1;
    }
    if (!py_run_source(&py, "print(kept[0][1])\n", output, sizeof(output)) ||
        strcmp(output, "2\n") != 0) {
        fprintf(stderr, "FAIL garbage_collection retained roots: %s output <%s>\n",
                py.error, output);
        py_deinit(&py);
        return 1;
    }
    py_deinit(&py);
    printf("PASS garbage_collection\n");
    return 0;
}

static int run_error_recovery_case(void)
{
    py_t py;
    char output[128];
    py_init(&py);

    if (py_run_source(&py, "print((1 + 2)\n", output, sizeof(output))) {
        fprintf(stderr, "FAIL error_recovery: malformed source unexpectedly passed\n");
        py_deinit(&py);
        return 1;
    }
    for (int iteration = 0; iteration < 20; ++iteration) {
        if (py_run_source(&py,
                          "def this_function_name_is_definitely_far_too_long():\n"
                          "    return 1\n",
                          output, sizeof(output)) || py.func_count != 0) {
            fprintf(stderr, "FAIL error_recovery: rejected function consumed a slot\n");
            py_deinit(&py);
            return 1;
        }
    }
    if (!py_run_source(&py, "print(6 * 7)\n", output, sizeof(output)) || strcmp(output, "42\n") != 0) {
        fprintf(stderr, "FAIL error_recovery: %s output <%s>\n", py.error, output);
        py_deinit(&py);
        return 1;
    }

    py_deinit(&py);
    printf("PASS error_recovery\n");
    return 0;
}

static int run_development_api_case(void)
{
    py_t py;
    char output[256];
    char variables[256];
    debug_fixture_t debug = {0};

    py_init(&py);
    py_set_debug_callback(&py, test_debug, &debug);
    py_set_native_callback(&py, test_native, NULL);
    if (!py_run_source(&py, "answer = math.double(21)\nprint(answer)\n",
                       output, sizeof(output)) || strcmp(output, "42\n") != 0) {
        fprintf(stderr, "FAIL development_api: %s output <%s>\n", py.error, output);
        py_deinit(&py);
        return 1;
    }
    if (!py_run_source(&py,
                       "print(sensors.available())\n"
                       "print(sensors.analog_read(0))\n",
                       output, sizeof(output)) || strcmp(output, "True\n1.25\n") != 0) {
        fprintf(stderr, "FAIL development_api sensors: %s output <%s>\n", py.error, output);
        py_deinit(&py);
        return 1;
    }
    if (!py_run_source(&py,
                       "import board, digitalio, analogio, busio\n"
                       "print(board.D10, board.A3, digitalio.PULL_UP)\n"
                       "print(digitalio.read(board.D1), analogio.read(board.A2), busio.present(64))\n",
                       output, sizeof(output)) || strcmp(output, "10 3 2\nTrue 2.5 True\n") != 0) {
        fprintf(stderr, "FAIL development_api compatibility modules: %s output <%s>\n",
                py.error, output);
        py_deinit(&py);
        return 1;
    }
    const py_profile_t *profile = py_get_profile(&py);
    py_format_variables(&py, variables, sizeof(variables));
    if (debug.statements < 2 || profile == NULL || profile->statements < 2 ||
        strstr(variables, "answer = 42") == NULL) {
        fprintf(stderr, "FAIL development_api: hooks/profile/variables incomplete\n");
        py_deinit(&py);
        return 1;
    }

    py_set_execution_limits(&py, 4, 4);
    if (py_run_source(&py, "i = 0\nwhile i < 20:\n    i = i + 1\n", output, sizeof(output)) ||
        strstr(py.error, "statement limit exceeded") == NULL ||
        py_get_traceback(&py)[0] == '\0') {
        fprintf(stderr, "FAIL development_api sandbox: %s traceback <%s>\n",
                py.error, py_get_traceback(&py));
        py_deinit(&py);
        return 1;
    }

    py_set_execution_limits(&py, 100, 4);
    if (!py_run_source(&py, "print(6 * 7)\n", output, sizeof(output)) ||
        strcmp(output, "42\n") != 0) {
        fprintf(stderr, "FAIL development_api recovery: %s\n", py.error);
        py_deinit(&py);
        return 1;
    }
    py_deinit(&py);
    printf("PASS development_api\n");
    return 0;
}

static int run_forever_loop_cancel_case(void)
{
    py_t py;
    char output[64];
    unsigned long statements = 0;

    py_init(&py);
    py_set_execution_limits(&py, PY_EXECUTION_UNLIMITED, 4);
    py_set_debug_callback(&py, stop_long_running_script, &statements);
    int ok = py_run_source(&py, "while True:\n    pass\n", output, sizeof(output));
    if (ok || statements <= 10000UL || strstr(py.error, "stopped") == NULL) {
        fprintf(stderr, "FAIL forever_loop_cancel: statements=%lu error <%s>\n",
                statements, py.error);
        py_deinit(&py);
        return 1;
    }
    py_deinit(&py);
    printf("PASS forever_loop_cancel\n");
    return 0;
}

int main(void)
{
    int failed = 0;
    const char *input_8[] = {"8"};

    failed |= run_case("arithmetic_variables",
                       "a = 4\n"
                       "b = 5\n"
                       "print(a * b + 2)\n",
                       "22\n", NULL, 0);

    failed |= run_case("while_loop",
                       "i = 0\n"
                       "total = 0\n"
                       "while i < 5:\n"
                       "    total = total + i\n"
                       "    i = i + 1\n"
                       "print(total)\n",
                       "10\n", NULL, 0);

    failed |= run_case("blank_lines_preserve_block_indent",
                       "if True:\n"
                       "    print('before')\n"
                       "\n"
                       "    print('after')\n",
                       "before\nafter\n", NULL, 0);

    failed |= run_case("fibonacci_87_uses_64_bit_integers",
                       "a = 0\n"
                       "b = 1\n"
                       "i = 0\n"
                       "while i < 87:\n"
                       "    next_value = a + b\n"
                       "    a = b\n"
                       "    b = next_value\n"
                       "    i = i + 1\n"
                       "print(a)\n",
                       "679891637638612258\n", NULL, 0);

    failed |= run_case("input_int_prompt",
                       "i = int(input(\"Enter a number: \"))\n"
                       "print(i + 2)\n",
                       "Enter a number: 10\n", input_8, 1);

    failed |= run_case("nested_input_method",
                       "items = []\n"
                       "items.append(int(input(\"Value: \")))\n"
                       "print(items[0])\n",
                       "Value: 8\n", input_8, 1);

    failed |= run_case("nested_input_multi_assignment",
                       "a, b = int(input(\"First: \")), 2\n"
                       "print(a + b)\n",
                       "First: 10\n", input_8, 1);

    failed |= run_case("recursion",
                       "def fact(n):\n"
                       "    if n <= 1:\n"
                       "        return 1\n"
                       "    return n * fact(n - 1)\n"
                       "print(fact(5))\n",
                       "120\n", NULL, 0);

    failed |= run_case("function_local_and_global_scope",
                       "value = 10\n"
                       "calls = 0\n"
                       "def local_change(value):\n"
                       "    value = value + 5\n"
                       "    hidden = 99\n"
                       "    return value\n"
                       "def count_call():\n"
                       "    global calls\n"
                       "    calls = calls + 1\n"
                       "    return calls\n"
                       "print(local_change(2), value)\n"
                       "print(count_call(), count_call(), calls)\n",
                       "7 10\n1 2 2\n", NULL, 0);

    failed |= run_case("nested_lexical_lookup",
                       "def outer(a):\n"
                       "    def inner(b):\n"
                       "        return a + b\n"
                       "    return inner(3)\n"
                       "print(outer(4))\n",
                       "7\n", NULL, 0);

    failed |= run_case("short_circuit_boolean_ops",
                       "calls = 0\n"
                       "def touch():\n"
                       "    global calls\n"
                       "    calls = calls + 1\n"
                       "    return True\n"
                       "print(False and touch(), True or touch(), calls)\n"
                       "print(False and missing_function(), True or (1 / 0))\n",
                       "False True 0\nFalse True\n", NULL, 0);

    failed |= run_case("rich_function_arguments",
                       "def describe(a, b=2, *rest, **named):\n"
                       "    return a + b + len(rest) + named['extra']\n"
                       "print(describe(1, extra=4))\n"
                       "print(describe(*(1, 3, 8), **{'extra': 5}))\n",
                       "7\n10\n", NULL, 0);

    failed |= run_case("exceptions",
                       "caught = False\n"
                       "cleaned = False\n"
                       "try:\n"
                       "    raise ValueError('bad value')\n"
                       "except ValueError as error:\n"
                       "    caught = True\n"
                       "finally:\n"
                       "    cleaned = True\n"
                       "try:\n"
                       "    broken = 1 / 0\n"
                       "except ZeroDivisionError:\n"
                       "    math_caught = True\n"
                       "print(caught, cleaned, math_caught, type(error))\n",
                       "True True True ValueError\n", NULL, 0);

    failed |= run_case("exception_hierarchy_and_else",
                       "normal = False\n"
                       "caught = False\n"
                       "try:\n"
                       "    normal = True\n"
                       "except Exception:\n"
                       "    normal = False\n"
                       "else:\n"
                       "    completed = True\n"
                       "try:\n"
                       "    raise IndexError('bad index')\n"
                       "except LookupError:\n"
                       "    caught = True\n"
                       "try:\n"
                       "    bad_math = 1 / 0\n"
                       "except ArithmeticError:\n"
                       "    arithmetic = True\n"
                       "try:\n"
                       "    missing = [1][4]\n"
                       "except LookupError:\n"
                       "    lookup = True\n"
                       "print(normal, completed, caught, arithmetic, lookup)\n",
                       "True True True True True\n", NULL, 0);

    failed |= run_case("from_standard_import",
                       "from math import sqrt, pi as circle_pi\n"
                       "root = sqrt\n"
                       "print(root(81), round(circle_pi, 3))\n",
                       "9 3.142\n", NULL, 0);

    failed |= run_case("comprehensions_and_types",
                       "squares = [x * x for x in range(7) if x % 2 == 0]\n"
                       "pairs = [x * 10 + y for x in range(3) if x > 0 for y in range(2)]\n"
                       "mapping = {x: x * x for x in range(4) if x > 0}\n"
                       "grid = {x * 10 + y: x + y for x in range(2) for y in range(2)}\n"
                       "remainders = {x % 3 for x in range(7)}\n"
                       "unique = {3, 1, 3}\n"
                       "raw = b'AB'\n"
                       "buffer = bytearray(raw)\n"
                       "buffer[1] = 67\n"
                       "print(squares, pairs, mapping[3], grid[11], len(remainders), len(unique), raw, buffer, type(unique))\n",
                       "[0, 4, 16, 36] [10, 11, 20, 21] 9 2 3 2 bytes([65, 66]) bytearray([65, 67]) set\n", NULL, 0);

    failed |= run_case("persistent_closure_snapshot",
                       "def configure(scale):\n"
                       "    offset = 2\n"
                       "    def transform(value):\n"
                       "        return value * scale + offset\n"
                       "    return transform\n"
                       "configured = configure(4)\n"
                       "print(configured(5))\n",
                       "22\n", NULL, 0);

    failed |= run_case("first_class_mutable_closures",
                       "def make_counter(start):\n"
                       "    count = start\n"
                       "    def increment(step=1):\n"
                       "        nonlocal count\n"
                       "        count = count + step\n"
                       "        return count\n"
                       "    return increment\n"
                       "first = make_counter(1)\n"
                       "second = make_counter(10)\n"
                       "alias = first\n"
                       "print(first(), alias(2), second(), first())\n",
                       "2 4 11 5\n", NULL, 0);

    failed |= run_case("collections",
                       "items = [1, 2]\n"
                       "items.append(3)\n"
                       "d = {\"a\": 7}\n"
                       "print(items[2] + d[\"a\"])\n",
                       "10\n", NULL, 0);

    failed |= run_case("iterables_membership",
                       "total = 0\n"
                       "for value in [2, 3, 5]:\n"
                       "    total = total + value\n"
                       "letters = ''\n"
                       "for letter in 'abc':\n"
                       "    letters = letters + letter\n"
                       "keys = ''\n"
                       "for key in {'x': 1, 'y': 2}:\n"
                       "    keys = keys + key\n"
                       "print(total, letters, keys, 3 in [1, 3], 'z' not in keys)\n",
                       "10 abc xy True True\n", NULL, 0);

    failed |= run_case("list_dict_methods",
                       "items = [2, 3]\n"
                       "items.insert(0, 1)\n"
                       "items.extend([4, 5])\n"
                       "last = items.pop()\n"
                       "items.remove(2)\n"
                       "copy = items.copy()\n"
                       "copy.reverse()\n"
                       "d = {'answer': 42}\n"
                       "print(last, items, copy, d.get('answer'), d.get('missing', 9), d.keys())\n",
                       "5 [1, 3, 4] [4, 3, 1] 42 9 ['answer']\n", NULL, 0);

    failed |= run_case("strings_sequences_numeric",
                       "print('Ab C'.lower().strip(), 'go' * 3)\n"
                       "print([1, 2] + [3], (1, 2) * 2)\n"
                       "print(-7 // 3, -7 % 3, 2 ** -2)\n"
                       "print(min([5, 2, 9]), max('cab'), round(2.6), any([0, 1]), all([1, True]))\n",
                       "ab c gogogo\n[1, 2, 3] (1, 2, 1, 2)\n-3 2 0.25\n2 c 3 True True\n",
                       NULL, 0);

    failed |= run_case("python_style_builtins",
                       "print(range(1, 6, 2))\n"
                       "print(sorted([3, 1, 2]), enumerate('ab', 4))\n"
                       "print(reversed([1, 2, 3]), zip('ab', [4, 5]))\n"
                       "print(bin(10), oct(9), hex(255), divmod(-7, 3))\n"
                       "print(0 or 'fallback', 'value' and 7)\n",
                       "[1, 3, 5]\n[1, 2, 3] [(4, 'a'), (5, 'b')]\n"
                       "[3, 2, 1] [('a', 4), ('b', 5)]\n"
                       "0b1010 0o11 0xff (-3, 2)\n"
                       "fallback 7\n",
                       NULL, 0);

    failed |= run_case("standard_modules",
                       "import math, random\n"
                       "import time as clock\n"
                       "print(round(math.pi, 5), round(math.e, 5), math.factorial(6), math.gcd(54, 24))\n"
                       "print(math.isfinite(math.inf), math.isnan(math.nan), round(math.degrees(math.pi), 1))\n"
                       "random.seed(123)\n"
                       "first = random.randint(1, 6)\n"
                       "random.seed(123)\n"
                       "second = random.randint(1, 6)\n"
                       "pick = random.choice('abc')\n"
                       "print(first == second, first >= 1 and first <= 6, pick in 'abc')\n"
                       "items = [1, 2, 3, 4]\n"
                       "random.shuffle(items)\n"
                       "print(sorted(items), random.getrandbits(5) >= 0, math.prod([2, 3, 4]), math.lcm(6, 8))\n"
                       "print(math.isclose(0.1 + 0.2, 0.3))\n"
                       "before = clock.monotonic()\n"
                       "clock.sleep_ms(1)\n"
                       "after = clock.ticks_ms()\n"
                       "print(clock.monotonic() >= before, clock.time() > 0, clock.ticks_diff(after, after) == 0)\n",
                       "3.14159 2.71828 720 6\nFalse True 180\nTrue True True\n"
                       "[1, 2, 3, 4] True 24 24\nTrue\nTrue True True\n",
                       NULL, 0);

    failed |= run_case("statistics_module",
                       "import statistics\n"
                       "data = [1, 2, 3]\n"
                       "print(statistics.mean(data), statistics.median(data))\n"
                       "print(statistics.pvariance(data), statistics.stdev(data))\n",
                       "2 2\n0.666667 1\n", NULL, 0);

    failed |= run_error_case("unavailable_import",
                             "import socket\n",
                             "module is not available");
    failed |= run_error_case("function_local_does_not_leak",
                             "def create_local():\n"
                             "    hidden = 42\n"
                             "create_local()\n"
                             "print(hidden)\n",
                             "undefined variable");
    failed |= run_error_case("missing_required_argument",
                             "def pair(a, b=2):\n"
                             "    return a + b\n"
                             "pair()\n",
                             "missing required function argument");
    failed |= run_error_case("long_variable_name",
                             "this_variable_name_is_definitely_far_too_long = 1\n",
                             "variable name too long");
    failed |= run_error_case("long_function_name",
                             "def this_function_name_is_definitely_far_too_long():\n"
                             "    return 1\n",
                             "function name too long");

    failed |= run_error_case("integer_add_overflow",
                             "print(9223372036854775807 + 1)\n",
                             "integer overflow");
    failed |= run_error_case("integer_power_overflow",
                             "print(2 ** 63)\n",
                             "integer overflow");
    failed |= run_error_case("integer_floor_division_overflow",
                             "value = -9223372036854775807 - 1\n"
                             "print(value // -1)\n",
                             "integer overflow");
    failed |= run_error_case("oversized_shift",
                             "print(1 << 64)\n",
                             "invalid or overflowing shift");
    failed |= run_error_case("expression_depth_limit",
                             "print((((((((((((((((((((((((((((((1))))))))))))))))))))))))))))))\n",
                             "expression nesting too deep");
    failed |= run_error_case("container_size_limit",
                             "items = []\n"
                             "i = 0\n"
                             "while i < 1025:\n"
                             "    items.append(i)\n"
                             "    i = i + 1\n",
                             "container too large");

    failed |= run_file_case();
    failed |= run_user_module_case();
    failed |= run_hardware_objects_case();
    failed |= run_large_source_case();
    failed |= run_streaming_input_case();
    failed |= run_repeated_lifecycle_case();
    failed |= run_same_runtime_case();
    failed |= run_garbage_collection_case();
    failed |= run_error_recovery_case();
    failed |= run_development_api_case();
    failed |= run_forever_loop_cancel_case();
    return failed ? 1 : 0;
}
