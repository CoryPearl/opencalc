#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    OPENCALC_SENSOR_DIGITAL_COUNT = 12,
    OPENCALC_SENSOR_ANALOG_COUNT = 4,
};

typedef enum {
    OPENCALC_SENSOR_INPUT = 0,
    OPENCALC_SENSOR_OUTPUT = 1,
    OPENCALC_SENSOR_INPUT_PULLUP = 2,
} opencalc_sensor_pin_mode_t;

void opencalc_sensor_hub_init(void);
bool opencalc_sensor_hub_available(void);

bool opencalc_sensor_digital_mode(int channel, opencalc_sensor_pin_mode_t mode);
bool opencalc_sensor_digital_write(int channel, bool high);
bool opencalc_sensor_digital_read(int channel, bool *high);

bool opencalc_sensor_analog_read_raw(int channel, int16_t *raw);
bool opencalc_sensor_analog_read_volts(int channel, double *volts);
bool opencalc_sensor_analog_read_differential(int positive_channel,
                                              int negative_channel,
                                              double *volts);
bool opencalc_sensor_analog_set_rate(int samples_per_second);
int opencalc_sensor_analog_get_rate(void);

bool opencalc_sensor_i2c_present(uint8_t address);
bool opencalc_sensor_i2c_read(uint8_t address, uint8_t reg,
                             uint8_t *data, size_t size);
bool opencalc_sensor_i2c_write(uint8_t address, uint8_t reg,
                              const uint8_t *data, size_t size);

void opencalc_sensor_set_audio_enabled(bool enabled);
void opencalc_sensor_set_power_led(bool enabled);
void opencalc_sensor_prepare_sleep(void);
void opencalc_sensor_resume(void);

#ifdef __cplusplus
}
#endif
