# tiny-python

A tiny Python-like interpreter written in C for ESP32 and other small embedded targets.

This is not CPython. It is a compact interpreter for running simple scripts on devices where a full Python runtime is too large. Most interpreter buffers are fixed-size, while lists, tuples, and dictionaries use heap storage so they can grow dynamically. The API can run one line, a source string, or a file from SPIFFS/LittleFS/SD.

## Features

- Integers, floats, booleans, strings, lists, tuples, dictionaries, and `None`
- Basic f-strings like `f"hello {name}"`
- Variables and augmented assignment
- Tuple-style assignment like `a, b = 1, 0`
- `print(...)` with optional `end="..."`
- `input()` and `input("prompt")` through a host callback
- Dynamic lists with `[]`, list literals, nested storage, indexing, item assignment, slicing with steps, `append(...)`, `len(...)`, and printing
- Tuples with literals, nested storage, indexing, slicing with steps, `len(...)`, and printing
- Dictionaries with literals, nested storage, key lookup, assignment after creation, `len(...)`, and printing
- Simple functions with parameters, return values, and recursion
- `if`, `elif`, and `else`
- `while`
- `for ... in range(...)`
- Comments with `#`
- Single-line statements and nested indented blocks
- Builtins: `len`, `int`, `float`, `str`, `bool`, `abs`, `min`, `max`, `pow`, `input`
- GPIO builtins: `pinMode`, `digitalWrite`, `digitalRead`
- Additional builtins: `ord`, `chr`, `type`
- Bitwise operators and shifts
- `break`, `continue`, and no-op `global`
- File execution with `py_run_file(...)`
- One-line stdio setup with `py_use_stdio(...)`
- Optional real-time output streaming callback
- Optional GPIO callbacks with `pinMode(...)`, `digitalWrite(...)`, and `digitalRead(...)`
- Error messages include line and column information when available

## Files

```text
tiny-python.c
tiny-python.h
```

Copy both files into your project and include only the header from your app code.

## Quick Start

```c
#include <stdio.h>
#include "tiny-python.h"

int main(void) {
    py_t py;

    py_init(&py);
    py_use_stdio(&py);

    if (!py_run_source(&py,
        "x = 2\n"
        "x += 3\n"
        "print('x is', x)\n",
        NULL,
        0)) {
        printf("python error: %s\n", py.error);
        py_deinit(&py);
        return 1;
    }

    py_deinit(&py);
    return 0;
}
```

Output:

```text
x is 5
```

## Build On Desktop

The implementation includes an optional desktop test runner.

```sh
cc -std=c99 -Wall -Wextra -I. -DPY_DESKTOP_MAIN tiny-python.c -o tiny-python-test
./tiny-python-test script.py
```

## ESP32 Usage

Place the files in your ESP-IDF or PlatformIO project:

```text
main/
  main.c
  tiny-python.c
  tiny-python.h
```

Simple stdio-style setup:

```c
#include <stdio.h>
#include "tiny-python.h"

void app_main(void) {
    py_t py;

    py_init(&py);
    py_use_stdio(&py);

    if (!py_run_file(&py, "/spiffs/main.py", NULL, 0)) {
        printf("python error: %s\n", py.error);
    }

    py_deinit(&py);
}
```

`py_use_stdio(...)` routes `print(...)` to `stdout` and `input(...)` to `stdin`. If your ESP32 project uses UART, USB serial, a display keyboard, BLE, or another input source, install custom callbacks with `py_set_output_callback(...)` and `py_set_input_callback(...)` instead.

Mount SPIFFS, LittleFS, or SD before calling `py_run_file(...)`. The interpreter uses standard `fopen`, so the path must be available through the ESP32 VFS, such as `/spiffs/main.py`.

## OpenCalc OS Integration

OpenCalc OS runs script files from `/data/scripts/` through `py_run_file(...)`. The Python app scans the USB-backed storage folder, runs the selected script on the main UI task, and routes `print(...)` output to the on-screen Python console. `input()` opens a modal keypad input line on the calculator screen; Enter submits, `CLEAR` deletes, and `DEL/Back` cancels. Serial button input only queues button numbers; script execution itself is handled by the main UI loop to avoid running the interpreter from the serial input task stack.

## GPIO

