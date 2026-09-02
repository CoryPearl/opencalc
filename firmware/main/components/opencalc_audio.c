#include "opencalc_audio.h"

#include "opencalc_config.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/i2s_common.h"
#include "driver/i2s_pdm.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AUDIO_SHUTDOWN_GPIO GPIO_NUM_13
#define AUDIO_OUTPUT_GPIO GPIO_NUM_41
#define AUDIO_CHUNK_SAMPLES 256
#define AUDIO_RING_SAMPLES 4096
#define AUDIO_TONE_VOICES 4
#define AUDIO_TASK_STACK 4096

typedef struct {
    uint32_t phase;
    uint32_t phase_step;
    uint32_t samples_left;
    int16_t amplitude;
} tone_voice_t;

static const char *TAG = "audio";
static int s_volume_percent = OPENCALC_AUDIO_VOLUME_PERCENT;

#if OPENCALC_GAME_AUDIO_ENABLED
static i2s_chan_handle_t s_tx_channel;
static TaskHandle_t s_audio_task;
static portMUX_TYPE s_audio_lock = portMUX_INITIALIZER_UNLOCKED;
static EXT_RAM_BSS_ATTR int16_t s_pcm_ring[AUDIO_RING_SAMPLES];
static size_t s_pcm_read;
static size_t s_pcm_write;
static size_t s_pcm_count;
static tone_voice_t s_tones[AUDIO_TONE_VOICES];
static bool s_ready;
static bool s_game_active;

static void amp_set_enabled(bool enabled)
{
    /* PAM8302A SD is active low. */
    gpio_set_level(AUDIO_SHUTDOWN_GPIO, enabled ? 1 : 0);
}

static void clear_audio_state_locked(void)
{
    s_pcm_read = 0;
    s_pcm_write = 0;
    s_pcm_count = 0;
    memset(s_tones, 0, sizeof(s_tones));
}

static int16_t clamp_sample(int32_t sample)
{
    if (sample > INT16_MAX) return INT16_MAX;
    if (sample < INT16_MIN) return INT16_MIN;
    return (int16_t)sample;
}

static void fill_audio_chunk(int16_t samples[AUDIO_CHUNK_SAMPLES])
{
    portENTER_CRITICAL(&s_audio_lock);
    for (size_t i = 0; i < AUDIO_CHUNK_SAMPLES; i++) {
        int32_t mixed = 0;
        if (s_pcm_count > 0) {
            mixed = s_pcm_ring[s_pcm_read];
            s_pcm_read = (s_pcm_read + 1) % AUDIO_RING_SAMPLES;
            s_pcm_count--;
        }

        for (int voice = 0; voice < AUDIO_TONE_VOICES; voice++) {
            tone_voice_t *tone = &s_tones[voice];
            if (tone->samples_left == 0) continue;
            mixed += (tone->phase & 0x80000000u) ? tone->amplitude : -tone->amplitude;
            tone->phase += tone->phase_step;
            tone->samples_left--;
        }
        samples[i] = clamp_sample(mixed);
    }
    portEXIT_CRITICAL(&s_audio_lock);
}

static void audio_output_task(void *arg)
{
    (void)arg;
    int16_t samples[AUDIO_CHUNK_SAMPLES];
    bool amplifier_enabled = false;

    while (true) {
        bool active;
        portENTER_CRITICAL(&s_audio_lock);
        active = s_game_active && s_volume_percent > 0;
        portEXIT_CRITICAL(&s_audio_lock);

        if (!active) {
            if (amplifier_enabled) {
                amp_set_enabled(false);
                amplifier_enabled = false;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (!amplifier_enabled) {
            memset(samples, 0, sizeof(samples));
            size_t ignored = 0;
            (void)i2s_channel_write(s_tx_channel,
                                    samples,
                                    sizeof(samples),
                                    &ignored,
                                    100);
            vTaskDelay(pdMS_TO_TICKS(10));
            amp_set_enabled(true);
            amplifier_enabled = true;
        }

        fill_audio_chunk(samples);
        size_t bytes_written = 0;
        esp_err_t err = i2s_channel_write(s_tx_channel,
                                          samples,
                                          sizeof(samples),
                                          &bytes_written,
                                          100);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "PDM write failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
}
#endif

void opencalc_audio_init(void)
{
#if OPENCALC_USE_REAL_PCB && OPENCALC_USE_NEW_AUDIO_PCB
    /* Keep the amplifier hard-muted even when game audio is compiled out. */
    gpio_config_t shutdown_config = {
        .pin_bit_mask = 1ULL << AUDIO_SHUTDOWN_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&shutdown_config));
    gpio_set_level(AUDIO_SHUTDOWN_GPIO, 0);
#endif

#if OPENCALC_GAME_AUDIO_ENABLED
    if (s_ready) return;

    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel_config.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&channel_config, &s_tx_channel, NULL));

    i2s_pdm_tx_config_t pdm_config = {
        .clk_cfg = I2S_PDM_TX_CLK_DAC_DEFAULT_CONFIG(OPENCALC_AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_PDM_TX_SLOT_DAC_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                       I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = GPIO_NUM_NC,
            .dout = AUDIO_OUTPUT_GPIO,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_pdm_tx_mode(s_tx_channel, &pdm_config));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx_channel));

    BaseType_t task_ok = xTaskCreatePinnedToCore(audio_output_task,
                                                 "game_audio",
                                                 AUDIO_TASK_STACK,
                                                 NULL,
                                                 7,
                                                 &s_audio_task,
                                                 OPENCALC_WORKER_CORE);
    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to start audio output task");
        (void)i2s_channel_disable(s_tx_channel);
        (void)i2s_del_channel(s_tx_channel);
        s_tx_channel = NULL;
        return;
    }
    s_ready = true;
    ESP_LOGI(TAG,
             "PAM8302A game audio ready: PDM GPIO=%d, SD GPIO=%d, rate=%dHz",
             AUDIO_OUTPUT_GPIO,
             AUDIO_SHUTDOWN_GPIO,
             OPENCALC_AUDIO_SAMPLE_RATE);
