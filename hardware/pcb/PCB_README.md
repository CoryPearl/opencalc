# OpenCalc PCB

A plain-language summary of the current OpenCalc PCB target. This document is for the current V5 KiCad PCB, not the older V1/V2/V3/V4 experiments.

## Brain

- ESP32-S3-WROOM-1-N16R8 module with 16 MB flash and 8 MB PSRAM.
- Decoupling capacitors close to the ESP32 module power pins.
- Reset and Boot controls are exposed for bring-up and flashing.
- Firmware target is the real PCB profile in `firmware/main/config.h`.

## Prototype History

The current board target is the second major prototype PCB family, V5. The older V4 prototype render is kept for reference at [`hardware/pcb/V4/pcb_done_v4_full.png`](V4/pcb_done_v4_full.png).

Compared with earlier prototypes, the V5 direction focuses on:

- Smaller footprint LCD and matching compact body.
- Larger, cleaner button layout.
- More organized PCB routing and labeling.
- Soldered battery pads instead of the earlier connector-focused plan.
- Easier back-side access to GND and bring-up test points.
- Better antenna keepout/placement.
- Full case designed in CAD.
- Logo artwork on the PCB.
- Full power-off slide switch plus firmware software-off sleep.
- USB serial monitoring and USB mass storage through the same USB-C connector.
- Optional speaker/audio hardware for games.

## Power Path

USB-C VBUS feeds the charger/power-path stage, then the regulator creates the board 3.3 V rail.

- USB-C connector: `USB4085-GF-A`, one connector for power, flashing/monitoring, and USB storage.
- USB protection: `TPD2E2U06DCKR` ESD protection on D+/D-.
- Charger/power path: `BQ24074RGTR` single-cell Li-ion/LiPo charger with system output.
- 3.3 V regulator: `TPS63802DLAR` buck-boost regulator using the `74479275147` 4.7 uH inductor.
- Battery connection: single-cell protected Li-ion/LiPo solder pads or large plated pads for `VBAT` and `GND`.
- Battery monitor: 1M/1M resistor divider from `VBAT` to GPIO7 / ADC1 channel 6, with 100 nF from the ADC node to GND.
- Hardware power switch: `JS102011SAQN` slide switch may be used as a full battery disconnect.
- Status LEDs: red power LED, plus charger/power-good indicators if populated.

Use a protected single-cell Li-ion/LiPo pack unless protection is added elsewhere.

## USB-C

- One USB-C connector handles power, flashing/monitoring, and USB mass storage.
- CC1 and CC2 use 5.1 kOhm pulldowns for USB device mode.
- USB D- / D+ route through ESD protection to ESP32-S3 native USB GPIO19 / GPIO20.
- VBUS routes through the input protection/fuse path to the charger input.

## Display

- LCD target: 320x240 ILI9341-compatible SPI display, mechanically represented by the current display model in `opencalc_pcb_V5`.
- Connector: `FH12-18S-0.5SH(55)` 18-pin FPC connector.
- Firmware uses write-only SPI for the new PCB: LCD MISO is not connected.
- Firmware display nets:
  - `LCD_SCLK` -> GPIO12
  - `LCD_MOSI` -> GPIO11
  - `LCD_CS` -> GPIO10
  - `LCD_DC` -> GPIO14
  - `LCD_RST` -> GPIO15
  - `LCD_BACKL_PIN` -> GPIO47
- Touch pins are not wired on this PCB revision and are disabled in firmware.
- Backlight is controlled from GPIO47 through the AO3401A 3.3 V high-side backlight circuit. Do not drive the LCD LED/backlight from 5 V.
- For this V5 backlight circuit, firmware should use `OPENCALC_USE_AO3401A_BACKLIGHT 1`.

## Game Audio

The new PCB adds game audio. The ESP32-S3 does not have an analog DAC, so firmware outputs PDM that must be filtered before the speaker amp.

