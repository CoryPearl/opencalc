/**
 * board_init.c
 *
 * ILI9341 SPI LCD initialisation for ESP32-S3.
 * And button matrix initilization
 */

#include "board_init.h"
#include "opencalc_config.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_sleep.h"
#include "esp_heap_caps.h"

#include "esp_lcd_ili9341.h"
#include "freertos/semphr.h"

#include <string.h>

static const char *TAG = "board_init";
static TaskHandle_t s_event_task = NULL;
static SemaphoreHandle_t s_display_mutex = NULL;
static volatile bool s_keypad_interrupt_pending = false;
static volatile bool s_touch_interrupt_pending = false;
static bool s_gpio_isr_service_ready = false;
static int s_backlight_brightness = 80;
static adc_oneshot_unit_handle_t s_battery_adc_unit = NULL;
static adc_cali_handle_t s_battery_adc_cali = NULL;
static bool s_battery_adc_ready = false;
static bool s_battery_adc_calibrated = false;

static void log_wakeup_reason(void)
{
    uint32_t causes = esp_sleep_get_wakeup_causes();
    if (causes == 0) {
        ESP_LOGI(TAG, "Wake reason: power-on/reset");
        return;
    }

    ESP_LOGI(TAG, "Wake reason mask: 0x%08" PRIx32, causes);
    if (causes & BIT(ESP_SLEEP_WAKEUP_EXT1)) {
        ESP_LOGI(TAG, "EXT1 wake status: 0x%016llx", esp_sleep_get_ext1_wakeup_status());
    }
}

/* ── Pin map ─────────────────────────────────────────────────── */
// Screen.
//
// OpenCalc V3 EasyEDA schematic wiring.
// Source: hardware/pcb/V3/SCH_Schematic1_1-P1_2026-06-03.png
//
// ESP32-S3 module nets:
//   ROW0..ROW9          -> GPIO1,2,42,4,5,6,48,8,9,16
//   COL0..COL4          -> GPIO17,18,38,39,40, pulled up to 3V3
//   USB_DM / USB_DP     -> GPIO19 / GPIO20
//   UART_TX / UART_RX   -> TXD0 / RXD0
//   VBAT_DIV            -> GPIO7 / ADC1 channel 6
//   CHG_STAT            -> GPIO41, active low from BQ24074 CHG#
//   LCD_BACKL           -> GPIO47, drives TPS22918 ON pin
//
// LCD touch is not wired on this PCB revision, so ENABLE_TOUCH_INPUT is 0.
//
//   VCC                  -> 3V3
//   GND                  -> GND
//   LCD_CS               -> GPIO10
//   LCD_RST              -> GPIO15
//   LCD_DC / RS          -> GPIO14
//   LCD_SDI / LCD_MOSI   -> GPIO11
//   LCD_SCK / LCD_SCLK   -> GPIO12
//   LCD_LED / backlight  -> TPS22918 load-switch output through 33 ohm.
//                            TPS22918 ON/EN is driven by GPIO47.
//                            Do not feed the LCD LED pin with 5V.
//   LCD_SDO / LCD_MISO   -> GPIO13
//   Touch pins           -> unwired
#ifndef OPENCALC_HARDWARE_PROFILE_DEV_BOARD
#define OPENCALC_HARDWARE_PROFILE_DEV_BOARD 0
#endif
#ifndef OPENCALC_HARDWARE_PROFILE_PCB_V3
#define OPENCALC_HARDWARE_PROFILE_PCB_V3 1
#endif
#ifndef OPENCALC_HARDWARE_PROFILE
#define OPENCALC_HARDWARE_PROFILE OPENCALC_HARDWARE_PROFILE_DEV_BOARD
#endif

#define OPENCALC_HARDWARE_IS_PCB_V3 (OPENCALC_HARDWARE_PROFILE == OPENCALC_HARDWARE_PROFILE_PCB_V3)

#define ENABLE_KEYPAD_MATRIX_INPUT OPENCALC_HARDWARE_IS_PCB_V3
#ifndef OPENCALC_KEYPAD_HAS_PER_KEY_DIODES
#define OPENCALC_KEYPAD_HAS_PER_KEY_DIODES 0
#endif
#ifndef OPENCALC_KEYPAD_ROW_SETTLE_US
#define OPENCALC_KEYPAD_ROW_SETTLE_US 5
#endif
#define ENABLE_TOUCH_INPUT 0
#define PIN_NUM_LCD_SCLK 12
#define PIN_NUM_LCD_MOSI 11
#define PIN_NUM_LCD_MISO 13
#define PIN_NUM_LCD_DC   14
#define PIN_NUM_LCD_RST  15
#define PIN_NUM_LCD_CS   10
#if OPENCALC_HARDWARE_IS_PCB_V3
#define PIN_NUM_LCD_BCKL GPIO_NUM_47
#else
#define PIN_NUM_LCD_BCKL GPIO_NUM_NC
#endif
#define PIN_NUM_TOUCH_CLK GPIO_NUM_NC
#define PIN_NUM_TOUCH_DIN GPIO_NUM_NC
#define PIN_NUM_TOUCH_DO  GPIO_NUM_NC
#define PIN_NUM_TOUCH_CS  GPIO_NUM_NC
#define PIN_NUM_TOUCH_IRQ GPIO_NUM_NC
#define ONBOARD_RGB_LED_ENABLED 0
#define PIN_NUM_ONBOARD_RGB_LED GPIO_NUM_NC
#define LCD_BCKL_LEDC_MODE LEDC_LOW_SPEED_MODE
#define LCD_BCKL_LEDC_TIMER LEDC_TIMER_0
#define LCD_BCKL_LEDC_CHANNEL LEDC_CHANNEL_0
#define LCD_BCKL_LEDC_RES LEDC_TIMER_10_BIT
#define LCD_BCKL_LEDC_MAX_DUTY 1023
#define POWER_BUTTON_ENABLED 0
#define PIN_NUM_POWER_BUTTON GPIO_NUM_NC
#define POWER_HOLD_ENABLED 0
#define PIN_NUM_POWER_HOLD GPIO_NUM_NC
#define BATTERY_MONITOR_ENABLED OPENCALC_HARDWARE_IS_PCB_V3
#define BATTERY_ADC_UNIT ADC_UNIT_1
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_6
#define PIN_NUM_BATTERY_ADC GPIO_NUM_7
#define BATTERY_ADC_ATTEN ADC_ATTEN_DB_12
#define BATTERY_DIVIDER_R_TOP_OHMS 100000.0f
#define BATTERY_DIVIDER_R_BOTTOM_OHMS 100000.0f
#define BATTERY_EMPTY_MV 3300
#define BATTERY_FULL_MV 4200
#define BATTERY_CHARGE_STATUS_ENABLED 0
#define PIN_NUM_BATTERY_CHARGE_STATUS GPIO_NUM_41
#define BATTERY_CHARGE_STATUS_ACTIVE_LOW 1
 
