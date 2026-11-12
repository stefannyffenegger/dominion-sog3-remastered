/* The engine's audio library, 0x100447fb-0x1004504c: the wave mixer's main loop (fill
 * the next free 8 KB buffer with every playing sound, hand it to waveOut), whole-device
 * fades, and the MIDI sequencer (events sent from a one-shot multimedia timer). Unused
 * by Dominion. Rewritten from the original. */
#include "orig.h"
#include "utils.h"
#include "audio.h"
#include "audio_int.h"

#define SLOT(p, off) F(uint32_t, (p), (off)) /* a buffer slot: WAVEHDR *, data, used */

/* ---- device fades, a step every 200 ms ---- */

/* 0x1004485c */
void __cdecl wave_fade_up(void)
{
    F(uint32_t, g_audio, A_VOLUME_NOW) =
        wave_volume_step(F(uint32_t, g_audio, A_VOLUME_NOW), F(int16_t, g_audio, A_FADE_LEFT),
                    F(int16_t, g_audio, A_FADE_RIGHT), F(uint32_t, g_audio, A_FADE_FROM));
    F(int16_t, g_audio, A_FADE_STEPS)--;
    if (!F(int16_t, g_audio, A_FADE_STEPS) || F(uint32_t, g_audio, A_VOLUME_NOW) == F(uint32_t, g_audio, A_FADE_FROM)) {
        F(uint32_t, g_audio, A_FADE) = 0;
        F(uint32_t, g_audio, A_VOLUME_NOW) = F(uint32_t, g_audio, A_FADE_FROM);
    }
    wave_set_device_volume(F(uint32_t, g_audio, A_VOLUME_NOW));
}

/* 0x100448fe: at the end the sounds stop and the volume comes back */
void __cdecl wave_fade_down(void)
{
    F(uint32_t, g_audio, A_VOLUME_NOW) =
        wave_volume_step(F(uint32_t, g_audio, A_VOLUME_NOW), (int16_t)-F(int16_t, g_audio, A_FADE_LEFT),
                    (int16_t)-F(int16_t, g_audio, A_FADE_RIGHT), F(uint32_t, g_audio, A_FADE_FROM));
    F(int16_t, g_audio, A_FADE_STEPS)--;
    if (!F(int16_t, g_audio, A_FADE_STEPS) || !F(uint32_t, g_audio, A_VOLUME_NOW)) {
        F(uint32_t, g_audio, A_FADE) = 0;
        wave_stop_all(NULL);
        F(uint32_t, g_audio, A_VOLUME_NOW) = F(uint32_t, g_audio, A_FADE_FROM);
    }
    wave_set_device_volume(F(uint32_t, g_audio, A_VOLUME_NOW));
}

/* 0x100447fb: A_FADE 1 fades up, else down */
void __cdecl wave_fade_step(void)
{
    DWORD now = timeGetTime();
    if (F(uint32_t, g_audio, A_FADE_TIME) > now)
        return;
    F(uint32_t, g_audio, A_FADE_TIME) = now + 200;
    if (F(uint32_t, g_audio, A_FADE) == 1)
        wave_fade_up();
    else
        wave_fade_down();
}

/* ---- mixing ---- */

/* 0x10044aae: takes back the oldest buffer once the device is done with it */
void __cdecl wave_reclaim(void)
{
    uint8_t *slot = F(uint8_t *, g_audio, A_BUF_PLAY);
    if (!(((WAVEHDR *)(uintptr_t)SLOT(slot, 0))->dwFlags & WHDR_DONE))
        return;
    SLOT(slot, 8) = 0;
    F(int16_t, g_audio, A_BUFFERS_QUEUED)--;
    slot += 0xc;
    if (slot >= F(uint8_t *, g_audio, A_BUFFERS_END))
        slot = g_audio + A_BUFFERS;
    F(uint8_t *, g_audio, A_BUF_PLAY) = slot;
}

/* 0x10044b26: mixes the playing sounds into the next buffer, 16-bit; returns how many */
uint32_t __cdecl wave_mix16(void)
{
    uint32_t n = 0, longest = 0;
    WAVEHDR *h = (WAVEHDR *)(uintptr_t)SLOT(F(uint8_t *, g_audio, A_BUF_FILL), 0);
    int16_t *out = (int16_t *)h->lpData;
    for (uint8_t **p = (uint8_t **)(g_audio + A_WAVES); p < F(uint8_t **, g_audio, A_WAVES_END); p++) {
        uint8_t *s = *p;
        if (!s[W_PLAYING] || !s[0x82])
            continue;
        n++;
        uint32_t len = mix_sound16(s, out, 0x2000, n) & 0xffff;
        if (longest < len)
            longest = len;
        if (F(int32_t, g_audio, A_PRIORITY) < F(int16_t, s, W_PRIORITY))
            F(int32_t, g_audio, A_PRIORITY) = F(int16_t, s, W_PRIORITY);
    }
    h->dwBufferLength = longest;
    return n & 0xffff;
}

/* 0x10044c28: the same for an 8-bit device */
uint16_t __cdecl wave_mix8(void)
{
    uint16_t n = 0;
    uint32_t longest = 0;
    WAVEHDR *h = (WAVEHDR *)(uintptr_t)SLOT(F(uint8_t *, g_audio, A_BUF_FILL), 0);
    uint8_t *out = (uint8_t *)h->lpData;
    for (uint8_t **p = (uint8_t **)(g_audio + A_WAVES); p < F(uint8_t **, g_audio, A_WAVES_END); p++) {
        uint8_t *s = *p;
        if (!s[W_PLAYING] || !s[0x82])
            continue;
        n++;
        uint32_t len = mix_sound8(s, out, 0x2000, n) & 0xffff;
        if (longest < len)
            longest = len;
        if (F(int32_t, g_audio, A_PRIORITY) < F(int16_t, s, W_PRIORITY))
            F(int32_t, g_audio, A_PRIORITY) = F(int16_t, s, W_PRIORITY);
    }
    h->dwBufferLength = longest;
    return n;
}

