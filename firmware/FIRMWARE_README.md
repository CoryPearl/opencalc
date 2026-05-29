# OpenCalc Firmware

ESP-IDF firmware for the OpenCalc ESP32-S3 board. It brings up the ILI9341 LCD, touch controller, button matrix, USB mass storage, script storage, calculator apps, serial button simulation, and optional Doom support.

## Build and Flash

Clone the repo, install/source ESP-IDF, then build or flash from this directory:

```bash
git clone https://github.com/CoryPearl/opencalc.git
cd opencalc/firmware
idf.py build
idf.py flash
```

Use `idf.py flash`, not `idf.py app-flash`, because the normal flash target also writes the generated storage image to the `storage` partition.

To edit the firmware, change files under `main/`, rebuild with `idf.py build`, then flash with `idf.py flash`. Use `idf.py monitor` to view logs after flashing.

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

The firmware supports both the physical 10×5 button matrix and serial button simulation. When serial input is enabled, type a button number from `1` to `50` and press Enter. Serial button requests are queued and handled by the main UI task so scripts and drawing do not run from the serial input task. The serial numbering matches `hardware/graphic-designs/Design-V2-keymap.png`.

Important key behavior:

- `DEL` is Back.
- `CLEAR` deletes the current character/token.
- `2nd` + `CLEAR` clears the current input field.
- `ON (Home)` returns to the home screen, and quits Doom when Doom is active.
- `sqrt` inserts `sqrt()`; `2nd` + `sqrt` inserts an editable nth-root box.
- `^2` inserts square; `2nd` + `^2` inserts an editable exponent box.
- `[]/[]` inserts a vertical fraction box; `2nd` + `[]/[]` inserts inline division.
- `Alpha` + `sin/cos/tan` inserts `csc(`/`sec(`/`cot(`.
- `Alpha` + `1/2/3` enters `G/H/I`.
- `PRGM` opens the program menu. `2nd` + `PRGM` jumps straight to the scripts browser.
- `2nd` + `Up` increases backlight brightness; `2nd` + `Down` decreases it.
- In menus, number keys select the matching menu item.

## Calculator Feature Coverage

The firmware exposes only features that have a real implementation path. TI-style features not listed as working here should be treated as planned, not complete.

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

Python-style scripts live in `storage_image/scripts/` before flashing and `/data/scripts/` at runtime. `PRGM` opens a program menu with Run, Edit, New, and Delete entries. Run/Edit/Delete open the scripts browser; editing and deleting are done over USB for now. New creates a starter `programNN.py` file in `/data/scripts`. `2nd` + `PRGM` jumps directly to the scripts browser. When a script starts, the LCD switches to a Python output/input console screen. Script `print(...)` output appears there.

When a script calls `input()`, the console opens a keypad input line. Use number keys, operators, `Alpha` letters, `CLEAR` to delete, `2nd` + `CLEAR` to clear the input, and `Enter` to submit. `DEL/Back` cancels the input. When the script finishes, the screen prompts you to press Enter or Back to return to the script list.

Example flashed script:

```text
storage_image/scripts/fib.py
```

If monitor output says the flashed app checksum does not match the built app, rebuild and flash again before debugging script behavior.

## Doom

Doom is optional and uses the shareware IWAD at:

```text
storage_image/doom1.wad
```

After flashing, press `Alpha` then `2nd` to toggle Doom on or off.

Doom controls:

- Direction keys move.
- `Y=` shoots.
- `Window` cycles weapons.
- `Enter` selects/uses menus.
- `Mode` is back/escape.
- `ON (Home)` quits Doom and returns home.

## Notes

The storage partition is generated from `main/config.h` into `partitions.csv`. If storage contents look wrong after changing the partition or image, rebuild and flash with `idf.py flash` so both the partition table and `build/storage.bin` are written.