// Button matrix
#define PIN_NUM_KEYPAD_ROW0 GPIO_NUM_1
#define PIN_NUM_KEYPAD_ROW1 GPIO_NUM_2
#define PIN_NUM_KEYPAD_ROW2 GPIO_NUM_42
#define PIN_NUM_KEYPAD_ROW3 GPIO_NUM_4
#define PIN_NUM_KEYPAD_ROW4 GPIO_NUM_5
#define PIN_NUM_KEYPAD_ROW5 GPIO_NUM_6
#define PIN_NUM_KEYPAD_ROW6 GPIO_NUM_48
#define PIN_NUM_KEYPAD_ROW7 GPIO_NUM_8
#define PIN_NUM_KEYPAD_ROW8 GPIO_NUM_9
#define PIN_NUM_KEYPAD_ROW9 GPIO_NUM_16

#define PIN_NUM_KEYPAD_COL0 GPIO_NUM_17
#define PIN_NUM_KEYPAD_COL1 GPIO_NUM_18
#define PIN_NUM_KEYPAD_COL2 GPIO_NUM_38
#define PIN_NUM_KEYPAD_COL3 GPIO_NUM_39
#define PIN_NUM_KEYPAD_COL4 GPIO_NUM_40
static const gpio_num_t KEYPAD_ROW_PINS[BOARD_KEYPAD_ROWS] = {
    PIN_NUM_KEYPAD_ROW0,
    PIN_NUM_KEYPAD_ROW1,
    PIN_NUM_KEYPAD_ROW2,
    PIN_NUM_KEYPAD_ROW3,
    PIN_NUM_KEYPAD_ROW4,
    PIN_NUM_KEYPAD_ROW5,
    PIN_NUM_KEYPAD_ROW6,
    PIN_NUM_KEYPAD_ROW7,
    PIN_NUM_KEYPAD_ROW8,
    PIN_NUM_KEYPAD_ROW9,
};

static const gpio_num_t KEYPAD_COL_PINS[BOARD_KEYPAD_COLS] = {
    PIN_NUM_KEYPAD_COL0,
    PIN_NUM_KEYPAD_COL1,
    PIN_NUM_KEYPAD_COL2,
    PIN_NUM_KEYPAD_COL3,
    PIN_NUM_KEYPAD_COL4,
};

static const board_key_t KEYPAD_MAP[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS] = {
    {
        {"y=", "plot", NULL, BOARD_KEY_ROLE_NONE},
        {"window", "tblset", NULL, BOARD_KEY_ROLE_NONE},
        {"zoom", "format", NULL, BOARD_KEY_ROLE_NONE},
        {"trace", "calc", NULL, BOARD_KEY_ROLE_NONE},
        {"graph", "table", NULL, BOARD_KEY_ROLE_NONE},
    },
    {
        {"2nd", NULL, NULL, BOARD_KEY_ROLE_NONE},
        {"mode", "quit", NULL, BOARD_KEY_ROLE_NONE},
        {"stat", "list/distr", NULL, BOARD_KEY_ROLE_NONE},
        {"left", NULL, NULL, BOARD_KEY_ROLE_NONE},
        {"up", "brightness up", NULL, BOARD_KEY_ROLE_NONE},
    },
    {
        {"alpha", "alpha lock", NULL, BOARD_KEY_ROLE_NONE},
        {"XthetaTn", NULL, NULL, BOARD_KEY_ROLE_NONE},
        {"back", NULL, NULL, BOARD_KEY_ROLE_NONE},
        {"down", "brightness down", NULL, BOARD_KEY_ROLE_NONE},
        {"right", NULL, NULL, BOARD_KEY_ROLE_NONE},
    },
    {
        {"math", "= != < > <= >=", "A", BOARD_KEY_ROLE_NONE},
        {"[]/[]", "frac/dec", "B", BOARD_KEY_ROLE_NONE},
        {"prgm", "scripts", "C", BOARD_KEY_ROLE_NONE},
        {"vars", "convert", "D", BOARD_KEY_ROLE_NONE},
        {"del", "clear", "E", BOARD_KEY_ROLE_NONE},
    },
    {
        {"sqrt", "root[]", "F", BOARD_KEY_ROLE_NONE},
        {"sin", "sin-1", "csc(", BOARD_KEY_ROLE_NONE},
        {"cos", "cos-1", "sec(", BOARD_KEY_ROLE_NONE},
        {"tan", "tan-1", "cot(", BOARD_KEY_ROLE_NONE},
        {"pi", "e", "J", BOARD_KEY_ROLE_NONE},
    },
    {
        {"^2", "^[]", "K", BOARD_KEY_ROLE_NONE},
        {",", "E", "L", BOARD_KEY_ROLE_NONE},
        {"(", "{", "M", BOARD_KEY_ROLE_NONE},
        {")", "}", "N", BOARD_KEY_ROLE_NONE},
        {"+", NULL, "O", BOARD_KEY_ROLE_NONE},
    },
    {
        {"log", "10^[]", "P", BOARD_KEY_ROLE_NONE},
        {"7", NULL, "Q", BOARD_KEY_ROLE_NONE},
        {"8", NULL, "R", BOARD_KEY_ROLE_NONE},
        {"9", NULL, "S", BOARD_KEY_ROLE_NONE},
        {"*", NULL, "T", BOARD_KEY_ROLE_NONE},
    },
    {
        {"ln", "e^[]", "U", BOARD_KEY_ROLE_NONE},
        {"4", NULL, "V", BOARD_KEY_ROLE_NONE},
        {"5", NULL, "W", BOARD_KEY_ROLE_NONE},
        {"6", NULL, "X", BOARD_KEY_ROLE_NONE},
        {"-", NULL, "Y", BOARD_KEY_ROLE_NONE},
    },
    {
        {"sto", "get", "Z", BOARD_KEY_ROLE_NONE},
        {"1", NULL, "G", BOARD_KEY_ROLE_NONE},
        {"2", NULL, "H", BOARD_KEY_ROLE_NONE},
        {"3", NULL, "I", BOARD_KEY_ROLE_NONE},
        {"/", "%", NULL, BOARD_KEY_ROLE_NONE},
    },
    {
        {"on (home)", "off", NULL, BOARD_KEY_ROLE_HOME},
        {"0", NULL, NULL, BOARD_KEY_ROLE_NONE},
        {".", NULL, "#", BOARD_KEY_ROLE_NONE},
        {"(-)", "ans", NULL, BOARD_KEY_ROLE_NONE},
        {"enter", NULL, NULL, BOARD_KEY_ROLE_NONE},
    },
};

