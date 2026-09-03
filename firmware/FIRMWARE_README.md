# OpenCalc OS

OpenCalc OS is the open-source ESP-IDF operating environment for the OpenCalc ESP32-S3 graphing calculator. It brings up the ILI9341 LCD, button matrix, USB mass storage and CDC serial, script storage, the 12-app calculator interface, numerical and symbolic math engines, power management, and the optional five-game launcher.

The primary symbolic backend is now the Giac engine from the KhiCAS lineage.
It runs on a serialized 64 KB PSRAM-backed task and supports general symbolic
evaluation, exact arithmetic, simplification, solving, calculus, complex
expressions, matrices, and persistent CAS variables. Eigenmath and OpenCalc's
small native symbolic routines remain fallback paths while Giac completes its
hardware validation pass.

For a one-boot engine check, set `OPENCALC_GIAC_BOOT_SELF_TEST` to `1`.
The serial log then reports exact arithmetic, algebra, solving, calculus,
complex, matrix, persistent-variable, and internal/PSRAM heap checks. Return it
to `0` for normal firmware.

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

Current target: ESP32-S3 with 16 MB flash, 8 MB PSRAM, ILI9341 320x240 LCD, 10x5 diode-isolated keypad matrix, one USB-C data/power connection, LiPo battery support, PWM backlight control, battery ADC, and optional game audio on the new PCB.

The firmware is now split across both ESP32-S3 cores:

- UI, LCD drawing, keypad dispatch, and game drawing run on `OPENCALC_UI_CORE`.
- Serial button simulation and async graph/math jobs run on `OPENCALC_WORKER_CORE`.
- Calculator Enter evaluates inline for predictable key response; numeric solver solving and graph Calc tools use worker jobs so longer math work does not directly block drawing.
- Continuous UI/game pacing is capped by `OPENCALC_TARGET_FPS` in `main/config.h`.
- Power-save mode lowers CPU max frequency and caps brightness using values in `main/config.h`.

See [App Status](APP_STATUS.md) for the current implementation status of every
OpenCalc OS app and game.

Recent local build status:

- The last completed `idf.py build` passed before the Giac backend was imported. The
  Giac-enabled image size and link-time DRAM use have not been measured yet.
