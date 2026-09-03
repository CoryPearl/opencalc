<p align="center">
  <img src="img/opencalc_logo_light.png" width="100" alt="OpenCalc logo">
</p>

OpenCalc is an open-source graphing calculator inspired by the TI-84, built around an ESP32-S3, a color LCD, USB-C, Python-style scripting, games, and expandable firmware.

OpenCalc-authored software is licensed under **GPL-3.0-or-later**. Bundled components retain their original licenses; see [Third-Party Notices](THIRD_PARTY_NOTICES.md).

The calculator runs **OpenCalc OS**, the project’s open-source ESP32-S3 operating environment. OpenCalc OS includes the calculator interface, math and graphing engines, built-in apps, scripting runtime, USB file storage, keypad handling, display drivers, game support, and power management.

This project was partially funded by [PCBWay](https://pcbway.com).

Blog: [https://corypearl.github.io/opencalc/blog.html](https://corypearl.github.io/opencalc/blog.html)

<p align="center">
  <a href="hardware/pcb/opencalc_pcb_V5/img/opencalc_case_spin.gif"><img src="hardware/pcb/opencalc_pcb_V5/img/opencalc_case_spin.gif" alt="OpenCalc rotating case render" height="250"></a>
  <a href="hardware/pcb/opencalc_pcb_V5/img/opencalc_pcb_V5_full.png"><img src="hardware/pcb/opencalc_pcb_V5/img/opencalc_pcb_V5_full.png" alt="OpenCalc V5 PCB" height="250"></a>
<a href="firmware/open_calc_ui_calculator_demo.gif"><img src="firmware/open_calc_ui_calculator_demo.gif" alt="OpenCalc OS demo" height="250"></a>
<br>
<sub>Click an image to enlarge it.</sub>

</p>

## Highlights

- ESP32-S3-WROOM-1-N16R8 with 16 MB flash, a 6 MB firmware partition, and 8 MB PSRAM.
- 320x240 color display with a full-frame DMA renderer and a configurable 45 FPS UI/game loop.
- 10x5 diode-isolated keypad with interrupt-assisted scanning, held keys, and multi-key game input.
- One USB-C port for power, firmware flashing, CDC serial monitoring, and 8 MB of FAT file storage.
- 12-app OpenCalc OS launcher covering Calculator, Graph, Table, Python, Statistics, Lists, Matrices, Solver, Settings, Finance, Conics, and Inequality tools.
- Textbook-style calculator entry for fractions, roots, exponents, history, variables, complex numbers, and numerical calculus.
- Embedded Giac/KhiCAS-derived CAS for exact arithmetic, simplification, expansion, factoring, equations, derivatives, integrals, limits, sums, products, series, complex math, and matrix expressions, with Eigenmath and native polynomial routines as fallbacks.
- Cartesian, parametric, polar, and sequence graphing with mode-aware tables, tracing, calculus analysis, points of interest, and intersection tools.
- Statistics catalog with descriptive statistics, regressions, tests, intervals, ANOVA, distributions, and basic statistical plots.
- Ten matrices up to 99x99 with determinant, inverse, RREF, transpose, augment, list conversion, and row operations.
- Built-in Tiny Python app with script browsing, creation, editing, deletion, LCD output, and keypad-driven `input()`.
- Game launcher with Tetris, Doom, Snake, Breakout, and mapper-0 NES/Mario support, persistent high scores, and optional game audio.
- Dual-core runtime with asynchronous graph/math work, PSRAM-aware large allocations, persistent NVS settings, and configurable performance/power-save limits.
- Wi-Fi and Bluetooth available in hardware, disabled by default in firmware to save battery.
- Current PCB includes battery charging and monitoring, PWM backlight control, software-off sleep, a full power switch, optional PAM8302A audio, and back-side test pads.

## Rough production Cost Estimate

| Cost area                           | Estimate per unit |
| ----------------------------------- | ----------------- |
| PCB electronics parts from BOM      | $11-$16           |
| 320x240 LCD                         | $5                |
| 3.7 V 2000 mAh Li-ion battery       | $3-$5             |
| Cheap keycaps / keypad plastics     | $2-$4             |
| 3d printed case                     | $3-$6             |
| PCB fabrication + SMT assembly      | $5-$9             |
| Bulk shipping / logistics allowance | $3-$6             |
| Estimated production cost           | $32-$51           |

## Project Layout

```text
firmware/   OpenCalc OS source for the ESP32-S3 calculator
hardware/   key layouts, graphics, PCB files, case files, and hardware docs
docs/       notes and project website files
```

## Quick Start

Plug the OpenCalc USB-C port into your computer. [ESP-IDF Get Started guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) (If not installed yet).

```zsh
cd firmware
idf
idf.py build
```

Hold Boot, press Reset, then release both.

```zsh
idf.py flash
idf.py monitor
```

## Documentation

- [Firmware README](firmware/FIRMWARE_README.md): build settings, flashing, storage image, controls, apps, games, and firmware status.
- [App Status](firmware/APP_STATUS.md): implementation status for every OpenCalc OS app and game.
- [PCB README](hardware/pcb/PCB_README.md): current V5 PCB parts, pin map, power path, display, keypad, battery, audio, and bring-up checks.
- [Tiny Python README](firmware/main/components/tiny-python-readme.md): scripting runtime details.

## Current Status

OpenCalc is working prototype hardware and software. Display, keypad, storage, power, calculator, graphing, scripting, and game paths have been exercised on current hardware. A real Giac/KhiCAS-derived CAS backend is now integrated in source, but its OpenCalc ESP-IDF build size, runtime memory behavior, and physical-device result rendering still need validation. Remaining work is concentrated in that CAS validation, richer TI/AP statistics workflows, large-matrix stress testing and editing, advanced graph presentation features, and continued UI polish. See the [app-by-app status](firmware/APP_STATUS.md) for the exact boundary between implemented and partial behavior.

## Credits

- OpenCalc OS, hardware, and enclosure design: [Cory Pearl](https://github.com/CoryPearl).
- CAS engines: [Giac/Xcas](https://xcas.univ-grenoble-alpes.fr/) by Bernard Parisse and contributors, with the [KhiCAS](https://github.com/KhiCAS) ESP32 port lineage under GPL-3.0-or-later; [Eigenmath](https://github.com/georgeweigt/eigenmath) remains as a BSD-2-Clause fallback.
- Doom engine: id Software's GPL Doom source, Simon Howard's Chocolate Doom work, and [doomgeneric](https://github.com/ozkl/doomgeneric) by ozkl, under GPL-2.0-or-later notices.
- NES emulation: [Anemoia-ESP32](https://github.com/Shim06/Anemoia-ESP32) by Shim06 and contributors, under GPLv3.
- Platform: [Espressif ESP-IDF](https://github.com/espressif/esp-idf), TinyUSB, and the ESP LCD ILI9341 component under their respective licenses.
- Prototype project funding was provided in part by [PCBWay](https://pcbway.com).

Complete copyright, license, and game-data details are recorded in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Doom WADs and NES ROMs are separate game data and are not licensed under the OpenCalc firmware license.
