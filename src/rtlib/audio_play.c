/* The engine's audio library, 0x1003f036-0x10040555: playing, stopping and querying
 * MIDI and wave sounds (the sounds are made by audio_objs.c, mixed by the mixer). None
 * of this runs in Dominion, which plays its sound through WAVEPLAY.R32. Rewritten from
 * the original. */
#include "orig.h"
#include "crt.h"
#include "utils.h"
#include "audio.h"
#include "audio_int.h"

static const uint8_t g_midi_defaults[8]; /* 0x100887c8: no loops, no notification */

/* Converts len bytes of src (format in_fmt) into dst (out_fmt); returns the size
 * written. Without buffers only the size it would be. */
uint32_t __stdcall AudioResampleWave(const WaveFormat *out_fmt, const WaveFormat *in_fmt, uint32_t len, uint8_t *dst,
                                     const uint8_t *src)
{
    uint8_t *state = LocalAlloc(LMEM_FIXED, 9);
    if (!state)
        return 0;
    uint32_t n = resample_setup(state, out_fmt, in_fmt, len);
    if (dst && src)
        n = resample(state, dst, n, src, len, NULL);
    LocalFree(state);
    return n;
}

/* ---- MIDI sounds: a list of pointers at A_MIDIS ---- */

/* 0x1003f281: id if it is one of our MIDI sounds, else 0 */
uint8_t *__stdcall midi_find(uint8_t *m)
{
    for (uint8_t **p = (uint8_t **)(g_audio + A_MIDIS); p < F(uint8_t **, g_audio, A_MIDI_END); p++)
        if (*p == m)
            return m;
    return NULL;
}

/* 0x1003f150 */
void __stdcall midis_set_34(uint32_t v)
{
    for (uint8_t **p = (uint8_t **)(g_audio + A_MIDIS); p < F(uint8_t **, g_audio, A_MIDI_END); p++)
        F(uint32_t, *p, 0x34) = v;
}

/* 0x1003f19f: tells the window the sound ended (message 0x52c) */
void __stdcall midi_notify_end(uint8_t *m)
{
    if (F(uint32_t, m, M_NOTIFY) && F(HWND, g_audio, A_WND)) {
        F(uint32_t, m, M_NOTIFY) = 0;
        PostMessageA(F(HWND, g_audio, A_WND), 0x52c, F(WPARAM, m, 0x20), F(LPARAM, m, 0x24));
    }
}

/* 0x1003f1fc. original: the notification flag is cleared first, so no message is sent */
void __stdcall midi_stop(uint8_t *m)
{
    F(uint32_t, m, M_NOTIFY) = 0;
    if (F(uint8_t *, g_audio, A_MIDI_CURRENT) == m) {
        midi_stop_playing();
        midi_all_notes_off();
        midi_notify_end(m);
    }
}

void __stdcall AudioBreakMidiLoops(uint8_t *a, uint8_t *id)
{
    g_audio = a;
    uint8_t *m = midi_find(id);
    if (m)
        F(uint32_t, m, M_LOOPS_LEFT) = 0;
}

/* 0x1003f34c: params: loops (short), notify (byte at +2), its argument (+4) */
int __stdcall midi_start(uint8_t *m, const uint8_t *params)
{
    if (F(uint8_t *, g_audio, A_MIDI_CURRENT))
        midi_stop(F(uint8_t *, g_audio, A_MIDI_CURRENT));
    if (!midi_open())
        return 0;
    F(int32_t, m, M_LOOPS) = F(int16_t, params, 0);
    F(int32_t, m, M_LOOPS_LEFT) = F(int32_t, m, M_LOOPS);
    F(uint32_t, m, M_NOTIFY) = params[2] != 0;
    F(uint32_t, m, 0x24) = F(uint32_t, m, M_NOTIFY) ? F(uint32_t, params, 4) : 0;
    F(uint16_t, m, 4) = 0;
    F(uint8_t *, g_audio, A_MIDI_CURRENT) = m;
    if (!F(uint32_t, g_audio, A_PAUSED))
        midi_play_events(m);
    return 1;
}

int __stdcall AudioPlayMidi(uint8_t *a, uint8_t *id, const uint8_t *params)
{
    g_audio = a;
    uint8_t *m = midi_find(id);
    if (!m)
        return 0;
    if (!params)
        params = g_midi_defaults;
    if (!midi_start(m, params))
        F(uint32_t, m, M_NOTIFY) = 0;
    return 1;
}

