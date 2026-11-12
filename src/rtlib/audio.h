/* The engine's audio library, the parts the game runs (audio.c) */
#ifndef DOMINION_RTLIB_AUDIO_H
#define DOMINION_RTLIB_AUDIO_H

#include <stdint.h>
#include <windows.h>
#include <mmsystem.h>

UINT __stdcall audio_ini_int(LPCSTR key, INT def);
void __stdcall audio_ini_write(LPCSTR key, LPCSTR value);
void __stdcall audio_ini_write_int(LPCSTR key, uint32_t value);
void __cdecl audio_save_cd_volume(void);
void __cdecl audio_save_midi_volume(void);
void __cdecl audio_save_wave_volume(void);
void __cdecl audio_read_settings(void);
void __cdecl audio_find_cd_aux(void);
void __cdecl audio_check_midi(void);
void __cdecl audio_check_wave(void);
void __cdecl audio_restore_cd(void);
void __cdecl audio_library_start(void);
uint8_t *__cdecl audio_ctx_new(void);
uint32_t __stdcall audio_ctx_free(uint8_t *a);
void *__stdcall AudioInitLibrary(HWND wnd, uint32_t version);
void __stdcall AudioExitLibrary(uint8_t *a);
void __stdcall AudioResetLibrary(uint8_t *a);
struct ServiceProc;
void __stdcall AudioSetServiceProc(uint8_t *a, const struct ServiceProc *set, struct ServiceProc *old);
void __stdcall AudioService(uint8_t *a);
uint32_t __stdcall AudioGetWaveDeviceInfo(uint8_t *a, uint32_t *info);
void __stdcall audio_set_waves_active(uint8_t on);
void __stdcall audio_waves_restart(int reset);
int __stdcall AudioStopCD(uint8_t *a);
void __stdcall AudioActivate(uint8_t *a, int active);
void __stdcall AudioMute(uint8_t *a, int16_t on);
void __stdcall AudioPause(uint8_t *a, int pause);
void __stdcall AudioPauseMidi(uint8_t *a, int pause);

/* sounds, devices, CD (audio_*.c) */
struct Source;
/* RtGetWaveDeviceFormat's: the device format; returns the device */
uint32_t __stdcall wave_device_format(uint8_t *unused, uint32_t *out);
void __stdcall AudioBreakMidiLoops(uint8_t *a, uint8_t *id);
int __stdcall AudioPlayMidi(uint8_t *a, uint8_t *id, const uint8_t *params);
void __stdcall AudioSetMidi(uint8_t *a, uint8_t *id, const uint8_t *set);
void __stdcall AudioStopMidi(uint8_t *a, uint8_t *id);
void __stdcall AudioBreakWaveLoops(uint8_t *a, uint8_t *id);
void __stdcall AudioClearNotifications(uint8_t *a);
int __stdcall AudioGetWaveSoundInfo(uint8_t *a, uint8_t *id, uint32_t *out);
uint32_t __stdcall AudioGetWaveSoundTime(uint8_t *a, uint8_t *id);
int __stdcall AudioPlayWave(uint8_t *a, uint8_t *id, const uint8_t *params);
void __stdcall AudioSetWaveSoundPriority(uint8_t *a, uint8_t *id, int16_t priority);
int __stdcall AudioSetWaveSoundVolume(uint8_t *a, uint8_t *id, uint8_t volume);
void __stdcall AudioStopWave(uint8_t *a, uint8_t *id, const uint32_t *params);
void __stdcall AudioCloseWave(uint8_t *a);
uint32_t __stdcall AudioGetWaveVolume(uint8_t *a);
void __stdcall AudioOpenWave(uint8_t *a, const WAVEFORMATEX *f);
void __stdcall AudioSetWaveFormats(uint8_t *a, uint16_t formats);
void __stdcall AudioSetWaveVolume(uint8_t *a, uint32_t v);
uint8_t *__stdcall AudioCreateMidi(uint8_t *a, const struct Source *src, const uint32_t *info);
uint32_t __stdcall AudioDestroyMidi(uint8_t *a, uint8_t *m);
uint8_t *__stdcall AudioCreateWave(uint8_t *a, const struct Source *src, const uint8_t *info);
uint32_t __stdcall AudioDestroyWave(uint8_t *a, uint8_t *s);
uint8_t *__stdcall AudioCreateWaveFileSound(uint8_t *a, LPSTR path, uint8_t flags);
uint32_t __stdcall AudioGetCDPosition(uint8_t *a);
uint32_t __stdcall AudioGetCDTracks(uint8_t *a);
uint32_t __stdcall AudioGetCDVolume(uint8_t *a);
uint32_t __stdcall AudioGetCDTrackLengths(uint8_t *a, uint8_t from, uint8_t to, uint32_t *out);
int __stdcall AudioIsCDPlaying(uint8_t *a);
int __stdcall AudioPlayCD(uint8_t *a, uint32_t from, uint32_t to);
int __stdcall AudioPlayCDTrack(uint8_t *a, uint8_t track);
int __stdcall AudioSetCDVolume(uint8_t *a, DWORD v);
uint32_t __stdcall AudioGetMidiVolume(uint8_t *a);
HMIDIOUT __stdcall AudioOpenMidiDevice(uint8_t *a, uint32_t *remap_drums);
void __stdcall AudioSetMidiVolume(uint8_t *a, DWORD v);

#endif
