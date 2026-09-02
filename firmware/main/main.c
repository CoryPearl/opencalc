// Cory Pearl
// 05/22/26

// See config.h for settings

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_timer.h"

#include "components/board_init.h"
#include "components/opencalc_audio.h"
#include "components/opencalc_config.h"
#include "components/opencalc_persist.h"
#include "components/opencalc_power.h"
#include "components/opencalc_ui.h"
#include "components/storage.h"
#include "components/usb_msc.h"

#define OPENCALC_FRAME_PERIOD_US (1000000LL / OPENCALC_TARGET_FPS)

#if OPENCALC_ENABLE_SERIAL_BUTTON_INPUT
static void serial_button_task(void *arg) 
{
    (void)arg;

    printf("\nSerial button input enabled. Type a number 1-50, then Enter.\n");
    printf("Each entry is normal / 2nd / alpha where applicable.\n");
    printf("01 y=/plot      02 window/tblset 03 zoom/format  04 trace/calc   05 graph/table\n");
    printf("06 2nd          07 mode/quit     08 stat/list    09 left         10 up/bright+\n");
    printf("11 alpha/lock   12 XthetaTn      13 back         14 down/bright- 15 right\n");
    printf("16 math/ops/A   17 []/[]/frac/B  18 prgm/scripts/C 19 vars/conv/D 20 del/clear/E\n");
    printf("21 sqrt/root/F  22 sin/asin/csc  23 cos/acos/sec  24 tan/atan/cot 25 pi/e/J\n");
    printf("26 ^2/^[]/K     27 comma/E/L     28 (/{/M       29 )/}/N       30 +/O\n");
    printf("31 log/10^/P    32 7/Q           33 8/R         34 9/S         35 */T\n");
    printf("36 ln/e^/U      37 4/V           38 5/W         39 6/X         40 -/Y\n");
    printf("41 sto/get/Z    42 1/G           43 2/H         44 3/I         45 / / %%\n");
    printf("46 on/home/off  47 0             48 ./#           49 (-)/ans     50 enter\n\n");
    printf("button> ");
    fflush(stdout);

    char line[32];
    size_t line_len = 0;
    while (true) {
        int ch = getchar();
        if (ch == EOF) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            if (line_len == 0) {
                continue;
            }
            printf("\n");
            line[line_len] = '\0';
            line_len = 0;
        } else if (ch == '\b' || ch == 0x7f) {
            if (line_len > 0) {
                line_len--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        } else {
            if (line_len + 1 < sizeof(line)) {
                line[line_len++] = (char)ch;
                putchar(ch);
                fflush(stdout);
            }
            continue;
        }

        char *p = line;
        while (isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0') {
            continue;
        }

        int number = atoi(p);
        opencalc_ui_queue_button_number(number);
        printf("button> ");
        fflush(stdout);
    }
}
#endif

static void opencalc_ui_task(void *arg)
{
    (void)arg;

    board_set_event_task(xTaskGetCurrentTaskHandle());
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
    usb_msc_mount_usb();
#endif

    opencalc_ui_init();
    opencalc_ui_draw();

#if OPENCALC_DEBUG_LOG_FPS
    int frame_count = 0;
    int64_t last_fps_log_us = esp_timer_get_time();
#endif

    while (true) {
        int64_t frame_start_us = esp_timer_get_time();

        if (opencalc_ui_doom_active()) {
            opencalc_ui_tick_doom();
        } else {
            opencalc_ui_tick();
        }

    #if OPENCALC_ENABLE_SERIAL_BUTTON_INPUT
        opencalc_ui_handle_serial_buttons();
    #endif

        opencalc_ui_handle_keypad_interrupt();
        // opencalc_ui_handle_touch_interrupt();

#if OPENCALC_DEBUG_LOG_FPS
        frame_count++;
        int64_t now_us = esp_timer_get_time();
        if (now_us - last_fps_log_us >= 1000000) {
            printf("fps=%d\n", frame_count);
            fflush(stdout);
            frame_count = 0;
            last_fps_log_us = now_us;
        }
#endif

        /* Rendering and LCD DMA time count toward the frame budget. Sleeping a
         * full frame period after rendering was limiting games to about 10 FPS. */
        int64_t frame_elapsed_us = esp_timer_get_time() - frame_start_us;
        int64_t frame_wait_us = OPENCALC_FRAME_PERIOD_US - frame_elapsed_us;
        if (frame_wait_us > 0) {
            TickType_t wait_ticks = pdMS_TO_TICKS((frame_wait_us + 999) / 1000);
            if (wait_ticks > 0) {
                ulTaskNotifyTake(pdTRUE, wait_ticks);
            } else {
                taskYIELD();
            }
        } else {
            taskYIELD();
        }
    }
}

void app_main(void) {
    opencalc_persist_init();
    opencalc_power_init();
    init_storage();
    init_usb_msc();
    storage_set_label();

    board_init();
    opencalc_audio_init();

#if OPENCALC_ENABLE_SERIAL_BUTTON_INPUT
    BaseType_t serial_ok = xTaskCreatePinnedToCore(
        serial_button_task,
        "serial_buttons",
        16384,
        NULL,
        5,
        NULL,
        OPENCALC_WORKER_CORE);
    if (serial_ok != pdPASS) {
        printf("ERROR: failed to start serial button task\n");
    }
#endif

    BaseType_t ui_ok = xTaskCreatePinnedToCore(
        opencalc_ui_task,
        "opencalc_ui",
        OPENCALC_UI_TASK_STACK,
        NULL,
        6,
        NULL,
        OPENCALC_UI_CORE);
    if (ui_ok != pdPASS) {
        printf("ERROR: failed to start UI task; running UI on app_main\n");
        opencalc_ui_task(NULL);
    }
}
