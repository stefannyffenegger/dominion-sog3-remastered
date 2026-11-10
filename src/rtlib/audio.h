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

#endif
