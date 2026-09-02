#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void opencalc_audio_init(void);
bool opencalc_audio_available(void);

/* Exactly one game owns audio at a time. These calls also control amp SD. */
void opencalc_audio_game_begin(void);
void opencalc_audio_game_end(void);

/* Non-blocking synthesized effect. Frequency zero is ignored. */
void opencalc_audio_play_tone(uint16_t frequency_hz,
                              uint16_t duration_ms,
                              uint8_t volume_percent);

/* Queue signed mono PCM at OPENCALC_AUDIO_SAMPLE_RATE. */
size_t opencalc_audio_write_pcm16(const int16_t *samples,
                                  size_t sample_count,
                                  uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
