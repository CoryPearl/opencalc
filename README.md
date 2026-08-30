# OpenCalc

This project was partially funded by [https://pcbway.com](PCBWay) in order to be able to test my PCB designs. They were extreamly easy to work with and communicated fast and well, I highly recomend untilizing them.

Blog: [https://corypearl.github.io/opencalc/blog.html](https://corypearl.github.io/opencalc/blog.html)

OpenCalc is an open source graphing calculator project inspired by the TI-84, but built around a faster ESP32-S3, a color LCD, USB-C, Python-style scripting, and expandable firmware.

The calculator runs **OpenCalc OS**, the project’s open-source ESP32-S3 operating environment. OpenCalc OS includes the calculator interface, math and graphing engines, built-in apps, Python-style scripting runtime, USB file storage, keypad handling, display drivers, and power management.

<p align="center">
  <img src="hardware/pcb/pcb_done_v4_full.png" alt="PCB Front" width="60%">

  <!-- <img src="hardware/pcb/pcb_done_v4_front.png" alt="PCB Front" width="45%"> -->
  <!-- <img src="hardware/pcb/pcb_done_v4_back.png" alt="PCB Back" width="45%"> -->
</p>

\*\*First prototype PCB

## Why OpenCalc Is Different

OpenCalc is meant to feel like a graphing calculator, but with more open hardware and a much more flexible firmware base than a closed classroom calculator.

- **ESP32-S3 processor:** dual-core 32-bit MCU running up to 240 MHz, giving the calculator far more general-purpose compute headroom than older calculator hardware.
- **More memory to build with:** the firmware targets ESP32-S3 boards with RAM plus PSRAM support, so graphics, scripting, menus, and games have room to grow.
- **8 MB user storage partition:** scripts, WAD files, and other user files live in a FAT storage area exposed by the calculator.
- **Easy USB-C file access:** plug the USB port into a computer and it appears as `opencalc` storage, so files can be added without special linking software.
- **OpenCalc OS:** the calculator UI, math parser, graphing, keypad, touch, storage, apps, pcbdesigns, and hardware behavior are all open source.
- **Modern extras:** color LCD, USB mass storage, Python-style scripts, and a configurable app launcher.

The current prototype includes:

- ILI9341 LCD output
- 10x5 button matrix support
- USB mass storage named `opencalc`
- flashed script/storage image support
- Python style scripting - [README](firmware/main/components/tiny-python-readme.md)
- graphing calculator UI with calculator, graph, table, apps, settings, scripts, matrix, stats, finance, conics, and inequality graphing
- A few games
- Wi-Fi + bluetooth available but disabled for now due to no use for them

Cost breakdown (Single PCB not bulk, before shipping and not including manufacturing):

- The board: ~$7
- The soldederd parts: ~$34
- Battery: $6
- Screen: $6

A little higher than I'd like but this is the first prototype (still way less than the bs $130 TI though).

## User And Debug

Test pads are located on the back, as well as 2 LEDs. One LED is on the front which is a red power LED, then the 2 on the back are a Charging green LED and a Power good blue LED

Power off is software-only for the current firmware: `2nd` + `On` shuts off the display/backlight and puts the ESP32-S3 into deep sleep. The PCB should prioritize low quiescent current parts and an ON/HOME key matrix wake path instead of a required firmware-controlled true power latch.

See config.h in firmware to change many diffrent settings in the opencalc software.

## Games

Game menu is toggled from the calculator with `Alpha` then `2nd`. Press the same combo again to leave Doom and return to the calculator UI.

| Calculator button        | Doom action               |
| ------------------------ | ------------------------- |
| Up / Down / Left / Right | Move                      |
| `Y=`                     | Fire                      |
| `Window`                 | Cycle weapons             |
| `Zoom`                   | Use / open doors          |
| `Trace`                  | Open / close automap      |
| `1`-`7`                  | Select weapon slot        |
| Hold `Mode`              | Run / sprint              |
| Hold `Del` + Left/Right  | Strafe                    |
| `Enter`                  | Menu/select               |
| `Back`                   | Escape/back               |
| `On (Home)`              | Quit Doom and return home |

| Calculator button        | Mario action                    |
| ------------------------ | ------------------------------- |
| Up / Down / Left / Right | Move                            |
| `Y=`                     | B / action                      |
| `Zoom`                   | A / jump                        |
| `Enter`                  | Start                           |
| `Back`                   | Select/back                     |
| `On (Home)`              | Quit Mario and return game menu |

\*All other game controlls are listed on screen

Other games:

- Tetris
- Snake
- Breakout

## Project Layout

```text
firmware/   OpenCalc OS source for the ESP32-S3 calculator
hardware/   key layouts, graphics, and hardware design files
docs/       notes and project website files
```

## Flash And Monitor

Plug USB-C port into computer.

While holding boot, press reset, then let go of both.

From a fresh terminal:

```zsh
cd firmware
idf
idf.py build
idf.py flash
idf.py monitor
```

If you need the specific port name, list available ports with:

```zsh
ls /dev/cu.*
```

After flashing

When you mointor after flashing, the board will be in serial mode. Press the reset button on the back once to boot the software and be able to check outputs in the USB monitor.

See [firmware/FIRMWARE_README.md](firmware/FIRMWARE_README.md) for build, flash, USB storage, and Doom setup notes.
See [hardware/pcb/pcb.md](hardware/pcb/pcb.md) for info on the current pcb design.

## Status

This is still prototype hardware and OpenCalc OS software. The current goal is to keep the calculator booting directly into a usable interface, expose files over USB when connected to a computer, and continue filling out the TI-style apps with working implementations rather than placeholder screens.

## Plans

- switch to esp32-s31 for a big speed boost
- increase charging speed form 500mah to 1000/2000mah
- Reorganize PCB
- Flush out firmware

## Credits

Doom port in /doomgeneric made by @ozkl on github at [https://github.com/ozkl/doomgeneric](https://github.com/ozkl/doomgeneric)

NES Emulator in /mairo created by @Shim06, @jethomson, and @ferytell on github at [https://github.com/Shim06/Anemoia-ESP32](https://github.com/Shim06/Anemoia-ESP32)

All other code and designs created by Cory Pearl.
