#pragma once

/*
 * OpenCalc feature toggles.
 *
 * Set these to 1 or 0 before building.
 */
#define OPENCALC_ENABLE_BLUETOOTH 0
#define OPENCALC_ENABLE_WIFI 0
#define OPENCALC_ENABLE_DOOM 1
#define OPENCALC_ENABLE_SERIAL_BUTTON_INPUT 1 // For debugging without having to use a physical button matrix

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
#define OPENCALC_USE_REAL_PCB 0

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