/* ── Display geometry ─────────────────────────────────────────── */
#define LCD_H_RES  320
#define LCD_V_RES  240
#define LCD_X_GAP  0
#define LCD_Y_GAP  0
#define LCD_HOST   SPI2_HOST
#define TOUCH_HOST SPI3_HOST

/* ── SPI clock ────────────────────────────────────────────────── */
/* ILI9341 SPI can usually run much faster than bring-up speed.
   Drop this to 10 MHz if long breadboard wires cause artifacts. */
#if OPENCALC_HARDWARE_IS_PCB_V3
#define LCD_SPI_CLOCK_HZ  (60 * 1000 * 1000)
#else
#define LCD_SPI_CLOCK_HZ  (20 * 1000 * 1000)
#endif
#define TOUCH_SPI_CLOCK_HZ (2 * 1000 * 1000)

static esp_lcd_panel_handle_t s_panel_handle = NULL;
static uint16_t *s_full_frame_565 = NULL;
#if ENABLE_TOUCH_INPUT
static spi_device_handle_t s_touch_handle = NULL;
#endif

static inline uint16_t lcd_rgb888_to_rgb565(uint32_t pixel)
{
    uint8_t r = (uint8_t)((pixel >> 16) & 0xff);
    uint8_t g = (uint8_t)((pixel >> 8) & 0xff);
    uint8_t b = (uint8_t)(pixel & 0xff);
    uint16_t rgb565 = (uint16_t)(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
    return (uint16_t)((rgb565 << 8) | (rgb565 >> 8));
}

static inline uint16_t lcd_swap_rgb565(uint16_t pixel)
{
    return (uint16_t)((pixel << 8) | (pixel >> 8));
}

static void lcd_clear_physical_panel(esp_lcd_panel_handle_t panel_handle)
{
    enum {
        CHUNK_H = 40,
    };

    DMA_ATTR static uint16_t black_buf[LCD_H_RES * CHUNK_H];

    for (int i = 0; i < LCD_H_RES * CHUNK_H; i++) {
        black_buf[i] = 0x0000;
    }

    for (int y = 0; y < LCD_V_RES; y += CHUNK_H) {
        int h = CHUNK_H;
        if (y + h > LCD_V_RES) {
            h = LCD_V_RES - y;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + h, black_buf));
    }
}

void board_display_lock(void)
{
    if (s_display_mutex != NULL) {
        xSemaphoreTake(s_display_mutex, portMAX_DELAY);
    }
}

void board_display_unlock(void)
{
    if (s_display_mutex != NULL) {
        xSemaphoreGive(s_display_mutex);
    }
}

static void onboard_led_off(void)
{
#if ONBOARD_RGB_LED_ENABLED
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_NUM_ONBOARD_RGB_LED,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(PIN_NUM_ONBOARD_RGB_LED, 0);
#endif
}

