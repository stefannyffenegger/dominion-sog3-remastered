/* The engine's audio library, 0x10040555-0x10041514: the waveOut device: picking a
 * format, opening, the eight 8 KB buffers, volume and fades, pause and reset. Never
 * opened in Dominion. Rewritten from the original. */
#include "orig.h"
#include "utils.h"
#include "audio.h"
#include "audio_int.h"

/* The formats a device may support (WAVE_FORMAT_* flags), in the order tried */
static const struct {
    uint16_t flag, rate;
    uint8_t channels, bits;
} g_formats[12] = {
    {WAVE_FORMAT_1M16, 11025, 1, 16}, {WAVE_FORMAT_1M08, 11025, 1, 8}, {WAVE_FORMAT_1S16, 11025, 2, 16},
    {WAVE_FORMAT_1S08, 11025, 2, 8},  {WAVE_FORMAT_2S16, 22050, 2, 16}, {WAVE_FORMAT_2S08, 22050, 2, 8},
    {WAVE_FORMAT_2M16, 22050, 1, 16}, {WAVE_FORMAT_2M08, 22050, 1, 8},  {WAVE_FORMAT_4S16, 44100, 2, 16},
    {WAVE_FORMAT_4S08, 44100, 2, 8},  {WAVE_FORMAT_4M16, 44100, 1, 16}, {WAVE_FORMAT_4M08, 44100, 1, 8},
};

/* 0x10079b38: the format when none is asked for, 22 kHz 16-bit mono */
static const WAVEFORMATEX g_default_format = {WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};

/* a buffer slot: the header, its data, bytes used */
#define BUF(i, off) F(uint32_t, g_audio, A_BUFFERS + (i) * 0xc + (off))

/* 0x10040555 */
uint32_t __cdecl wave_buffers_free(void)
{
    for (uint16_t i = 0; i < g_max_wave_buffers; i++) {
        WAVEHDR *h = (WAVEHDR *)(uintptr_t)BUF(i, 0);
        if (!h)
            continue;
        F(uint16_t, g_audio, A_WAVE_ERROR) =
            (uint16_t)waveOutUnprepareHeader(F(HWAVEOUT, g_audio, A_WAVEOUT), h, sizeof *h);
        wave_check_error("waveOutUnprepareHeader");
        winfree(h);
        BUF(i, 0) = 0;
    }
    memset(g_audio + A_BUFFERS, 0, 0x60);
    if (F(void *, g_audio, 0x2e))
        winfree(F(void *, g_audio, 0x2e));
    F(void *, g_audio, 0x2e) = NULL;
    F(uint32_t, g_audio, 0x36) = 0;
    if (F(void *, g_audio, 0x32))
        winfree(F(void *, g_audio, 0x32));
    F(void *, g_audio, 0x32) = NULL;
    F(uint32_t, g_audio, 0x3a) = 0;
    return 0;
}

/* 0x10041101: the device volume (left and right words) */
void __stdcall wave_set_device_volume(uint32_t v)
{
    if (!F(HWAVEOUT, g_audio, A_WAVEOUT) || g_no_wave_volume)
        return;
    F(uint32_t, g_audio, A_VOLUME_NOW) = v;
    /* original: the result is not kept, so the check sees the previous call's */
    waveOutSetVolume((HWAVEOUT)(uintptr_t)F(uint32_t, g_audio, A_DEVICE_ID), F(uint32_t, g_audio, A_VOLUME_NOW));
    wave_check_error("waveOutSetVolume");
    if (F(uint16_t, g_audio, A_WAVE_ERROR))
        g_no_wave_volume = 1;
}

/* 0x100406b7: fades every sound out over ms (in steps of 200 ms) */
void __stdcall wave_fade_all(uint32_t ms)
{
    if (g_no_wave || !F(HWAVEOUT, g_audio, A_WAVEOUT) || F(uint32_t, g_audio, A_FADE))
        return;
    F(uint16_t, g_audio, A_FADE_STEPS) = (uint16_t)(ms * 5 / 1000);
    if (!F(uint16_t, g_audio, A_FADE_STEPS)) {
        wave_set_device_volume(0);
        return;
    }
    F(uint32_t, g_audio, A_FADE) = 0xff;
    F(uint32_t, g_audio, A_FADE_FROM) = F(uint32_t, g_audio, A_VOLUME_NOW);
    uint32_t steps = F(uint16_t, g_audio, A_FADE_STEPS);
    F(uint16_t, g_audio, A_FADE_LEFT) = (uint16_t)(((steps >> 1) + F(uint16_t, g_audio, A_FADE_FROM)) / steps);
    F(uint16_t, g_audio, A_FADE_RIGHT) = (uint16_t)(((F(uint32_t, g_audio, A_FADE_FROM) >> 16) + (steps >> 1)) / steps);
    F(uint32_t, g_audio, A_FADE_TIME) = timeGetTime() + 200;
}

