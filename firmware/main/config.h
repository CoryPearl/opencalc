#pragma once

/*
 * OpenCalc feature toggles.
 *
 * Set these to 1 or 0 before building.
 */
#define OPENCALC_ENABLE_BLUETOOTH 0
#define OPENCALC_ENABLE_WIFI 0
#define OPENCALC_ENABLE_DOOM 1
#define OPENCALC_ENABLE_GIAC_CAS 1 // Giac/KhiCAS symbolic backend; Eigenmath remains the fallback
#define OPENCALC_ENABLE_AUTOMATED_TESTS 0 // Master gate: 0 prevents every boot-time automated test
#define OPENCALC_GIAC_BOOT_SELF_TEST 0 // Set to 1 temporarily for a serial-logged CAS hardware smoke test
#define OPENCALC_MATRIX_BOOT_STRESS_TEST 0 // Set to 1 temporarily for destructive 99x99 PSRAM/operation checks
#define OPENCALC_DEVICE_SELF_TEST 0 // Set to 1 for boot-time CAS/script/storage/sleep/keypad/game/profile diagnostics
#define OPENCALC_ENABLE_GAME_AUDIO 0 // Master switch for game sound
#define OPENCALC_USE_NEW_AUDIO_PCB 0 // 0 = old PCB; 1 = write-only LCD + PAM8302A audio PCB
#define OPENCALC_ENABLE_USB_CDC_CONSOLE 1 // Same USB-C cable shows storage + serial monitor
#define OPENCALC_ENABLE_POWER_STATUS_LED 0 // Active-high LED: legacy GPIO21 or scientific-I/O MCP23017 GPA1
#define OPENCALC_ENABLE_SCIENTIFIC_IO 0 // ADS1115 + MCP23017 sensor header on the new PCB
#define OPENCALC_USE_AO3401A_BACKLIGHT 0 // 0 = old TPS22918 active-high; 1 = new AO3401A active-low
#define OPENCALC_EXPORT_USB_STORAGE_TO_HOST 0 // 0 keeps /data mounted for app/game testing; 1 shows flash drive on the laptop
#define OPENCALC_USB_CDC_STARTUP_BANNER_DELAY_MS 100
#define OPENCALC_USB_OWNERSHIP_TIMEOUT_MS 3000UL // Wait for TinyUSB's asynchronous MSC mount-complete event
#define OPENCALC_DEBUG_LOG_KEYPAD_PRESSES 0 // Print physical keypad presses to USB serial
#define OPENCALC_DEBUG_LOG_FPS 0 // Print UI/game tick FPS to USB serial once per second
#define OPENCALC_DEBUG_TETRIS_HEALTH 0 // Log Tetris heap/stack health every 5 seconds
#define OPENCALC_DEBUG_MEMORY_HEALTH 0 // Check all heaps and log UI heap/stack health every 5 seconds
#define OPENCALC_KEYPAD_POLL_WHEN_NO_INTERRUPT 0 // Use row GPIO interrupts; only scan after a key edge
#define OPENCALC_DEBUG_LOG_RAW_KEYPAD_LEVELS 0 // Print raw row/column GPIO levels during keypad bring-up
#define OPENCALC_ENABLE_SERIAL_BUTTON_INPUT 0 // For debugging without having to use a physical button matrix
#define OPENCALC_FLASH_STORAGE_IMAGE 1 // Flash firmware/storage_image into the storage partition

/*
 * New PCB game audio.
 *
 * Scientific-I/O PCB: MCP23017 GPA0 -> PAM8302A SD
 * Legacy new-audio PCB: GPIO13 -> PAM8302A SD
 * GPIO41 -> I2S PDM audio output -> low-pass filter -> AC-coupled PAM8302A input
 *
 * The ESP32-S3 has no analog DAC. GPIO41 therefore emits a high-rate PDM bit
 * stream, not analog audio. The PCB must low-pass filter it before the amp.
 * GPIO13 was LCD MISO and GPIO41 was CHG_STAT on the old PCB; selecting the
 * new PCB makes the LCD write-only. Scientific I/O repurposes GPIO13 as SDA.
 */
#define OPENCALC_AUDIO_SAMPLE_RATE 16000
#define OPENCALC_AUDIO_VOLUME_PERCENT 45
#define OPENCALC_GAME_AUDIO_ENABLED \
    (OPENCALC_ENABLE_GAME_AUDIO && OPENCALC_USE_REAL_PCB && OPENCALC_USE_NEW_AUDIO_PCB)

