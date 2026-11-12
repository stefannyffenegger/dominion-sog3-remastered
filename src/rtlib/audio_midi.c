/* The engine's audio library, 0x10043620-0x10043bd0: the MIDI device: opening,
 * volume, mute, pause. Preparing MIDI data is in midi_prepare.c. Unused by Dominion.
 * Rewritten from the original. */
#include "orig.h"
#include "utils.h"
#include "audio.h"
#include "audio_int.h"

/* ---- the device ---- */

/* 0x100436d0 */
void __stdcall midi_report_error(const char *what)
{
    char text[0x100];
    const char *t =
        midiOutGetErrorTextA(F(uint16_t, g_audio, A_MIDI_ERROR), text, sizeof text) ? "Unknown Code" : text;
    DebugStr("Midi Error: %s", what);
    DebugStr(" %s", t);
}

/* 0x100436a1 */
void __cdecl midi_check_error(const char *what)
{
    if (F(uint16_t, g_audio, A_MIDI_ERROR))
        midi_report_error(what);
}

/* 0x10043751 */
void __stdcall midi_set_device_volume(DWORD v)
{
    if (F(HMIDIOUT, g_audio, A_MIDIOUT)) {
        F(DWORD, g_audio, A_MIDI_VOLUME_NOW) = v;
        F(uint16_t, g_audio, A_MIDI_ERROR) = (uint16_t)midiOutSetVolume(
            (HMIDIOUT)(uintptr_t)F(uint32_t, g_audio, A_MIDI_DEVICE_ID), F(DWORD, g_audio, A_MIDI_VOLUME_NOW));
    }
    midi_check_error("midiOutSetVolume");
}

/* 0x10043a96: "all notes off" on a channel */
void __stdcall midi_channel_off(uint8_t ch)
{
    midiOutShortMsg(F(HMIDIOUT, g_audio, A_MIDIOUT), (DWORD)(ch | 0xb0) | 0x7b00);
}

/* 0x10043a33 */
void __cdecl midi_all_notes_off(void)
{
    if (!F(HMIDIOUT, g_audio, A_MIDIOUT))
        return;
    for (uint32_t ch = 0; ch < 16; ch++)
        midi_channel_off((uint8_t)ch);
    midiOutReset(F(HMIDIOUT, g_audio, A_MIDIOUT));
}

/* 0x10043620 */
uint32_t __cdecl midi_close(void)
{
    if (F(HMIDIOUT, g_audio, A_MIDIOUT)) {
        midi_stop_playing();
        midi_all_notes_off();
        midi_set_device_volume(F(DWORD, g_audio, A_MIDI_VOLUME_DEVICE));
        F(uint16_t, g_audio, A_MIDI_ERROR) = (uint16_t)midiOutClose(F(HMIDIOUT, g_audio, A_MIDIOUT));
        midi_check_error("midiOutClose");
        F(HMIDIOUT, g_audio, A_MIDIOUT) = NULL;
    }
    return 0;
}

/* 0x100438c6 */
int __cdecl midi_device_open(void)
{
    if (g_no_midi)
        return 0;
    if (F(HMIDIOUT, g_audio, A_MIDIOUT))
        return 1;
    F(uint16_t, g_audio, A_MIDI_ERROR) = (uint16_t)midiOutOpen((HMIDIOUT *)(g_audio + A_MIDIOUT), MIDI_MAPPER, 0, 0, 0);
    midi_check_error("midiOutOpen");
    if (F(uint16_t, g_audio, A_MIDI_ERROR))
        return 0;
    F(uint16_t, g_audio, A_MIDI_ERROR) =
        (uint16_t)midiOutGetID(F(HMIDIOUT, g_audio, A_MIDIOUT), (UINT *)(g_audio + A_MIDI_DEVICE_ID));
    midi_check_error("midiOutGetID");
    F(uint16_t, g_audio, A_MIDI_ERROR) = (uint16_t)midiOutGetVolume(
        (HMIDIOUT)(uintptr_t)F(uint32_t, g_audio, A_MIDI_DEVICE_ID), (DWORD *)(g_audio + A_MIDI_VOLUME_DEVICE));
    midi_check_error("midiOutGetVolume");
    return 1;
}

uint32_t __stdcall AudioGetMidiVolume(uint8_t *a)
{
    g_audio = a;
    if (!F(HMIDIOUT, a, A_MIDIOUT) && !midi_device_open())
        return 0;
    return F(uint32_t, g_audio, A_MIDI_VOLUME_SET) ? F(DWORD, g_audio, A_MIDI_VOLUME_NOW)
                                                     : F(DWORD, g_audio, A_MIDI_VOLUME_DEVICE);
}

/* 0x100437bc */
void __cdecl midi_apply_mute(void)
{
    if (!F(uint32_t, g_audio, A_MUTED)) {
        midi_set_device_volume(F(DWORD, g_audio, A_MIDI_VOLUME_SAVED));
    } else {
        F(DWORD, g_audio, A_MIDI_VOLUME_SAVED) = AudioGetMidiVolume(g_audio);
        midi_set_device_volume(0);
    }
}

/* 0x1004380f: opens the device for playing, silent and at the MidiVolume setting */
uint32_t __cdecl midi_open(void)
{
    if (!midi_device_open())
        return 0;
    if (F(UINT, g_audio, A_MIDI_TIMER))
        timeKillEvent(F(UINT, g_audio, A_MIDI_TIMER));
    F(UINT, g_audio, A_MIDI_TIMER) = 0;
    midi_all_notes_off();
    F(DWORD, g_audio, A_MIDI_VOLUME_NOW) = (DWORD)g_midi_volume << 16 | g_midi_volume;
    F(uint32_t, g_audio, A_MIDI_VOLUME_SET) = 1;
    midi_set_device_volume(F(DWORD, g_audio, A_MIDI_VOLUME_NOW));
    if (F(uint32_t, g_audio, A_MUTED))
        midi_apply_mute();
    return 1;
}

/* 0x100439c9 */
void __stdcall midi_pause(int pause)
{
    if (!pause) {
        midi_play_events(F(uint8_t *, g_audio, A_MIDI_CURRENT));
        return;
    }
    if (F(UINT, g_audio, A_MIDI_TIMER))
        timeKillEvent(F(UINT, g_audio, A_MIDI_TIMER));
    F(UINT, g_audio, A_MIDI_TIMER) = 0;
    midi_all_notes_off();
}

/* out: the RemapMidiDrums setting; returns the device */
HMIDIOUT __stdcall AudioOpenMidiDevice(uint8_t *a, uint32_t *remap_drums)
{
    g_audio = a;
    *remap_drums = g_remap_drums;
    return midi_device_open() ? F(HMIDIOUT, g_audio, A_MIDIOUT) : NULL;
}

/* remembered as MidiVolume in dominion.ini */
void __stdcall AudioSetMidiVolume(uint8_t *a, DWORD v)
{
    g_audio = a;
    F(DWORD, a, A_MIDI_VOLUME_NOW) = v;
    F(uint32_t, g_audio, A_MIDI_VOLUME_SET) = 1;
    midi_set_device_volume(v);
    g_midi_volume = (uint16_t)v;
    audio_save_midi_volume();
}