/* settings: +0 change channels, +1 change +0x2a/+0x28 (from +6/+8), +2 channel mask,
 * +4 the channels to silence */
void __stdcall AudioSetMidi(uint8_t *a, uint8_t *id, const uint8_t *set)
{
    g_audio = a;
    uint8_t *m = id ? midi_find(id) : F(uint8_t *, a, A_MIDI_CURRENT);
    if (!m)
        return;
    if (set[1]) {
        F(uint16_t, m, 0x2a) = F(uint16_t, set, 6);
        F(uint16_t, m, 0x28) = F(uint16_t, set, 8);
    }
    if (set[0]) {
        uint32_t mask = F(uint16_t, set, 2), on = F(uint16_t, set, 4);
        for (uint32_t ch = 0; ch < 16; ch++, mask >>= 1, on >>= 1) {
            uint8_t v = on & 1;
            if ((mask & 1) && v != m[0x40 + ch]) {
                m[0x40 + ch] = v;
                if (v)
                    midi_channel_off((uint8_t)ch);
            }
        }
    }
}

void __stdcall AudioStopMidi(uint8_t *a, uint8_t *id)
{
    g_audio = a;
    uint8_t *m = id ? midi_find(id) : F(uint8_t *, a, A_MIDI_CURRENT);
    if (m)
        midi_stop(m);
}

/* ---- wave sounds: a list of pointers at A_WAVES ---- */

/* 0x1003fa44 */
uint8_t *__stdcall wave_find(uint8_t *s)
{
    for (uint8_t **p = (uint8_t **)(g_audio + A_WAVES); p < F(uint8_t **, g_audio, A_WAVES_END); p++)
        if (*p == s)
            return s;
    return NULL;
}

/* 0x1003f5e0: the sound starts playing */
void __stdcall wave_sound_started(uint8_t *s)
{
    if (!s[W_NO_PRIORITY] || F(int16_t, s, W_PRIORITY))
        F(int32_t, g_audio, A_PRIORITY) = F(int16_t, s, W_PRIORITY);
    F(uint16_t, s, W_LOOPS_LEFT) = F(uint16_t, s, W_LOOPS);
    if (!s[W_PLAYING])
        F(int16_t, g_audio, A_WAVES_ACTIVE)++;
    s[W_PLAYING] = 1;
}

/* 0x1003f742: when a sound with a random delay plays next */
void __stdcall wave_sound_schedule(uint8_t *s, uint32_t now)
{
    uint8_t delay = s[W_DELAY_RANGE] ? (uint8_t)(s[W_DELAY] + (crt_rand() & 0xff) % s[W_DELAY_RANGE]) : s[W_DELAY];
    F(uint32_t, s, W_TIME) = (uint32_t)delay * 1000 + now;
}

/* 0x1003f6ad: the sound stopped; a repeating one is scheduled again, else the window
 * hears of it in the service call */
void __stdcall wave_sound_ended(uint8_t *s)
{
    if (s[W_PLAYING]) {
        s[W_PLAYING] = 0;
        F(int16_t, g_audio, A_WAVES_ACTIVE)--;
    }
    if (s[W_REPEAT]) {
        wave_sound_schedule(s, timeGetTime());
    } else if (s[W_NOTIFY] && F(HWND, g_audio, A_WND)) {
        s[W_NOTIFY_DUE] = 1;
        s[W_NOTIFY] = 0;
    }
}

/* 0x1003f8f2: a fade-out over `ms` in `steps` steps; with `extend` it starts that much
 * before the end */
void __stdcall wave_fade_out(uint8_t *s, uint32_t ms, uint8_t steps, uint8_t extend)
{
    uint32_t n = F(uint32_t, g_audio, A_BYTES_PER_SEC) * ms / 1000;
    if (!F(uint32_t, g_audio, A_8BIT))
        n >>= 1;
    F(uint32_t, s, 0x30) = n / (uint32_t)(steps + 1);
    F(uint32_t, s, 0x34) = F(uint32_t, s, 0x30);
    if (F(uint32_t, s, 0x34))
        s[0x8c] = 1;
    if (extend) {
        uint32_t len = F(uint32_t, s, W_LENGTH) << (s[W_SHIFT] & 0x1f);
        if (!F(uint32_t, g_audio, A_8BIT))
            len >>= 1;
        len *= (uint32_t)(F(int16_t, s, W_LOOPS) + 1);
        if (s[0x8b])
            len -= (uint32_t)s[W_VOLUME] * F(uint32_t, s, 0x2c);
        if (n <= len)
            F(uint32_t, s, 0x34) += len - n;
    }
}