static void IRAM_ATTR keypad_gpio_isr(void *arg)
{
    (void)arg;
    s_keypad_interrupt_pending = true;

    BaseType_t higher_priority_task_woken = pdFALSE;
    if (s_event_task != NULL) {
        vTaskNotifyGiveFromISR(s_event_task, &higher_priority_task_woken);
    }
    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

#if ENABLE_TOUCH_INPUT
static void IRAM_ATTR touch_gpio_isr(void *arg)
{
    (void)arg;
    s_touch_interrupt_pending = true;

    BaseType_t higher_priority_task_woken = pdFALSE;
    if (s_event_task != NULL) {
        vTaskNotifyGiveFromISR(s_event_task, &higher_priority_task_woken);
    }
    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
#endif

static void gpio_isr_service_init(void)
{
    if (s_gpio_isr_service_ready) {
        return;
    }

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
    s_gpio_isr_service_ready = true;
}

static void keypad_set_idle(void)
{
#if ENABLE_KEYPAD_MATRIX_INPUT
    for (int row = 0; row < BOARD_KEYPAD_ROWS; row++) {
        gpio_set_level(KEYPAD_ROW_PINS[row], 0);
    }
#endif
}

#if OPENCALC_WAKE_FROM_KEYPAD_IN_SOFTWARE_OFF && ENABLE_KEYPAD_MATRIX_INPUT
static void keypad_prepare_for_deep_sleep(gpio_num_t wake_pin)
{
    for (int row = 0; row < BOARD_KEYPAD_ROWS; row++) {
        gpio_set_level(KEYPAD_ROW_PINS[row], 0);
        (void)gpio_sleep_sel_en(KEYPAD_ROW_PINS[row]);
        (void)gpio_sleep_set_direction(KEYPAD_ROW_PINS[row], GPIO_MODE_OUTPUT);
        (void)gpio_sleep_set_pull_mode(KEYPAD_ROW_PINS[row], GPIO_FLOATING);
    }

    for (int col = 0; col < BOARD_KEYPAD_COLS; col++) {
        gpio_set_pull_mode(KEYPAD_COL_PINS[col], GPIO_PULLUP_ONLY);
        (void)gpio_sleep_sel_en(KEYPAD_COL_PINS[col]);
        (void)gpio_sleep_set_direction(KEYPAD_COL_PINS[col], GPIO_MODE_INPUT);
        (void)gpio_sleep_set_pull_mode(KEYPAD_COL_PINS[col], GPIO_PULLUP_ONLY);
    }

    gpio_set_pull_mode(wake_pin, GPIO_PULLUP_ONLY);
}
#endif

/* ── Backlight ────────────────────────────────────────────────── */
static void backlight_on(void)
{
    if (PIN_NUM_LCD_BCKL < 0) {
        return;
    }

    ESP_LOGI(TAG, "Backlight PWM GPIO=%d. Drive/control this from 3V3 only, never 5V.", PIN_NUM_LCD_BCKL);

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LCD_BCKL_LEDC_MODE,
        .duty_resolution = LCD_BCKL_LEDC_RES,
        .timer_num = LCD_BCKL_LEDC_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t channel_cfg = {
        .gpio_num = PIN_NUM_LCD_BCKL,
        .speed_mode = LCD_BCKL_LEDC_MODE,
        .channel = LCD_BCKL_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LCD_BCKL_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));
    board_set_backlight_brightness(s_backlight_brightness);
}

static void backlight_off(void)
{
    if (PIN_NUM_LCD_BCKL < 0) {
        return;
    }

    ledc_set_duty(LCD_BCKL_LEDC_MODE, LCD_BCKL_LEDC_CHANNEL, 0);
    ledc_update_duty(LCD_BCKL_LEDC_MODE, LCD_BCKL_LEDC_CHANNEL);
}

static void sleep_drive_output(gpio_num_t pin, int level)
{
    if (pin == GPIO_NUM_NC) {
        return;
    }

    (void)gpio_reset_pin(pin);
    (void)gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    (void)gpio_set_level(pin, level);
    (void)gpio_sleep_sel_en(pin);
    (void)gpio_sleep_set_direction(pin, GPIO_MODE_OUTPUT);
    (void)gpio_sleep_set_pull_mode(pin, GPIO_FLOATING);
}

static void sleep_drive_input_pulldown(gpio_num_t pin)
{
    if (pin == GPIO_NUM_NC) {
        return;
    }

    (void)gpio_reset_pin(pin);
    (void)gpio_set_direction(pin, GPIO_MODE_INPUT);
    (void)gpio_set_pull_mode(pin, GPIO_PULLDOWN_ONLY);
    (void)gpio_sleep_sel_en(pin);
    (void)gpio_sleep_set_direction(pin, GPIO_MODE_INPUT);
    (void)gpio_sleep_set_pull_mode(pin, GPIO_PULLDOWN_ONLY);
}

static void lcd_prepare_for_deep_sleep(void)
{
    backlight_off();

    if (s_panel_handle != NULL) {
        (void)esp_lcd_panel_disp_on_off(s_panel_handle, false);
    }

    /*
     * The PCB V3 backlight uses GPIO47 to drive the TPS22918 ON pin.
     * Put that pin back under GPIO control and hold it low for deep sleep.
     */
    sleep_drive_output(PIN_NUM_LCD_BCKL, 0);

    sleep_drive_output(PIN_NUM_LCD_CS, 1);
    sleep_drive_output(PIN_NUM_LCD_DC, 0);
    sleep_drive_output(PIN_NUM_LCD_RST, 0);
    sleep_drive_output(PIN_NUM_LCD_SCLK, 0);
    sleep_drive_output(PIN_NUM_LCD_MOSI, 0);
    sleep_drive_input_pulldown(PIN_NUM_LCD_MISO);
}

static void keypad_init(void)
{
#if !ENABLE_KEYPAD_MATRIX_INPUT
    ESP_LOGI(TAG, "Keypad matrix disabled for this hardware profile");
    return;
#else
    uint64_t row_mask = 0;
    uint64_t col_mask = 0;

    for (int row = 0; row < BOARD_KEYPAD_ROWS; row++) {
        row_mask |= (1ULL << KEYPAD_ROW_PINS[row]);
    }

    for (int col = 0; col < BOARD_KEYPAD_COLS; col++) {
        col_mask |= (1ULL << KEYPAD_COL_PINS[col]);
    }

    gpio_config_t row_cfg = {
        .pin_bit_mask = row_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&row_cfg));

    gpio_config_t col_cfg = {
        .pin_bit_mask = col_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&col_cfg));

    keypad_set_idle();
    gpio_isr_service_init();

    for (int col = 0; col < BOARD_KEYPAD_COLS; col++) {
        ESP_ERROR_CHECK(gpio_isr_handler_add(KEYPAD_COL_PINS[col], keypad_gpio_isr, NULL));
    }

    ESP_LOGI(TAG,
             "Keypad ready: %dx%d matrix, per-key diodes=%d, settle=%dus",
             BOARD_KEYPAD_ROWS,
             BOARD_KEYPAD_COLS,
             OPENCALC_KEYPAD_HAS_PER_KEY_DIODES,
             OPENCALC_KEYPAD_ROW_SETTLE_US);
#endif
}

static void power_button_init(void)
{
#if POWER_BUTTON_ENABLED
    gpio_config_t power_cfg = {
        .pin_bit_mask = (1ULL << PIN_NUM_POWER_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&power_cfg));
#endif

#if POWER_HOLD_ENABLED
    gpio_config_t hold_cfg = {
        .pin_bit_mask = (1ULL << PIN_NUM_POWER_HOLD),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&hold_cfg));
    gpio_set_level(PIN_NUM_POWER_HOLD, 1);
#endif
}

static void battery_monitor_init(void)
{
#if BATTERY_CHARGE_STATUS_ENABLED
    gpio_config_t charge_cfg = {
        .pin_bit_mask = (1ULL << PIN_NUM_BATTERY_CHARGE_STATUS),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&charge_cfg));
    ESP_LOGI(TAG, "Battery charge-status GPIO=%d active_%s",
             PIN_NUM_BATTERY_CHARGE_STATUS,
             BATTERY_CHARGE_STATUS_ACTIVE_LOW ? "low" : "high");
#endif

#if BATTERY_MONITOR_ENABLED
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_battery_adc_unit));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = BATTERY_ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_battery_adc_unit, BATTERY_ADC_CHANNEL, &chan_cfg));

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t cali_err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_battery_adc_cali);
    
    s_battery_adc_calibrated = cali_err == ESP_OK;
    s_battery_adc_ready = true;
    ESP_LOGI(TAG,
             "Battery monitor ADC GPIO=%d channel=%d divider=%.0f/%.0f calibrated=%d",
             PIN_NUM_BATTERY_ADC,
             BATTERY_ADC_CHANNEL,
             (double)BATTERY_DIVIDER_R_TOP_OHMS,
             (double)BATTERY_DIVIDER_R_BOTTOM_OHMS,
             s_battery_adc_calibrated);
#else
    ESP_LOGI(TAG, "Battery monitor disabled; UI battery icon will show fallback level");
#endif
}

