# OpenCalc OS

OpenCalc OS is the open-source ESP-IDF operating environment for the OpenCalc ESP32-S3 graphing calculator. It brings up the ILI9341 LCD, touch controller, button matrix, USB mass storage, script storage, calculator apps, math and graphing engines, serial button simulation, power management, and optional Doom support.

## Current Firmware Status

Current target: ESP32-S3 with 16 MB flash, 8 MB PSRAM, ILI9341 320x240 LCD, 10x5 diode-isolated keypad matrix, one USB-C data/power connection, LiPo battery support, PWM backlight control, battery ADC, and charger-status input.

The firmware is now split across both ESP32-S3 cores:

- UI, LCD drawing, keypad dispatch, and game drawing run on `OPENCALC_UI_CORE`.
- Serial button simulation and async graph/math jobs run on `OPENCALC_WORKER_CORE`.
- Calculator evaluation, numeric solver solving, and graph Calc tools are queued as worker jobs so long math work does not directly block or mutate the UI state.
- Continuous UI/game pacing is capped by `OPENCALC_TARGET_FPS` in `main/config.h`.
- Power-save mode lowers CPU max frequency and caps brightness using values in `main/config.h`.

Latest verified build status:

- `idf.py build` passes.
- App binary size: about `0xdbb10`.
- Factory app partition: `0x400000` bytes.
- Free app partition space: about `0x3244f0` bytes, roughly 79%.
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
#define OPENCALC_ENABLE_SERIAL_BUTTON_INPUT 0
#define OPENCALC_FLASH_STORAGE_IMAGE 1
#define OPENCALC_TARGET_FPS 45
#define OPENCALC_POWER_SAVE_CPU_MAX_MHZ 160
#define OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT 35
#define OPENCALC_USE_REAL_PCB 1
```

The storage image always comes from `storage_image/`. To test without Doom,
remove or move `storage_image/doom1.wad` before building/flashing.

Partition sizes are also set in `main/config.h`:

```c
#define OPENCALC_FACTORY_APP_PARTITION_SIZE 0x400000
#define OPENCALC_STORAGE_PARTITION_SIZE 0x800000
```

The top-level `CMakeLists.txt` reads those values and regenerates `partitions.csv` before ESP-IDF builds the partition table. The storage partition starts immediately after the factory app partition, so increasing the app partition moves storage later in flash. Keep the total layout within the ESP32-S3 module flash size.

## USB Storage

The board exposes the FAT storage partition over the USB port. With the default config it is 8 MB. Files that should be flashed automatically live in:

```text
storage_image/
  doom1.wad
  scripts/
    fib.py
