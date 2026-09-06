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
- Scientific sensor expansion using MCP23017T-E/SO digital I/O and
  ADS1115IDGST 16-bit analog acquisition.

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
- `AUDIO_SD` -> MCP23017 `GPA0`, PAM8302 shutdown control.
- `AUDIO_OUT` -> GPIO41, ESP32-S3 PDM audio output.
- GPIO13 / GPIO21 -> scientific-I/O SDA / SCL.
- GPIO41 is no longer charger status.
- GPIO13 is no longer LCD MISO.
- Firmware audio requires:

```c
#define OPENCALC_ENABLE_GAME_AUDIO 1
#define OPENCALC_USE_NEW_AUDIO_PCB 1
#define OPENCALC_ENABLE_SCIENTIFIC_IO 1
```

Settings -> Audio controls runtime game volume in 5% steps.

## Scientific Sensor Header

The proposed V5 scientific-I/O option uses the existing 3.3 V logic domain:

- `MCP23017T-E/SO` at I2C address `0x20` supplies 12 bidirectional user channels
  `D0-D11`. GPA0 is reserved for `AUDIO_SD`; GPA1 is reserved for `PWR_LED`;
  GPA7 and GPB7 are deliberately unused by this board design.
- `ADS1115IDGST` at I2C address `0x48` supplies `A0-A3`, four multiplexed
  16-bit single-ended inputs with differential-pair support and rates up to
  860 samples/s.
- ESP GPIO13 is SDA and GPIO21 is SCL. Fit one pair of 4.7 kOhm pull-ups to 3V3.
- Do not allocate GPIO35-GPIO37: the ESP32-S3-WROOM-1-N16R8 octal memory uses
  them. Do not use GPIO46 for this header: it is input-only and a strapping pin.
- The firmware does not provide UART on this revision. Add an I2C UART bridge
  if the product requires a dedicated sensor UART.

Recommended labeled 2x15, 2.54 mm female header:

| Pin | Label | Pin | Label |
| --- | ----- | --- | ----- |
| 1 | `3V3` | 2 | `GND` |
| 3 | `D0` | 4 | `D1` |
| 5 | `D2` | 6 | `D3` |
| 7 | `D4` | 8 | `D5` |
| 9 | `D6` | 10 | `D7` |
| 11 | `D8` | 12 | `D9` |
| 13 | `D10` | 14 | `D11` |
| 15 | `3V3` | 16 | `GND` |
| 17 | `A0` | 18 | `GND` |
| 19 | `A1` | 20 | `GND` |
| 21 | `A2` | 22 | `GND` |
| 23 | `A3` | 24 | `GND` |
| 25 | `SDA` | 26 | `SCL` |
| 27 | `ADS_RDY` | 28 | `GND` |
| 29 | `MCP_INTA` | 30 | `MCP_INTB` |

Pins 17-24 pair each analog input with its own adjacent ground. Pins 1 and 15
duplicate 3V3, while pins 2, 16, 18, 20, 22, 24, and 28 provide seven ground
connections distributed across the header. These are duplicate rail
connections, not separately switched supplies.

All `D0-D11` channels are bidirectional. Leave GPA7 and GPB7 unconnected and do
not place them on the user header. Label every header signal on silkscreen,
mark pin 1, and add
the matching opening to the enclosure design.
The KiCad and case files are intentionally not modified by the firmware change.
`ADS_RDY`, `MCP_INTA`, and `MCP_INTB` are exposed for measurement or external
use but are not routed back to ESP GPIOs in this pin-constrained revision.

