#pragma once

/*
 * OpenCalc feature toggles.
 *
 * Set these to 1 or 0 before building.
 */
#define OPENCALC_ENABLE_BLUETOOTH 0
#define OPENCALC_ENABLE_WIFI 0
#define OPENCALC_ENABLE_DOOM 1
#define OPENCALC_ENABLE_SERIAL_BUTTON_INPUT 0 // For debugging without having to use a physical button matrix
#define OPENCALC_FLASH_STORAGE_IMAGE 1 // Flash firmware/storage_image into the storage partition

/*
 * Dual-core task layout.
 *
 * UI/game drawing runs on core 1. Setup, USB/storage, serial simulation, and
 * future math worker jobs stay on core 0. Power-save mode lowers frequency and
 * brightness, but does not try to shut a core off at runtime.
 */
#define OPENCALC_UI_CORE 1
#define OPENCALC_WORKER_CORE 0
#define OPENCALC_UI_TASK_STACK 32768

/*
 * Global UI/game pacing.
 *
 * This caps the main display/game tick loop. Event-driven screens can still
 * redraw immediately after a key press, but continuous animation/game drawing
 * uses this target.
 */
#define OPENCALC_TARGET_FPS 45

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

#if OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT < 1 || OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT > 100
#error "OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT must be 1..100"
#endif

#if OPENCALC_POWER_SAVE_CPU_MAX_MHZ != 80 && OPENCALC_POWER_SAVE_CPU_MAX_MHZ != 160 && OPENCALC_POWER_SAVE_CPU_MAX_MHZ != 240
#error "OPENCALC_POWER_SAVE_CPU_MAX_MHZ should be 80, 160, or 240"
#endif

/*
 * The PCB keypad uses one isolation diode in series with every switch.
 * Columns are pulled high and rows are scanned low, so each diode must have
 * its anode toward COL0..COL4 and its cathode/bar toward ROW0..ROW9.
 */
#define OPENCALC_KEYPAD_HAS_PER_KEY_DIODES 1
#define OPENCALC_KEYPAD_ROW_SETTLE_US 5

/*
 * Hardware switch.
 *
 * 0 = testing/dev board:
 *     LCD + USB/storage + serial button input only. Does not touch PCB-only
 *     keypad, touch, battery, charge-status, or backlight GPIOs.
 *
 * 1 = real OpenCalc PCB:
 *     Enables the full keypad matrix, touch controller, battery monitor,
 *     charge indicator, backlight PWM, and ON/HOME wake path.
 */
#define OPENCALC_USE_REAL_PCB 1

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
#define OPENCALC_FACTORY_APP_PARTITION_SIZE 0x400000 // 4mb
#define OPENCALC_STORAGE_PARTITION_SIZE 0x800000 // 8mb

/*
 * Power behavior is software-off only: the firmware enters ESP32-S3 deep sleep
 * and can wake from the ON key path when OPENCALC_WAKE_FROM_KEYPAD_IN_SOFTWARE_OFF
 * is enabled. Hardware latch cutoff is not used.
 */
