#include "opencalc_sensor_hub.h"

#include "opencalc_config.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define SENSOR_I2C_PORT I2C_NUM_0
#define SENSOR_TIMEOUT_MS 100

#define MCP_IODIRA 0x00
#define MCP_IODIRB 0x01
#define MCP_GPPUA 0x0c
#define MCP_GPPUB 0x0d
#define MCP_GPIOA 0x12
#define MCP_GPIOB 0x13
#define MCP_OLATA 0x14
#define MCP_OLATB 0x15
#define MCP_AUDIO_SD_BIT 0
#define MCP_POWER_LED_BIT 1

#define ADS_CONVERSION 0x00
#define ADS_CONFIG 0x01

static const char *TAG = "sensor_hub";

#if OPENCALC_ENABLE_SCIENTIFIC_IO
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_mcp;
static i2c_master_dev_handle_t s_ads;
static SemaphoreHandle_t s_lock;
static bool s_mcp_ready;
static bool s_ads_ready;
static uint16_t s_mcp_direction = 0xffff;
static uint16_t s_mcp_pullups;
static uint16_t s_mcp_output;
static int s_ads_rate = OPENCALC_ADS1115_DEFAULT_RATE_HZ;

static bool bus_take(void)
{
    return s_lock != NULL && xSemaphoreTake(s_lock, pdMS_TO_TICKS(500)) == pdTRUE;
}

static void bus_give(void)
{
    xSemaphoreGive(s_lock);
}

static bool mcp_write8(uint8_t reg, uint8_t value)
{
    uint8_t payload[2] = {reg, value};
    return i2c_master_transmit(s_mcp, payload, sizeof(payload), SENSOR_TIMEOUT_MS) == ESP_OK;
}

static bool mcp_sync_direction(void)
{
    return mcp_write8(MCP_IODIRA, (uint8_t)s_mcp_direction) &&
           mcp_write8(MCP_IODIRB, (uint8_t)(s_mcp_direction >> 8));
}

static bool mcp_sync_pullups(void)
{
    return mcp_write8(MCP_GPPUA, (uint8_t)s_mcp_pullups) &&
           mcp_write8(MCP_GPPUB, (uint8_t)(s_mcp_pullups >> 8));
}

static bool mcp_sync_output(void)
{
    return mcp_write8(MCP_OLATA, (uint8_t)s_mcp_output) &&
           mcp_write8(MCP_OLATB, (uint8_t)(s_mcp_output >> 8));
}

static int mcp_bit_for_channel(int channel)
{
    if (channel < 0 || channel >= OPENCALC_SENSOR_DIGITAL_COUNT) return -1;
    /* D0-D4 use GPA2-GPA6; D5-D11 use GPB0-GPB6. Skip GPA7/GPB7. */
    return channel < 5 ? channel + 2 : channel + 3;
}

static int ads_rate_bits(int rate)
{
    static const int rates[] = {8, 16, 32, 64, 128, 250, 475, 860};
    int best = 0;
    for (int i = 1; i < 8; i++) {
        if (rate >= rates[i]) best = i;
    }
    s_ads_rate = rates[best];
    return best;
}

static bool ads_read_mux(int mux, int16_t *raw)
{
    if (!s_ads_ready || raw == NULL || !bus_take()) return false;
    int rate_bits = ads_rate_bits(s_ads_rate);
    uint16_t config = (uint16_t)(0x8000u | ((uint16_t)mux << 12) |
                                 (1u << 9) | (1u << 8) |
                                 ((uint16_t)rate_bits << 5) | 0x0003u);
    uint8_t write_config[3] = {
        ADS_CONFIG, (uint8_t)(config >> 8), (uint8_t)config
    };
    esp_err_t err = i2c_master_transmit(s_ads, write_config, sizeof(write_config), SENSOR_TIMEOUT_MS);
    if (err == ESP_OK) {
        uint32_t wait_ms = (uint32_t)((1000 + s_ads_rate - 1) / s_ads_rate) + 1;
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
        uint8_t reg = ADS_CONVERSION;
        uint8_t bytes[2] = {0};
        err = i2c_master_transmit_receive(s_ads, &reg, 1, bytes, sizeof(bytes), SENSOR_TIMEOUT_MS);
        if (err == ESP_OK) *raw = (int16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
    }
    bus_give();
    return err == ESP_OK;
}

static bool add_device(uint8_t address, i2c_master_dev_handle_t *handle)
{
    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = OPENCALC_SENSOR_I2C_FREQUENCY_HZ,
    };
    return i2c_master_bus_add_device(s_bus, &config, handle) == ESP_OK;
}
#endif

