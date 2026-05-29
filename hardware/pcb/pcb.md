# OpenCalc PCB

This file is the working PCB bill of materials and build checklist for the
current OpenCalc board target. The PCB workflow target is EasyEDA Pro/JLCEDA so
the design can move cleanly into JLCPCB fabrication and assembly.

The V3 schematic (`v3_schem.pdf`) is the authoritative source for designators,
part values, and net names. This document reflects that schematic.

## Design Target

- ESP32-S3 graphing calculator PCB
- One USB-C connector for charging, flashing, serial monitor, and storage access
- LCDWiki MSP2807 2.8-inch SPI ILI9341 touch display module on a 1x14 2.54 mm female connector (`J_LCD`)
- 10×5 keypad layout with 50 physical buttons
- Adafruit 258 style protected 3.7 V 1200 mAh JST-PH LiPo battery on the back of the PCB
- Back-side `2x6` user/debug header (`U9`) with `GND`, `3V3`, `5V/VBUS`, `VBAT`, `RESET/EN`, `BOOT/GPIO0`, UART, USB debug, `PWR_HOLD`, and spare GPIOs
- Software-off power behavior: the firmware turns off the display/backlight and enters ESP32-S3 deep sleep; do not design around a required firmware-controlled true power latch
- 16 MB flash ESP32-S3 module so firmware can use the 8 MB FAT storage partition
- EasyEDA Pro/JLCEDA project source for the next PCB revision

## EasyEDA Project Setup

The next PCB revision should be created in EasyEDA Pro/JLCEDA.

1. Create a new EasyEDA Pro/JLCEDA project named `OpenCalc PCB V3`.
2. Create schematic sheets for:
   - Core ESP32-S3
   - USB-C, charger, and low-quiescent power tree
   - LCD and touch
   - 10×5 button matrix
   - User/debug header and test pads
3. Use EasyEDA/LCSC parts for SMT assembly wherever possible.
4. Create custom footprints only where needed, especially for the MSP2807 LCD
   module outline/header and the battery keepout.
5. Keep the EasyEDA source files in this hardware folder when the design is
   ready.

Required EasyEDA/JLCPCB outputs:

| Output          | EasyEDA Path                            | Purpose                  |
| --------------- | --------------------------------------- | ------------------------ |
| Gerber zip      | Export > PCB Fabrication Files (Gerber) | PCB fabrication          |
| BOM             | Export > Bill of Materials (BOM)        | JLCPCB assembly          |
| Pick and place  | Document > Export Pick and Place File   | SMT placement            |
| 3D/STEP preview | 3D view > Export 3D File                | Mechanical fit check     |
| Schematic PDF   | Export > PDF/Image                      | Review and documentation |

EasyEDA design checks:

- Run Design Manager / DRC before exporting Gerbers.
- Check the 2D and 3D preview before ordering.
- Verify the BOM includes real LCSC/JLCPCB part numbers before assembly.
- Verify the MSP2807 display outline, 1x14 female connector orientation, and front-window
  alignment.
- Verify the battery fits on the back without vias, sharp leads, or tall parts
  underneath.
- Verify USB-C `D+`/`D-` route only to ESP32-S3 native USB pins.
- Verify `CC1` and `CC2` each have their own 5.1k pulldown to GND (R13 and R14).
- Verify the back-side `2x6` user/debug header (`U9`) includes `GND`, `3V3`,
  `5V/VBUS`, `VBAT`, `RESET/EN`, `BOOT/GPIO0`, UART, USB debug, `PWR_HOLD`,
  and the documented spare GPIO pins.
- Verify `EN/RESET` and `GPIO0/BOOT` are reachable on the header for recovery flashing.
- Verify every keypad switch has its own series diode so simultaneous key
  presses work without ghosting, especially for Doom movement plus fire.

## Core Components

