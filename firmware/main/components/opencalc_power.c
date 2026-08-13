#include "opencalc_power.h"

#include "opencalc_config.h"

#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

#if CONFIG_PM_ENABLE
#include "esp_pm.h"
#endif

#if OPENCALC_ENABLE_WIFI && CONFIG_ESP_WIFI_ENABLED
#include "esp_wifi.h"
#endif

#if OPENCALC_ENABLE_BLUETOOTH && CONFIG_BT_ENABLED
#include "esp_bt.h"
#endif

static const char *TAG = "power";
static bool s_power_save = false;

static void stop_wireless_if_disabled(void)
{
#if OPENCALC_ENABLE_WIFI && CONFIG_ESP_WIFI_ENABLED
    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGW(TAG, "wifi stop: %s", esp_err_to_name(err));
    }
    err = esp_wifi_deinit();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGW(TAG, "wifi deinit: %s", esp_err_to_name(err));
    }
#endif

#if OPENCALC_ENABLE_BLUETOOTH && CONFIG_BT_ENABLED
    esp_bt_controller_status_t status = esp_bt_controller_get_status();
    if (status == ESP_BT_CONTROLLER_STATUS_ENABLED) {
        esp_err_t err = esp_bt_controller_disable();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "bt disable: %s", esp_err_to_name(err));
        }
        status = esp_bt_controller_get_status();
    }
    if (status == ESP_BT_CONTROLLER_STATUS_INITED) {
        esp_err_t err = esp_bt_controller_deinit();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "bt deinit: %s", esp_err_to_name(err));
        }
    }
#endif
}

void opencalc_power_init(void)
{
    stop_wireless_if_disabled();
    opencalc_power_set_power_save(false);
}

void opencalc_power_set_power_save(bool enabled)
{
    s_power_save = enabled;

#if CONFIG_PM_ENABLE
    esp_pm_config_t pm_config = {
        .max_freq_mhz = enabled ? OPENCALC_POWER_SAVE_CPU_MAX_MHZ : 160,
        .min_freq_mhz = 80,
        .light_sleep_enable = false,
    };
    esp_err_t err = esp_pm_configure(&pm_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "pm configure failed: %s", esp_err_to_name(err));
    }
#else
    ESP_LOGD(TAG, "CONFIG_PM_ENABLE is off; power-save CPU scaling disabled");
#endif
}

bool opencalc_power_get_power_save(void)
{
    return s_power_save;
}
