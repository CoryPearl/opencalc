#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "opencalc_config.h"
#include "esp_log.h"
#include "storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#if OPENCALC_ENABLE_USB_CDC_CONSOLE && CONFIG_TINYUSB_CDC_ENABLED
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_console.h"
#endif

#include <stdio.h>
#include <sys/stat.h>

static const char *TAG = "usb";
static tinyusb_msc_storage_handle_t storage_handle = NULL;
static volatile bool storage_mount_complete = false;
static volatile bool storage_mount_failed = false;
static volatile bool storage_transition_pending = false;
static volatile tinyusb_msc_mount_point_t storage_mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP;
static volatile tinyusb_msc_mount_point_t storage_transition_target = TINYUSB_MSC_STORAGE_MOUNT_APP;
static EventGroupHandle_t storage_events = NULL;
static SemaphoreHandle_t storage_transition_lock = NULL;

#define STORAGE_EVENT_COMPLETE BIT0
#define STORAGE_EVENT_FAILED BIT1

static void storage_event_cb(tinyusb_msc_storage_handle_t handle, tinyusb_msc_event_t *event, void *arg)
{
    (void)handle;
    (void)arg;

    const char *owner = (event->mount_point == TINYUSB_MSC_STORAGE_MOUNT_USB) ? "usb" : "app";
    ESP_LOGI(TAG, "MSC event %d, owner=%s", event->id, owner);

    if (event->id == TINYUSB_MSC_EVENT_MOUNT_START) {
        storage_transition_pending = true;
        storage_mount_complete = false;
        storage_mount_failed = false;
        if (storage_events != NULL) {
            xEventGroupClearBits(storage_events, STORAGE_EVENT_COMPLETE | STORAGE_EVENT_FAILED);
        }
    } else if (event->id == TINYUSB_MSC_EVENT_MOUNT_COMPLETE) {
        storage_mount_point = event->mount_point;
        storage_mount_complete = true;
        storage_transition_pending = false;
        if (storage_events != NULL) xEventGroupSetBits(storage_events, STORAGE_EVENT_COMPLETE);
    } else if (event->id == TINYUSB_MSC_EVENT_MOUNT_FAILED ||
               event->id == TINYUSB_MSC_EVENT_FORMAT_REQUIRED ||
               event->id == TINYUSB_MSC_EVENT_FORMAT_FAILED) {
        storage_mount_failed = true;
        storage_transition_pending = false;
        if (storage_events != NULL) xEventGroupSetBits(storage_events, STORAGE_EVENT_FAILED);
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

    if (storage_events == NULL) storage_events = xEventGroupCreate();
    if (storage_transition_lock == NULL) storage_transition_lock = xSemaphoreCreateMutex();
    if (storage_events == NULL || storage_transition_lock == NULL) {
        ESP_LOGW(TAG, "USB storage disabled: ownership synchronization allocation failed");
        return;
    }

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
    storage_transition_pending = false;
    xEventGroupSetBits(storage_events, STORAGE_EVENT_COMPLETE);

    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(usb_event_cb);
    err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "TinyUSB disabled: %s", esp_err_to_name(err));
        return;
    }

#if OPENCALC_ENABLE_USB_CDC_CONSOLE
#if CONFIG_TINYUSB_CDC_ENABLED
    const tinyusb_config_cdcacm_t cdc_cfg = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = NULL,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL,
    };
    err = tinyusb_cdcacm_init(&cdc_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "TinyUSB CDC disabled: %s", esp_err_to_name(err));
        return;
    }

    err = tinyusb_console_init(TINYUSB_CDC_ACM_0);
    if (err == ESP_OK) {
        setvbuf(stdin, NULL, _IONBF, 0);
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
        vTaskDelay(pdMS_TO_TICKS(OPENCALC_USB_CDC_STARTUP_BANNER_DELAY_MS));
        printf("\nOpenCalc USB CDC console ready\n");
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
        printf("Storage and serial are active on this USB-C port.\n");
#else
        printf("Serial is active; storage is mounted by the app for runtime file access.\n");
#endif
        fflush(stdout);
        ESP_LOGI(TAG, "TinyUSB CDC console enabled");
    } else {
        ESP_LOGW(TAG, "TinyUSB CDC console redirect failed: %s", esp_err_to_name(err));
    }