| Qty | Component                                     | Designators | Package / Footprint                      | Example Part                   | Supplier Part                        |
| --: | --------------------------------------------- | ----------- | ---------------------------------------- | ------------------------------ | ------------------------------------ |
|   1 | ESP32-S3 module with 16 MB flash + 8 MB PSRAM | U1          | ESP32-S3-WROOM-1 module                  | ESP32-S3-WROOM-1-N16R8         | LCSC/JLC target C2913202             |
|   1 | 3.3 V buck regulator                          | U2          | TSOT-26 / W-DFN package per EasyEDA part | Diodes Inc. AP63203WU-7        | LCSC C780769                         |
|   1 | Buck regulator inductor                       | U3          | Per AP63203 reference design             | 4.7 µH shielded, 2 A or higher | Verify EasyEDA/JLC part before order |
|   1 | LiPo charger + power-path IC                  | U8          | VQFN-16 / RGT package                    | TI BQ24074RGTR                 | LCSC C54313                          |
|   1 | LCD backlight switch transistor               | Q1          | SOT-23                                   | 2N7002                         | JLCPCB C8545                         |
|   1 | USB ESD protection array                      | D1          | SC70-3 / SOT-5X3                         | TI TPD2E2U06DCKR               | Verify JLC/LCSC before order         |

## Display And Touch

| Qty | Component                             | Designators          | Package / Footprint                               | Example Part                                                                              | Supplier Part            |
| --: | ------------------------------------- | -------------------- | ------------------------------------------------- | ----------------------------------------------------------------------------------------- | ------------------------ |
|   1 | 2.8-inch SPI TFT touch display module | —                    | 50.0 x 86.0 mm module keepout/female-header mount | LCDWiki MSP2807                                                                           | External module          |
|   1 | LCD female connector                  | J_LCD                | 1x14 2.54 mm through-hole female header/socket    | Removable MSP2807 14-pin interface                                                        | External/mechanical part |
|   1 | ILI9341 LCD controller                | Integrated in module | On display module                                 | ILI9341                                                                                   | Included with LCD module |
|   1 | Touch interface                       | Integrated in module | On display module                                 | MSP2807 touch pins: T_CLK, T_CS, T_DIN, T_DO, T_IRQ                                       | Included with LCD module |
|   1 | Backlight control network             | Q1, R4, R6, R7       | SOT-23 + 0402/0603 resistors                      | 2N7002 with 33 Ω series resistor (R4), 10 kΩ gate pulldown (R6), 100 Ω gate resistor (R7) | Q1 JLCPCB C8545          |

Display placement requirements:

- Lay out the PCB against the LCDWiki MSP2807 module dimensions: 50.0 mm x 86.0 mm.
- Keep the display active area centered in the front opening.
- Use the MSP2807 pin labels in the schematic: `VCC`, `GND`, `CS`, `RESET`,
  `DC/RS`, `SDI(MOSI)`, `SCK`, `LED`, `SDO(MISO)`, `T_CLK`, `T_CS`, `T_DIN`,
  `T_DO`, and `T_IRQ`.
- Install a 1x14 2.54 mm female header/socket on the PCB so the MSP2807 module can plug in and be removed.
- The firmware currently expects LCD SPI and touch wiring on the pins listed in
  `hardware/pcb.md`; keep those net names identical.

## Buttons

| Qty | Component                                | Designators    | Package / Footprint      | Example Part                      | Supplier Part                 |
| --: | ---------------------------------------- | -------------- | ------------------------ | --------------------------------- | ----------------------------- |
|  50 | Momentary keypad switches in 10×5 matrix | SW1–SW50       | SMD 6.0 x 3.6 mm tactile | Korean Hroparts K2-1107ST-A4DW-06 | LCSC — verify before order    |
|  50 | Keypad isolation diodes                  | D_KEY1–D_KEY50 | SOD-323 / SOD-523        | 1N4148W / 1N4148WS or equivalent  | LCSC C2128 / verify footprint |

Notes:

- The target layout is 50 physical buttons total in a 10 row × 5 column scan matrix.
- The ON/HOME key is part of the 10×5 scan matrix. Firmware treats it as
  `ON (HOME)` while running and `2nd` + `ON` enters software off.