#if OPENCALC_ENABLE_GAME_AUDIO != 0 && OPENCALC_ENABLE_GAME_AUDIO != 1
#error "OPENCALC_ENABLE_GAME_AUDIO must be 0 or 1"
#endif

/*
 * Scientific sensor header.
 *
 * The former AUDIO_SD/PWR_LED ESP pins, GPIO13/21, host a 400 kHz I2C bus
 * shared by the MCP23017 (0x20), ADS1115 (0x48), and external I2C sensors.
 * MCP23017 outputs return AUDIO_SD and PWR_LED control to those circuits.
 * The N16R8 module consumes GPIO35..37 for octal memory, while GPIO46 is an
 * input-only strapping pin, so this revision intentionally exposes no UART.
 *
 * MCP23017 GPA0 and GPA1 are reserved for amplifier shutdown and the awake
 * power LED. GPA7/GPB7 are intentionally unused by the board design; the
 * remaining twelve bidirectional pins are exposed as D0..D11.
 */
#define OPENCALC_SENSOR_I2C_SDA_GPIO 13
#define OPENCALC_SENSOR_I2C_SCL_GPIO 21
#define OPENCALC_SENSOR_I2C_FREQUENCY_HZ 400000
#define OPENCALC_MCP23017_I2C_ADDRESS 0x20
#define OPENCALC_ADS1115_I2C_ADDRESS 0x48
#define OPENCALC_ADS1115_DEFAULT_RATE_HZ 128

#if OPENCALC_ENABLE_SCIENTIFIC_IO != 0 && OPENCALC_ENABLE_SCIENTIFIC_IO != 1
#error "OPENCALC_ENABLE_SCIENTIFIC_IO must be 0 or 1"
#endif

#if OPENCALC_ENABLE_SCIENTIFIC_IO && !OPENCALC_USE_NEW_AUDIO_PCB
#error "Scientific I/O needs the write-only LCD/new-audio PCB pin map"
#endif

#if OPENCALC_USE_NEW_AUDIO_PCB != 0 && OPENCALC_USE_NEW_AUDIO_PCB != 1
#error "OPENCALC_USE_NEW_AUDIO_PCB must be 0 or 1"
#endif

#if OPENCALC_AUDIO_SAMPLE_RATE < 8000 || OPENCALC_AUDIO_SAMPLE_RATE > 48000
#error "OPENCALC_AUDIO_SAMPLE_RATE must be 8000..48000"
#endif

#if OPENCALC_AUDIO_VOLUME_PERCENT < 0 || OPENCALC_AUDIO_VOLUME_PERCENT > 100
#error "OPENCALC_AUDIO_VOLUME_PERCENT must be 0..100"
#endif

/*
 * Dual-core task layout.
 *
 * UI/game drawing runs on core 1. Setup, USB/storage, serial simulation, and
 * future math worker jobs stay on core 0. Power-save mode lowers frequency and
 * brightness, but does not try to shut a core off at runtime.
 */
#define OPENCALC_UI_CORE 1
#define OPENCALC_WORKER_CORE 0
#define OPENCALC_UI_TASK_STACK 20480
#define OPENCALC_MATH_WORKER_TASK_STACK 24576 // PSRAM-backed; TCB and queues remain in internal RAM
#define OPENCALC_GIAC_TASK_STACK 65536 // Allocated from PSRAM, not scarce internal DRAM
#define OPENCALC_CAS_TIMEOUT_MS 10000UL // Keep the UI responsive if Giac stalls or exhausts memory
#define OPENCALC_CAS_CANCEL_POLL_MS 20UL
#define OPENCALC_CAS_RECOVERY_GRACE_MS 1500UL // Allow Giac's cooperative abort checks to unwind and recycle its context
#define OPENCALC_SCRIPT_TASK_STACK 32768 // Reserved early; parser temporaries are split to keep safe input/recursion margin
#define OPENCALC_SCRIPT_TASK_STACK_MIN 32768 // FATFS forbids using a PSRAM-backed task stack here
#define OPENCALC_SCRIPT_STATEMENT_LIMIT 250000UL
#define OPENCALC_SCRIPT_CALL_DEPTH_LIMIT 8UL
#define OPENCALC_SCRIPT_TIMEOUT_MS 30000UL
#define OPENCALC_INTERNAL_HEAP_RESERVE_BYTES (16U * 1024U)
#define OPENCALC_PSRAM_RESERVE_BYTES (512U * 1024U)