void opencalc_sensor_hub_init(void)
{
#if !OPENCALC_ENABLE_SCIENTIFIC_IO
    ESP_LOGI(TAG, "Scientific I/O disabled in config.h");
#else
    if (s_bus != NULL) return;
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        ESP_LOGE(TAG, "Could not allocate I2C mutex");
        return;
    }
    i2c_master_bus_config_t config = {
        .i2c_port = SENSOR_I2C_PORT,
        .sda_io_num = OPENCALC_SENSOR_I2C_SDA_GPIO,
        .scl_io_num = OPENCALC_SENSOR_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };
    esp_err_t err = i2c_new_master_bus(&config, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(err));
        s_bus = NULL;
        return;
    }

    s_mcp_ready = add_device(OPENCALC_MCP23017_I2C_ADDRESS, &s_mcp) &&
                  i2c_master_probe(s_bus, OPENCALC_MCP23017_I2C_ADDRESS,
                                   SENSOR_TIMEOUT_MS) == ESP_OK;
    s_ads_ready = add_device(OPENCALC_ADS1115_I2C_ADDRESS, &s_ads) &&
                  i2c_master_probe(s_bus, OPENCALC_ADS1115_I2C_ADDRESS,
                                   SENSOR_TIMEOUT_MS) == ESP_OK;

    if (s_mcp_ready && bus_take()) {
        s_mcp_direction &= (uint16_t)~(BIT(MCP_AUDIO_SD_BIT) |
                                      BIT(MCP_POWER_LED_BIT));
        s_mcp_output &= (uint16_t)~BIT(MCP_AUDIO_SD_BIT);
#if OPENCALC_ENABLE_POWER_STATUS_LED
        s_mcp_output |= BIT(MCP_POWER_LED_BIT);
#endif
        bool ok = mcp_sync_output() && mcp_sync_pullups() && mcp_sync_direction();
        bus_give();
        s_mcp_ready = ok;
    }
    ESP_LOGI(TAG,
             "Scientific I/O: I2C SDA=%d SCL=%d, MCP23017=%s, ADS1115=%s, D0-D11/A0-A3",
             OPENCALC_SENSOR_I2C_SDA_GPIO, OPENCALC_SENSOR_I2C_SCL_GPIO,
             s_mcp_ready ? "ready" : "missing",
             s_ads_ready ? "ready" : "missing");
#endif
}

bool opencalc_sensor_hub_available(void)
{
#if OPENCALC_ENABLE_SCIENTIFIC_IO
    return s_mcp_ready || s_ads_ready;
#else
    return false;
#endif
}

bool opencalc_sensor_digital_mode(int channel, opencalc_sensor_pin_mode_t mode)
{
#if OPENCALC_ENABLE_SCIENTIFIC_IO
    int bit = mcp_bit_for_channel(channel);
    if (!s_mcp_ready || bit < 0 || mode < OPENCALC_SENSOR_INPUT ||
        mode > OPENCALC_SENSOR_INPUT_PULLUP || !bus_take()) return false;
    if (mode == OPENCALC_SENSOR_OUTPUT) s_mcp_direction &= (uint16_t)~BIT(bit);
    else s_mcp_direction |= BIT(bit);
    if (mode == OPENCALC_SENSOR_INPUT_PULLUP) s_mcp_pullups |= BIT(bit);
    else s_mcp_pullups &= (uint16_t)~BIT(bit);
    bool ok = mcp_sync_pullups() && mcp_sync_direction();
    bus_give();
    return ok;
#else
    (void)channel; (void)mode;
    return false;
#endif
}

bool opencalc_sensor_digital_write(int channel, bool high)
{
#if OPENCALC_ENABLE_SCIENTIFIC_IO
    int bit = mcp_bit_for_channel(channel);
    if (!s_mcp_ready || bit < 0 || (s_mcp_direction & BIT(bit)) != 0 || !bus_take()) return false;
    if (high) s_mcp_output |= BIT(bit); else s_mcp_output &= (uint16_t)~BIT(bit);
    bool ok = mcp_sync_output();
    bus_give();
    return ok;
#else
    (void)channel; (void)high;
    return false;
#endif
}

bool opencalc_sensor_digital_read(int channel, bool *high)
{
#if OPENCALC_ENABLE_SCIENTIFIC_IO
    int bit = mcp_bit_for_channel(channel);
    if (!s_mcp_ready || high == NULL || bit < 0 || !bus_take()) return false;
    uint8_t reg = bit < 8 ? MCP_GPIOA : MCP_GPIOB;
    uint8_t value = 0;
    bool ok = i2c_master_transmit_receive(s_mcp, &reg, 1, &value, 1,
                                           SENSOR_TIMEOUT_MS) == ESP_OK;
    if (ok) *high = (value & BIT(bit & 7)) != 0;
    bus_give();
    return ok;
#else
    (void)channel; (void)high;
    return false;
#endif
}

bool opencalc_sensor_analog_read_raw(int channel, int16_t *raw)
{
#if OPENCALC_ENABLE_SCIENTIFIC_IO
    return channel >= 0 && channel < OPENCALC_SENSOR_ANALOG_COUNT &&
           ads_read_mux(4 + channel, raw);
#else
    (void)channel; (void)raw;
    return false;
#endif
}

