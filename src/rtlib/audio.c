/* The engine's audio library: setup, settings, focus/mute/pause and the service call.
 * Rewritten from the original (0x1003ea50-0x10044600, the parts the game runs).
 *
 * The library also has a software wave mixer on waveOut, a MIDI sequencer and CD audio
 * over MCI (about 180 more functions). Traced with DOMINION_TRACE through the menus and
 * a mission, Dominion never opens a wave, MIDI or CD device here: all its sound and music
 * goes through WAVEPLAY.R32, which hooks into AudioService. Those parts stay original for
 * now and are reached through ORIG(); they only run once a device is open. */
#include "orig.h"
#include "crt.h"
#include "audio.h"
#include "audio_int.h"

/* ---- settings in dominion.ini [audio] ---- */

UINT __stdcall audio_ini_int(LPCSTR key, INT def)
{
    return GetPrivateProfileIntA("audio", key, def, "dominion.ini");
}

void __stdcall audio_ini_write(LPCSTR key, LPCSTR value)
{
    WritePrivateProfileStringA("audio", key, value, "dominion.ini");
}

void __stdcall audio_ini_write_int(LPCSTR key, uint32_t value)
{
    char s[16];
    wsprintfA(s, "%i", value);
    audio_ini_write(key, s);
}

void __cdecl audio_save_cd_volume(void)
{
    audio_ini_write_int("CdVolume", g_cd_volume);
}

void __cdecl audio_save_midi_volume(void)
{
    audio_ini_write_int("MidiVolume", g_midi_volume);
}

void __cdecl audio_save_wave_volume(void)
{
    audio_ini_write_int("WaveVolume", g_wave_volume);
}

void __cdecl audio_read_settings(void)
{
    g_cd_volume = (uint16_t)audio_ini_int("CdVolume", 0x7fff);
    g_midi_volume = (uint16_t)audio_ini_int("MidiVolume", 0x7fff);
    g_wave_volume = (uint16_t)audio_ini_int("WaveVolume", 0x7fff);
    g_wave_format_mask = (uint16_t)audio_ini_int("WaveFormatMask", 0xff);
    g_profile_cd = audio_ini_int("ProfileCD", 1);
    g_no_wave_volume = audio_ini_int("DisableWaveVolume", 0);
    g_remap_drums = audio_ini_int("RemapMidiDrums", 0);
    g_max_wave_buffers = audio_ini_int("MaxWaveBuffers", 8);
    if (g_max_wave_buffers > 8)
        g_max_wave_buffers = 8;
    else if (g_max_wave_buffers < 4)
        g_max_wave_buffers = 4;
}

/* ---- devices ---- */

/* The CD player's line on the mixer ("aux" device); its volume is restored at exit. */
void __cdecl audio_find_cd_aux(void)
{
    UINT n = auxGetNumDevs();
    for (UINT i = 0; i < n; i++) {
        AUXCAPSA caps;
        auxGetDevCapsA(i, &caps, sizeof caps);
        if (caps.wTechnology == AUXCAPS_CDAUDIO) {
            g_cd_aux_found = 1;
            g_cd_aux = i;
            auxGetVolume(i, &g_cd_aux_volume);
            break;
        }
    }
    g_cd_profile = g_profile_cd;
}

void __cdecl audio_check_midi(void)
{
    if (midiOutGetDevCapsA(0, g_midi_caps, 0x34) != MMSYSERR_NOERROR)
        g_no_midi = 1;
}

void __cdecl audio_check_wave(void)
{
    if (waveOutGetDevCapsA(0, g_wave_caps, 0x34) != MMSYSERR_NOERROR)
        g_no_wave = 1;
    else if (g_wave_caps->dwSupport & WAVECAPS_SYNC)
        g_wave_sync = 1;
}

void __cdecl audio_restore_cd(void)
{
    if (g_cd_open && GetCurrentProcess() == g_cd_owner)
        cd_close();
    if (g_cd_aux_found)
        auxSetVolume(g_cd_aux, g_cd_aux_volume);
}

/* ---- library ---- */

void __cdecl audio_library_start(void)
{
    g_audio_ready = 1;
    audio_read_settings();
    crt_srand(timeGetTime()); /* the engine's own C runtime: the same random sequence */
    audio_find_cd_aux();
    audio_check_midi();
    audio_check_wave();
}

/* The first context uses a static block, later ones are allocated */
uint8_t *__cdecl audio_ctx_new(void)
{
    uint8_t *a = g_static_used ? LocalAlloc(LMEM_FIXED, A_SIZE) : g_static_ctx;
    g_static_used = 1;
    memset(a, 0, A_SIZE); /* original: no check for a failed allocation */
    return a;
}

uint32_t __stdcall audio_ctx_free(uint8_t *a)
{
    if (a) {
        if (a == g_static_ctx)
            g_static_used = 0;
        else
            LocalFree(a);
    }
    return 0;
}

/* version must be 0x0400 */
void *__stdcall AudioInitLibrary(HWND wnd, uint32_t version)
{
    uint8_t *a;
    if (!g_audio_ready)
        audio_library_start();
    if ((version & 0xffff) != 0x0400)
        return NULL;
    a = audio_ctx_new();
    if (!a)
        return NULL;
    g_audio = a;
    F(HWND, a, A_WND) = wnd;
    return a;
}

void __stdcall AudioExitLibrary(uint8_t *a)
{
    g_audio = a;
    audio_restore_cd();
    midi_sounds_free();
    wave_sounds_free();
    audio_ctx_free(a);
}

void __stdcall AudioResetLibrary(uint8_t *a)
{
    g_audio = a;
    F(int32_t, a, A_PRIORITY) = 0;
}