- Software off is ESP32-S3 deep sleep, not true rail cutoff. The PCB should
  minimize sleep current by using low-Iq regulators/charger parts, avoiding
  always-on indicator LEDs, and ensuring the LCD backlight is GPIO-switchable
  and defaults off.
- Keypad diodes are required for V3. The firmware supports multi-key scanning,
  but without one diode per key the hardware can ghost when multiple buttons are
  held.
- Diode direction for the current firmware scan: columns are pulled up and rows
  are driven low one at a time. Each key diode should conduct from the column
  side toward the active row side, so place the diode anode on the column side
  and the cathode/bar toward the row side.
- Include the ON/HOME key's matrix diode like every other key. Place it on the
  firmware wake column so the calculator can wake cleanly from software off.

## Connectors And External Interfaces

| Qty | Component                   | Designators | Package / Footprint                    | Example Part      | Supplier Part                                         |
| --: | --------------------------- | ----------- | -------------------------------------- | ----------------- | ----------------------------------------------------- |
|   1 | USB-C receptacle, USB 2.0   | S1          | USB-C 16-pin receptacle                | GCT USB4085-GF-A  | LCSC C7095263, confirm footprint before order         |
|   1 | LiPo battery connector      | Battery     | JST-PH 2-pin through-hole              | XY-B2B-PH-K-S     | Verify polarity matches selected battery before order |
|   1 | User/debug expansion header | U9          | 2x6 2.54 mm through-hole female header | 12-pin 2×6 header | DNP footprint, plated holes required                  |

## USB Polyfuses

| Qty | Component                        | Designators    | Package / Footprint  | Example Part        | Supplier Part                |
| --: | -------------------------------- | -------------- | -------------------- | ------------------- | ---------------------------- |
|   4 | Resettable polyfuse, 500 mA hold | U4, U5, U6, U7 | 1206 resettable fuse | Bourns MF-MSMF050-2 | Verify JLC/LCSC before order |

Note: The schematic places one polyfuse on each USB VBUS, D+, and D− line as individual protection elements (U4–U7).

## Battery

| Qty | Component              | Designators | Package / Footprint                             | Example Part                                       | Supplier Part    |
| --: | ---------------------- | ----------- | ----------------------------------------------- | -------------------------------------------------- | ---------------- |
|   1 | Protected LiPo battery | Battery     | Back-side keepout: 34 mm x 62 mm x 5 mm minimum | Adafruit Product 258, 3.7 V 1200 mAh JST-PH        | External battery |
|   1 | Battery mounting area  | BAT_KEEP    | Back-side mechanical keepout                    | 34 mm x 62 mm battery outline plus cable bend room | PCB keepout      |

Battery placement requirements:

- Put the battery on the back of the PCB.
- Reserve at least 34 mm x 62 mm x 5 mm, plus extra space for wire bend radius.
- Use the `XY-B2B-PH-K-S` footprint; verify polarity matches the selected battery before ordering.
- The selected battery includes protection circuitry; keep any additional PCB protection
  circuitry as DNP unless switching to an unprotected cell.
- Do not place sharp through-hole leads or tall back-side parts under the battery.

## Passive Components

### Capacitors

| Qty | Value  | Designators    | Package     | Purpose                                               |
| --: | ------ | -------------- | ----------- | ----------------------------------------------------- |
|   1 | 10 µF  | C1             | 0603 / 0805 | ESP32-S3 bulk decoupling                              |
|   4 | 100 nF | C3, C4, C5, C6 | 0402 / 0603 | ESP32-S3 local decoupling                             |
|   1 | 10 µF  | C8             | 0603 / 0805 | AP63203 / VSYS input bulk cap                         |
|   1 | 100 nF | C9             | 0402 / 0603 | VSYS rail decoupling                                  |
|   1 | 100 nF | C10            | 0402 / 0603 | 3V3 rail decoupling                                   |
|   1 | 22 µF  | C11            | 0805        | 3V3 rail bulk output cap                              |
|   1 | 100 nF | C12            | 0402 / 0603 | USB VBUS decoupling                                   |
|   1 | 10 µF  | C14            | 0603 / 0805 | VBAT rail bulk cap                                    |
|   1 | 100 nF | C15            | 0402 / 0603 | Battery voltage divider filter cap                    |
|   1 | 4.7 µF | C17            | 0603 / 0805 | Additional bulk decoupling (see schematic)            |
|   1 | 100 nF | C7             | 0402 / 0603 | RESET line debounce / decoupling                      |
|   1 | 1 µF   | C2             | 0402 / 0603 | CHG_STAT / CHG_PG signal filtering                    |
|   1 | 100 nF | C18            | 0402 / 0603 | Additional decoupling (verify placement in schematic) |

