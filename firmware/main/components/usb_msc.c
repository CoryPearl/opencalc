#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "esp_log.h"
#include "storage.h"

#include <sys/stat.h>

static const char *TAG = "usb";
static tinyusb_msc_storage_handle_t storage_handle = NULL;
static volatile bool storage_mount_complete = false;
static volatile bool storage_mount_failed = false;
static volatile tinyusb_msc_mount_point_t storage_mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP;

static void storage_event_cb(tinyusb_msc_storage_handle_t handle, tinyusb_msc_event_t *event, void *arg)
{
    (void)handle;
    (void)arg;

    const char *owner = (event->mount_point == TINYUSB_MSC_STORAGE_MOUNT_USB) ? "usb" : "app";
    ESP_LOGI(TAG, "MSC event %d, owner=%s", event->id, owner);

    if (event->id == TINYUSB_MSC_EVENT_MOUNT_START) {
        storage_mount_complete = false;
        storage_mount_failed = false;
    } else if (event->id == TINYUSB_MSC_EVENT_MOUNT_COMPLETE) {
        storage_mount_point = event->mount_point;
        storage_mount_complete = true;
    } else if (event->id == TINYUSB_MSC_EVENT_MOUNT_FAILED ||
               event->id == TINYUSB_MSC_EVENT_FORMAT_REQUIRED ||
               event->id == TINYUSB_MSC_EVENT_FORMAT_FAILED) {
        storage_mount_failed = true;
    }
}

static void usb_event_cb(tinyusb_event_t *event, void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "USB event %d", event->id);
}

void init_usb_msc(void)
{
    wl_handle_t wl_handle = storage_wl_handle();
    if (wl_handle == WL_INVALID_HANDLE) {
        ESP_LOGW(TAG, "USB storage disabled: no storage partition");
        return;
    }

    ESP_LOGI(TAG, "Starting TinyUSB MSC");

    const tinyusb_msc_driver_config_t driver_cfg = {
        .callback = storage_event_cb,
    };
    esp_err_t err = tinyusb_msc_install_driver(&driver_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "TinyUSB MSC disabled: %s", esp_err_to_name(err));
        return;
    }

    const tinyusb_msc_storage_config_t storage_cfg = {
        .medium.wl_handle = wl_handle,
        .fat_fs = {
            .base_path = "/data",
            .config = {
                .format_if_mount_failed = false,
                .max_files = 8,
                .allocation_unit_size = 4096,
            },
        },
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP,
    };
    err = tinyusb_msc_new_storage_spiflash(&storage_cfg, &storage_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "USB storage disabled: %s", esp_err_to_name(err));
        return;
    }
    storage_mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP;
    storage_mount_complete = true;
    storage_mount_failed = false;

    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(usb_event_cb);
    err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "TinyUSB disabled: %s", esp_err_to_name(err));
    }
}

static bool set_storage_mount_point(tinyusb_msc_mount_point_t mount_point)
{
    if (!storage_handle) {
        return false;
    }
    if (storage_mount_complete && !storage_mount_failed && storage_mount_point == mount_point) {
        return true;
    }

    storage_mount_complete = false;
    storage_mount_failed = false;
    esp_err_t err = tinyusb_msc_set_storage_mount_point(storage_handle, mount_point);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "USB storage ownership change failed: %s", esp_err_to_name(err));
        return false;
    }

    if (storage_mount_failed || !storage_mount_complete || storage_mount_point != mount_point) {
        ESP_LOGW(TAG, "USB storage ownership change did not complete");
        return false;
    }

    return true;
}

bool usb_msc_mount_usb(void)
{
    if (!set_storage_mount_point(TINYUSB_MSC_STORAGE_MOUNT_USB)) {
        return false;
    }
    ESP_LOGI(TAG, "USB storage exported to host");
    return true;
}

bool usb_msc_mount_app(void)
{
    if (!set_storage_mount_point(TINYUSB_MSC_STORAGE_MOUNT_APP)) {
        return false;
    }
    ESP_LOGI(TAG, "USB storage mounted for app");
    return true;
}