#if ENABLE_TOUCH_INPUT
static esp_err_t touch_read_channel(uint8_t command, uint16_t *value)
{
    if (s_touch_handle == NULL || value == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t tx_data[3] = {command, 0x00, 0x00};
    uint8_t rx_data[3] = {0};
    spi_transaction_t trans = {
        .length = sizeof(tx_data) * 8,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };

    esp_err_t err = spi_device_polling_transmit(s_touch_handle, &trans);
    if (err != ESP_OK) {
        return err;
    }

    *value = (uint16_t)(((rx_data[1] << 8) | rx_data[2]) >> 3);
    return ESP_OK;
}
#endif

static void touch_init(void)
{
#if !ENABLE_TOUCH_INPUT
    ESP_LOGI(TAG, "Touch disabled for bring-up");
    return;
#else
    spi_bus_config_t touch_buscfg = {
        .mosi_io_num = PIN_NUM_TOUCH_DIN,
        .miso_io_num = PIN_NUM_TOUCH_DO,
        .sclk_io_num = PIN_NUM_TOUCH_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 3,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(TOUCH_HOST, &touch_buscfg, SPI_DMA_CH_AUTO));

    gpio_config_t irq_cfg = {
        .pin_bit_mask = (1ULL << PIN_NUM_TOUCH_IRQ),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&irq_cfg));
    gpio_isr_service_init();
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_NUM_TOUCH_IRQ, touch_gpio_isr, NULL));

    spi_device_interface_config_t touch_cfg = {
        .clock_speed_hz = TOUCH_SPI_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = PIN_NUM_TOUCH_CS,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(TOUCH_HOST, &touch_cfg, &s_touch_handle));
    ESP_LOGI(TAG,
             "Touch ready: T_CLK=%d T_DIN=%d T_DO=%d CS=%d IRQ=%d",
             PIN_NUM_TOUCH_CLK,
             PIN_NUM_TOUCH_DIN,
             PIN_NUM_TOUCH_DO,
             PIN_NUM_TOUCH_CS,
             PIN_NUM_TOUCH_IRQ);
#endif
}

/* ── Public init ──────────────────────────────────────────────── */
void board_init(void)
{
    log_wakeup_reason();
    onboard_led_off();

    if (s_display_mutex == NULL) {
        s_display_mutex = xSemaphoreCreateMutex();
        ESP_ERROR_CHECK(s_display_mutex == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    }

    ESP_LOGI(TAG, "Initialising ILI9341 on SPI2, %dx%d", LCD_H_RES, LCD_V_RES);
    ESP_LOGI(TAG,
             "LCD pins: SCLK=%d MOSI=%d MISO=%d CS=%d DC=%d RST=%d BCKL=%d",
             PIN_NUM_LCD_SCLK,
             PIN_NUM_LCD_MOSI,
             PIN_NUM_LCD_MISO,
             PIN_NUM_LCD_CS,
             PIN_NUM_LCD_DC,
             PIN_NUM_LCD_RST,
             PIN_NUM_LCD_BCKL);

    /* 1. SPI bus */
    spi_bus_config_t buscfg = {
        .mosi_io_num     = PIN_NUM_LCD_MOSI,
        .miso_io_num     = PIN_NUM_LCD_MISO,
        .sclk_io_num     = PIN_NUM_LCD_SCLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        /* enough for one full 320x240 frame at 16 bpp */
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /* 2. LCD IO over SPI */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = PIN_NUM_LCD_DC,
        .cs_gpio_num       = PIN_NUM_LCD_CS,
        .pclk_hz           = LCD_SPI_CLOCK_HZ,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        /*
         * The draw helpers reuse one DMA line buffer per frame chunk. Keep the
         * panel IO queue shallow so the buffer is not overwritten while SPI DMA
         * is still transmitting the previous chunk.
         */
        .trans_queue_depth = 1,
    };
    ESP_ERROR_CHECK(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                 &io_cfg, &io_handle));

    /* 3. ILI9341 panel */
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(
        esp_lcd_new_panel_ili9341(io_handle, &panel_cfg, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    backlight_on();

    /* Put the native 240x320 ILI9341 GRAM into landscape addressing. */
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));
    lcd_clear_physical_panel(panel_handle);
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, LCD_X_GAP, LCD_Y_GAP));
    ESP_LOGI(TAG, "LCD visible gap: x=%d y=%d", LCD_X_GAP, LCD_Y_GAP);

    s_full_frame_565 = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    if (s_full_frame_565 == NULL) {
        ESP_LOGW(TAG, "Full-frame DMA buffer unavailable; falling back to chunked LCD updates");
    } else {
        ESP_LOGI(TAG, "Full-frame LCD DMA buffer ready: %d bytes", LCD_H_RES * LCD_V_RES * 2);
    }

    s_panel_handle = panel_handle;
    keypad_init();
    touch_init();
    onboard_led_off();
    power_button_init();
    battery_monitor_init();
    ESP_LOGI(TAG, "Display ready");
}

void board_set_event_task(TaskHandle_t task)
{
    s_event_task = task;
}

const board_key_t *board_keypad_key_at(int row, int col)
{
    if (row < 0 || row >= BOARD_KEYPAD_ROWS || col < 0 || col >= BOARD_KEYPAD_COLS) {
        return NULL;
    }

    const board_key_t *key = &KEYPAD_MAP[row][col];
    if (key->normal == NULL) {
        return NULL;
    }

    return key;
}

bool board_keypad_scan(int *row, int *col)
{
#if !ENABLE_KEYPAD_MATRIX_INPUT
    (void)row;
    (void)col;
    return false;
#else
    for (int scan_row = 0; scan_row < BOARD_KEYPAD_ROWS; scan_row++) {
        for (int r = 0; r < BOARD_KEYPAD_ROWS; r++) {
            gpio_set_level(KEYPAD_ROW_PINS[r], 1);
        }

        gpio_set_level(KEYPAD_ROW_PINS[scan_row], 0);
        esp_rom_delay_us(OPENCALC_KEYPAD_ROW_SETTLE_US);

        for (int scan_col = 0; scan_col < BOARD_KEYPAD_COLS; scan_col++) {
            if (gpio_get_level(KEYPAD_COL_PINS[scan_col]) == 0) {
                if (row != NULL) {
                    *row = scan_row;
                }
                if (col != NULL) {
                    *col = scan_col;
                }
                keypad_set_idle();
                return true;
            }
        }
    }

    keypad_set_idle();
    return false;
#endif
}