GPIO support is callback-based so the interpreter still compiles on both desktop and ESP32. After installing GPIO callbacks, Python scripts can use:

```python
pinMode(2, OUTPUT)
digitalWrite(2, HIGH)
print(digitalRead(2))
```

GPIO constants:

- `LOW` is `0`
- `HIGH` is `1`
- `INPUT` is `0`
- `OUTPUT` is `1`

ESP-IDF callback example:

```c
#include "driver/gpio.h"
#include "tiny-python.h"

static int gpio_mode(int pin, int mode, void *user_data) {
    (void)user_data;
    gpio_mode_t gpio_mode_value = mode ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
    return gpio_set_direction((gpio_num_t)pin, gpio_mode_value) == ESP_OK;
}

static int gpio_write(int pin, int value, void *user_data) {
    (void)user_data;
    return gpio_set_level((gpio_num_t)pin, value ? 1 : 0) == ESP_OK;
}

static int gpio_read(int pin, int *value, void *user_data) {
    (void)user_data;
    if (value == NULL) {
        return 0;
    }
    *value = gpio_get_level((gpio_num_t)pin);
    return 1;
}

void app_main(void) {
    py_t py;

    py_init(&py);
    py_use_stdio(&py);
    py_set_gpio_callbacks(&py, gpio_mode, gpio_write, gpio_read, NULL);

    if (!py_run_file(&py, "/spiffs/main.py", NULL, 0)) {
        printf("python error: %s\n", py.error);
    }

    py_deinit(&py);
}
```

If Python code calls `pinMode(...)`, `digitalWrite(...)`, or `digitalRead(...)` without callbacks installed, execution fails with a GPIO callback error.

## Script Examples

```python
name = "ESP32"
count = 3

print("hello", name)
print("len", len(name))

if count == 1:
    print("one")
elif count == 3:
    print("three")
else:
    print("other")

for i in range(5):
    print(i)

while count > 0:
    count -= 1

print("done", count)

a, b = 1, 0
print("pair", a, b)

def add(a, b):
    return a + b

print("add", add(2, 3))
print("same line", end=" ")
print("next")
print(f"add result: {add(2, 3)}")

def print_fibonacci(n):
    a, b = 0, 1
    for i in range(n):
        print(a, end=" ")
        a, b = b, a + b

print_fibonacci(10)

for row in range(2):
    for col in range(3):
        if col == 1:
            print(f"{row}:{col}", end=" ")
        else:
            print("x", end=" ")

x = 0
nums = []
while x < 5:
    s = input("Enter a number: ")
    nums.append(s)
    x += 1

print(nums)

data = [10, 20, [30, 40]]
print(data[0])
print(data[-1])
print(data[0:2])
print(data[::-1])
data[0] = 99
data[2][1] = 41
print(data)

point = (3, 4)
print(point)
print(point[1])

settings = {"name": "opencalc", "nums": data, 7: point}
settings["mode"] = "demo"
print(settings["name"])
print(settings["nums"][2][1])
print(settings[7])
print(settings["mode"])

print(5 / 2)
print(5 // 2)
print(float("2.5") * 2)

pinMode(2, OUTPUT)
digitalWrite(2, HIGH)
print(digitalRead(2))
```

The `x += 1` line matters. Without it, `while x < 5` never becomes false and the script keeps asking for input until the input callback fails or the interpreter loop limit is reached.

Single-line forms also work:

```python
if x > 10: print("big") else: print("small")
for i in range(3): print(i)
while x < 5: x += 1
```

## Supported Syntax

Values:

- Integers: `123`, `-5`
- Floats: `1.5`, `-2.25`
- Strings: `"hello"`, `'hello'`
- Lists: `[]`, `[1, "two", True]`
- Tuples: `()`, `(1, "two")`, `(1,)`
- Dictionaries: `{}`, `{"name": "esp32", 7: [1, 2]}`
- F-strings: `f"hello {name}"`, `f"{a + b}"`
- Booleans: `True`, `False`
- `None`

Operators:

- Arithmetic: `+`, `-`, `*`, `/`, `//`, `%`, `**`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Comparisons: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Identity-style comparisons: `is`, `is not`
- Logic: `and`, `or`, `not`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`

Statements:

- `name = expression`
- `name, name = expression, expression`
- `name.append(expression)`
- `name[index]`
- `name[start:stop]`
- `name[start:stop:step]`
- `name[index] = expression`
- `name[index] += expression`
- `dict_value[key]`
- `dict_value[key] = expression`
- `print(expression, ...)`
- `print(expression, ..., end=" ")`
- `input()`
- `input("prompt")`
- `pinMode(pin, INPUT)`
- `pinMode(pin, OUTPUT)`
- `digitalWrite(pin, LOW)`
- `digitalWrite(pin, HIGH)`
- `digitalRead(pin)`
- `def name(param, ...): statement`
- `def name(param, ...): indented block`
- `if expression: statement`
- `if expression: statement elif expression: statement else: statement`
- Nested `if` / `elif` / `else` blocks
- `while expression: statement`
- Nested `while` blocks
- `for name in range(...): statement`
- Nested `for` blocks
- `break`
- `continue`
- `global name`
- `pass`

`range(...)` supports:

- `range(stop)`
- `range(start, stop)`
- `range(start, stop, step)`

Lists support:

- Empty lists: `nums = []`
- Simple literals: `nums = [1, "two", True]`
- Appending: `nums.append(value)`
- Indexing: `nums[0]`, `nums[-1]`
- Slicing: `nums[1:3]`, `nums[:2]`, `nums[2:]`, `nums[::2]`, `nums[::-1]`
- Item assignment: `nums[0] = 5`, `nums[1] += 2`
- Nested lists, tuples, and dictionaries
- Length: `len(nums)`
- Printing: `print(nums)`

Tuples support:

- Empty tuples: `items = ()`
- Simple literals: `items = (1, "two")`
- Single-item tuples: `items = (1,)`
- Indexing and slicing with steps
- Nested lists, tuples, and dictionaries
- Length and printing

Dictionaries support:

- Empty dictionaries: `data = {}`
- Simple literals: `data = {"name": "esp32", 7: [1, 2]}`
- Lookup by key: `data["name"]`, `data[7]`
- Assignment after creation: `data["name"] = "new"`
- Nested lists, tuples, and dictionaries
- Length and printing

`input(...)` supports:

- `input()` with no prompt
- `input("prompt: ")` with a prompt written through the output callback or output buffer
- Returned input is always a string, so use `int(input("n: "))` for numbers

Division and floats:

- `/` returns a float result: `5 / 2` prints `2.5`
- `//` returns an integer-style result: `5 // 2` prints `2`
- `float(...)` converts strings and numbers to float values

## Input And Output

The easiest setup is `py_use_stdio(...)`. It sends Python `print(...)` output to `stdout` and reads Python `input(...)` from `stdin`.

```c
py_t py;

py_init(&py);
py_use_stdio(&py);

if (!py_run_file(&py, "/spiffs/main.py", NULL, 0)) {
    printf("python error: %s\n", py.error);
}

py_deinit(&py);
```

With `py_use_stdio(...)`, you normally pass `NULL, 0` for the output buffer because output streams immediately:

```c
py_run_source(&py, "print('hello')\n", NULL, 0);
```

If you do not install an output callback, `print(...)` is copied into the output buffer you provide:

```c
char output[256];

if (py_run_source(&py, "print('hello')\n", output, sizeof(output))) {
    printf("%s", output);
}
```

`input(...)` always needs an input source. If no input callback is installed, code like `name = input("name: ")` fails with `input callback not set`.

For custom ESP32 I/O, install callbacks. Use this when you want to read from UART, USB serial, a keypad, BLE, or another device-specific input source:

```c
static void serial_out(const char *text, void *user_data) {
    (void)user_data;
    printf("%s", text);
    fflush(stdout);
}

static int serial_in(char *buffer, size_t buffer_size, void *user_data) {
    (void)user_data;

    if (fgets(buffer, (int)buffer_size, stdin) == NULL) {
        return 0;
    }
    return 1;
}

py_t py;
py_init(&py);
py_set_output_callback(&py, serial_out, NULL);
py_set_input_callback(&py, serial_in, NULL);
```

The output callback receives chunks of text exactly as Python prints them. The input callback must write a null-terminated string into `buffer` and return `1` on success or `0` on failure. Newlines from input are stripped by `input(...)`.

