# OpenCalc OS

OpenCalc OS is the open-source ESP-IDF operating environment for the OpenCalc ESP32-S3 graphing calculator. It brings up the ILI9341 LCD, button matrix, USB mass storage and CDC serial, script storage, the 12-app calculator interface, numerical and symbolic math engines, power management, and the optional five-game launcher.

Large worksheet state is stored in a versioned, CRC-checked FAT snapshot with
atomic backup recovery. Calculator history and active input, graph equations
and windows, lists, populated matrix cells, finance values, and conic worksheets
survive reset and deep sleep without consuming the small NVS partition.

The primary symbolic backend is now the Giac engine from the KhiCAS lineage.
It runs on a serialized 64 KB PSRAM-backed task and supports general symbolic
evaluation, exact arithmetic, simplification, solving, calculus, complex
expressions, matrices, and persistent CAS variables. Eigenmath and OpenCalc's
small native symbolic routines remain fallback paths while Giac completes its
hardware validation pass.

Automated boot tests are disabled by default. For a one-boot engine check, set
both `OPENCALC_ENABLE_AUTOMATED_TESTS` and `OPENCALC_GIAC_BOOT_SELF_TEST` to `1`.
The serial log then reports exact arithmetic, algebra, solving, calculus,
complex, matrix, persistent-variable, and internal/PSRAM heap checks. Return it
to `0` for normal firmware. Keeping `OPENCALC_ENABLE_AUTOMATED_TESTS` at `0`
prevents every boot-time automated suite from running regardless of its
individual switch.

```
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
C++                             58           6138           9185         122229
C                              275          17502          12436          81433
C/C++ Header                   252           7141           7654          44088
Markdown                         4            176              0            555
JSON                           266              0              0            320
CMake                            3             11              0            213
YAML                             3             19             69            205
Python                           1              1             22             15
-------------------------------------------------------------------------------
SUM:                           862          30988          29366         249058
-------------------------------------------------------------------------------
```

## Current Firmware Status

Current target: ESP32-S3 with 16 MB flash, 8 MB PSRAM, ILI9341 320x240 LCD, 10x5 diode-isolated keypad matrix, one USB-C data/power connection, LiPo battery support, PWM backlight control, battery ADC, optional game audio, and optional ADS1115/MCP23017 scientific I/O.

The firmware is now split across both ESP32-S3 cores:

- UI, LCD drawing, keypad dispatch, and game drawing run on `OPENCALC_UI_CORE`.
- Serial button simulation and async graph/math jobs run on `OPENCALC_WORKER_CORE`.
- Calculator, numeric solver, symbolic solver, and Graph Calc jobs run through a
  24 KB PSRAM-backed worker. Calculator evaluation is fully asynchronous; `Back`
  cancels the active UI request, stale results are ignored, and Giac waits are
  bounded by `OPENCALC_CAS_TIMEOUT_MS`.
- Continuous UI/game pacing is capped by `OPENCALC_TARGET_FPS` in `main/config.h`.
- Power-save mode lowers CPU max frequency and caps brightness using values in `main/config.h`.

See [App Status](APP_STATUS.md) for the current implementation status of every
OpenCalc OS app and game.

The UI is being decomposed into bounded components: `opencalc_calc.c` owns
calculator evaluation and history, `opencalc_ui_work.c` owns PSRAM-backed async
job/result lifetimes, and `opencalc_ui_canvas.c` owns the framebuffer and shared
raster primitives. Existing domain modules own statistics, conics,
inequalities, references, math, persistence, and each game; `opencalc_ui.c`
remains the coordinator for page layouts and keypad routing.

Recent local build status (September 4, 2026):

- The Giac-enabled ESP-IDF target build completes successfully.
- App binary size: `0x4f68f0` bytes (5,204,208 bytes).
- Factory app partition: `0x600000` bytes (6 MB).
- Free app partition space: `0x109710` bytes (1,087,248 bytes, 17%).
- Storage partition: `0x800000` bytes, generated from `storage_image/`.

## Build and Flash