bool board_keypad_scan_matrix(bool pressed[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS])
{
#if !ENABLE_KEYPAD_MATRIX_INPUT
    if (pressed != NULL) {
        memset(pressed, 0, sizeof(bool) * BOARD_KEYPAD_ROWS * BOARD_KEYPAD_COLS);
    }
    return false;
#else
    bool any_pressed = false;

    if (pressed != NULL) {
        memset(pressed, 0, sizeof(bool) * BOARD_KEYPAD_ROWS * BOARD_KEYPAD_COLS);
    }

    for (int scan_row = 0; scan_row < BOARD_KEYPAD_ROWS; scan_row++) {
        for (int r = 0; r < BOARD_KEYPAD_ROWS; r++) {
            gpio_set_level(KEYPAD_ROW_PINS[r], 1);
        }

        gpio_set_level(KEYPAD_ROW_PINS[scan_row], 0);
        esp_rom_delay_us(OPENCALC_KEYPAD_ROW_SETTLE_US);

        for (int scan_col = 0; scan_col < BOARD_KEYPAD_COLS; scan_col++) {
            if (gpio_get_level(KEYPAD_COL_PINS[scan_col]) == 0) {
                any_pressed = true;
                if (pressed != NULL) {
                    pressed[scan_row][scan_col] = true;
                }
            }
        }
    }

    keypad_set_idle();
    return any_pressed;
#endif
}

bool board_keypad_take_interrupt(void)
{
#if !ENABLE_KEYPAD_MATRIX_INPUT
    return false;
#else
    if (!s_keypad_interrupt_pending) {
        return false;
    }

    s_keypad_interrupt_pending = false;
    return true;
#endif
}

bool board_power_button_pressed(void)
{
#if POWER_BUTTON_ENABLED
    return gpio_get_level(PIN_NUM_POWER_BUTTON) == 0;
#else
    return false;
#endif
}

bool board_touch_scan(int *x, int *y)
{
#if !ENABLE_TOUCH_INPUT
    (void)x;
    (void)y;
    return false;
#else
    if (s_touch_handle == NULL) {
        return false;
    }

    if (gpio_get_level(PIN_NUM_TOUCH_IRQ) != 0) {
        return false;
    }

    uint16_t raw_x = 0;
    uint16_t raw_y = 0;
    esp_err_t x_err = touch_read_channel(0xd0, &raw_x);
    esp_err_t y_err = touch_read_channel(0x90, &raw_y);
    if (x_err != ESP_OK || y_err != ESP_OK) {
        ESP_LOGW(TAG, "Touch read failed: x=%s y=%s", esp_err_to_name(x_err), esp_err_to_name(y_err));
        return false;
    }

    if (x != NULL) {
        *x = raw_x;
    }
    if (y != NULL) {
        *y = raw_y;
    }

    return true;
#endif
}

bool board_touch_take_interrupt(void)
{
#if !ENABLE_TOUCH_INPUT
    return false;
#else
    if (!s_touch_interrupt_pending) {
        return false;
    }

    s_touch_interrupt_pending = false;
    return true;
#endif
}

void board_enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "Entering software off (deep sleep)");

    lcd_prepare_for_deep_sleep();
    onboard_led_off();

    ESP_ERROR_CHECK(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL));

#if OPENCALC_WAKE_FROM_KEYPAD_IN_SOFTWARE_OFF && ENABLE_KEYPAD_MATRIX_INPUT
    const gpio_num_t wake_pin = PIN_NUM_KEYPAD_COL0;
    keypad_prepare_for_deep_sleep(wake_pin);
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << wake_pin, ESP_EXT1_WAKEUP_ANY_LOW));

    /*
     * Keep RTC peripherals powered so the wake-column pull-up remains active.
     * If this domain is powered off, the matrix column can float low and wake
     * immediately, which looks like a reboot instead of software off.
     */
    (void)esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
#else
    keypad_set_idle();
    ESP_LOGI(TAG, "Keypad wake disabled for software off; reset or power cycle to wake");
    (void)esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
#endif
#ifdef ESP_PD_DOMAIN_RTC_SLOW_MEM
    (void)esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
#endif
#ifdef ESP_PD_DOMAIN_RTC_FAST_MEM
    (void)esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
#endif
#ifdef ESP_PD_DOMAIN_XTAL
    (void)esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
#endif
#ifdef ESP_PD_DOMAIN_MODEM
    (void)esp_sleep_pd_config(ESP_PD_DOMAIN_MODEM, ESP_PD_OPTION_OFF);
#endif

#if OPENCALC_WAKE_FROM_KEYPAD_IN_SOFTWARE_OFF && ENABLE_KEYPAD_MATRIX_INPUT
    while (gpio_get_level(wake_pin) == 0) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
#endif

    esp_deep_sleep_start();
}

void board_set_backlight_brightness(int percent)
{
    if (percent < 5) {
        percent = 5;
    } else if (percent > 100) {
        percent = 100;
    }

    s_backlight_brightness = percent;
    if (PIN_NUM_LCD_BCKL < 0) {
        return;
    }

    uint32_t duty = (uint32_t)(LCD_BCKL_LEDC_MAX_DUTY * percent / 100);
    ledc_set_duty(LCD_BCKL_LEDC_MODE, LCD_BCKL_LEDC_CHANNEL, duty);
    ledc_update_duty(LCD_BCKL_LEDC_MODE, LCD_BCKL_LEDC_CHANNEL);
}

int board_get_backlight_brightness(void)
{
    return s_backlight_brightness;
}

bool board_battery_get_voltage_mv(int *millivolts)
{
#if BATTERY_MONITOR_ENABLED
    if (!s_battery_adc_ready || s_battery_adc_unit == NULL || millivolts == NULL) {
        return false;
    }

    int raw = 0;
    if (adc_oneshot_read(s_battery_adc_unit, BATTERY_ADC_CHANNEL, &raw) != ESP_OK) {
        return false;
    }

    int adc_mv = 0;
    if (s_battery_adc_calibrated && s_battery_adc_cali != NULL) {
        if (adc_cali_raw_to_voltage(s_battery_adc_cali, raw, &adc_mv) != ESP_OK) {
            return false;
        }
    } else {
        adc_mv = raw * 3300 / 4095;
    }

    float scale = (BATTERY_DIVIDER_R_TOP_OHMS + BATTERY_DIVIDER_R_BOTTOM_OHMS) /
        BATTERY_DIVIDER_R_BOTTOM_OHMS;
    *millivolts = (int)((float)adc_mv * scale);
    return true;
#else
    (void)millivolts;
    return false;
#endif
}

