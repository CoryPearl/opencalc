#include "tiny-python.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char **values;
    size_t count;
    size_t index;
} input_fixture_t;

static int test_input(char *buffer, size_t buffer_size, void *user_data)
{
    input_fixture_t *fixture = (input_fixture_t *)user_data;
    if (buffer == NULL || buffer_size == 0 || fixture == NULL || fixture->index >= fixture->count) {
        return 0;
    }
    snprintf(buffer, buffer_size, "%s", fixture->values[fixture->index++]);
    return 1;
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

static int run_repeated_lifecycle_case(void)
{
    const char *source =
        "def fib(n):\n"
        "    if n <= 1:\n"
        "        return n\n"
        "    return fib(n - 1) + fib(n - 2)\n"
        "values = [fib(6), fib(7)]\n"
        "print(values[0] + values[1])\n";

    for (int iteration = 0; iteration < 20; iteration++) {
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
    if (!py_run_source(&py, "print(6 * 7)\n", output, sizeof(output)) || strcmp(output, "42\n") != 0) {
        fprintf(stderr, "FAIL error_recovery: %s output <%s>\n", py.error, output);
        py_deinit(&py);
        return 1;
    }

    py_deinit(&py);
    printf("PASS error_recovery\n");
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

    failed |= run_case("input_int_prompt",
                       "i = int(input(\"Enter a number: \"))\n"
                       "print(i + 2)\n",
                       "Enter a number: 10\n", input_8, 1);

    failed |= run_case("recursion",
                       "def fact(n):\n"
                       "    if n <= 1:\n"
                       "        return 1\n"
                       "    return n * fact(n - 1)\n"
                       "print(fact(5))\n",
                       "120\n", NULL, 0);

    failed |= run_case("collections",
                       "items = [1, 2]\n"
                       "items.append(3)\n"
                       "d = {\"a\": 7}\n"
                       "print(items[2] + d[\"a\"])\n",
                       "10\n", NULL, 0);

    failed |= run_file_case();
    failed |= run_repeated_lifecycle_case();
    failed |= run_error_recovery_case();
    return failed ? 1 : 0;
}