Clone the repo, install/source ESP-IDF, then build or flash from this directory:

```bash
git clone https://github.com/CoryPearl/opencalc.git
cd opencalc/firmware
idf.py build
idf.py flash
```

Use `idf.py flash`, not `idf.py app-flash`, because the normal flash target also writes the generated storage image to the `storage` partition.

To edit OpenCalc OS, change files under `main/`, rebuild with `idf.py build`, then flash with `idf.py flash`. Use `idf.py monitor` to view logs after flashing.

After flashing, press Reset once more if the board stays in download mode or the monitor does not reconnect cleanly.

Common commands:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

Local feature toggles live in:

```text
main/config.h
```

Important current toggles:

```c
#define OPENCALC_ENABLE_BLUETOOTH 0
#define OPENCALC_ENABLE_WIFI 0
#define OPENCALC_ENABLE_DOOM 1
#define OPENCALC_ENABLE_GIAC_CAS 1
#define OPENCALC_ENABLE_AUTOMATED_TESTS 0
#define OPENCALC_GIAC_BOOT_SELF_TEST 0
#define OPENCALC_MATRIX_BOOT_STRESS_TEST 0
#define OPENCALC_DEVICE_SELF_TEST 0
#define OPENCALC_ENABLE_GAME_AUDIO 0
#define OPENCALC_USE_NEW_AUDIO_PCB 0
#define OPENCALC_ENABLE_SCIENTIFIC_IO 0
#define OPENCALC_ENABLE_SERIAL_BUTTON_INPUT 0
#define OPENCALC_FLASH_STORAGE_IMAGE 1
#define OPENCALC_TARGET_FPS 45
#define OPENCALC_POWER_SAVE_CPU_MAX_MHZ 160
#define OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT 35
#define OPENCALC_USE_REAL_PCB 1
```

These toggles cover Wi-Fi, Bluetooth, the Giac/KhiCAS CAS backend and optional
boot smoke test, Doom, game audio, target FPS, testing-vs-PCB mode, storage
  image flashing, CPU frequency, USB CDC serial, power-save brightness caps,
  scientific sensor I/O, and the script statement/depth/time limits.

The storage image always comes from `storage_image/`. To test without Doom,
remove or move `storage_image/doom1.wad` before building/flashing.

Partition sizes are also set in `main/config.h`:

```c
#define OPENCALC_FACTORY_APP_PARTITION_SIZE 0x600000
#define OPENCALC_STORAGE_PARTITION_SIZE 0x800000
```

The top-level `CMakeLists.txt` reads those values and regenerates `partitions.csv` before ESP-IDF builds the partition table. The 6 MB app begins at `0x10000`; the 8 MB storage partition begins at `0x610000` and ends at `0xe10000`, leaving `0x1f0000` bytes unallocated on the 16 MB flash device.

## USB Storage

The board exposes the FAT storage partition over the USB port. With the default config it is 8 MB. Files that should be flashed automatically live in:

```text
storage_image/
  doom1.wad
  mario.nes
  graph.bmp        optional 320x240 graph background
  scripts/
    fib.py
    logger.py
```

On the current single USB-C hardware, TinyUSB exposes CDC serial and MSC storage on the same connector. On macOS, monitor the CDC port that appears as something like `/dev/cu.usbmodemopencalc1`. Older ESP32-S3 dev boards may still expose separate COM and native-USB ports.

## Current PCB Pin Map

Set `OPENCALC_USE_NEW_AUDIO_PCB` to `0` for the existing board or `1` for the new board that removes LCD MISO and charger status. Game volume is controlled from Settings -> Audio in 5% steps and is saved in NVS; `OPENCALC_AUDIO_VOLUME_PERCENT` is only the default/factory value. The two profiles use these assignments:

| Function                             | Old PCB                                             | New audio PCB         |
| ------------------------------------ | --------------------------------------------------- | --------------------- |
| LCD SCLK / MOSI / MISO               | `12` / `11` / `13`                                  | `12` / `11` / unwired |
| LCD CS / DC / RST                    | `10` / `14` / `15`                                  | `10` / `14` / `15`    |
| LCD backlight PWM/load-switch enable | `47`                                                | `47`                  |
| USB D- / D+                          | `19` / `20`                                         | `19` / `20`           |
| Battery ADC divider                  | `7`                                                 | `7`                   |
| Charger status input                 | `41`                                                | removed               |
| PAM8302A shutdown / PDM audio        | none                                                | MCP23017 `GPA0` / `41` with scientific I/O; otherwise `13` / `41` |
| Sensor I2C SDA / SCL                 | none                                                | `13` / `21` with scientific I/O |
| Power status LED                     | `21` when enabled                                   | MCP23017 `GPA1` with scientific I/O |
| Keypad rows 0-9                      | `1`, `2`, `42`, `4`, `5`, `6`, `48`, `8`, `9`, `16` | same                  |
| Keypad columns 0-4                   | `17`, `18`, `38`, `39`, `40`                        | same                  |

GPIO41 emits PDM rather than analog audio. The new PCB must low-pass filter and
AC-couple that signal before the PAM8302A input. With scientific I/O enabled,
GPIO13 and GPIO21 become SDA/SCL; MCP23017 GPA0 drives amplifier `SD` and must
have an external pull-down so the amplifier stays shut down during reset.

Touch input is intentionally out of scope for the current PCB revision.

Expose test pads or a small header for `GND`, `3V3`, `5V/VBUS`, `VBAT`, `RESET/EN`, `GPIO0/BOOT`, `UART0 TX/RX`, and `USB D+/D-`. Any spare GPIO exposed for user hardware should not share LCD, keypad, battery ADC, charger status, native USB, or console UART nets.

Power off is software-only in the current firmware. `2nd` + `On` turns off the display/backlight and enters the configured low-power sleep path; it does not drive a `POWER_HOLD` latch or cut the battery rail.

## Controls

OpenCalc OS supports both the physical 10×5 button matrix and serial button simulation. When serial input is enabled, type a button number from `1` to `50` and press Enter. Serial button requests are queued and handled by the main UI task so scripts and drawing do not run from the serial input task. The serial numbering matches `hardware/graphic-designs/Design-V2-keymap.png`.

Important key behavior:

- `DEL` is Back.
- `CLEAR` deletes the current character/token.
- `2nd` + `CLEAR` clears the current input field.
- `ON (Home)` returns to the home screen, and exits the active game when a game is active.
- `DEL/Back` returns to the previous screen or previously active app; it does
  not force a launcher jump when a prior app exists.
- `sqrt` inserts `sqrt()`; `2nd` + `sqrt` inserts an editable nth-root box.
- `^2` inserts square; `2nd` + `^2` inserts an editable exponent box.
- `[]/[]` inserts a vertical fraction box; `2nd` + `[]/[]` inserts inline division.
- `Alpha` + `sin/cos/tan` inserts `csc(`/`sec(`/`cot(`.
- `Alpha` + `1/2/3` enters `G/H/I`.
- `Alpha` + `Zoom` opens the Reference Center with the periodic table and
  math, physics, and engineering formula databases.
- `Trace` cycles enabled graph series; `Alpha` + `Graph` opens linked symbolic
  analysis for the currently selected series.
- `PRGM` opens the program menu. `2nd` + `PRGM` jumps straight to the scripts browser.
- `2nd` + `Y=` opens statistical plots, `2nd` + `Window` opens table setup, and
  `2nd` + `Graph` opens the current graph-mode table.
- `2nd` + `Up` increases backlight brightness; `2nd` + `Down` decreases it.
- In menus, number keys select the matching menu item.

## Calculator Feature Coverage

OpenCalc OS exposes only features that have a real implementation path. TI-style features not listed as working here should be treated as planned, not complete.

Working now:

- 12-app launcher with centered app-specific icons, keyboard navigation, a status header, and battery state.
- 10 rectangular `Y=` graph slots with color graphing.
- 15 graph colors available internally for graph/app drawing.
- Graph trace with zeros, y-intercepts, local min/max, and intersection markers.
- Fast jump to the nearest intersection when two enabled functions intersect in the current graph window.
- Graph window controls for x/y max and x/y tick spacing.
- Table view for Cartesian, parametric, polar, and sequence graph modes, with
  configurable start, step, visible-row count, decimal precision, and horizontal
  paging when a mode has more series than fit on screen.
- Parametric, polar, and sequence graphing modes with mode-aware trace stepping and Graph Calc value/derivative/integral analysis. Cartesian intersections are refined; non-Cartesian intersections use sampled plotted-point matching.
- Linked symbolic analysis uses the same active equation as Graph and Table. Press `Trace` to cycle enabled series, then `Alpha` + `Graph` to view exact CAS derivative, integral, roots, and asymptotic/end behavior. In that view, `Left`/`Right` changes series, `Up`/`Down` changes the analysis point, `Y=` toggles a Cartesian tangent, `Window` toggles integral shading from zero to the selected x-value, and `Enter` returns to the graph.
- Graph Format (`2nd` + `Zoom`) provides full/split graph-table views, per-series line/thick/dotted/point styles, grid control, and optional `/data/graph.bmp` backgrounds (uncompressed RGB, `320x240`, 24-bit or 32-bit).
- Advanced Graph Calc semantics follow each mode: parametric and polar derivatives report `dy/dx`, parametric integration computes `integral y dx`, polar integration computes enclosed area, and sequence calculus uses forward differences and discrete sums.
- Calculator math parser with trig, powers, roots, vertical fractions, compact exponent/root rendering, probability basics, derivatives, definite integrals, and a growing CAS layer.
- Calculator expressions accept up to 768 bytes and results up to 1024 bytes.
  Long answers open in a dedicated scrollable result view instead of being
  clipped to the history row.
- Unit-aware scalar arithmetic with implicit number-unit multiplication,
  compound units, integer powers, compatible-unit addition/subtraction,
  dimensional mismatch errors, and automatic SI conversion. Both `5 m / 2 s`
  and keypad-friendly `5m/2s` evaluate to `2.5 m/s`.
- Functional `STO`, `VARS`, and `GET` workflows: store into `A`-`Z` or custom
  names, browse categorized calculator state with type/value previews, insert
  references or current values, and rename/delete user variables. User
  variables are persisted in NVS and substituted safely into CAS expressions.
- Embedded Giac/KhiCAS-derived symbolic engine with exact arithmetic, general
  simplification, equation solving, symbolic calculus, complex expressions,
  matrices, and persistent variables. It runs serially on a dedicated 64 KB
  PSRAM-backed task. OpenCalc's native polynomial routines and PSRAM-aware
  Eigenmath port remain fallback paths.
- CAS dispatch recognizes symbolic and special-function calls inside composed
  expressions, not only at the outermost call. OpenCalc aliases `deriv` to
  `diff`, `int`/`defint` to `integrate`, the calculator's base-10 `log` to
  Giac's `log10`, and several matrix command names at the Giac boundary. The
  existing numeric implementations retain control of `nDeriv`, `fnInt`,
  decimal/fraction conversion, and random-number functions.
- Calculus syntax supports `nDeriv(expr,value)`, `nDeriv(expr,var,value)`, `fnInt(expr,a,b)`, `fnInt(expr,var,a,b)`, plus lightweight symbolic `deriv(expr[,var])` and `int(expr[,var])` helpers for common forms. Numeric calculus uses symbolic evaluation first when a supported pattern exists, then falls back to numerical approximation.
- Solver dashboard with exact Giac equation/polynomial workflows, advanced
  symbolic system commands, nearby real/complex numerical solving, bounded
  multi-root scans, adjustable precision, persistent saved problems, and
  detailed polynomial/root and up-to-99-variable linear-system result screens.
- Lists `L1` through `L6`, up to 999 values each, with a slot-based workspace,
  live preview, editor, and sum/min/max/clear tools.
