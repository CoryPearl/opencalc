# OpenCalc

Blog: [https://corypearl.github.io/opencalc/blog.html](https://corypearl.github.io/opencalc/blog.html)

OpenCalc is an open source graphing calculator project inspired by the TI-84, but built around a faster ESP32-S3, a color LCD, USB-C, Python-style scripting, and expandable firmware.

## Why OpenCalc Is Different

OpenCalc is meant to feel like a graphing calculator, but with more open hardware and a much more flexible firmware base than a closed classroom calculator.

- **ESP32-S3 processor:** dual-core 32-bit MCU running up to 240 MHz, giving the calculator far more general-purpose compute headroom than older calculator hardware.
- **More memory to build with:** the firmware targets ESP32-S3 boards with RAM plus PSRAM support, so graphics, scripting, menus, and games have room to grow.
- **8 MB user storage partition:** scripts, WAD files, and other user files live in a FAT storage area exposed by the calculator.
- **Easy USB-C file access:** plug the USB port into a computer and it appears as `opencalc` storage, so files can be added without special linking software.
- **Open firmware:** the calculator UI, math parser, graphing, keypad, touch, storage, and app behavior are all editable in this repo.
- **Modern extras:** color LCD, touch input, USB mass storage, Python-style scripts, doom, and a configurable app launcher.

The current prototype firmware brings up:

- ILI9341 LCD output
- touch input
- 10×5 button matrix support
- USB mass storage named `opencalc`
- flashed script/storage image support
- graphing calculator UI with calculator, graph, table, apps, settings, scripts, matrix, stats, finance, conics, and inequality graphing
- serial button simulation by typing button numbers 1-50 and pressing Enter
- optional Doom support using `doom1.wad`

## User And Debug Header

Test pads on the back.

Power off is software-only for the current firmware: `2nd` + `On` shuts off the display/backlight and puts the ESP32-S3 into deep sleep. The PCB should prioritize low quiescent current parts and an ON/HOME key matrix wake path instead of a required firmware-controlled true power latch.

## Doom Controls

Doom is toggled from the calculator with `Alpha` then `2nd`. Press the same combo again to leave Doom and return to the calculator UI.

| Calculator button        | Doom action               |
| ------------------------ | ------------------------- |
| Up / Down / Left / Right | Move                      |
| `Y=`                     | Fire                      |
| `Window`                 | Cycle weapons             |
| `Enter`                  | Menu/select               |
| `Mode`                   | Escape/back               |
| `On (Home)`              | Quit Doom and return home |

When using serial button simulation, type the button number and press Enter. The serial numbering matches [Design-V2-keymap.png](hardware/graphic-designs/Design-V2-keymap.png). `01` fires in Doom, `02` cycles weapons, `09` moves left, `10` moves up, `14` moves down, `15` moves right, `46` quits Doom and returns home, and `50` is Enter. Serial input queues button presses for the main UI task instead of running app code directly from the serial input task.

General calculator controls: `DEL` is Back, `CLEAR` deletes, and `2nd` + `CLEAR` clears the current input field. `Alpha` + `sin/cos/tan` inserts `csc(`/`sec(`/`cot(`; `Alpha` + `1/2/3` now enters `G/H/I`.

`PRGM` opens the program menu with Run/Edit/New/Delete entries. `2nd` + `PRGM` jumps straight to the scripts browser.

## Project Layout

```text
firmware/   ESP-IDF firmware for the ESP32-S3 prototype
hardware/   key layouts, graphics, and hardware design files
docs/       notes and project website files
```

See [firmware/FIRMWARE_README.md](firmware/FIRMWARE_README.md) for build, flash, USB storage, and Doom setup notes.

## Status

This is still prototype hardware and firmware. The current goal is to keep the calculator booting directly into a usable interface, expose files over USB when connected to a computer, and continue filling out the TI-style apps with working implementations rather than placeholder screens.

## Credits

Created by Cory Pearl.
