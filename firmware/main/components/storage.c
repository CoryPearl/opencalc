#include "esp_partition.h"
#include "wear_levelling.h"
#include "esp_err.h"
#include "esp_log.h"
#include "ff.h"

static wl_handle_t wl_handle = WL_INVALID_HANDLE;
static const char *TAG = "storage";

void storage_set_label(void) {
    if (wl_handle == WL_INVALID_HANDLE) {
        return;
    }

    FRESULT res = f_setlabel("opencalc");
    if (res != FR_OK) {
        ESP_LOGW(TAG, "Failed to set volume label: %d", res);
        return;
    }

    ESP_LOGI(TAG, "Volume label set to opencalc");
}

void init_storage(void) {
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_FAT,
        "storage"
    );

    if (!partition) {
        ESP_LOGW(TAG, "Storage disabled: partition not found");
        return;
    }

    esp_err_t err = wl_mount(partition, &wl_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Storage disabled: %s", esp_err_to_name(err));
        wl_handle = WL_INVALID_HANDLE;
        return;
    }
}

wl_handle_t storage_wl_handle(void) {
    return wl_handle;
}