/* 0x100407d5 */
void __cdecl wave_apply_mute(void)
{
    if (!F(uint32_t, g_audio, A_MUTED)) {
        wave_set_device_volume(F(uint32_t, g_audio, A_VOLUME_SAVED));
    } else {
        F(uint32_t, g_audio, A_VOLUME_SAVED) = F(uint32_t, g_audio, A_VOLUME_NOW);
        wave_set_device_volume(0);
    }
}

/* 0x10040f73: the table entry that matches the format and the mask, or -1 */
int16_t __stdcall wave_format_index(const WAVEFORMATEX *f, uint16_t mask)
{
    for (int16_t i = 0; i < 12; i++)
        if ((mask & g_formats[i].flag) && g_formats[i].channels == f->nChannels && g_formats[i].bits == f->wBitsPerSample &&
            g_formats[i].rate == f->nSamplesPerSec)
            return i;
    return -1;
}

/* 0x10040e32: the format to open the device with: the sound's own, the same in 8 bits,
 * or the first the device and the settings allow; 0xffff for none */
uint16_t __stdcall wave_find_format(const WAVEFORMATEX *f)
{
    uint16_t mask = (uint16_t)(g_wave_caps->dwFormats &
                               (F(uint16_t, g_audio, A_FORMATS) ? F(uint16_t, g_audio, A_FORMATS) : g_wave_format_mask));
    if (!mask)
        return 0xffff;
    if (f) {
        int16_t i = wave_format_index(f, mask);
        if (i >= 0)
            return (uint16_t)i;
        WAVEFORMATEX f8;
        memcpy(&f8, f, 16);
        if (f8.wBitsPerSample == 16) {
            f8.wBitsPerSample = 8;
            i = wave_format_index(&f8, mask);
            if (i >= 0)
                return (uint16_t)i;
        }
    }
    for (int16_t i = 0; i < 12; i++)
        if (mask & g_formats[i].flag)
            return (uint16_t)i;
    return 0xffff;
}

/* 0x10040d05: sets the device format. The original's "none found" test compared the
 * 16-bit result with 0xffffffff, never true, and went on with table entry 65535; ours
 * gives up. */
int __stdcall wave_pick_format(const WAVEFORMATEX *f)
{
    uint32_t i = wave_find_format(f);
    if (i == 0xffff)
        return 0;
    WAVEFORMATEX *d = (WAVEFORMATEX *)(g_audio + A_FORMAT);
    F(uint16_t, g_audio, A_FORMAT_FLAG) = g_formats[i].flag;
    d->wFormatTag = WAVE_FORMAT_PCM;
    d->nChannels = g_formats[i].channels;
    d->nSamplesPerSec = g_formats[i].rate;
    d->wBitsPerSample = g_formats[i].bits;
    uint8_t align = (uint8_t)(g_formats[i].channels * (g_formats[i].bits >> 3));
    d->nBlockAlign = align;
    d->nAvgBytesPerSec = (uint32_t)g_formats[i].rate * align;
    return 1;
}

/* 0x10040b94: prepares the buffers */
uint32_t __cdecl wave_buffers_alloc(void)
{
    uint16_t i = 0;
    WAVEHDR *h;
    while (i < g_max_wave_buffers && (h = winmalloc(sizeof(WAVEHDR) + 0x2000, 0))) {
        memset(h, 0, sizeof *h);
        h->lpData = (LPSTR)(h + 1);
        h->dwBufferLength = 0x2000;
        F(uint16_t, g_audio, A_WAVE_ERROR) = (uint16_t)waveOutPrepareHeader(F(HWAVEOUT, g_audio, A_WAVEOUT), h, sizeof *h);
        wave_check_error("waveOutPrepareHeader");
        if (F(uint16_t, g_audio, A_WAVE_ERROR))
            break; /* original: the header is not freed */
        BUF(i, 0) = (uint32_t)(uintptr_t)h;
        BUF(i, 8) = 0;
        BUF(i, 4) = (uint32_t)(uintptr_t)h->lpData;
        i++;
    }
    F(uint8_t *, g_audio, A_BUFFERS_END) = g_audio + A_BUFFERS + i * 0xc;
    return g_max_wave_buffers == i || wave_buffers_free();
}

