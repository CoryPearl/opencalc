# Tiny Python Capability Audit

Tiny Python is OpenCalc's compact, source-level scripting runtime. The goal is
to keep its existing syntax and predictable memory behavior while exposing the
parts of embedded Python that are useful on a calculator. It is not a fork of
CircuitPython and does not claim source compatibility with arbitrary
CircuitPython programs or drivers.

The comparison baseline is CircuitPython's official
[standard-library reference](https://docs.circuitpython.org/en/10.0.x/docs/library/)
and [runtime overview](https://docs.circuitpython.org/en/latest/README.html).
CircuitPython itself ships board-dependent subsets, while its broad device
support comes from a large external driver ecosystem.

## Current Language Surface

- Numbers, booleans, strings, `None`, lists, tuples, dictionaries, sets,
  `bytes`, and mutable `bytearray` values
- Assignment, augmented assignment, indexing, slicing, and collection methods
- First-class function values with per-call locals, nested lexical lookup,
  working `global` and `nonlocal`, mutable bounded closures, recursion, return
  values, defaults, keyword arguments, `*args`, and `**kwargs`
- Short-circuit, operand-returning `and`/`or`, `if`/`elif`/`else`, `while`,
  `for`, nested list/dictionary/set comprehensions, `try`/`except`/`else`/
  `finally`, a built-in exception hierarchy, `raise`,
  `break`, `continue`, `pass`, imports, and basic f-strings
- `import` and `from ... import ...` for built-ins and isolated sibling `.py`
  modules, with aliases, bounded depth, and a per-run module cache
- Common Python builtins including `range`, `enumerate`, `reversed`, `zip`,
  `sorted`, `divmod`, and integer base conversion
- A 32 KB source limit, up to 8,192 dynamically allocated tokens per parser,
  96 module-variable slots including preloaded names, 48 local slots per active
  call, 48 functions, bounded recursion, and bounded object/container allocation
- Cancellable forever loops, interruptible input/sleep, tracebacks,
  breakpoints, variable inspection, and profiling
- Mark-and-sweep collection for unreachable containers, including cycles

## Modules

| Module | Implemented surface |
| --- | --- |
| `math` | Common trig/log functions, constants, `factorial`, `gcd`, `lcm`, `prod`, `isclose`; OpenCalc adds `eval` and `cas` |
| `random` | Seeded PRNG, ranges, choices, shuffle, random bits, uniform and normal values |
| `time` | Wall/monotonic clocks, ticks helpers, and cancellable sleeps |
| `statistics` | Mean, median, population/sample variance and standard deviation |
| `board` | Named expansion channels `D0-D11` and `A0-A3` |
| `digitalio` | Procedural calls plus `DigitalInOut` objects with input/output/read/write methods |
| `analogio` | Procedural calls plus `AnalogIn` objects with raw/voltage methods |
| `busio` | Protected register-based I2C calls plus a fixed-bus `I2C` object |
| `graphics`, `keys`, `audio` | Bounded display commands, keypad state, volume, and tones |
| `storage` | Sandboxed read/write/append/size/rename/remove under `/data/user` |
| `sensors` | Trigger waits, captures, direct `L1-L6` logging, digital/analog/I2C access |

## Where OpenCalc Goes Further

- `math.cas()` and `math.eval()` connect scripts to the calculator math stack.
- Sensor captures can be written directly to persistent calculator lists and
  analyzed in Lists, Graph, or Statistics without parsing a separate file.
- The editor includes breakpoints, stepping, variable inspection, traceback
  viewing, profiling, and cooperative cancellation on the device.
- Native services are bounded and serialized through OS-owned drawing,
  storage, audio, and scientific-I/O paths.

## Deliberate Compatibility Boundary

Tiny Python does not implement user-defined classes, lambdas, generators,
general context-manager protocols, async code, package directories, Python
bytecode, arbitrary-precision integers, complex values, complete Unicode, or
generator expressions. Exception handling catches runtime errors; lexer/parser
errors occur before execution and therefore cannot be caught. Custom exception
classes, chained exceptions, and first-class traceback objects remain absent.

Scope behavior is Python-like for normal calls: assignments are local,
`global` targets the script or module table, and `nonlocal` updates a captured
binding. Function values can be returned, aliased, and called later; separate
factory calls retain separate closure snapshots. Closure environments and the
function table remain bounded, so this is not CPython's unbounded cell model.
Keyword-only and positional-only declarations and keyword arguments to
built-in/native calls are not implemented.

`DigitalInOut`, `AnalogIn`, and fixed-bus `I2C` wrappers provide object-shaped
access, `value` properties, and bounded `with` cleanup while preserving channel
validation and I2C protections. They are not the complete CircuitPython object
model or external driver ABI.

The V5 hardware profile also has no script UART, SPI header, networking stack,
USB HID/MIDI API, PWM API, or compatibility with CircuitPython's external
driver bundle. Adding names that imitate those APIs without the underlying
runtime and hardware semantics would make scripts misleading and fragile.

## Recommended Direction

Continue improving the current runtime where features fit its bounded model:
more tests, useful collection/string helpers, calculator-aware modules, and
stable sensor drivers. If running general CircuitPython libraries becomes a
requirement, add a separate MicroPython/CircuitPython-derived runtime instead
of turning Tiny Python into an incomplete reimplementation. The existing Tiny
Python syntax and OpenCalc modules can remain the default calculator scripting
environment.