#if OPENCALC_ENABLE_GIAC_CAS != 0 && OPENCALC_ENABLE_GIAC_CAS != 1
#error "OPENCALC_ENABLE_GIAC_CAS must be 0 or 1"
#endif

#if OPENCALC_ENABLE_AUTOMATED_TESTS != 0 && OPENCALC_ENABLE_AUTOMATED_TESTS != 1
#error "OPENCALC_ENABLE_AUTOMATED_TESTS must be 0 or 1"
#endif

#if OPENCALC_GIAC_BOOT_SELF_TEST != 0 && OPENCALC_GIAC_BOOT_SELF_TEST != 1
#error "OPENCALC_GIAC_BOOT_SELF_TEST must be 0 or 1"
#endif

#if OPENCALC_MATRIX_BOOT_STRESS_TEST != 0 && OPENCALC_MATRIX_BOOT_STRESS_TEST != 1
#error "OPENCALC_MATRIX_BOOT_STRESS_TEST must be 0 or 1"
#endif

#if OPENCALC_DEVICE_SELF_TEST != 0 && OPENCALC_DEVICE_SELF_TEST != 1
#error "OPENCALC_DEVICE_SELF_TEST must be 0 or 1"
#endif

#if OPENCALC_ENABLE_GIAC_CAS && OPENCALC_GIAC_TASK_STACK < 49152
#error "OPENCALC_GIAC_TASK_STACK is too small for reliable Giac evaluation"
#endif

#if OPENCALC_CAS_TIMEOUT_MS < 250 || OPENCALC_CAS_CANCEL_POLL_MS < 1 || OPENCALC_CAS_CANCEL_POLL_MS > OPENCALC_CAS_TIMEOUT_MS || OPENCALC_CAS_RECOVERY_GRACE_MS < OPENCALC_CAS_CANCEL_POLL_MS
#error "OpenCalc CAS timeout settings are invalid"
#endif

#if OPENCALC_MATH_WORKER_TASK_STACK < 16384
#error "OPENCALC_MATH_WORKER_TASK_STACK is too small for calculator and graph jobs"
#endif

#if OPENCALC_SCRIPT_TASK_STACK < OPENCALC_SCRIPT_TASK_STACK_MIN || OPENCALC_SCRIPT_TASK_STACK_MIN < 32768
#error "OpenCalc script worker stack settings are too small"
#endif

#if OPENCALC_SCRIPT_STATEMENT_LIMIT < 1000 || OPENCALC_SCRIPT_CALL_DEPTH_LIMIT < 1 || OPENCALC_SCRIPT_TIMEOUT_MS < 1000
#error "OpenCalc script sandbox limits are too small"
#endif

/*
 * Global UI/game pacing.
 *
 * This caps the main display/game tick loop. Event-driven screens can still
 * redraw immediately after a key press, but continuous animation/game drawing
 * uses this target.
 */
#define OPENCALC_TARGET_FPS 45

/* Performance settings used outside power-save mode. */
#define OPENCALC_CPU_MAX_MHZ 240
#define OPENCALC_LCD_SPI_CLOCK_MHZ 60

/*
 * Power-save behavior.
 *
 * When Settings -> Power save is enabled, CPU max frequency is lowered and
 * brightness is capped at this percentage. Use ESP-IDF-supported CPU values
 * for the ESP32-S3, typically 80, 160, or 240 MHz depending on sdkconfig.
 */
#define OPENCALC_POWER_SAVE_CPU_MAX_MHZ 160
#define OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT 35

#if OPENCALC_TARGET_FPS < 1
#error "OPENCALC_TARGET_FPS must be at least 1"
#endif

#if OPENCALC_CPU_MAX_MHZ != 80 && OPENCALC_CPU_MAX_MHZ != 160 && OPENCALC_CPU_MAX_MHZ != 240
#error "OPENCALC_CPU_MAX_MHZ should be 80, 160, or 240"
#endif

#if OPENCALC_LCD_SPI_CLOCK_MHZ < 10 || OPENCALC_LCD_SPI_CLOCK_MHZ > 80
#error "OPENCALC_LCD_SPI_CLOCK_MHZ must be 10..80"
#endif

#if OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT < 1 || OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT > 100
#error "OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT must be 1..100"
#endif