#else
    ESP_LOGW(TAG, "USB CDC console requested but CONFIG_TINYUSB_CDC_ENABLED is off");
#endif
#endif
}

static bool set_storage_mount_point(tinyusb_msc_mount_point_t mount_point)
{
    if (!storage_handle || storage_events == NULL || storage_transition_lock == NULL) {
        return false;
    }
    TickType_t timeout_ticks = pdMS_TO_TICKS(OPENCALC_USB_OWNERSHIP_TIMEOUT_MS);
    if (timeout_ticks == 0) timeout_ticks = 1;
    if (xSemaphoreTake(storage_transition_lock, timeout_ticks) != pdTRUE) {
        ESP_LOGW(TAG, "USB storage ownership change is already in progress");
        return false;
    }

    bool success = false;
    if (storage_transition_pending) {
        ESP_LOGW(TAG, "Waiting for previous USB storage ownership change");
        EventBits_t pending_events = xEventGroupWaitBits(
            storage_events,
            STORAGE_EVENT_COMPLETE | STORAGE_EVENT_FAILED,
            pdTRUE,
            pdFALSE,
            timeout_ticks);
        if (storage_transition_pending ||
            (pending_events & (STORAGE_EVENT_COMPLETE | STORAGE_EVENT_FAILED)) == 0) {
            ESP_LOGW(TAG, "Previous USB storage ownership change is still pending");
            goto done;
        }
        if ((pending_events & STORAGE_EVENT_FAILED) != 0) {
            ESP_LOGW(TAG, "Previous USB storage ownership change failed");
            goto done;
        }
    }
    if (storage_mount_complete && !storage_mount_failed && storage_mount_point == mount_point) {
        success = true;
        goto done;
    }

    storage_mount_complete = false;
    storage_mount_failed = false;
    storage_transition_pending = true;
    storage_transition_target = mount_point;
    xEventGroupClearBits(storage_events, STORAGE_EVENT_COMPLETE | STORAGE_EVENT_FAILED);
    esp_err_t err = tinyusb_msc_set_storage_mount_point(storage_handle, mount_point);
    if (err != ESP_OK) {
        storage_transition_pending = false;
        ESP_LOGW(TAG, "USB storage ownership change failed: %s", esp_err_to_name(err));
        goto done;
    }

    EventBits_t events = xEventGroupWaitBits(
        storage_events,
        STORAGE_EVENT_COMPLETE | STORAGE_EVENT_FAILED,
        pdTRUE,
        pdFALSE,
        timeout_ticks);
    if ((events & STORAGE_EVENT_FAILED) != 0) {
        ESP_LOGW(TAG, "USB storage ownership change reported failure");
        goto done;
    }
    if ((events & STORAGE_EVENT_COMPLETE) == 0) {
        ESP_LOGW(TAG, "USB storage ownership change timed out after %u ms",
                 (unsigned)OPENCALC_USB_OWNERSHIP_TIMEOUT_MS);
        goto done;
    }

    success = !storage_transition_pending && storage_mount_complete && !storage_mount_failed &&
        storage_mount_point == mount_point && storage_transition_target == mount_point;
    if (!success) ESP_LOGW(TAG, "USB storage completed with an unexpected owner");

done:
    xSemaphoreGive(storage_transition_lock);
    return success;
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

bool usb_msc_app_storage_available(void)
{
    return storage_handle != NULL && !storage_transition_pending && storage_mount_complete &&
        !storage_mount_failed &&
        storage_mount_point == TINYUSB_MSC_STORAGE_MOUNT_APP;
}