typedef void(__stdcall *ServiceFn)(void *);
struct ServiceProc {
    void(__stdcall *proc)(void *);
    void *arg;
};
typedef struct ServiceProc ServiceProc;

/* Hooks proc(arg) into AudioService, which the engine calls every main-loop pass */
void __stdcall AudioSetServiceProc(uint8_t *a, const ServiceProc *set, ServiceProc *old)
{
    g_audio = a;
    if (old) {
        old->proc = F(void *, a, A_SERVICE);
        old->arg = F(void *, a, A_SERVICE_ARG);
    }
    if (set) {
        F(void *, a, A_SERVICE) = (void *)set->proc;
        F(void *, a, A_SERVICE_ARG) = set->arg;
    }
}

void __stdcall AudioService(uint8_t *a)
{
    DWORD now;
    g_audio = a;
    if (F(void *, a, A_SERVICE))
        F(ServiceFn, a, A_SERVICE)(F(void *, a, A_SERVICE_ARG));
    if (!F(HWAVEOUT, a, A_WAVEOUT) ||
        (F(int16_t, a, A_MIXING) == 0 && F(int16_t, a, A_WAVES_PLAYING) == 0)) {
        timeGetTime(); /* original: result unused */
        return;
    }
    /* the wave mixer is running (never in Dominion, see the top) */
    if (F(uint32_t, a, A_FADE))
        wave_fade_step();
    F(int32_t, a, A_PRIORITY) = 0;
    if (wave_mix())
        return;
    now = timeGetTime();
    for (uint8_t **p = (uint8_t **)(a + A_WAVES); p < F(uint8_t **, a, A_WAVES_END); p++) {
        uint8_t *s = *p;
        if (!s[0x82])
            continue;
        if (now < F(uint32_t, s, 0x3c) && F(int32_t, a, A_PRIORITY) < F(int16_t, s, 0x80))
            F(int32_t, a, A_PRIORITY) = F(int16_t, s, 0x80);
        if (s[0x84] && F(uint32_t, s, 0x3c) < now)
            wave_sound_timeout(s);
        if (!g_cd_open) {
            if (!s[0x86] && !s[0x88] && F(uint32_t, s, 0x04) <= F(uint32_t, s, 0x10))
                wave_sound_refill(s);
            if (s[0x8a] && !s[0x85] && F(uint32_t, s, 0x38) <= now) {
                wave_sound_rewind(s);
                wave_sound_started(s);
            }
        }
    }
}

/* Out: the device's formats and the format flag in use; returns 0 = no wave device, 1, 2 = sync */
uint32_t __stdcall AudioGetWaveDeviceInfo(uint8_t *a, uint32_t *info)
{
    g_audio = a;
    if (info) {
        info[0] = g_wave_caps->dwFormats;
        info[1] = F(HWAVEOUT, a, A_WAVEOUT) ? F(uint16_t, a, A_FORMAT_FLAG) : 0;
    }
    if (g_no_wave)
        return 0;
    return g_wave_sync ? 2 : 1;
}

/* ---- focus, mute, pause ---- */

void __stdcall audio_set_waves_active(uint8_t on)
{
    for (uint8_t **p = (uint8_t **)(g_audio + A_WAVES); p < F(uint8_t **, g_audio, A_WAVES_END); p++)
        (*p)[0x82] = on;
}

void __stdcall audio_waves_restart(int reset)
{
    if (reset)
        wave_reset();
    audio_set_waves_active(1);
}

int __stdcall AudioStopCD(uint8_t *a)
{
    g_audio = a;
    return g_cd_open ? cd_device_stop() : 0;
}

/* The game window lost (0) or got (1) the focus: devices are closed and reopened */
void __stdcall AudioActivate(uint8_t *a, int active)
{
    g_audio = a;
    audio_set_waves_active((uint8_t)active);
    if (!active) {
        AudioStopCD(a);
        midi_close();
        wave_close();
    } else if (!F(uint32_t, g_audio, A_PAUSED)) {
        if (F(int16_t, g_audio, A_WAVES_ACTIVE))
            wave_open((const WAVEFORMATEX *)(g_audio + A_FORMAT));
        if (F(void *, g_audio, A_MIDI_CURRENT)) {
            midi_open();
            F(uint16_t, F(uint8_t *, g_audio, A_MIDI_CURRENT), 4) = 0;
            midi_play_events(F(void *, g_audio, A_MIDI_CURRENT));
        }
    }
}

/* on: 1 = mute, 0 = sound, negative = toggle */
void __stdcall AudioMute(uint8_t *a, int16_t on)
{
    uint32_t was;
    g_audio = a;
    was = F(uint32_t, a, A_MUTED);
    if (on < 0)
        F(uint32_t, a, A_MUTED) = !was;
    else
        F(uint32_t, a, A_MUTED) = on >= 1;
    if (F(uint32_t, g_audio, A_MUTED) != was) {
        cd_apply_mute();
        wave_apply_mute();
        midi_apply_mute();
    }
}

void __stdcall AudioPause(uint8_t *a, int pause)
{
    uint32_t p = pause != 0;
    g_audio = a;
    if (F(uint32_t, a, A_PAUSED) == p)
        return;
    F(uint32_t, a, A_PAUSED) = p;
    wave_pause((int)p);
    if (F(void *, g_audio, A_MIDI_CURRENT))
        midi_pause((int)p);
}

void __stdcall AudioPauseMidi(uint8_t *a, int pause)
{
    uint32_t p = pause != 0;
    g_audio = a;
    if (F(uint32_t, a, A_MIDI_PAUSED) == p)
        return;
    F(uint32_t, a, A_MIDI_PAUSED) = p;
    if (F(void *, g_audio, A_MIDI_CURRENT))
        midi_pause((int)p);
}