### Resistors

| Qty | Value  | Designators        | Package     | Purpose                                                                                 |
| --: | ------ | ------------------ | ----------- | --------------------------------------------------------------------------------------- |
|   1 | 100 kΩ | R3                 | 0402 / 0603 | AP63203 feedback / enable resistor                                                      |
|   1 | 33 Ω   | R4                 | 0402 / 0603 | Backlight LED series resistor                                                           |
|   2 | 5.1 kΩ | R13, R14           | 0402 / 0603 | USB-C CC1 and CC2 pulldowns                                                             |
|   1 | 10 kΩ  | R15                | 0402 / 0603 | BQ24074 TS resistor / thermistor reference                                              |
|   1 | 1.8 kΩ | R16                | 0402 / 0603 | BQ24074 ISET fast-charge current (~500 mA target)                                       |
|   1 | 10 kΩ  | R17                | 0402 / 0603 | BQ24074 EN1 pull-up                                                                     |
|   1 | 10 kΩ  | R18                | 0402 / 0603 | BQ24074 EN2 pull-up                                                                     |
|   1 | 1.8 kΩ | R19                | 0402 / 0603 | BQ24074 ITERM termination-current threshold                                             |
|   2 | 100 kΩ | R20, R21           | 0402 / 0603 | Battery voltage divider (50/50 ratio, ×2 scale factor)                                  |
|   1 | 100 Ω  | R22                | 0402 / 0603 | Additional signal resistor (verify net in schematic)                                    |
|   1 | 100 Ω  | R23                | 0402 / 0603 | Additional signal resistor (verify net in schematic)                                    |
|   1 | 10 kΩ  | R1                 | 0402 / 0603 | EN/RESET pull-up                                                                        |
|   1 | 10 kΩ  | R2                 | 0402 / 0603 | GPIO0/BOOT pull-up                                                                      |
|   1 | 10 kΩ  | R6                 | 0402 / 0603 | Backlight MOSFET gate pulldown                                                          |
|   1 | 100 Ω  | R7                 | 0402 / 0603 | Backlight MOSFET gate resistor                                                          |
|   5 | 10 kΩ  | COL0–COL4 pull-ups | 0402 / 0603 | Keypad column pull-ups (schematic shows external; DNP if using internal ESP32 pull-ups) |

### Inductors

| Qty | Value                          | Designators | Package                        | Purpose                 |
| --: | ------------------------------ | ----------- | ------------------------------ | ----------------------- |
|   1 | 4.7 µH shielded, 2 A or higher | U3          | Match AP63203 reference design | Buck regulator inductor |

## User And Debug Header

The V3 schematic uses a `2x6` (12-pin) female header (`U9`) for user-accessible GPIO and recovery/debug access. Pin assignments from the schematic:

| Pin | Net      | Purpose / Notes                                                                                  |
| --: | -------- | ------------------------------------------------------------------------------------------------ |
|   1 | GND      | Ground reference                                                                                 |
|   2 | 3V3      | Regulated 3.3 V rail                                                                             |
|   3 | VBAT     | Raw LiPo battery rail                                                                            |
|   4 | USB_VBUS | USB-C VBUS when plugged in                                                                       |
|   5 | USB_DP   | Native USB D+ — debug/bring-up only, not general GPIO                                            |
|   6 | USB_DM   | Native USB D− — debug/bring-up only, not general GPIO                                            |
|   7 | RESET    | Pull to GND to reset the ESP32-S3                                                                |
|   8 | BOOT     | Pull to GND during reset to enter ROM bootloader                                                 |
|   9 | UART_TX  | ESP-IDF console UART TX (GPIO43)                                                                 |
|  10 | UART_RX  | ESP-IDF console UART RX (GPIO44)                                                                 |
|  11 | PWR_HOLD | Power hold net — not driven by current software-off firmware; reserved for future hardware latch |
|  12 | GND      | Ground reference                                                                                 |