/* 0x10040851: opens the device for the format (if it isn't open in a fitting one) */
uint32_t __stdcall wave_device_open(const WAVEFORMATEX *f)
{
    if (g_no_wave)
        return 0;
    if (!f)
        f = &g_default_format;
    if (F(HWAVEOUT, g_audio, A_WAVEOUT) && F(int16_t, g_audio, A_WAVES_ACTIVE))
        return 1;
    if (F(HWAVEOUT, g_audio, A_WAVEOUT) && F(int16_t, g_audio, A_FORMAT + 2) == (int16_t)f->nChannels &&
        F(int16_t, g_audio, A_FORMAT + 0xe) == (int16_t)f->wBitsPerSample &&
        F(uint32_t, g_audio, A_FORMAT + 4) == f->nSamplesPerSec)
        return 1;
    if (F(HWAVEOUT, g_audio, A_WAVEOUT))
        wave_close();
    if (!F(int16_t, g_audio, A_WAVES_ACTIVE) && !wave_pick_format(f))
        return 0;
    F(uint16_t, g_audio, A_WAVE_ERROR) = (uint16_t)waveOutOpen((HWAVEOUT *)(g_audio + A_WAVEOUT), WAVE_MAPPER,
                                                               (WAVEFORMATEX *)(g_audio + A_FORMAT), 0, 0,
                                                               g_wave_sync ? WAVE_ALLOWSYNC : 0);
    wave_check_error("waveOutOpen");
    DebugStr("Audio: open_wave %likHz, %i-bit, %s, bps=%li", F(uint32_t, g_audio, A_FORMAT + 4),
             (int)F(int16_t, g_audio, A_FORMAT + 0xe), F(uint16_t, g_audio, A_FORMAT + 2) == 1 ? "mono" : "stereo",
             F(uint32_t, g_audio, A_BYTES_PER_SEC));
    if (F(uint16_t, g_audio, A_WAVE_ERROR) || !F(HWAVEOUT, g_audio, A_WAVEOUT))
        return 0;
    if (!wave_buffers_alloc())
        return 0;
    F(uint32_t, g_audio, A_8BIT) = F(int16_t, g_audio, A_FORMAT + 0xe) == 8;
    F(uint8_t *, g_audio, A_BUF_PLAY) = g_audio + A_BUFFERS;
    F(uint8_t *, g_audio, A_BUF_FILL) = F(uint8_t *, g_audio, A_BUF_PLAY);
    F(uint16_t, g_audio, A_WAVE_ERROR) = (uint16_t)waveOutGetID(F(HWAVEOUT, g_audio, A_WAVEOUT), (UINT *)(g_audio + A_DEVICE_ID));
    wave_check_error("waveOutGetID");
    if (F(int16_t, g_audio, A_DEVICE_ID) < 0)
        F(uint32_t, g_audio, A_DEVICE_ID) = 0;
    if (!g_no_wave_volume) {
        F(uint16_t, g_audio, A_WAVE_ERROR) = (uint16_t)waveOutGetVolume(
            (HWAVEOUT)(uintptr_t)F(uint32_t, g_audio, A_DEVICE_ID), (DWORD *)(g_audio + A_VOLUME_DEVICE));
        wave_check_error("waveOutGetVolume");
        if (F(uint16_t, g_audio, A_WAVE_ERROR))
            g_no_wave_volume = 1;
    }
    F(uint32_t, g_audio, A_VOLUME_NOW) = F(uint32_t, g_audio, A_VOLUME_DEVICE);
    F(uint32_t, g_audio, A_VOLUME) = (uint32_t)g_wave_volume << 16 | g_wave_volume;
    wave_set_device_volume(F(uint32_t, g_audio, A_VOLUME));
    if (F(uint32_t, g_audio, A_MUTED))
        wave_apply_mute();
    return 1;
}

/* 0x1004081f */
uint32_t __stdcall wave_open(const WAVEFORMATEX *f)
{
    return wave_device_open(f) ? 1 : wave_close();
}