#elif OPENCALC_USE_REAL_PCB && OPENCALC_USE_NEW_AUDIO_PCB
    ESP_LOGI(TAG, "PAM8302A held in shutdown; game audio disabled");
#else
    ESP_LOGI(TAG, "Game audio disabled (old PCB profile)");
#endif
}

bool opencalc_audio_available(void)
{
#if OPENCALC_GAME_AUDIO_ENABLED
    return s_ready;
#else
    return false;
#endif
}

void opencalc_audio_set_volume_percent(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

#if OPENCALC_GAME_AUDIO_ENABLED
    portENTER_CRITICAL(&s_audio_lock);
    s_volume_percent = percent;
    if (s_volume_percent == 0) {
        clear_audio_state_locked();
    }
    portEXIT_CRITICAL(&s_audio_lock);
#else
    s_volume_percent = percent;
#endif
}

int opencalc_audio_get_volume_percent(void)
{
#if OPENCALC_GAME_AUDIO_ENABLED
    int percent;
    portENTER_CRITICAL(&s_audio_lock);
    percent = s_volume_percent;
    portEXIT_CRITICAL(&s_audio_lock);
    return percent;
#else
    return s_volume_percent;
#endif
}

void opencalc_audio_game_begin(void)
{
#if OPENCALC_GAME_AUDIO_ENABLED
    if (!s_ready) return;
    portENTER_CRITICAL(&s_audio_lock);
    clear_audio_state_locked();
    s_game_active = true;
    portEXIT_CRITICAL(&s_audio_lock);
    xTaskNotifyGive(s_audio_task);
#endif
}

void opencalc_audio_game_end(void)
{
#if OPENCALC_GAME_AUDIO_ENABLED
    if (!s_ready) return;
    amp_set_enabled(false);
    portENTER_CRITICAL(&s_audio_lock);
    s_game_active = false;
    clear_audio_state_locked();
    portEXIT_CRITICAL(&s_audio_lock);
#endif
}

void opencalc_audio_play_tone(uint16_t frequency_hz,
                              uint16_t duration_ms,
                              uint8_t volume_percent)
{
#if OPENCALC_GAME_AUDIO_ENABLED
    if (!s_ready || !s_game_active || frequency_hz == 0 || duration_ms == 0) return;
    if (volume_percent > 100) volume_percent = 100;

    portENTER_CRITICAL(&s_audio_lock);
    int selected = 0;
    for (int i = 0; i < AUDIO_TONE_VOICES; i++) {
        if (s_tones[i].samples_left == 0 ||
            s_tones[i].samples_left < s_tones[selected].samples_left) {
            selected = i;
        }
    }

    tone_voice_t *tone = &s_tones[selected];
    tone->phase = 0;
    tone->phase_step = (uint32_t)(((uint64_t)frequency_hz << 32) /
                                  OPENCALC_AUDIO_SAMPLE_RATE);
    tone->samples_left = ((uint32_t)duration_ms * OPENCALC_AUDIO_SAMPLE_RATE) / 1000;
    tone->amplitude = (int16_t)((INT16_MAX / 5) *
                                s_volume_percent * volume_percent /
                                10000);
    portEXIT_CRITICAL(&s_audio_lock);
#else
    (void)frequency_hz;
    (void)duration_ms;
    (void)volume_percent;
#endif
}

size_t opencalc_audio_write_pcm16(const int16_t *samples,
                                  size_t sample_count,
                                  uint32_t timeout_ms)
{
#if OPENCALC_GAME_AUDIO_ENABLED
    if (!s_ready || !s_game_active || samples == NULL || sample_count == 0) return 0;

    TickType_t started = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    size_t queued = 0;
    while (queued < sample_count) {
        portENTER_CRITICAL(&s_audio_lock);
        size_t free_samples = AUDIO_RING_SAMPLES - s_pcm_count;
        size_t batch = sample_count - queued;
        if (batch > free_samples) batch = free_samples;
        for (size_t i = 0; i < batch; i++) {
            int32_t scaled = (int32_t)samples[queued + i] * s_volume_percent / 100;
            s_pcm_ring[s_pcm_write] = clamp_sample(scaled);
            s_pcm_write = (s_pcm_write + 1) % AUDIO_RING_SAMPLES;
        }
        s_pcm_count += batch;
        portEXIT_CRITICAL(&s_audio_lock);
        queued += batch;

        if (queued == sample_count || timeout_ms == 0) break;
        if ((xTaskGetTickCount() - started) >= timeout_ticks) break;
        vTaskDelay(1);
    }
    return queued;
#else
    (void)samples;
    (void)sample_count;
    (void)timeout_ms;
    return 0;
#endif
}
