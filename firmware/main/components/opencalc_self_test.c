#include "opencalc_self_test.h"

#include "board_init.h"
#include "opencalc_audio.h"
#include "opencalc_breakout.h"
#include "opencalc_config.h"
#include "opencalc_doom.h"
#include "opencalc_giac.h"
#include "opencalc_mario.h"
#include "opencalc_persist.h"
#include "opencalc_sensor_hub.h"
#include "opencalc_snake.h"
#include "opencalc_tetris.h"
#include "tiny-python.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sleep.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "self_test";

static bool cas_test(void)
{
#if !OPENCALC_ENABLE_GIAC_CAS
    ESP_LOGW(TAG, "CAS: skipped (disabled)");
    return true;
#else
    char output[192] = {0};
    for (int iteration = 0; iteration < 20; iteration++) {
        if (!opencalc_giac_eval("factor(x^4-1)", false, output, sizeof(output)) ||
            strstr(output, "x") == NULL) {
            ESP_LOGE(TAG, "CAS loop failed at iteration %d: %s", iteration, output);
            return false;
        }
    }

    opencalc_giac_status_t timeout = opencalc_giac_eval_timed(
        "factor(x^64-1)", false, output, sizeof(output), 1, NULL, NULL);
    if (timeout != OPENCALC_GIAC_TIMEOUT && timeout != OPENCALC_GIAC_OK) {
        ESP_LOGE(TAG, "CAS timeout path returned %d", (int)timeout);
        return false;
    }
    bool recovered = opencalc_giac_eval("2+2", false, output, sizeof(output)) &&
        strcmp(output, "4") == 0;
    ESP_LOGI(TAG, "CAS loop/recovery: %s", recovered ? "PASS" : "FAIL");
    opencalc_giac_reset();
    return recovered;
#endif
}