/* 0x1003f838: stops a sound, fading it out when params (fade ms) ask */
void __stdcall wave_sound_stop(uint8_t *s, const uint32_t *params)
{
    s[W_NOTIFY_DUE] = 0;
    s[W_NOTIFY] = 0;
    s[W_REPEAT] = 0;
    if (!s[W_PLAYING])
        return;
    if (!params || !params[0] || !s[W_FADE_VOLUME]) {
        wave_sound_ended(s);
        wave_reset();
    } else {
        wave_fade_out(s, params[0], s[W_FADE_VOLUME], 0);
    }
    AudioService(g_audio);
}

/* 0x1003f7bf */
void __stdcall wave_stop_all(const uint32_t *params)
{
    if (!params || !params[0]) {
        wave_reset();
        for (uint8_t **p = (uint8_t **)(g_audio + A_WAVES); p < F(uint8_t **, g_audio, A_WAVES_END); p++)
            wave_sound_stop(*p, NULL);
    } else {
        wave_fade_all(params[0]);
    }
}

void __stdcall AudioBreakWaveLoops(uint8_t *a, uint8_t *id)
{
    g_audio = a;
    uint8_t *s;
    if (!g_no_wave && (s = wave_find(id)))
        F(uint16_t, s, W_LOOPS_LEFT) = 0;
}

void __stdcall AudioClearNotifications(uint8_t *a)
{
    g_audio = a;
    for (uint8_t **p = (uint8_t **)(a + A_WAVES); p < F(uint8_t **, g_audio, A_WAVES_END); p++) {
        (*p)[W_NOTIFY_DUE] = 0;
        (*p)[W_NOTIFY] = 0;
    }
}

/* out (0x1a bytes): the format (4 dwords), length, bytes, volume, playing */
int __stdcall AudioGetWaveSoundInfo(uint8_t *a, uint8_t *id, uint32_t *out)
{
    g_audio = a;
    if (!out)
        return 0;
    uint8_t *s = wave_find(id);
    if (!s)
        return 0;
    memset(out, 0, 0x1a);
    out[0] = F(uint32_t, s, W_FORMAT);
    out[1] = F(uint32_t, s, W_FORMAT + 4);
    out[2] = F(uint32_t, s, W_FORMAT + 8);
    out[3] = F(uint32_t, s, W_FORMAT + 0xc);
    out[4] = F(uint32_t, s, W_LENGTH);
    out[5] = out[4];
    if (s[0x8d])
        out[5] <<= F(int16_t, s, W_FORMAT + 0xe) == 16 ? 2 : 1;
    ((uint8_t *)out)[0x18] = s[W_VOLUME];
    ((uint8_t *)out)[0x19] = s[W_PLAYING];
    return 1;
}

/* 0x1003fc38: the sound's length in ms */
uint32_t __stdcall wave_sound_ms(uint8_t *s)
{
    return (F(uint32_t, s, W_LENGTH) * 100 << (s[W_SHIFT] & 0x1f)) / (F(uint32_t, s, W_FORMAT + 8) / 10);
}

uint32_t __stdcall AudioGetWaveSoundTime(uint8_t *a, uint8_t *id)
{
    g_audio = a;
    uint8_t *s = wave_find(id);
    return s ? wave_sound_ms(s) : 0;
}

/* 0x10040047: a fade-in over `ms` in `steps` steps */
void __stdcall wave_fade_in(uint8_t *s, uint32_t ms, uint8_t steps)
{
    uint32_t n = F(uint32_t, g_audio, A_BYTES_PER_SEC) * ms / 1000;
    if (!F(uint32_t, g_audio, A_8BIT))
        n >>= 1;
    F(uint32_t, s, 0x28) = n / steps;
    F(uint32_t, s, 0x2c) = F(uint32_t, s, 0x28);
    if (F(uint32_t, s, 0x2c)) {
        s[W_FADE_VOLUME] = 0;
        s[0x8b] = 1;
    }
}