- Statistics app with list editing, descriptive statistics, regressions, field-based setup screens for tests/intervals/distributions, one- and two-proportion inference, Welch two-sample inference, ANOVA, validated distribution domains, and scatter/XY-line/histogram/box/normal-probability plots.
- Matrix app with `A` through `J` storage up to 99x99, direct left/right slot
  selection, cell and dimension editors, addition/subtraction/multiplication,
  scalar multiplication, integer powers, determinant, inverse, REF/RREF,
  transpose, identity/zero creation, augmentation, row/column/submatrix
  extraction, list conversion, and typed row operations.
- Finance dashboard with direct TVM editing, `N`/`I%`/`PV`/`PMT`/`FV` solving,
  begin/end timing, `P/Y` and `C/Y`, a list-backed cash-flow ledger, and
  dedicated NPV/IRR results.
- Reference Center with a spatially navigable 118-element periodic table,
  element property cards, and browsable math, physics, and engineering formulas.
- Dedicated Conics workspace with circle/parabola/ellipse/hyperbola/general
  coefficient editors, rotated-conic classification, geometry and tangent
  analysis, point/focus-based construction including five-point general-conic
  fitting, four-conic overlays, Calculator handoff, and Graph export with
  automatic windows.
- Dedicated Inequalities workspace with exact Giac and numerical one-variable
  solving, interval/number-line/sign-chart views, real or integer domains,
  persistent six-relation systems, AND/OR region shading, explicit/vertical/
  implicit boundaries, Graph export, list endpoint/intersection export, and
  bounded linear-objective evaluation over detected feasible vertices.

Not complete yet, so not presented as finished:

- Non-Cartesian intersections and points of interest use plotted-sample matching rather than analytic refinement, so very close or tangent points can be missed.
- Full desktop Xcas parity and complete UI coverage of every Giac command. The
  embedded source omits desktop GUI, plotting, and some platform-dependent
  facilities. Giac itself currently formats at most 1024 bytes per result.
- Large 99x99 hardware stress validation for the heaviest matrix operations.
- Chi-square contingency-table editing, paired-data inference, regression inference/diagnostics, and broader physical-device validation against AP Statistics edge cases.

## Python Scripts

Python-style scripts live in `storage_image/scripts/` before flashing and `/data/scripts/` at runtime. `PRGM` opens a program menu with Run, Debug, Edit, New, and Delete entries. Run/Debug/Edit/Delete open the scripts browser. New creates a blank `programNN.py` file in `/data/scripts`. `2nd` + `PRGM` jumps directly to the scripts browser. When a script starts, the LCD switches to a Python output/input console screen. Script `print(...)` output appears there.

When a script calls `input()`, the console opens a keypad input line. Use number keys, operators, `Alpha` letters, `CLEAR` to delete, `2nd` + `CLEAR` to clear the input, and `Enter` to submit. `DEL/Back` cancels the input. When the script finishes, the screen prompts you to press Enter or Back to return to the script list.

Script execution is asynchronous. A dedicated 32 KB internal-RAM worker is
reserved early in boot, before USB and display allocations fragment DRAM. It runs
`py_run_file(...)`; parser pools, program buffers, and dynamic containers remain
in PSRAM. The internal task stack is required because scripts can access FATFS
while flash-cache operations are in progress. The UI task remains responsive and is the only task allowed
to draw the LCD or dispatch keypad events. Script output is copied through a
mutex-protected console buffer, and `input()` uses a task notification to wait
for text submitted by the UI. This architecture is host-regression tested and
build verified, but the latest worker handoff still needs a repeated physical
device run/input/cancel/exit soak test.

In the editor, move to a source line and press `Trace` to toggle its breakpoint.
Choose **Debug script** to pause before the first statement. While paused,
`Enter` executes the next statement, `Trace` continues to the next breakpoint,
and `Graph` cycles Console, Variables, Traceback, and Profile views. `Back` or
`Clear` requests a controlled stop. The same diagnostic views remain available
after a run completes.