- App binary size: `0x12d4b0` bytes (1,234,096 bytes).
- Factory app partition: `0x600000` bytes (6 MB).
- Free app partition space at the last measured `0x12d4b0` build size: `0x4d2b50` bytes, roughly 80%.
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
#define OPENCALC_GIAC_BOOT_SELF_TEST 0
#define OPENCALC_ENABLE_GAME_AUDIO 0
#define OPENCALC_USE_NEW_AUDIO_PCB 0
#define OPENCALC_ENABLE_SERIAL_BUTTON_INPUT 0
#define OPENCALC_FLASH_STORAGE_IMAGE 1
#define OPENCALC_TARGET_FPS 45
#define OPENCALC_POWER_SAVE_CPU_MAX_MHZ 160
#define OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT 35
#define OPENCALC_USE_REAL_PCB 1
```

These toggles cover Wi-Fi, Bluetooth, the Giac/KhiCAS CAS backend and optional
boot smoke test, Doom, game audio, target FPS, testing-vs-PCB mode, storage
image flashing, CPU frequency, USB CDC serial, and power-save brightness caps.

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
  scripts/
    fib.py
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
| PAM8302A shutdown / PDM audio        | none                                                | `13` / `41`           |
| Keypad rows 0-9                      | `1`, `2`, `42`, `4`, `5`, `6`, `48`, `8`, `9`, `16` | same                  |
| Keypad columns 0-4                   | `17`, `18`, `38`, `39`, `40`                        | same                  |

GPIO41 emits PDM rather than analog audio. The new PCB must low-pass filter and AC-couple that signal before the PAM8302A input. GPIO13 drives the amplifier `SD` pin and should have an external pull-down so the amplifier remains shut down during reset and sleep.

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
- `sqrt` inserts `sqrt()`; `2nd` + `sqrt` inserts an editable nth-root box.
- `^2` inserts square; `2nd` + `^2` inserts an editable exponent box.
- `[]/[]` inserts a vertical fraction box; `2nd` + `[]/[]` inserts inline division.
- `Alpha` + `sin/cos/tan` inserts `csc(`/`sec(`/`cot(`.
- `Alpha` + `1/2/3` enters `G/H/I`.
- `PRGM` opens the program menu. `2nd` + `PRGM` jumps straight to the scripts browser.
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
- Table view for Cartesian, parametric, polar, and sequence graph modes, with horizontal paging when a mode has more series than fit on screen.
- Parametric, polar, and sequence graphing modes with mode-aware trace stepping and Graph Calc value/derivative/integral analysis. Cartesian intersections are refined; non-Cartesian intersections use sampled plotted-point matching.
- Calculator math parser with trig, powers, roots, vertical fractions, compact exponent/root rendering, probability basics, derivatives, definite integrals, and a growing CAS layer.
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
- Numeric solver with `E1`, `E2`, guess-based real/complex solving, decimal-to-fraction real result display, degree-10 polynomial roots with a dedicated root-detail screen, complex output, and augmented-matrix linear system solving.
- Lists `L1` through `L6`, up to 999 values each, with edit/sort/sum/min/max support.
- Statistics app with list editing, sort/clear, 1-var and 2-var stats, regression tools, core tests/intervals, ANOVA, common probability distributions, result screens, and basic scatter/xy-line/histogram plots.
- Matrix app with `A` through `J` storage up to 99x99, including set/show/determinant/inverse/RREF/transpose/identity, augment-with-next, list conversion, and typed row operations.
- Finance TVM, NPV, and IRR helpers.
- Lines and conics templates that add solved graph curves into free `Y=` slots.
- Inequality graphing presets with shaded regions, overlap shading, vertical `x>=0`, and dotted/solid boundaries for strict/inclusive inequalities.

Not complete yet, so not presented as finished:

- Split-screen graph/table layout.
- Image graph backgrounds.
- Full desktop Xcas parity and complete UI coverage of every Giac command. The
  embedded source omits desktop GUI, plotting, and some platform-dependent
  facilities; long results are currently clipped to the calculator result
  field with an ellipsis.
- Dedicated detail screen for large linear-system solutions.
- Dedicated matrix cell editor and large 99x99 hardware stress validation.
- Full TI-level hypothesis tests, confidence intervals, distributions, and all regression models.

## Python Scripts

Python-style scripts live in `storage_image/scripts/` before flashing and `/data/scripts/` at runtime. `PRGM` opens a program menu with Run, Edit, New, and Delete entries. Run/Edit/Delete open the scripts browser. New creates a starter `programNN.py` file in `/data/scripts`. `2nd` + `PRGM` jumps directly to the scripts browser. When a script starts, the LCD switches to a Python output/input console screen. Script `print(...)` output appears there.

When a script calls `input()`, the console opens a keypad input line. Use number keys, operators, `Alpha` letters, `CLEAR` to delete, `2nd` + `CLEAR` to clear the input, and `Enter` to submit. `DEL/Back` cancels the input. When the script finishes, the screen prompts you to press Enter or Back to return to the script list.

Example flashed script:

```text
storage_image/scripts/fib.py
```

If monitor output says the flashed app checksum does not match the built app, rebuild and flash again before debugging script behavior.

Tiny Python is an embedded Python-like interpreter, not CPython. Before changing
the scripting runtime, run the host regression test:

```sh
cc -std=c11 -Wall -Wextra -Werror -I main/components tests/tiny_python_regression.c main/components/tiny-python.c -o /tmp/opencalc_tiny_python_regression
/tmp/opencalc_tiny_python_regression
```

The regression pass covers arithmetic, loops, recursion, collections,
`input()`, `int(input(...))`, `py_run_file()`, repeated interpreter lifecycle,
and recovery after a syntax error.

Run the local and general symbolic calculator regressions separately:

```sh
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -DOPENCALC_EIGENMATH_EMBEDDED=1 -I main/components tests/cas_regression.c main/components/opencalc_cas.c main/components/opencalc_eigenmath.c main/components/opencalc_math.c main/components/eigenmath/eigenmath.c -lm -o /tmp/opencalc_cas_regression
/tmp/opencalc_cas_regression
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -DOPENCALC_EIGENMATH_EMBEDDED=1 -I main/components tests/eigenmath_regression.c main/components/opencalc_eigenmath.c main/components/eigenmath/eigenmath.c -lm -o /tmp/opencalc_eigenmath_regression
/tmp/opencalc_eigenmath_regression
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