## Error Reporting

When a run function fails, it returns `0` and writes the error text into `py.error`.

```c
if (!py_run_file(&py, "/spiffs/main.py", NULL, 0)) {
    printf("python error: %s\n", py.error);
}
```

Most parser and runtime errors include the source line and column directly in the error string:

```text
line 2, col 11: expected expression
line 1, col 9: invalid character
line 4, col 3: unexpected indent
```

You can also read the numeric fields:

```c
printf("line: %u col: %u\n", (unsigned)py.error_line, (unsigned)py.error_col);
```

`py.error_line` and `py.error_col` are `0` when the error is not tied to a source position, such as `could not open file`.

## Public API

```c
void py_init(py_t *py);
```

Initializes the interpreter state. Call this before running code.

```c
void py_deinit(py_t *py);
```

Frees heap-backed lists, tuples, and dictionaries created by the interpreter. Call this when you are done with the interpreter instance.

```c
void py_use_stdio(py_t *py);
```

Convenience helper that routes `print(...)` to `stdout` and `input(...)` to `stdin`. This is the easiest setup for desktop tests and ESP32 projects using stdio-backed serial.

```c
void py_set_output_callback(py_t *py, void (*callback)(const char *text, void *user_data), void *user_data);
```

Installs a real-time output callback. When this is set, `print(...)` streams immediately and the output buffer passed to `py_run`, `py_run_source`, or `py_run_file` can be `NULL`.

```c
void py_set_input_callback(py_t *py, int (*callback)(char *buffer, size_t buffer_size, void *user_data), void *user_data);
```

Installs the input callback used by Python `input()`. The callback should write a null-terminated string into `buffer` and return `1` on success or `0` on failure.

```c
void py_set_gpio_callbacks(
    py_t *py,
    int (*mode_callback)(int pin, int mode, void *user_data),
    int (*write_callback)(int pin, int value, void *user_data),
    int (*read_callback)(int pin, int *value, void *user_data),
    void *user_data);
```

Installs the GPIO callbacks used by `pinMode(...)`, `digitalWrite(...)`, and `digitalRead(...)`.

```c
int py_run(py_t *py, const char *line, char *output, size_t output_size);
```

Runs one statement. Returns `1` on success and `0` on failure.

```c
int py_run_source(py_t *py, const char *source, char *output, size_t output_size);
```

Runs multiple lines from a C string. Variables persist across lines.

```c
int py_run_file(py_t *py, const char *path, char *output, size_t output_size);
```

Runs a script file. Variables persist across lines.

On failure, all run functions return `0`; see the Error Reporting section for `py.error`, `py.error_line`, and `py.error_col`.

## Configuration

These limits can be overridden before including `tiny-python.h` or through compiler defines:

```c
#define PY_MAX_VARS 32
#define PY_MAX_NAME 16
#define PY_MAX_STRING 256
#define PY_MAX_ERROR 96
#define PY_MAX_FUNCS 16
#define PY_MAX_PARAMS 8
#define PY_MAX_FUNC_BODY 512
#define PY_MAX_PROGRAM 2048
```

Additional implementation limits in `tiny-python.c`:

```c
#define PY_MAX_TOKENS 512
#define PY_MAX_LINE 160
```

OpenCalc OS may override these with compiler defines from its ESP-IDF build.

## Limitations

- This is an embedded Python-like interpreter, not CPython.
- Containers are heap-backed. Call `py_deinit(...)` when you are done with an interpreter instance.
- Lists and dictionaries support item assignment. Tuples and strings are immutable.
- Slice reading supports steps, including `items[::2]` and `items[::-1]`. Slice assignment is not implemented.
- Large scripts can hit fixed parser buffers such as `PY_MAX_PROGRAM`, `PY_MAX_FUNC_BODY`, and `PY_MAX_TOKENS`.
- Strings, input lines, and printed representations are bounded by `PY_MAX_STRING` / `PY_MAX_LINE`.
- Use `py_use_stdio(...)` or `py_set_output_callback(...)` for streaming output. Otherwise output must fit in the caller-provided buffer.
- Loop execution has a guard to stop accidental infinite loops.
- Not implemented yet: comprehensions, classes, imports/modules, exceptions, generators, decorators, and `with`.

## License

MIT