/* 0x100449a3: fills and queues the next free buffer; 0 when there was nothing to do */
uint32_t __cdecl wave_mix(void)
{
    if (F(int16_t, g_audio, A_BUFFERS_QUEUED))
        wave_reclaim();
    uint8_t *slot = F(uint8_t *, g_audio, A_BUF_FILL);
    if (!F(int16_t, g_audio, A_WAVES_ACTIVE) || SLOT(slot, 8))
        return 0;
    uint16_t n = F(uint32_t, g_audio, A_8BIT) ? wave_mix8() : (uint16_t)wave_mix16();
    if (!n)
        return 0;
    slot = F(uint8_t *, g_audio, A_BUF_FILL);
    SLOT(slot, 8) = 1;
    WAVEHDR *h = (WAVEHDR *)(uintptr_t)SLOT(slot, 0);
    h->dwFlags &= 0xfffffff2;
    waveOutWrite(F(HWAVEOUT, g_audio, A_WAVEOUT), h, sizeof *h);
    F(int16_t, g_audio, A_BUFFERS_QUEUED)++;
    slot += 0xc;
    if (slot >= F(uint8_t *, g_audio, A_BUFFERS_END))
        slot = g_audio + A_BUFFERS;
    F(uint8_t *, g_audio, A_BUF_FILL) = slot;
    return 1;
}

/* 0x10044d40: the sound's end time passed: message 0x52c (or 0x52d with a handler) */
void __stdcall wave_sound_timeout(uint8_t *s)
{
    if (!F(uint32_t, s, 0x4c)) {
        PostMessageA(F(HWND, g_audio, A_WND), 0x52c, F(WPARAM, s, 0x74), F(LPARAM, s, 0x78));
    } else {
        uint32_t *msg = winmalloc(0xc, 0);
        if (msg) {
            msg[0] = F(uint32_t, s, 0x4c);
            msg[1] = F(uint32_t, s, 0x70);
            msg[2] = (uint32_t)(uintptr_t)s;
            PostMessageA(F(HWND, g_audio, A_WND), 0x52d, 0, (LPARAM)msg);
        }
    }
    s[W_NOTIFY_DUE] = 0;
}

/* ---- the MIDI sequencer ---- */

/* 0x10044f43: at the end: again (loops left; negative = forever) or stop */
int __stdcall midi_loop(uint8_t *m)
{
    if (!F(int32_t, m, M_LOOPS_LEFT)) {
        midi_notify_end(m);
        F(uint8_t *, g_audio, A_MIDI_CURRENT) = NULL;
        return 0;
    }
    if (F(int32_t, m, M_LOOPS_LEFT) > 0)
        F(int32_t, m, M_LOOPS_LEFT)--;
    F(uint16_t, m, M_POS) = 0;
    return 1;
}

/* 0x10044fa2 */
void __cdecl midi_stop_playing(void)
{
    F(uint8_t *, g_audio, A_MIDI_CURRENT) = NULL;
    if (F(UINT, g_audio, A_MIDI_TIMER))
        timeKillEvent(F(UINT, g_audio, A_MIDI_TIMER));
    F(UINT, g_audio, A_MIDI_TIMER) = 0;
}

/* 0x10044df0: sends events up to the next wait and sets the timer for it (at least
 * 3 ms; +0x28/+0x2a scale the waits). Channel 9 (drums) moves to 15 with the
 * RemapMidiDrums setting; muted channels get no notes on. */
void __stdcall midi_play_events(uint8_t *m)
{
    const uint32_t *ev = F(const uint32_t *, m, 0);
    UINT ms;
    do {
        uint32_t e;
        for (;;) {
            if (F(uint16_t, m, M_COUNT) <= F(uint16_t, m, M_POS) && !midi_loop(m))
                return;
            e = ev[F(uint16_t, m, M_POS)];
            F(uint16_t, m, M_POS)++;
            if (e & 0xf0000000)
                break;
            if ((e & 0xf) == 9 && g_remap_drums)
                e |= 0xf;
            if (!m[0x40 + (e & 0xf)] || (e & 0xf0) != 0x90)
                midiOutShortMsg(F(HMIDIOUT, g_audio, A_MIDIOUT), e);
        }
        ms = e & 0x7fffffff;
        if (F(uint16_t, m, 0x2a))
            ms = F(uint16_t, m, 0x28) * ms / F(uint16_t, m, 0x2a);
    } while (!ms);
    if (ms < 3)
        ms = 3;
    F(UINT, g_audio, A_MIDI_TIMER) = timeSetEvent(ms, 1, AudioTimerProc, 0, TIME_ONESHOT);
}

/* the timer's callback */
void CALLBACK AudioTimerProc(UINT id, UINT msg, DWORD_PTR user, DWORD_PTR dw1, DWORD_PTR dw2)
{
    (void)id, (void)msg, (void)user, (void)dw1, (void)dw2;
    F(UINT, g_audio, A_MIDI_TIMER) = 0;
    if (F(uint8_t *, g_audio, A_MIDI_CURRENT) && F(HMIDIOUT, g_audio, A_MIDIOUT))
        midi_play_events(F(uint8_t *, g_audio, A_MIDI_CURRENT));
}
