#include "opencalc_persist.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "persist";
static nvs_handle_t s_nvs;
static bool s_ready = false;

void opencalc_persist_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS disabled: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_open("opencalc", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return;
    }

    s_ready = true;
}

uint32_t opencalc_persist_get_u32(const char *key, uint32_t fallback)
{
    if (!s_ready || key == NULL) {
        return fallback;
    }

    uint32_t value = fallback;
    esp_err_t err = nvs_get_u32(s_nvs, key, &value);
    return err == ESP_OK ? value : fallback;
}

bool opencalc_persist_set_u32(const char *key, uint32_t value)
{
    if (!s_ready || key == NULL) {
        return false;
    }

    esp_err_t err = nvs_set_u32(s_nvs, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(s_nvs);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS write %s failed: %s", key, esp_err_to_name(err));
        return false;
    }
    return true;
}

void opencalc_persist_factory_reset(void)
{
    if (!s_ready) {
        return;
    }

    esp_err_t err = nvs_erase_all(s_nvs);
    if (err == ESP_OK) {
        err = nvs_commit(s_nvs);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS factory reset failed: %s", esp_err_to_name(err));
    }
}