bool board_battery_get_percent(int *percent)
{
    int mv = 0;
    if (percent == NULL || !board_battery_get_voltage_mv(&mv)) {
        return false;
    }

    int pct = (mv - BATTERY_EMPTY_MV) * 100 / (BATTERY_FULL_MV - BATTERY_EMPTY_MV);
    if (pct < 0) {
        pct = 0;
    } else if (pct > 100) {
        pct = 100;
    }
    *percent = pct;
    return true;
}

bool board_battery_is_charging(void)
{
#if BATTERY_CHARGE_STATUS_ENABLED
    int level = gpio_get_level(PIN_NUM_BATTERY_CHARGE_STATUS);
    return BATTERY_CHARGE_STATUS_ACTIVE_LOW ? level == 0 : level != 0;
#else
    return false;
#endif
}

static const uint8_t *glyph_for(char c)
{
    static const uint8_t space[7]    = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t glyph_0[7]  = {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e};
    static const uint8_t glyph_1[7]  = {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e};
    static const uint8_t glyph_2[7]  = {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f};
    static const uint8_t glyph_3[7]  = {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e};
    static const uint8_t glyph_4[7]  = {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02};
    static const uint8_t glyph_5[7]  = {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e};
    static const uint8_t glyph_6[7]  = {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e};
    static const uint8_t glyph_7[7]  = {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    static const uint8_t glyph_8[7]  = {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e};
    static const uint8_t glyph_9[7]  = {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c};
    static const uint8_t glyph_a[7]  = {0x00, 0x0e, 0x01, 0x0f, 0x11, 0x11, 0x0f};
    static const uint8_t glyph_b[7]  = {0x10, 0x10, 0x1e, 0x11, 0x11, 0x11, 0x1e};
    static const uint8_t glyph_c[7]  = {0x00, 0x0e, 0x10, 0x10, 0x10, 0x10, 0x0e};
    static const uint8_t glyph_d[7]  = {0x01, 0x01, 0x0f, 0x11, 0x11, 0x11, 0x0f};
    static const uint8_t glyph_e[7]  = {0x00, 0x0e, 0x11, 0x1f, 0x10, 0x10, 0x0e};
    static const uint8_t glyph_f[7]  = {0x06, 0x08, 0x1c, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t glyph_g[7]  = {0x00, 0x0f, 0x11, 0x11, 0x0f, 0x01, 0x0e};
    static const uint8_t glyph_h[7]  = {0x10, 0x10, 0x1e, 0x11, 0x11, 0x11, 0x11};
    static const uint8_t glyph_i[7]  = {0x04, 0x00, 0x0c, 0x04, 0x04, 0x04, 0x0e};
    static const uint8_t glyph_j[7]  = {0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0c};
    static const uint8_t glyph_k[7]  = {0x10, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    static const uint8_t glyph_l[7]  = {0x0c, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e};
    static const uint8_t glyph_m[7]  = {0x00, 0x00, 0x1a, 0x15, 0x15, 0x15, 0x15};
    static const uint8_t glyph_n[7]  = {0x00, 0x00, 0x1e, 0x11, 0x11, 0x11, 0x11};
    static const uint8_t glyph_o[7]  = {0x00, 0x0e, 0x11, 0x11, 0x11, 0x11, 0x0e};
    static const uint8_t glyph_p[7]  = {0x00, 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10};
    static const uint8_t glyph_q[7]  = {0x00, 0x0f, 0x11, 0x11, 0x0f, 0x01, 0x01};
    static const uint8_t glyph_r[7]  = {0x00, 0x00, 0x16, 0x18, 0x10, 0x10, 0x10};
    static const uint8_t glyph_s[7]  = {0x00, 0x0f, 0x10, 0x0e, 0x01, 0x01, 0x1e};
    static const uint8_t glyph_t[7]  = {0x08, 0x08, 0x1c, 0x08, 0x08, 0x09, 0x06};
    static const uint8_t glyph_u[7]  = {0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0d};
    static const uint8_t glyph_v[7]  = {0x00, 0x00, 0x11, 0x11, 0x11, 0x0a, 0x04};
    static const uint8_t glyph_w[7]  = {0x00, 0x00, 0x11, 0x15, 0x15, 0x15, 0x0a};
    static const uint8_t glyph_x[7]  = {0x00, 0x00, 0x11, 0x0a, 0x04, 0x0a, 0x11};
    static const uint8_t glyph_y[7]  = {0x00, 0x11, 0x11, 0x0f, 0x01, 0x11, 0x0e};
    static const uint8_t glyph_z[7]  = {0x00, 0x1f, 0x02, 0x04, 0x08, 0x10, 0x1f};
    static const uint8_t dot[7]      = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c};
    static const uint8_t comma[7]    = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x08};
    static const uint8_t colon[7]    = {0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x00};
    static const uint8_t slash[7]    = {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
    static const uint8_t minus[7]    = {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00};
    static const uint8_t plus[7]     = {0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00};
    static const uint8_t star[7]     = {0x00, 0x15, 0x0e, 0x1f, 0x0e, 0x15, 0x00};
    static const uint8_t percent[7]  = {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03};
    static const uint8_t equals[7]   = {0x00, 0x00, 0x1f, 0x00, 0x1f, 0x00, 0x00};
    static const uint8_t hash[7]     = {0x0a, 0x0a, 0x1f, 0x0a, 0x1f, 0x0a, 0x0a};
    static const uint8_t lparen[7]   = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
    static const uint8_t rparen[7]   = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};
    static const uint8_t unknown[7]  = {0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};

    if (c >= 'A' && c <= 'Z') {
        c = (char)(c - 'A' + 'a');
    }

    switch (c) {
    case '0': return glyph_0;
    case '1': return glyph_1;
    case '2': return glyph_2;
    case '3': return glyph_3;
    case '4': return glyph_4;
    case '5': return glyph_5;
    case '6': return glyph_6;
    case '7': return glyph_7;
    case '8': return glyph_8;
    case '9': return glyph_9;
    case 'a': return glyph_a;
    case 'b': return glyph_b;
    case 'c': return glyph_c;
    case 'd': return glyph_d;
    case 'e': return glyph_e;
    case 'f': return glyph_f;
    case 'g': return glyph_g;
    case 'h': return glyph_h;
    case 'i': return glyph_i;
    case 'j': return glyph_j;
    case 'k': return glyph_k;
    case 'l': return glyph_l;
    case 'm': return glyph_m;
    case 'n': return glyph_n;
    case 'o': return glyph_o;
    case 'p': return glyph_p;
    case 'q': return glyph_q;
    case 'r': return glyph_r;
    case 's': return glyph_s;
    case 't': return glyph_t;
    case 'u': return glyph_u;
    case 'v': return glyph_v;
    case 'w': return glyph_w;
    case 'x': return glyph_x;
    case 'y': return glyph_y;
    case 'z': return glyph_z;
    case '.': return dot;
    case ',': return comma;
    case ':': return colon;
    case '/': return slash;
    case '-': return minus;
    case '+': return plus;
    case '*': return star;
    case '%': return percent;
    case '=': return equals;
    case '#': return hash;
    case '(': return lparen;
    case ')': return rparen;
    case ' ': return space;
    default: return unknown;
    }
}

void board_draw_text_screen(const char *text)
{
    enum {
        SCALE = 3,
        GLYPH_W = 5,
        GLYPH_H = 7,
        CHAR_ADVANCE = 6,
        MAX_TEXT_CHARS = 32,
        TEXT_BUF_W = MAX_TEXT_CHARS * CHAR_ADVANCE * SCALE,
        TEXT_BUF_H = GLYPH_H * SCALE,
        BLACK_CHUNK_H = 20,
    };

    DMA_ATTR static uint16_t black_buf[LCD_H_RES * BLACK_CHUNK_H];
    DMA_ATTR static uint16_t text_buf[TEXT_BUF_W * TEXT_BUF_H];

    if (s_panel_handle == NULL || text == NULL) {
        return;
    }

    for (int i = 0; i < LCD_H_RES * BLACK_CHUNK_H; i++) {
        black_buf[i] = 0x0000;
    }

    for (int y = 0; y < LCD_V_RES; y += BLACK_CHUNK_H) {
        int h = BLACK_CHUNK_H;
        if (y + h > LCD_V_RES) {
            h = LCD_V_RES - y;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel_handle, 0, y, LCD_H_RES, y + h, black_buf));
    }

    int len = 0;
    while (text[len] != '\0' && len < MAX_TEXT_CHARS) {
        len++;
    }

    int text_w = len * CHAR_ADVANCE * SCALE;
    int text_h = TEXT_BUF_H;
    int x0 = (LCD_H_RES - text_w) / 2;
    int y0 = (LCD_V_RES - text_h) / 2;

    for (int i = 0; i < text_w * text_h; i++) {
        text_buf[i] = 0x0000;
    }

    for (int ch = 0; ch < len; ch++) {
        const uint8_t *glyph = glyph_for(text[ch]);
        int char_x = ch * CHAR_ADVANCE * SCALE;

        for (int gy = 0; gy < GLYPH_H; gy++) {
            for (int gx = 0; gx < GLYPH_W; gx++) {
                if ((glyph[gy] & (1 << (GLYPH_W - 1 - gx))) == 0) {
                    continue;
                }

                int px0 = char_x + gx * SCALE;
                int py0 = gy * SCALE;
                for (int sy = 0; sy < SCALE; sy++) {
                    for (int sx = 0; sx < SCALE; sx++) {
                        text_buf[(py0 + sy) * text_w + px0 + sx] = 0xffff;
                    }
                }
            }
        }
    }

    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel_handle, x0, y0, x0 + text_w, y0 + text_h, text_buf));
}