#if OPENCALC_POWER_SAVE_CPU_MAX_MHZ != 80 && OPENCALC_POWER_SAVE_CPU_MAX_MHZ != 160 && OPENCALC_POWER_SAVE_CPU_MAX_MHZ != 240
#error "OPENCALC_POWER_SAVE_CPU_MAX_MHZ should be 80, 160, or 240"
#endif

/*
 * Battery monitor.
 *
 * VBAT_DIV is a resistor divider into GPIO7 / ADC1 channel 6. The current PCB
 * uses 1M over 1M to reduce sleep drain. Keep the 100nF capacitor on the ADC
 * node so the high-value divider can settle before ADC reads.
 */
#define OPENCALC_BATTERY_DIVIDER_R_TOP_OHMS 1000000
#define OPENCALC_BATTERY_DIVIDER_R_BOTTOM_OHMS 1000000
#define OPENCALC_BATTERY_MIN_VALID_MV 2500
#define OPENCALC_BATTERY_MAX_VALID_MV 4600
#define OPENCALC_BATTERY_EMPTY_MV 3300
#define OPENCALC_BATTERY_FULL_MV 4200

#if OPENCALC_BATTERY_DIVIDER_R_TOP_OHMS <= 0 || OPENCALC_BATTERY_DIVIDER_R_BOTTOM_OHMS <= 0
#error "Battery divider resistor values must be positive"
#endif

#if OPENCALC_BATTERY_EMPTY_MV >= OPENCALC_BATTERY_FULL_MV
#error "OPENCALC_BATTERY_EMPTY_MV must be lower than OPENCALC_BATTERY_FULL_MV"
#endif

/*
 * The PCB keypad uses one isolation diode in series with every switch.
 * Rows are read with pullups and columns are scanned low, so the diode/button
 * path must let a pressed key pull its row down through the active column.
 */
#define OPENCALC_KEYPAD_HAS_PER_KEY_DIODES 1
#define OPENCALC_KEYPAD_ROW_SETTLE_US 5

/*
 * Hardware switch.
 *
 * 0 = testing/dev board:
 *     LCD + USB/storage + serial button input only. Does not touch PCB-only
 *     keypad, battery, charge-status, or backlight GPIOs.
 *
 * 1 = real OpenCalc PCB:
 *     Enables the full keypad matrix, battery monitor, charge indicator,
 *     backlight PWM, and ON/HOME wake path.
 */
#define OPENCALC_USE_REAL_PCB 1

#if OPENCALC_ENABLE_SCIENTIFIC_IO && !OPENCALC_USE_REAL_PCB
#error "Scientific I/O requires OPENCALC_USE_REAL_PCB"
#endif

#define OPENCALC_HARDWARE_PROFILE_DEV_BOARD 0
#define OPENCALC_HARDWARE_PROFILE_PCB_V3 1

#if OPENCALC_USE_REAL_PCB
#define OPENCALC_HARDWARE_PROFILE OPENCALC_HARDWARE_PROFILE_PCB_V3
#define OPENCALC_WAKE_FROM_KEYPAD_IN_SOFTWARE_OFF 1
#else
#define OPENCALC_HARDWARE_PROFILE OPENCALC_HARDWARE_PROFILE_DEV_BOARD
#define OPENCALC_WAKE_FROM_KEYPAD_IN_SOFTWARE_OFF 0
#endif

/*
 * Flash partition sizes.
 *
 * These are read by the top-level CMakeLists.txt before ESP-IDF loads the
 * partition table. Change these, then run `idf.py build` or `idf.py flash`.
 *
 * Current target module: ESP32-S3 with 16 MB flash.
 */
#define OPENCALC_FACTORY_APP_PARTITION_SIZE 0x600000 // 6 MB
#define OPENCALC_STORAGE_PARTITION_SIZE 0x800000 // 8 MB

/*
 * Power behavior is software-off only.
 *
 * 1 = fast software-off: use light sleep, keep RAM/tasks alive, turn the LCD
 *     panel and backlight off, and resume quickly from ON/HOME.
 *
 * 0 = lowest software-off drain: use deep sleep. This fully reboots on wake,
 *     so startup is slower because bootloader, PSRAM, USB, LCD, and UI init run
 *     again.
 *
 * Hardware latch cutoff is not used.
 */
#define OPENCALC_SOFTWARE_OFF_USE_LIGHT_SLEEP 1