Every run has configurable statement, call-depth, and active-time limits in
`main/config.h`. The worker yields regularly so the FreeRTOS watchdog and UI can
run, and loops, recursion overflow, user cancellation, parser errors, and module
errors unwind through the normal interpreter cleanup path. Debugger pauses do
not consume the active-time budget. This is cooperative containment for Tiny
Python bytecode/source execution; it is not process isolation from defects in a
native C driver.

The following modules are preloaded, so scripts do not use `import`:

- `graphics.clear`, `pixel`, `line`, `rect`, and `text` queue bounded drawing
  commands that are replayed by the UI task.
- `keys.down(button)` reads calculator button numbers `1` through `50`.
- `storage.exists`, `read`, `write`, and `remove` operate only on simple file
  names inside `/data/user/`; absolute paths and parent traversal are rejected.
- `audio.available`, `volume`, and `tone` use the configured OpenCalc audio
  backend.
- `sensors.available`, `mode`, `digital_read`, and `digital_write` control
  header channels `D0-D11`. `pinMode`, `digitalRead`, and `digitalWrite` are
  aliases for the same channels. Modes are `0` input, `1` output, and `2`
  input with the MCP23017 weak pull-up. All twelve exposed channels are
  bidirectional; GPA7 and GPB7 are deliberately unused by this board design.
- `sensors.analog_read(channel)` returns volts and `analog_raw(channel)` returns
  the signed ADS1115 code for `A0-A3`. `analog_diff(positive, negative)` supports
  `A0-A1`, `A0-A3`, `A1-A3`, and `A2-A3`. `rate(8..860)` selects the nearest
  supported rate at or below the requested value.
- `sensors.list_clear(1..6)`, `list_append(list, value)`, and `list_count(list)`
  write measurements directly into the Lists/Statistics worksheet. The normal
  CRC-checked worksheet save then persists those samples.
- `sensors.capture(channel, count, rate, list)` replaces one list with up to 999
  consecutive voltage samples. `wait_analog(channel, threshold, direction,
  timeout_ms)` and `wait_digital(channel, state, timeout_ms)` provide bounded
  trigger waits; analog direction is `1` for at/above and `-1` for at/below.
- ADS1115 rate selection controls converter timing. Actual wall-clock capture
  rate is slightly lower because each sample also requires I2C and task time.
- `sensors.i2c_present(address)`, `i2c_read8`, `i2c_read16`, and `i2c_write8`
  support register-based external I2C sensors. Writes to the onboard `0x20`
  and `0x48` devices are blocked so scripts cannot corrupt system I/O state.
- `sensors.delay(ms)` provides paced acquisition. Existing `graphics` calls can
  plot each sample as it arrives; see `storage_image/scripts/logger.py`.
- `math.eval` evaluates numeric calculator expressions, `math.cas` evaluates a
  symbolic expression, and common scalar functions include `sin`, `cos`, `tan`,
  `sqrt`, `log`, `log10`, `exp`, `floor`, and `ceil`.

Example flashed script:

```text
storage_image/scripts/fib.py
storage_image/scripts/logger.py
```

Scientific I/O is disabled by default so current boards keep their existing
GPIO13/GPIO21 behavior. For the revised hardware use:

```c
#define OPENCALC_USE_REAL_PCB 1
#define OPENCALC_USE_NEW_AUDIO_PCB 1
#define OPENCALC_ENABLE_SCIENTIFIC_IO 1
```

The script-visible channel map is:

| Script channel | Hardware signal | Capability |
| -------------- | --------------- | ---------- |
| `D0-D4` | MCP23017 `GPA2-GPA6` | Digital input/output/pull-up |
| `D5-D11` | MCP23017 `GPB0-GPB6` | Digital input/output/pull-up |
| `A0-A3` | ADS1115 `AIN0-AIN3` | 16-bit single-ended analog input |
| `SDA`, `SCL` | ESP GPIO13, GPIO21 | Shared 3.3 V I2C bus |