/* 0x100400c6: the sound's format differs from the device's */
int __stdcall wave_needs_resample(uint8_t *s)
{
    return !(F(int16_t, g_audio, A_FORMAT + 2) == F(int16_t, s, W_FORMAT + 2) &&
             F(int32_t, g_audio, A_FORMAT + 4) == F(int32_t, s, W_FORMAT + 4) &&
             F(int16_t, g_audio, A_FORMAT + 0xe) == F(int16_t, s, W_FORMAT + 0xe));
}

/* 0x10040130: a sound that plays after a random delay (params +8, +0xc: from, to ms) */
int __stdcall wave_sound_delay(uint8_t *s, const uint8_t *params)
{
    s[W_REPEAT] = 1;
    if (s[W_PLAYING])
        F(int16_t, g_audio, A_WAVES_ACTIVE)--;
    s[W_PLAYING] = 0;
    uint8_t from = (uint8_t)(F(uint32_t, params, 8) / 1000), to = (uint8_t)(F(uint32_t, params, 0xc) / 1000);
    s[W_DELAY_RANGE] = from < to ? (uint8_t)(to - from + 1) : 0;
    s[W_DELAY] = from;
    wave_sound_schedule(s, timeGetTime());
    return 1;
}

/* 0x100401f1: stops the sounds that give way to a new one (message 0x52e to the window
 * for those with a handler) */
void __cdecl wave_sounds_yield(void)
{
    for (uint8_t **p = (uint8_t **)(g_audio + A_WAVES); p < F(uint8_t **, g_audio, A_WAVES_END); p++) {
        uint8_t *s = *p;
        if (!s[W_PLAYING] || s[W_NO_PRIORITY])
            continue;
        F(int16_t, g_audio, A_WAVES_ACTIVE)--;
        s[W_REPEAT] = 0;
        s[W_PLAYING] = 0;
        uint32_t *msg;
        if (F(uint32_t, s, 0x50) && (msg = winmalloc(0xc, 0))) {
            msg[0] = F(uint32_t, s, 0x50);
            msg[1] = F(uint32_t, s, 0x70);
            msg[2] = (uint32_t)(uintptr_t)s;
            PostMessageA(F(HWND, g_audio, A_WND), 0x52e, 0, (LPARAM)msg);
        }
    }
    F(int32_t, g_audio, A_PRIORITY) = 0;
}

/* 0x1003fcff: params (0x1e bytes): fade-in ms, fade-out ms, random delay (+8, +0xc),
 * loops (+0x10), notification argument (+0x14), keep others (+0x18), notify (+0x19),
 * set volume (+0x1a) to +0x1c */
int __stdcall wave_sound_play(uint8_t *s, const uint8_t *params)
{
    if ((!s[W_NO_PRIORITY] || F(int16_t, s, W_PRIORITY)) && F(int16_t, g_audio, A_BUFFERS_QUEUED) &&
        F(int16_t, s, W_PRIORITY) < F(int32_t, g_audio, A_PRIORITY))
        return 1; /* something more important plays */
    s[0x8c] = 0;
    s[0x8b] = 0;
    s[W_NOTIFY_DUE] = 0;
    if (params[0x1a])
        s[W_VOLUME] = params[0x1c];
    s[W_FADE_VOLUME] = s[W_VOLUME];
    F(uint16_t, s, W_LOOPS) = (uint16_t)F(uint32_t, params, 0x10);
    F(uint16_t, s, W_LOOPS_LEFT) = F(uint16_t, s, W_LOOPS);
    if (!wave_open((const WAVEFORMATEX *)(s + W_FORMAT)))
        return 0;
    s[W_RESAMPLED] = (uint8_t)wave_needs_resample(s);
    if (s[W_RESAMPLED])
        DebugStr("Audio: %04x is resampled", s);
    if (s[W_RESAMPLED])
        resample_setup(s + 0x93, (const WaveFormat *)(g_audio + A_FORMAT), (const WaveFormat *)(s + W_FORMAT), F(uint32_t, s, W_LENGTH) << (s[W_SHIFT] & 0x1f));
    if (F(uint32_t, g_audio, A_FADE)) {
        F(uint32_t, g_audio, A_FADE) = 0;
        wave_set_device_volume(F(uint32_t, g_audio, 0x42));
    }
    if (F(uint32_t, params, 0) && s[W_VOLUME])
        wave_fade_in(s, F(uint32_t, params, 0), s[W_VOLUME]);
    if (F(uint32_t, params, 4) && s[W_VOLUME] && F(int16_t, s, W_LOOPS) >= 0)
        wave_fade_out(s, F(uint32_t, params, 4), s[W_VOLUME], 1);
    wave_sound_rewind(s);
    s[W_REPEAT] = 0;
    if (F(uint32_t, params, 8))
        return wave_sound_delay(s, params);
    if (!s[W_NO_PRIORITY])
        wave_sounds_yield();
    if (F(int16_t, g_audio, A_BUFFERS_QUEUED) && !params[0x18])
        wave_reset();
    wave_sound_started(s);
    if (F(uint32_t, s, 0x4c)) {
        s[W_NOTIFY] = 1;
        F(uint32_t, s, 0x78) = 0;
        F(uint32_t, s, W_END_TIME) += timeGetTime();
    } else {
        s[W_NOTIFY] = params[0x19] != 0;
        F(uint32_t, s, 0x78) = s[W_NOTIFY] ? F(uint32_t, params, 0x14) : 0;
        uint32_t ms = wave_sound_ms(s);
        F(uint32_t, s, W_END_TIME) = ms + timeGetTime();
    }
    return 1;
}

