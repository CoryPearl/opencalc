# OpenCalc PCB

A plain-language summary of the current OpenCalc PCB target.

## Brain

- ESP32-S3-WROOM-1-N16R8 with 16 MB flash and 8 MB PSRAM.
- Standard decoupling capacitors close to the module power pins.
- Front red power LED that indicates the 3.3 V rail is present.
- Separate Reset and Boot controls for bring-up and flashing.

## Power Path

USB-C VBUS feeds the LiPo charger and system power path, then the regulator creates the board 3.3 V rail.

- Charger: BQ24074-style USB power-path LiPo charger.
- Regulator: AP63203WU or equivalent 3.3 V regulator section.
- Battery connector: 2-pin JST-PH/PHR-style LiPo connector.
- Battery monitor: resistor divider routed to the ESP32 ADC input used by firmware.
- Status LEDs: green charging LED and blue power-good LED on the back.

Use a protected single-cell LiPo pack unless protection is added elsewhere.

## USB-C

- One USB-C connector handles power, flashing/monitoring, and USB mass storage.
- CC1 and CC2 use 5.1 kΩ pulldowns for USB device mode.
- D+ and D- route through USB ESD protection to the ESP32-S3 native USB pins.
- VBUS routes to the charger/input power path.

## Display

- 14-pin, 2.54 mm female connector for the ILI9341 SPI display.
- Display nets include `3V3`, `GND`, `LCD_CS`, `LCD_RST`, `LCD_DC`, `LCD_MOSI`, `LCD_SCLK`, `LCD_MISO`, and `LCD_BL_A`.
- Touch pins are currently left unwired and disabled in firmware for bring-up.
- The backlight is controlled from ESP32 GPIO through a 3.3 V load-switch/PWM path. Do not drive the LED/backlight from 5 V.

## Keypad

- 10-row x 5-column matrix, 50 keys total.
- Each key has a series diode for multi-key support and anti-ghosting.
- Firmware scans by driving one column low at a time and reading row inputs with pull-ups.
- The diode direction must allow a pressed key to pull the selected row low only through the active low column.
- ON/HOME is part of the matrix and is also used as the wake key for software-off sleep.

## Current Firmware Pin Map

| Function | GPIO |
| -------- | ---- |
| LCD SCLK / MOSI / MISO | `12` / `11` / `13` |
| LCD CS / DC / RST | `10` / `14` / `15` |
| LCD backlight PWM/load-switch enable | `47` |
| USB D- / D+ | `19` / `20` |
| Battery ADC divider | `7` |
| Charger status input | `41` |
| Keypad rows 0-9 | `1`, `2`, `42`, `4`, `5`, `6`, `48`, `8`, `9`, `16` |
| Keypad columns 0-4 | `17`, `18`, `38`, `39`, `40` |

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

Any extra user GPIO header pins should be confirmed against the firmware pin map before routing. Do not reuse LCD, keypad, battery ADC, charger status, native USB, or UART console pins as general expansion pins.

## Software-Off Behavior

The current design uses software-off sleep, not a true power latch. In software-off mode, firmware turns off the LCD/backlight and puts the ESP32-S3 into a low-power sleep mode. ON/HOME wakes the device through the keypad wake path.

## Bring-Up Checks

- `3V3` should measure about 3.3 V to board ground.
- `VBUS` should measure about 5 V when USB-C is plugged in.
- LCD `VCC`, `CS`, and `RST` should be near 3.3 V when initialized.
- LCD `SCLK`, `MOSI`, and `DC` may read near 0 V on a multimeter when idle because they are fast digital signals.
- Backlight control should be driven only from the 3.3 V load-switch path.
