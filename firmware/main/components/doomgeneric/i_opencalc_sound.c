/* Lightweight OpenCalc Doom sound backend.
 *
 * Doom still dispatches its normal WAD sound events and channel lifecycle.
 * The first PAM8302A firmware uses compact synthesized voices instead of
 * caching/mixing the WAD's PCM lumps, keeping internal RAM use predictable.
 */

#include "i_sound.h"

#include "doomtype.h"
#include "opencalc_audio.h"
#include "opencalc_config.h"
#include "w_wad.h"

#include "esp_timer.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define OPENCALC_DOOM_SOUND_CHANNELS 8

static int64_t s_channel_ends[OPENCALC_DOOM_SOUND_CHANNELS];

static unsigned sound_name_hash(const char *name)
{
    unsigned hash = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)name; p && *p; p++) {
        hash ^= (unsigned)tolower(*p);
        hash *= 16777619u;
    }
    return hash;
}

static void sound_shape(const char *name,
                        uint16_t *frequency_hz,
                        uint16_t *duration_ms)
{
    unsigned hash = sound_name_hash(name ? name : "");
    *frequency_hz = (uint16_t)(180 + hash % 920);
    *duration_ms = (uint16_t)(45 + (hash >> 10) % 100);

    if (name == NULL) return;
    if (strstr(name, "pistol") || strstr(name, "shot") || strstr(name, "plasma")) {
        *frequency_hz = 150;
        *duration_ms = 75;
    } else if (strstr(name, "door") || strstr(name, "swtch")) {
        *frequency_hz = 360;
        *duration_ms = 120;
    } else if (strstr(name, "item") || strstr(name, "getpow")) {
        *frequency_hz = 960;
        *duration_ms = 90;
    } else if (strstr(name, "pain") || strstr(name, "death")) {
        *frequency_hz = 120;
        *duration_ms = 190;
    }
}

static boolean opencalc_sound_init(boolean use_sfx_prefix)
{
    (void)use_sfx_prefix;
    memset(s_channel_ends, 0, sizeof(s_channel_ends));
    return opencalc_audio_available();
}

static void opencalc_sound_shutdown(void)
{
    memset(s_channel_ends, 0, sizeof(s_channel_ends));
}

static int opencalc_sound_lump_num(sfxinfo_t *sfxinfo)
{
    if (sfxinfo == NULL) return -1;
    char lump_name[12];
    snprintf(lump_name, sizeof(lump_name), "ds%.8s", sfxinfo->name);
    return W_CheckNumForName(lump_name);
}

static void opencalc_sound_update(void)
{
}

static void opencalc_sound_update_params(int channel, int vol, int sep)
{
    (void)channel;
    (void)vol;
    (void)sep;
}

static int opencalc_sound_start(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
    (void)sep;
    if (channel < 0 || channel >= OPENCALC_DOOM_SOUND_CHANNELS || sfxinfo == NULL) return -1;

    uint16_t frequency_hz;
    uint16_t duration_ms;
    sound_shape(sfxinfo->name, &frequency_hz, &duration_ms);
    uint8_t volume_percent = (uint8_t)(vol <= 0 ? 1 : (vol >= 127 ? 100 : vol * 100 / 127));
    opencalc_audio_play_tone(frequency_hz, duration_ms, volume_percent);
    s_channel_ends[channel] = esp_timer_get_time() + (int64_t)duration_ms * 1000;
    return channel;
}

static void opencalc_sound_stop(int channel)
{
    if (channel >= 0 && channel < OPENCALC_DOOM_SOUND_CHANNELS) {
        s_channel_ends[channel] = 0;
    }
}

static boolean opencalc_sound_is_playing(int channel)
{
    return channel >= 0 && channel < OPENCALC_DOOM_SOUND_CHANNELS &&
           esp_timer_get_time() < s_channel_ends[channel];
}

static void opencalc_sound_precache(sfxinfo_t *sounds, int num_sounds)
{
    (void)sounds;
    (void)num_sounds;
}

static snddevice_t s_sound_devices[] = {
    SNDDEVICE_SB,
    SNDDEVICE_PAS,
    SNDDEVICE_GUS,
    SNDDEVICE_WAVEBLASTER,
    SNDDEVICE_SOUNDCANVAS,
    SNDDEVICE_AWE32,
};

sound_module_t DG_sound_module = {
    s_sound_devices,
    (int)(sizeof(s_sound_devices) / sizeof(s_sound_devices[0])),
    opencalc_sound_init,
    opencalc_sound_shutdown,
    opencalc_sound_lump_num,
    opencalc_sound_update,
    opencalc_sound_update_params,
    opencalc_sound_start,
    opencalc_sound_stop,
    opencalc_sound_is_playing,
    opencalc_sound_precache,
};
