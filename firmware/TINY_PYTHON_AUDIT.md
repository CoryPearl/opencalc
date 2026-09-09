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

- Numbers, booleans, strings, `None`, lists, tuples, and dictionaries
- Assignment, augmented assignment, indexing, slicing, and collection methods
- Functions, recursion, return values, `if`/`elif`/`else`, `while`, and `for`
- `break`, `continue`, `pass`, imports with aliases, and basic f-strings
- Common Python builtins including `range`, `enumerate`, `reversed`, `zip`,
  `sorted`, `divmod`, and integer base conversion
- An 8 KB source limit, 384 tokens per compiled script, 48 total variable slots
  including preloaded names, 16 functions, bounded recursion, and bounded
  object/container allocation
- Cancellable forever loops, interruptible input/sleep, tracebacks,
  breakpoints, variable inspection, and profiling

## Modules

| Module | Implemented surface |
| --- | --- |
| `math` | Common trig/log functions, constants, `factorial`, `gcd`, `lcm`, `prod`, `isclose`; OpenCalc adds `eval` and `cas` |
| `random` | Seeded PRNG, ranges, choices, shuffle, random bits, uniform and normal values |
| `time` | Wall/monotonic clocks, ticks helpers, and cancellable sleeps |
| `statistics` | Mean, median, population/sample variance and standard deviation |
| `board` | Named expansion channels `D0-D11` and `A0-A3` |
| `digitalio` | Procedural mode/read/write access and constants |
| `analogio` | Procedural voltage/raw/differential reads and sample-rate control |
| `busio` | Protected register-based I2C probing, reads, and writes |
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

Tiny Python does not implement exceptions, classes, lambdas, comprehensions,
generators, context managers, async code, user-package discovery, Python
bytecode, arbitrary-precision integers, complete Unicode, `bytes`, `bytearray`,
`set`, or a tracing garbage collector. Its `board`, `digitalio`, `analogio`, and
`busio` modules are procedural OpenCalc APIs, not CircuitPython's object model.

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