Spare / extra GPIO nets (`EXTERA_1` through `EXTERA_5`) are also broken out in the schematic for additional expansion if needed.

Header requirements:

- `EN/RESET` must keep its normal pull-up/reset network (R1 + C7); the header only exposes the net.
- `GPIO0/BOOT` must keep its normal pull-up (R2); the header only exposes the net.
- Do not use `USB D+` or `USB D-` as general GPIO. They are included only for bring-up/debug access.
- Do not place tall header pins under the battery.

## Additional Test Pads

The schematic includes labeled test points for the following nets. Place them on the back side near the relevant power or signal section:

| Net        | Purpose                       |
| ---------- | ----------------------------- |
| `3V3`      | Regulated 3.3 V probe pad     |
| `GND`      | Ground probe pad              |
| `VBAT`     | Battery rail probe pad        |
| `USB_VBUS` | USB-C input voltage probe pad |
| `USB_DP`   | USB D+ signal probe pad       |
| `USB_DM`   | USB D− signal probe pad       |
| `RESET`    | Reset net probe pad           |
| `BOOT`     | Boot net probe pad            |
| `UART_TX`  | Console TX probe pad          |
| `UART_RX`  | Console RX probe pad          |
| `PWR_HOLD` | Power hold net probe pad      |

## Mechanical And Assembly

| Qty | Component            | Designators | Notes                                                                      |
| --: | -------------------- | ----------- | -------------------------------------------------------------------------- |
|   1 | PCB                  | PCB1        | Calculator main board                                                      |
|   1 | LiPo battery         | Battery     | Adafruit 258 style 34 x 62 x 5 mm protected JST-PH pack on back            |
|   1 | LCD                  | J_LCD       | LCDWiki MSP2807 module plugged into 1x14 2.54 mm female connector on front |
|   4 | Mounting holes       | MH1–MH4     | M2.5 clearance holes, adjust only if final case requires it                |
|   1 | Case / front shell   | CASE1       | Not part of PCB assembly BOM                                               |
|   1 | Button mat / keycaps | KEYMAT1     | Align with 10×5 graphic design                                             |

## Open Items Before Ordering

- Confirm R22 and R23 net assignments in schematic (currently showing as 100 Ω but purpose unclear from PDF extraction).
- Confirm C18 placement and purpose in the final schematic.
- Confirm `EXTERA_1` through `EXTERA_5` GPIO assignments and document them.
- `PWR_HOLD` net is present in the schematic and on the debug header (pin 11) but is not driven by the current software-off firmware — document the intended hardware behavior before adding a latch circuit.
- Confirm regulator current headroom for ESP32-S3, LCD logic, and backlight.
- Confirm the ON/HOME key is in the scan matrix, has its diode, and is on the firmware wake column for software-off wake.
- Confirm measured software-off current with the display/backlight off, ESP32-S3 in deep sleep, no charger LEDs populated, and low-Iq power parts selected.
- Confirm final EasyEDA/JLC footprints and distributor availability for the listed target parts before ordering.
- Verify battery connector (`XY-B2B-PH-K-S`) polarity matches the selected LiPo pack before ordering.

## Reference Parts

- Display: LCDWiki MSP2807, 2.8-inch SPI ILI9341 touch module:
  https://www.lcdwiki.com/index.php?title=2.8inch_SPI_Module_ILI9341_SKU:MSP2807
- Battery: Adafruit Product 258, protected 3.7 V 1200 mAh JST-PH LiPo:
  https://www.adafruit.com/product/258
- EasyEDA Pro/JLCEDA editor:
  https://pro.easyeda.com/editor
- EasyEDA/JLCPCB exports needed: Gerber, BOM, and pick-and-place.
