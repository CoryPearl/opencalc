# Tiny Python Start Guide

Tiny Python is OpenCalc's bounded scripting language for calculator programs,
automation, graphics, and sensor experiments. It uses familiar Python syntax,
but it is not full CPython or a drop-in CircuitPython runtime.

## Start a Program

Press `PRGM` to create, edit, run, or debug a script. `2nd` + `PRGM` opens the
script browser directly. Scripts are stored in `/data/scripts/` on the
calculator. Files added to `firmware/storage_image/scripts/` are included when
the storage image is flashed.

When a script requests input, type with the calculator keypad and press
`Enter`. Press Back or Clear to stop a running script cooperatively.

## Language Basics

Tiny Python supports numbers, booleans, strings, lists, tuples, dictionaries,
sets, `bytes`, `bytearray`, functions, recursion, local variables, defaults,
keyword arguments, `*args`, `**kwargs`, f-strings, nested comprehensions, exceptions,
conditionals, iterable loops, and cancellable forever loops.

```python
def average(values):
    if len(values) == 0:
        raise ValueError("no values")
    return sum(values) / len(values)

samples = [x * x for x in range(8) if x % 2 == 0]

try:
    print("average:", average(samples))
except ValueError as error:
    print(error)
```

## Imports and Modules

The compact standard modules are `math`, `random`, `time`, and `statistics`.
OpenCalc also provides `graphics`, `keys`, `audio`, `storage`, `sensors`,
`board`, `digitalio`, `analogio`, and `busio`.

Sibling files can be imported by name. For example, `import helpers` loads
`helpers.py` from the running script's directory. Module globals are isolated,
and `from helpers import convert as convert_value` is supported. Package
directories, reload, and general search paths are not supported.

```python
import math
import random
import helpers

print(math.sqrt(81))
print(random.randint(1, 6))
print(helpers.convert(12))
```

Calculator math is available through `math.eval("2+2")` and symbolic CAS
commands through `math.cas("factor(x^2-1)")`.

## Files, Graphics, Keys, and Audio

`storage` reads and writes simple file names inside `/data/user/`. Graphics
commands are queued safely to the display task, and scripts can inspect keypad
buttons or play tones when audio hardware is enabled.

```python
storage.write("result.txt", "measurement complete")
graphics.clear(1054752)
graphics.text(20, 30, "Ready", 16777215)

if keys.down(50):
    audio.tone(880, 100)
```

Available operations include pixel, line, rectangle, and text drawing;
sandboxed file read/write/append/rename/remove; key-state reads; audio volume;
and tones.

## Scientific I/O

On the scientific-I/O PCB profile, scripts can use twelve 3.3 V digital
channels (`D0-D11`), four 16-bit analog inputs (`A0-A3`), and the protected
shared I2C bus. Never connect a 5 V signal directly to these pins.

```python
import time
import statistics
import board, digitalio, analogio, busio

led = digitalio.DigitalInOut(board.D0)
probe = analogio.AnalogIn(board.A0)
i2c = busio.I2C()
led.switch_to_output(False)

try:
    samples = []
    sensors.list_clear(1)

    for index in range(20):
        volts = probe.voltage()
        samples.append(volts)
        sensors.list_append(1, volts)
        led.write(index % 2 == 0)
        time.sleep(0.1)

    print("average:", statistics.mean(samples))
    print("I2C 0x76 present:", i2c.present(118))
finally:
    led.write(False)
    led.deinit()
    probe.deinit()
    i2c.deinit()
```

Measurements saved with `sensors.list_append()` can be opened directly in the
Lists, Graph, and Statistics apps. The `sensors` module also provides bounded
captures, digital and analog trigger waits, sample-rate selection, differential
analog reads, and protected register-level I2C access.

## Debugging

In the editor, press `Trace` on a source line to toggle a breakpoint. Choose
Debug instead of Run, then use:

| Key           | Action                                                 |
| ------------- | ------------------------------------------------------ |
| `Enter`       | Execute the next statement                             |
| `Trace`       | Continue to the next breakpoint                        |
| `Graph`       | Cycle Console, Variables, Traceback, and Profile views |
| Back or Clear | Stop the script cooperatively                          |

## Limits

Programs can use up to 32 KB of source and 8,192 parser tokens, with 48
functions, 96 module variables, 48 locals per call, bounded call depth, and
bounded container storage. Parser token storage grows on demand in PSRAM rather
than reserving the maximum for every parser.
Containers are garbage-collected, including cycles. Tiny Python does not
provide user-defined classes, generators, package directories,
arbitrary-precision integers, complete Unicode, networking, USB HID/MIDI, or
compatibility with arbitrary CircuitPython drivers.

For every API and the exact compatibility boundary, see the
[full scripting documentation](firmware/main/components/tiny-python-readme.md),
[capability audit](firmware/TINY_PYTHON_AUDIT.md), and
[OpenCalc OS guide](guied.MD#python-programs).