int __stdcall AudioPlayWave(uint8_t *a, uint8_t *id, const uint8_t *params)
{
    uint8_t defaults[0x20];
    g_audio = a;
    uint8_t *s = wave_find(id);
    if (!s)
        return 0;
    if (!params) {
        memset(defaults, 0, 0x1e);
        params = defaults;
    }
    if (!wave_sound_play(s, params)) {
        wave_sound_ended(s);
        return 0;
    }
    return 1;
}

void __stdcall AudioSetWaveSoundPriority(uint8_t *a, uint8_t *id, int16_t priority)
{
    g_audio = a;
    uint8_t *s = wave_find(id);
    if (s)
        F(int16_t, s, W_PRIORITY) = priority;
}

/* volume 0-16 */
int __stdcall AudioSetWaveSoundVolume(uint8_t *a, uint8_t *id, uint8_t volume)
{
    g_audio = a;
    uint8_t *s = wave_find(id);
    if (!s)
        return 0;
    s[W_VOLUME] = volume > 0x10 ? 0x10 : volume;
    s[W_FADE_VOLUME] = s[W_VOLUME];
    return 1;
}

/* id 0: every sound. params: fade-out ms */
void __stdcall AudioStopWave(uint8_t *a, uint8_t *id, const uint32_t *params)
{
    g_audio = a;
    if (g_no_wave)
        return;
    if (!id) {
        wave_stop_all(params);
    } else {
        uint8_t *s = wave_find(id);
        if (s)
            wave_sound_stop(s, params);
    }
}

/* 0x100404d7: a waveOut error in the debug output */
void __stdcall wave_report_error(const char *what)
{
    char text[0x100];
    const char *t = waveOutGetErrorTextA(F(uint16_t, g_audio, A_WAVE_ERROR), text, sizeof text) ? "Unknown Code" : text;
    DebugStr("Wave Error: %s", what);
    DebugStr(" %s", t);
}

/* 0x100404ab */
void __cdecl wave_check_error(const char *what)
{
    if (F(uint16_t, g_audio, A_WAVE_ERROR))
        wave_report_error(what);
}

/* 0x10040420: closes the wave device */
uint32_t __cdecl wave_close(void)
{
    if (!F(HWAVEOUT, g_audio, A_WAVEOUT))
        return 0;
    wave_stop_all(NULL);
    if (F(uint32_t, g_audio, 0x4e) != F(uint32_t, g_audio, 0x3e))
        wave_set_device_volume(F(uint32_t, g_audio, 0x4e));
    wave_buffers_free();
    F(uint16_t, g_audio, A_WAVE_ERROR) = (uint16_t)waveOutClose(F(HWAVEOUT, g_audio, A_WAVEOUT));
    wave_check_error("waveOutClose");
    F(HWAVEOUT, g_audio, A_WAVEOUT) = 0;
    return 0;
}