The complete physical connector numbering, electrical limits, event-pin
behavior, and wiring examples are in the
[Rear 30-Pin Header guide](../guied.MD#rear-30-pin-header).

The `sensors` module is preloaded; do not write `import sensors`. A minimal
hardware check is:

```python
print("hub", sensors.available())
sensors.mode(0, OUTPUT)
sensors.digital_write(0, 1)
sensors.delay(250)
sensors.digital_write(0, 0)
print("A0 volts", sensors.analog_read(0))
```

To acquire data for the calculator apps:

```python
sensors.rate(128)
sensors.list_clear(1)
if sensors.wait_digital(1, 1, 10000):
    count = sensors.capture(0, 250, 128, 1)
    print("saved", count, "samples to L1")
```

Open Lists or Statistics after the script exits to inspect `L1`. `capture()`
replaces the selected list; use `list_append()` in a custom loop when samples
need conversion, filtering, timestamps in another list, or live drawing.
`i2c_read16()` combines the first received byte as the most-significant byte;
swap bytes in the script for sensors that transmit little-endian registers.

There is intentionally no sensor UART in this PCB profile. GPIO35-GPIO37 are
used by octal memory on the N16R8 module and GPIO46 is an input-only strapping
pin. Add an I2C-to-UART bridge if a dedicated external UART is required.

If monitor output says the flashed app checksum does not match the built app, rebuild and flash again before debugging script behavior.

Tiny Python is an embedded Python-like interpreter, not CPython. Before changing
the scripting runtime, run the host regression test:

```sh
cc -std=c11 -Wall -Wextra -Werror -I main/components tests/tiny_python_regression.c main/components/tiny-python.c -lm -o /tmp/opencalc_tiny_python_regression
/tmp/opencalc_tiny_python_regression
```

The regression pass covers arithmetic, range and container iteration,
membership, collection methods, sequence operations, Python-style numeric
semantics, common builtins, recursion, `input()`, `py_run_file()`, repeated
interpreter lifecycle, repeated same-runtime execution, syntax-error recovery,
debug/profile callbacks, native module dispatch, bounded execution, and reuse
after a forced limit error.

Run the local and general symbolic calculator regressions separately:

```sh
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -DOPENCALC_EIGENMATH_EMBEDDED=1 -I main/components tests/cas_regression.c main/components/opencalc_cas.c main/components/opencalc_eigenmath.c main/components/opencalc_math.c main/components/eigenmath/eigenmath.c -lm -o /tmp/opencalc_cas_regression
/tmp/opencalc_cas_regression
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -DOPENCALC_EIGENMATH_EMBEDDED=1 -I main/components tests/eigenmath_regression.c main/components/opencalc_eigenmath.c main/components/eigenmath/eigenmath.c -lm -o /tmp/opencalc_eigenmath_regression
/tmp/opencalc_eigenmath_regression
```

Run the graph-mode evaluator and coordinate regression with:

```sh
cc -std=c11 -Wall -Wextra -Werror -I main/components tests/graph_modes_regression.c main/components/opencalc_math.c -lm -o /tmp/opencalc_graph_modes_regression
/tmp/opencalc_graph_modes_regression
```

Run the probability, interval, tail, and edge-case regression with:

```sh
cc -std=c11 -Wall -Wextra -Werror -I main/components tests/stats_regression.c main/components/opencalc_stats.c -lm -o /tmp/opencalc_stats_regression
/tmp/opencalc_stats_regression
```

Run the conic geometry, classification, construction, and graph-expression
regression with:

```sh
cc -std=c11 -Wall -Wextra -Werror -I main/components tests/conics_regression.c main/components/opencalc_conics.c -lm -o /tmp/opencalc_conics_regression
/tmp/opencalc_conics_regression
```

Run the inequality parser, interval, compound-relation, domain-break, and
implicit x/y evaluator regression with:

```sh
cc -std=c11 -Wall -Wextra -Werror -I main/components tests/inequality_regression.c main/components/opencalc_inequality.c main/components/opencalc_math.c -lm -o /tmp/opencalc_inequality_regression
/tmp/opencalc_inequality_regression
```

Variable registry/parser regression:

```sh
cc -std=c11 -Wall -Wextra -Werror -I main/components tests/variables_regression.c main/components/opencalc_math.c -lm -o /tmp/opencalc_variables_regression
/tmp/opencalc_variables_regression
```

Unit arithmetic and dimensional-analysis regression:

```sh
cc -std=c11 -Wall -Wextra -Werror -I main/components tests/units_regression.c main/components/opencalc_units.c -lm -o /tmp/opencalc_units_regression
/tmp/opencalc_units_regression
```

Eigenmath is included under its BSD-2-Clause license in `main/components/eigenmath/`.

OpenCalc-authored firmware is licensed under GPL-3.0-or-later. Bundled third-party code retains its original copyright and license terms; see the repository's [license](../LICENSE) and [third-party notices](../THIRD_PARTY_NOTICES.md).

## Games

Press `Alpha` then `2nd` to open the game menu. The current menu includes Tetris, Doom, Snake, Breakout, and Mario. High scores are saved in on-chip NVS, so they survive power off and do not depend on USB storage. On the new audio PCB, Tetris, Snake, and Breakout use synthesized effects, Doom uses its game sound events, and Mario runs its NES APU channels. The amplifier is shut down outside games.

Doom is optional and uses the shareware IWAD at:

```text
storage_image/doom1.wad
```

Doom controls:

| Calculator button        | Doom action                    |
| ------------------------ | ------------------------------ |
| Up / Down / Left / Right | Move                           |
| `Y=`                     | Fire                           |
| `Window`                 | Cycle weapons                  |
| `Zoom`                   | Use / open doors               |
| `Trace`                  | Open / close automap           |
| `1`-`7`                  | Select weapon slot             |
| Hold `Mode`              | Run / sprint                   |
| Hold `Stat` + Left/Right | Strafe                         |
| `Enter`                  | Menu/select                    |
| `Back`                   | Escape/back                    |
| `On (Home)`              | Quit Doom and return game menu |

Mario currently checks for the ROM at:

```text
storage_image/mario.nes
```

Mario controls are mapped to match Doom-style play:

| Calculator button        | Mario action                    |
| ------------------------ | ------------------------------- |
| Up / Down / Left / Right | Move                            |
| `Y=`                     | B / action                      |
| `Zoom`                   | A / jump                        |
| `Enter`                  | Start                           |
| `Back`                   | Select/back                     |
| `On (Home)`              | Quit Mario and return game menu |

Mario loads `storage_image/mario.nes` through the real NES path: iNES cartridge loader, mapper 0 cartridge support, 6502 CPU, NES bus, controller state, PPU 2C02 framebuffer, then the ILI9341 display driver. On the new audio PCB, Mario routes NES APU audio through the shared OpenCalc game-audio path. For now, use a mapper 0 `.nes` ROM such as the standard Super Mario Bros cartridge format.

Tetris controls:

| Calculator button | Tetris action               |
| ----------------- | --------------------------- |
| Left / Right      | Move piece                  |
| Down              | Soft drop                   |
| `Y=`              | Hard drop                   |
| `Window`          | Hold piece                  |
| Up                | Rotate                      |
| `Back`            | Pause/back                  |
| `On (Home)`       | Quit Tetris and return menu |

The production 10x5 keypad expects one series diode per key. The firmware scans by driving one column low at a time and reading rows with pull-ups. The diode direction must allow a pressed key to pull its row low only through the selected low column. This enables simultaneous movement, sprint, strafe, fire, and use inputs without matrix ghosting.

The battery icon reads the VBAT divider on GPIO7. The default firmware values match a 1M/1M divider with a 100nF capacitor on the ADC node, then map the measured Li-ion voltage through a discharge curve instead of a straight line. If the ADC reading is missing or outside the valid range, the UI crosses out an empty battery icon instead of showing a fake level.

## Notes

The storage partition is generated from `main/config.h` into `partitions.csv`. If storage contents look wrong after changing the partition or image, rebuild and flash with `idf.py flash` so both the partition table and `build/storage.bin` are written.