static bool script_test(void)
{
    py_t *py = heap_caps_calloc(1, sizeof(*py), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (py == NULL) return false;
    char output[96] = {0};
    static const char source[] =
        "def sum_to(n):\n"
        "    if n <= 0:\n"
        "        return 0\n"
        "    return n + sum_to(n - 1)\n"
        "values = []\n"
        "for i in range(6):\n"
        "    values.append(sum_to(i))\n"
        "print(values)\n";
    py_init(py);
    py_set_execution_limits(py, 10000, OPENCALC_SCRIPT_CALL_DEPTH_LIMIT);
    bool ok = py_run_source(py, source, output, sizeof(output)) &&
        strstr(output, "15") != NULL;
    if (!ok) ESP_LOGE(TAG, "Tiny Python: %s", py->error);
    py_deinit(py);
    heap_caps_free(py);
    ESP_LOGI(TAG, "Tiny Python recursion/containers: %s", ok ? "PASS" : "FAIL");
    return ok;
}

static bool persistence_test(void)
{
    static const char key[] = "selftest";
    char previous[48] = {0};
    bool had_previous = opencalc_persist_get_string(key, previous, sizeof(previous));
    char readback[48] = {0};
    bool nvs_ok = opencalc_persist_set_string(key, "opencalc-persistence-ok") &&
        opencalc_persist_get_string(key, readback, sizeof(readback)) &&
        strcmp(readback, "opencalc-persistence-ok") == 0;
    if (had_previous) (void)opencalc_persist_set_string(key, previous);
    else (void)opencalc_persist_erase(key);

    bool fat_ok = mkdir("/data/.opencalc", 0775) == 0 || errno == EEXIST;
    FILE *file = fat_ok ? fopen("/data/.opencalc/selftest.tmp", "wb") : NULL;
    static const char payload[] = "OpenCalc persistence test\n";
    fat_ok = file != NULL && fwrite(payload, 1, sizeof(payload), file) == sizeof(payload) &&
        fflush(file) == 0;
    if (file != NULL && fclose(file) != 0) fat_ok = false;
    file = fat_ok ? fopen("/data/.opencalc/selftest.tmp", "rb") : NULL;
    char buffer[sizeof(payload)] = {0};
    fat_ok = file != NULL && fread(buffer, 1, sizeof(buffer), file) == sizeof(buffer) &&
        memcmp(buffer, payload, sizeof(payload)) == 0;
    if (file != NULL) fclose(file);
    remove("/data/.opencalc/selftest.tmp");
    ESP_LOGI(TAG, "Persistence NVS/FAT: %s", nvs_ok && fat_ok ? "PASS" : "FAIL");
    return nvs_ok && fat_ok;
}

static bool sleep_test(void)
{
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (esp_sleep_enable_timer_wakeup(20000) != ESP_OK) return false;
    esp_err_t error = esp_light_sleep_start();
    uint32_t causes = esp_sleep_get_wakeup_causes();
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    bool ok = error == ESP_OK && (causes & BIT(ESP_SLEEP_WAKEUP_TIMER)) != 0;
    ESP_LOGI(TAG, "Timed light sleep/wake: %s", ok ? "PASS" : "FAIL");
    return ok;
}

static bool keypad_test(void)
{
    bool map_ok = true;
    for (int row = 0; row < BOARD_KEYPAD_ROWS; row++) {
        for (int col = 0; col < BOARD_KEYPAD_COLS; col++) {
            const board_key_t *key = board_keypad_key_at(row, col);
            if (key == NULL || key->normal == NULL || key->normal[0] == '\0') map_ok = false;
        }
    }
    bool pressed[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
    bool stuck_key = board_keypad_scan_matrix(pressed);
    bool ok = map_ok && !stuck_key;
    ESP_LOGI(TAG, "Keypad map/idle scan: %s%s", ok ? "PASS" : "FAIL",
             stuck_key ? " (key held or row stuck low)" : "");
    return ok;
}

static bool games_test(void)
{
    bool ok = true;
    opencalc_tetris_init();
    opencalc_tetris_enter();
    ok = ok && opencalc_tetris_active() && opencalc_tetris_press_button_number(46) &&
        !opencalc_tetris_active();
    opencalc_snake_init();
    opencalc_snake_enter();
    ok = ok && opencalc_snake_active() && opencalc_snake_press_button_number(46) &&
        !opencalc_snake_active();
    opencalc_breakout_init();
    opencalc_breakout_enter();
    ok = ok && opencalc_breakout_active() && opencalc_breakout_press_button_number(46) &&
        !opencalc_breakout_active();

    if (opencalc_mario_rom_available()) {
        opencalc_mario_init();
        opencalc_mario_enter();
        ok = ok && opencalc_mario_active() && opencalc_mario_press_button_number(46) &&
            !opencalc_mario_active();
    } else {
        ESP_LOGW(TAG, "Mario launch/exit skipped: /data/mario.nes missing");
    }
    ESP_LOGI(TAG, "Game launch/exit: %s; Doom WAD: %s",
             ok ? "PASS" : "FAIL", opencalc_doom_wad_available() ? "ready" : "missing");
    return ok;
}

static bool board_profile_test(void)
{
#if OPENCALC_ENABLE_SCIENTIFIC_IO
    bool sensor_ok = opencalc_sensor_hub_available() &&
        opencalc_sensor_i2c_present(OPENCALC_MCP23017_I2C_ADDRESS) &&
        opencalc_sensor_i2c_present(OPENCALC_ADS1115_I2C_ADDRESS);
#else
    bool sensor_ok = true;
#endif
#if OPENCALC_GAME_AUDIO_ENABLED
    bool audio_ok = opencalc_audio_available();
#else
    bool audio_ok = true;
#endif
    ESP_LOGI(TAG, "Configured sensor/audio profile: %s", sensor_ok && audio_ok ? "PASS" : "FAIL");
    return sensor_ok && audio_ok;
}

int opencalc_self_test_run(void)
{
    ESP_LOGI(TAG, "===== OpenCalc on-device self-test start =====");
    int failures = 0;
    if (!cas_test()) failures++;
    if (!script_test()) failures++;
    if (!persistence_test()) failures++;
    if (!sleep_test()) failures++;
    if (!keypad_test()) failures++;
    if (!games_test()) failures++;
    if (!board_profile_test()) failures++;
    ESP_LOGI(TAG, "===== self-test complete: %d failure(s) =====", failures);
    return failures;
}