See the [Rear 30-Pin Header guide](../../guied.MD#rear-30-pin-header) for the
pin-1 orientation rule, a detailed description of every physical pin,
electrical restrictions, Tiny Python channel numbers, and example sensor
wiring.

Analog inputs must remain between GND and 3V3. A larger configured PGA range
does not make the physical pins 5 V tolerant. Add low-leakage ESD protection
and, where suitable for the target bandwidth, a 1 kOhm series resistor plus a
small capacitor at each analog input. Never connect mains or an unknown voltage
directly to this header.

### Exact chip wiring

For the `MCP23017T-E/SO` 28-pin wide-SOIC package:

- Pin 9 `VDD` -> `3V3`; pin 10 `VSS` -> GND; place 100 nF from pin 9 to pin 10.
- Pin 12 `SCL` -> ESP GPIO21 and the header `SCL` signal.
- Pin 13 `SDA` -> ESP GPIO13 and the header `SDA` signal.
- Pins 15, 16, and 17 (`A0`, `A1`, `A2`) -> GND for address `0x20`.
- Pin 18 `RESET` -> 3V3 through 10 kOhm; optionally add a reset test pad.
- Pin 20 `INTA` and pin 19 `INTB` -> header event pins; do not tie them together.
- Pin 21 `GPA0` -> PAM8302A `SD`. Add 100 kOhm from `SD` to GND so audio is
  disabled before firmware configures the expander.
- Pin 22 `GPA1` -> the existing power-LED resistor/LED path, active high. Size
  the LED resistor for a few milliamps rather than using the expander maximum.
- Pins 23-27 `GPA2-GPA6` -> `D0-D4`; pin 28 `GPA7` -> unconnected.
- Pins 1-7 `GPB0-GPB6` -> `D5-D11`; pin 8 `GPB7` -> unconnected.
- Pins 11 and 14 are NC for the I2C part; leave them unconnected.

For the `ADS1115IDGST` DGS/VSSOP-10 package:

- Pin 8 `VDD` -> 3V3; pin 3 `GND` -> GND; place 100 nF directly between them.
- Pin 10 `SCL` -> ESP GPIO21/shared `SCL`; pin 9 `SDA` -> ESP GPIO13/shared `SDA`.
- Pin 1 `ADDR` -> GND for address `0x48`.
- Pins 4, 5, 6, and 7 (`AIN0-AIN3`) -> protected header signals `A0-A3`.
- Pin 2 `ALERT/RDY` -> header `ADS_RDY`; add a 10 kOhm pull-up to 3V3 because
  this output is open drain.

Only fit one SDA pull-up and one SCL pull-up for the complete bus. Start with
4.7 kOhm to 3V3. Place both IC bypass capacitors close to their supply pins,
keep the analog traces away from LCD SPI/PDM/backlight switching, and connect
all sensor grounds to the board ground plane.

Enable this hardware in `firmware/main/config.h` with
`OPENCALC_ENABLE_SCIENTIFIC_IO 1`. Tiny Python can read, trigger, plot, print,
and save measurements directly into persistent `L1-L6` Statistics data.

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
| Audio shutdown / PDM output | MCP23017 `GPA0` / `41` with scientific I/O |
| Keypad rows 0-9 | `1`, `2`, `42`, `4`, `5`, `6`, `48`, `8`, `9`, `16` |
| Keypad columns 0-4 | `17`, `18`, `38`, `39`, `40` |
| Sensor I2C SDA / SCL | `13` / `21` |
| Power status LED | MCP23017 `GPA1` if `OPENCALC_ENABLE_POWER_STATUS_LED` is enabled |
| Touch | unwired / disabled |

Recommended V5 firmware profile:

```c
#define OPENCALC_USE_REAL_PCB 1
#define OPENCALC_USE_NEW_AUDIO_PCB 1
#define OPENCALC_USE_AO3401A_BACKLIGHT 1
#define OPENCALC_ENABLE_SCIENTIFIC_IO 1
```

Enable `OPENCALC_ENABLE_GAME_AUDIO` separately for game sound and
`OPENCALC_ENABLE_POWER_STATUS_LED` separately for the awake power LED.

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

Graph split views, graph styling, and `/data/graph.bmp` backgrounds are software
features and require no V5 PCB changes. Their framebuffer and worker storage use
the module's external PSRAM, so the N16R8 target remains the expected module.
The CAS, graph/math, and Python script workers also use PSRAM-backed stacks, so
PSRAM is a firmware requirement rather than an optional performance upgrade.

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
- With scientific I/O enabled, the serial log should report both MCP23017 and
  ADS1115 as `ready`. A `missing` result means to inspect address straps, power,
  ground, SDA/SCL continuity, pull-ups, and soldering before testing scripts.
- Idle `SDA` and `SCL` should each measure near 3.3 V. If either remains low,
  disconnect power and check for a short, reversed part, or device holding the bus.
- An I2C scan should find MCP23017 at `0x20` and ADS1115 at `0x48`.
- MCP23017 GPA0 / `AUDIO_SD` should stay low while audio is inactive, then rise
  near 3.3 V while enabled game audio is running.
- With the status-LED toggle enabled, MCP23017 GPA1 should be high while awake
  and low during software-off sleep.
- Before connecting an unknown sensor, test A0 with a known source between GND
  and 3.3 V, such as the wiper of a 10 kOhm potentiometer across 3V3 and GND.
- Confirm scripts reject digital channel numbers outside `D0-D11`.
- Run `logger.py`, vary A0, then open Lists or Statistics and confirm 60 values
  appear in L1 and remain after the worksheet persistence delay and a reboot.