void board_draw_rgb888_frame_320x200(const uint32_t *pixels)
{
    enum {
        FRAME_W = 320,
        SRC_H = 200,
        DST_H = 240,
        CHUNK_H = 40,
    };

    DMA_ATTR static uint16_t line_buf[FRAME_W * CHUNK_H];

    if (s_panel_handle == NULL || pixels == NULL) {
        return;
    }

    for (int y = 0; y < DST_H; y += CHUNK_H) {
        int h = CHUNK_H;
        if (y + h > DST_H) {
            h = DST_H - y;
        }

        for (int row = 0; row < h; row++) {
            int src_y = ((y + row) * SRC_H) / DST_H;
            const uint32_t *src = pixels + src_y * FRAME_W;
            uint16_t *dst = line_buf + row * FRAME_W;
            for (int x = 0; x < FRAME_W; x++) {
                dst[x] = lcd_rgb888_to_rgb565(src[x]);
            }
        }

        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel_handle, 0, y, FRAME_W, y + h, line_buf));
    }
}

void board_draw_rgb565_frame_320x200(const uint16_t *pixels)
{
    enum {
        FRAME_W = 320,
        SRC_H = 200,
        DST_H = 240,
        CHUNK_H = 40,
    };

    DMA_ATTR static uint16_t line_buf[FRAME_W * CHUNK_H];

    if (s_panel_handle == NULL || pixels == NULL) {
        return;
    }

    for (int y = 0; y < DST_H; y += CHUNK_H) {
        int h = CHUNK_H;
        if (y + h > DST_H) {
            h = DST_H - y;
        }

        for (int row = 0; row < h; row++) {
            int src_y = ((y + row) * SRC_H) / DST_H;
            const uint16_t *src = pixels + src_y * FRAME_W;
            uint16_t *dst = line_buf + row * FRAME_W;
            for (int x = 0; x < FRAME_W; x++) {
                dst[x] = lcd_swap_rgb565(src[x]);
            }
        }

        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel_handle, 0, y, FRAME_W, y + h, line_buf));
    }
}

void board_draw_rgb888_frame_320x240(const uint32_t *pixels)
{
    enum {
        FRAME_W = 320,
        FRAME_H = 240,
        CHUNK_H = 40,
    };

    DMA_ATTR static uint16_t line_buf[FRAME_W * CHUNK_H];

    if (s_panel_handle == NULL || pixels == NULL) {
        return;
    }

    if (s_full_frame_565 != NULL) {
        for (int i = 0; i < FRAME_W * FRAME_H; i++) {
            s_full_frame_565[i] = lcd_rgb888_to_rgb565(pixels[i]);
        }

        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel_handle, 0, 0, FRAME_W, FRAME_H, s_full_frame_565));
        return;
    }

    for (int y = 0; y < FRAME_H; y += CHUNK_H) {
        int h = CHUNK_H;
        if (y + h > FRAME_H) {
            h = FRAME_H - y;
        }

        for (int row = 0; row < h; row++) {
            const uint32_t *src = pixels + (y + row) * FRAME_W;
            uint16_t *dst = line_buf + row * FRAME_W;
            for (int x = 0; x < FRAME_W; x++) {
                dst[x] = lcd_rgb888_to_rgb565(src[x]);
            }
        }

        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel_handle, 0, y, FRAME_W, y + h, line_buf));
    }
}