bool opencalc_sensor_analog_read_volts(int channel, double *volts)
{
    int16_t raw = 0;
    if (volts == NULL || !opencalc_sensor_analog_read_raw(channel, &raw)) return false;
    *volts = (double)raw * 0.000125;
    return true;
}

bool opencalc_sensor_analog_read_differential(int positive_channel,
                                              int negative_channel,
                                              double *volts)
{
#if OPENCALC_ENABLE_SCIENTIFIC_IO
    int mux = -1;
    if (positive_channel == 0 && negative_channel == 1) mux = 0;
    else if (positive_channel == 0 && negative_channel == 3) mux = 1;
    else if (positive_channel == 1 && negative_channel == 3) mux = 2;
    else if (positive_channel == 2 && negative_channel == 3) mux = 3;
    int16_t raw = 0;
    if (mux < 0 || volts == NULL || !ads_read_mux(mux, &raw)) return false;
    *volts = (double)raw * 0.000125;
    return true;
#else
    (void)positive_channel; (void)negative_channel; (void)volts;
    return false;
#endif
}

bool opencalc_sensor_analog_set_rate(int samples_per_second)
{
#if OPENCALC_ENABLE_SCIENTIFIC_IO
    if (samples_per_second < 8 || samples_per_second > 860) return false;
    (void)ads_rate_bits(samples_per_second);
    return true;
#else
    (void)samples_per_second;
    return false;
#endif
}

int opencalc_sensor_analog_get_rate(void)
{
#if OPENCALC_ENABLE_SCIENTIFIC_IO
    return s_ads_rate;
#else
    return 0;
#endif
}

bool opencalc_sensor_i2c_present(uint8_t address)
{
#if OPENCALC_ENABLE_SCIENTIFIC_IO
    if (s_bus == NULL || address < 0x08 || address > 0x77 || !bus_take()) return false;
    bool present = i2c_master_probe(s_bus, address, SENSOR_TIMEOUT_MS) == ESP_OK;
    bus_give();
    return present;
#else
    (void)address;
    return false;
#endif
}

#if OPENCALC_ENABLE_SCIENTIFIC_IO
static bool external_i2c_address(uint8_t address)
{
    return address >= 0x08 && address <= 0x77 &&
           address != OPENCALC_MCP23017_I2C_ADDRESS &&
           address != OPENCALC_ADS1115_I2C_ADDRESS;
}
#endif

bool opencalc_sensor_i2c_read(uint8_t address, uint8_t reg,
                             uint8_t *data, size_t size)
{
#if OPENCALC_ENABLE_SCIENTIFIC_IO
    if (!external_i2c_address(address) || data == NULL || size == 0 || size > 16 || !bus_take()) return false;
    i2c_master_dev_handle_t device = NULL;
    bool ok = add_device(address, &device) &&
              i2c_master_transmit_receive(device, &reg, 1, data, size,
                                          SENSOR_TIMEOUT_MS) == ESP_OK;
    if (device != NULL) i2c_master_bus_rm_device(device);
    bus_give();
    return ok;
#else
    (void)address; (void)reg; (void)data; (void)size;
    return false;
#endif
}

bool opencalc_sensor_i2c_write(uint8_t address, uint8_t reg,
                              const uint8_t *data, size_t size)
{
#if OPENCALC_ENABLE_SCIENTIFIC_IO
    if (!external_i2c_address(address) || data == NULL || size == 0 || size > 16 || !bus_take()) return false;
    uint8_t payload[17] = {reg};
    memcpy(payload + 1, data, size);
    i2c_master_dev_handle_t device = NULL;
    bool ok = add_device(address, &device) &&
              i2c_master_transmit(device, payload, size + 1, SENSOR_TIMEOUT_MS) == ESP_OK;
    if (device != NULL) i2c_master_bus_rm_device(device);
    bus_give();
    return ok;
#else
    (void)address; (void)reg; (void)data; (void)size;
    return false;
#endif
}

static void set_reserved_output(int bit, bool enabled)
{
#if OPENCALC_ENABLE_SCIENTIFIC_IO
    if (!s_mcp_ready || !bus_take()) return;
    if (enabled) s_mcp_output |= BIT(bit); else s_mcp_output &= (uint16_t)~BIT(bit);
    (void)mcp_sync_output();
    bus_give();
#else
    (void)bit; (void)enabled;
#endif
}

void opencalc_sensor_set_audio_enabled(bool enabled)
{
    set_reserved_output(MCP_AUDIO_SD_BIT, enabled);
}

void opencalc_sensor_set_power_led(bool enabled)
{
#if OPENCALC_ENABLE_POWER_STATUS_LED
    set_reserved_output(MCP_POWER_LED_BIT, enabled);
#else
    (void)enabled;
#endif
}

void opencalc_sensor_prepare_sleep(void)
{
    opencalc_sensor_set_audio_enabled(false);
    opencalc_sensor_set_power_led(false);
}

void opencalc_sensor_resume(void)
{
    opencalc_sensor_set_power_led(true);
}