```

On boot the volume label is set to `opencalc`, and the USB device strings are set to OpenCalc. The COM port is for flashing/monitoring; the USB port is the one that appears as storage on a computer.

## User And Debug Header

The V3 PCB target includes a back-side `1x16` unpopulated 2.54 mm user/debug header. It should expose:

| Pin | Net                  | Use                                                                                   |
| --: | -------------------- | ------------------------------------------------------------------------------------- |
|   1 | `GND`                | Ground reference                                                                      |
|   2 | `3V3`                | Regulated 3.3 V rail for low-current add-ons                                          |
|   3 | `5V/VBUS`            | USB-C VBUS when plugged in                                                            |
|   4 | `VBAT`               | Raw protected LiPo battery rail                                                       |
|   5 | `EN / RESET`         | Pull to `GND` to reset                                                                |
|   6 | `GPIO0 / BOOT`       | Pull to `GND` during reset for ROM bootloader                                         |
|   7 | `GPIO43 / TX0`       | Serial console TX                                                                     |
|   8 | `GPIO44 / RX0`       | Serial console RX                                                                     |
|   9 | `USB D+`             | USB debug only, not general GPIO                                                      |
|  10 | `USB D-`             | USB debug only, not general GPIO                                                      |
|  11 | Reserved / DNP       | Optional future power-control or debug net; not used by current software-off firmware |
|  12 | `GPIO33`             | User-usable spare GPIO                                                                |
|  13 | `GPIO34`             | User-usable spare GPIO                                                                |
|  14 | `GPIO35 / VBAT_DIV`  | Battery voltage divider ADC input                                                     |
|  15 | `GPIO36 / LCD_BACKL` | LCD backlight load-switch enable/PWM                                                  |
|  16 | `GPIO37 / CHG_STAT`  | Charger status input, active-low                                                      |

The preferred general expansion pins are `GPIO33` and `GPIO34`. The LCD, touch, keypad, battery ADC, backlight control, charger status, native USB, and console UART pins are already assigned.

Power off is software-only in the current firmware. `2nd` + `On` turns off the display/backlight and enters ESP32-S3 deep sleep; it does not drive a `POWER_HOLD` latch or cut the battery rail.

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

- 10 rectangular `Y=` graph slots with color graphing.
- 15 graph colors available internally for graph/app drawing.
- Graph trace with zeros, y-intercepts, local min/max, and intersection markers.
- Fast jump to the nearest intersection when two enabled functions intersect in the current graph window.
- Graph window controls for x/y max and x/y tick spacing.
- Table view for enabled rectangular functions.
- Calculator math parser with trig, powers, roots, vertical fractions, compact exponent/root rendering, probability basics, derivatives, and definite integrals.
- Numeric solver with `E1`, `E2`, guess-based solving, and decimal-to-fraction result display.
- Lists `L1` through `L6`, up to 999 values each, with edit/sort/sum/min/max support.
- Statistics app with list editing, sort/clear, 1-var stats, 2-var stats from `L1`,`L2`, and basic linear regression.
- Matrix app with one working matrix `A` up to 4x4, including set/show/determinant/inverse/RREF/transpose/identity.
- Finance TVM, NPV, and IRR helpers.
- Lines and conics templates that add solved graph curves into free `Y=` slots.
- Inequality graphing presets with shaded regions, overlap shading, vertical `x>=0`, and dotted/solid boundaries for strict/inclusive inequalities.

Not complete yet, so not presented as finished:

- Parametric, polar, and sequence graphing modes.
- Split-screen graph/table layout.
- Image graph backgrounds.
- Full polynomial root finder up to order 10.
- Full 10-equation/10-unknown system solver.
- Full 10-matrix, 99x99 matrix storage and row operation suite.
- Full TI-level hypothesis tests, confidence intervals, distributions, and all regression models.

## Python Scripts

Python-style scripts live in `storage_image/scripts/` before flashing and `/data/scripts/` at runtime. `PRGM` opens a program menu with Run, Edit, New, and Delete entries. Run/Edit/Delete open the scripts browser. New creates a starter `programNN.py` file in `/data/scripts`. `2nd` + `PRGM` jumps directly to the scripts browser. When a script starts, the LCD switches to a Python output/input console screen. Script `print(...)` output appears there.

When a script calls `input()`, the console opens a keypad input line. Use number keys, operators, `Alpha` letters, `CLEAR` to delete, `2nd` + `CLEAR` to clear the input, and `Enter` to submit. `DEL/Back` cancels the input. When the script finishes, the screen prompts you to press Enter or Back to return to the script list.

Example flashed script:

```text
storage_image/scripts/fib.py
```

If monitor output says the flashed app checksum does not match the built app, rebuild and flash again before debugging script behavior.

## Games

Press `Alpha` then `2nd` to open the game menu. The current menu includes Tetris, Doom, Snake, Breakout, and Mario. High scores are saved in on-chip NVS, so they survive power off and do not depend on USB storage.

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
| Hold `Del` + Left/Right  | Strafe                         |
| `Enter`                  | Menu/select                    |
| `Back`                   | Escape/back                    |
| `On (Home)`              | Quit Doom and return game menu |

Mario currently checks for the ROM at:

```text
storage_image/mario.nes
```

Mario controls are mapped to match Doom-style play:

| Calculator button        | Mario action                     |
| ------------------------ | -------------------------------- |
| Up / Down / Left / Right | Move                             |
| `Y=`                     | B / action                       |
| `Zoom`                   | A / jump                         |
| `Enter`                  | Start                            |
| `Back`                   | Select/back                      |
| `On (Home)`              | Quit Mario and return game menu  |

Mario loads `storage_image/mario.nes` through the real NES path: iNES cartridge loader, mapper 0 cartridge support, 6502 CPU, NES bus, controller state, PPU 2C02 framebuffer, then the ILI9341 display driver. Audio is disabled. For now, use a mapper 0 `.nes` ROM such as the standard Super Mario Bros cartridge format.

The production 10x5 keypad expects one series diode per key. With columns
pulled high and rows scanned low, diode anodes face the columns and diode
cathodes/bars face the rows. This enables simultaneous movement, sprint,
strafe, fire, and use inputs without matrix ghosting.

## Notes

The storage partition is generated from `main/config.h` into `partitions.csv`. If storage contents look wrong after changing the partition or image, rebuild and flash with `idf.py flash` so both the partition table and `build/storage.bin` are written.