/* 0x10041036: on resume the sounds' end times move by the time paused */
void __stdcall wave_pause(int pause)
{
    if (!F(HWAVEOUT, g_audio, A_WAVEOUT))
        return;
    DWORD now = timeGetTime();
    if (!pause) {
        uint32_t since = F(uint32_t, g_audio, A_PAUSE_TIME);
        for (uint8_t **p = (uint8_t **)(g_audio + A_WAVES); p < F(uint8_t **, g_audio, A_WAVES_END); p++)
            if (F(uint32_t, *p, W_END_TIME))
                F(uint32_t, *p, W_END_TIME) += now - since;
        waveOutRestart(F(HWAVEOUT, g_audio, A_WAVEOUT));
    } else {
        F(uint32_t, g_audio, A_PAUSE_TIME) = now;
        waveOutPause(F(HWAVEOUT, g_audio, A_WAVEOUT));
    }
}

/* 0x1004117d: stops the device and empties the buffers */
void __cdecl wave_reset(void)
{
    if (!F(HWAVEOUT, g_audio, A_WAVEOUT) || !F(int16_t, g_audio, A_BUFFERS_QUEUED))
        return;
    F(uint16_t, g_audio, A_WAVE_ERROR) = (uint16_t)waveOutReset(F(HWAVEOUT, g_audio, A_WAVEOUT));
    wave_check_error("waveOutReset");
    while (!(((WAVEHDR *)(uintptr_t)F(uint32_t, F(uint8_t *, g_audio, A_BUF_PLAY), 0))->dwFlags & WHDR_DONE))
        ;
    for (uint16_t i = 0; i < g_max_wave_buffers; i++)
        BUF(i, 8) = 0;
    F(uint8_t *, g_audio, A_BUF_FILL) = g_audio + A_BUFFERS;
    F(uint8_t *, g_audio, A_BUF_PLAY) = F(uint8_t *, g_audio, A_BUF_FILL);
    F(int16_t, g_audio, A_BUFFERS_QUEUED) = 0;
    F(int32_t, g_audio, A_PRIORITY) = 0;
}

void __stdcall AudioCloseWave(uint8_t *a)
{
    g_audio = a;
    wave_close();
}

/* 0x10041299 (RtGetWaveDeviceFormat): out gets the device format; returns the device.
 * original: the context argument is not used, the last one is */
uint32_t __stdcall wave_device_format(uint8_t *unused, uint32_t *out)
{
    (void)unused;
    uint8_t *a = g_audio;
    if (F(HWAVEOUT, g_audio, A_WAVEOUT) && out) {
        out[0] = F(uint32_t, g_audio, A_FORMAT);
        out[1] = F(uint32_t, a, A_FORMAT + 4);
        out[2] = F(uint32_t, a, A_FORMAT + 8);
        out[3] = F(uint32_t, a, A_FORMAT + 0xc);
    }
    return F(uint32_t, g_audio, A_WAVEOUT);
}

/* the volume set (or, opening the device for a moment, the device's) */
uint32_t __stdcall AudioGetWaveVolume(uint8_t *a)
{
    g_audio = a;
    if (F(uint32_t, a, A_VOLUME_SET))
        return F(uint32_t, a, A_VOLUME);
    if (F(HWAVEOUT, a, A_WAVEOUT))
        return F(uint32_t, a, A_VOLUME_NOW);
    if (!wave_device_open(NULL))
        return 0;
    uint32_t v = F(uint32_t, g_audio, A_VOLUME_NOW);
    wave_close();
    return v;
}

void __stdcall AudioOpenWave(uint8_t *a, const WAVEFORMATEX *f)
{
    g_audio = a;
    wave_open(f);
}

/* the WAVE_FORMAT_* flags allowed (0: the WaveFormatMask setting) */
void __stdcall AudioSetWaveFormats(uint8_t *a, uint16_t formats)
{
    g_audio = a;
    F(uint16_t, a, A_FORMATS) = formats;
}

/* remembered as WaveVolume in dominion.ini */
void __stdcall AudioSetWaveVolume(uint8_t *a, uint32_t v)
{
    g_audio = a;
    if (g_no_wave)
        return;
    F(uint32_t, a, A_VOLUME_SET) = 1;
    F(uint32_t, g_audio, A_VOLUME) = v;
    wave_set_device_volume(F(uint32_t, g_audio, A_VOLUME));
    g_wave_volume = (uint16_t)v;
    audio_save_wave_volume();
}

/* 0x100414b0 */
void __cdecl midi_sounds_free(void)
{
    for (uint8_t **p = (uint8_t **)(g_audio + A_MIDIS); p < F(uint8_t **, g_audio, A_MIDI_END); p++)
        midi_sound_free(*p);
    if (F(uint32_t, g_audio, A_MIDIOUT))
        midi_close();
}