- Speaker amp: `PAM8302AADCR`.
- Speaker: `CMS-151504-SMT-TR` or compatible small speaker.
- `AUDIO_SD` -> GPIO13, PAM8302 shutdown control.
- `AUDIO_OUT` -> GPIO41, ESP32-S3 PDM audio output.
- GPIO41 is no longer charger status.
- GPIO13 is no longer LCD MISO.
- Firmware audio requires:

```c
#define OPENCALC_ENABLE_GAME_AUDIO 1
#define OPENCALC_USE_NEW_AUDIO_PCB 1
```

Settings -> Audio controls runtime game volume in 5% steps.

## Keypad

- 10-row x 5-column matrix, 50 keys total.
- Each key has a series diode for multi-key support and anti-ghosting.
- Firmware scans by driving one column low at a time and reading row inputs with pull-ups.
- The diode direction must allow a pressed key to pull the selected row low only through the active low column.
- ON/HOME is part of the matrix and is also used as the wake key for software-off sleep.

## Firmware Pin Map

| Function | GPIO / Net |
| -------- | ---------- |
| LCD SCLK / MOSI / MISO | `12` / `11` / unwired |
| LCD CS / DC / RST | `10` / `14` / `15` |
| LCD backlight PWM | `47` |
| USB D- / D+ | `19` / `20` |
| Battery ADC divider | `7` |
| Charger status input | removed / not used by firmware |
| Audio shutdown / PDM output | `13` / `41` |
| Keypad rows 0-9 | `1`, `2`, `42`, `4`, `5`, `6`, `48`, `8`, `9`, `16` |
| Keypad columns 0-4 | `17`, `18`, `38`, `39`, `40` |
| Power status LED | `21` if `OPENCALC_ENABLE_POWER_STATUS_LED` is enabled |
| Touch | unwired / disabled |

Recommended V5 firmware profile:

```c
#define OPENCALC_USE_REAL_PCB 1
#define OPENCALC_USE_NEW_AUDIO_PCB 1
#define OPENCALC_USE_AO3401A_BACKLIGHT 1
```

## Test Pads And Headers

Keep easy back-side access to:

- `GND`
- `3V3`
- `5V/VBUS`
- `VBAT`
- `RESET/EN`
- `GPIO0/BOOT`
- `UART0 TX/RX`
- `USB D+/D-`

Any extra user GPIO header pins should be confirmed against the firmware pin map before routing. Do not reuse LCD, keypad, battery ADC, native USB, audio, or UART console pins as general expansion pins.

## Software-Off Behavior

The current firmware uses software-off sleep. In software-off mode, firmware turns off the LCD/backlight and puts the ESP32-S3 into the configured low-power sleep path. ON/HOME wakes the device through the keypad wake path.

The optional hardware slide switch can still be used as a true full battery disconnect, but firmware does not depend on a power-hold latch.

## Hardware Tooling

Current hardware work uses KiCad for the V5 PCB files, FreeCAD for case design, Affinity for graphics/layout work, and ViewSTL or similar tooling for quick 3D model checks. Older EasyEDA experiments may still exist in the repo, but the current PCB README describes the V5 KiCad design.

## Bring-Up Checks

- `VBUS` should measure about 5 V when USB-C is plugged in.
- `3V3` should measure about 3.3 V to board ground.
- `VBAT` should match the battery voltage, usually about 3.0-4.2 V for a single Li-ion/LiPo cell.
- `VBAT_DIV` should be about half of `VBAT` with the default 1M/1M divider.
- LCD `VCC`, `CS`, and `RST` should be near 3.3 V when initialized.
- LCD `SCLK`, `MOSI`, and `DC` may read near 0 V on a multimeter when idle because they are fast digital signals.
- Backlight must be powered from the 3.3 V AO3401A path, not 5 V.
- `AUDIO_SD` should be low when no game audio is active and high when game audio is enabled during gameplay.
- `AUDIO_OUT` is a fast PDM signal, so a multimeter will not show meaningful audio waveform detail.
