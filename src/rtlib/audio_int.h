/* The engine's audio library: the context layout and globals shared by audio*.c */
#ifndef DOMINION_RTLIB_AUDIO_INT_H
#define DOMINION_RTLIB_AUDIO_INT_H

#include "orig.h"

/* the audio context: 0x2fa bytes, fields at the original's (unaligned) offsets */
#define F(type, ctx, off) (*(type *)((uint8_t *)(ctx) + (off)))
#define A_WND 0x000           /* HWND for notifications */
#define A_SERVICE 0x004       /* AudioServiceProc: proc, then its argument */
#define A_SERVICE_ARG 0x008
#define A_MUTED 0x00c
#define A_PAUSED 0x010
#define A_WAVES_PLAYING 0x020 /* short */
#define A_WAVES_END 0x022     /* end of the wave sound list at 0xe6 */
#define A_WAVEOUT 0x026       /* HWAVEOUT, 0 = closed */
#define A_MIXING 0x02a        /* short */
#define A_WAVES_ACTIVE 0x02c  /* short */
#define A_PRIORITY 0x05a
#define A_FORMAT 0x066
#define A_WAVE_CHANNELS 0x060 /* short */
#define A_FADE 0x076
#define A_WAVES 0x0e6
#define A_MIDI_CURRENT 0x27a
#define A_MIDI_END 0x27e      /* end of the MIDI list at 0x2aa */
#define A_MIDIOUT 0x292
#define A_MIDI_PAUSED 0x2a2
#define A_SIZE 0x2fa
#define A_WAVE_ERROR 0x064    /* the last waveOut result (short) */
#define A_BYTES_PER_SEC 0x06e /* the device format's nAvgBytesPerSec (format at A_FORMAT) */
#define A_7A 0x07a            /* 0 halves byte counts (fades) */
#define A_MIDIS 0x2aa         /* MIDI sound list, to A_MIDI_END */

/* a MIDI sound */
#define M_LOOPS_LEFT 0x2c
#define M_LOOPS 0x30
#define M_NOTIFY 0x38         /* post message 0x52c at the end */

/* a wave sound */
#define W_LENGTH 0x14         /* in samples (shifted by W_SHIFT for bytes) */
#define W_TIME 0x38           /* when a delayed sound plays next */
#define W_END_TIME 0x3c
#define W_FORMAT 0x54         /* WAVEFORMATEX */
#define W_LOOPS_LEFT 0x7c     /* short */
#define W_LOOPS 0x7e          /* short */
#define W_PRIORITY 0x80       /* short */
#define W_NOTIFY 0x83
#define W_NOTIFY_DUE 0x84
#define W_PLAYING 0x85
#define W_RESAMPLED 0x87
#define W_NO_PRIORITY 0x89
#define W_REPEAT 0x8a         /* plays again after a random delay */
#define W_SHIFT 0x8e
#define W_VOLUME 0x8f         /* 0-16 */
#define W_FADE_VOLUME 0x90
#define W_DELAY 0x91          /* seconds */
#define W_DELAY_RANGE 0x92

/* globals of the original */
#define g_audio G(uint8_t *, 0x100884bc)      /* the context of the last call */
#define g_audio_ready G(uint32_t, 0x100884c0)
#define g_static_ctx ((uint8_t *)ORIG_PTR(0x100884c8))
#define g_static_used G(uint32_t, 0x100887c4)
#define g_cd_volume G(uint16_t, 0x1008840c)
#define g_midi_volume G(uint16_t, 0x1008840e)
#define g_wave_volume G(uint16_t, 0x10088410)
#define g_wave_format_mask G(uint16_t, 0x1008840a)
#define g_profile_cd G(uint32_t, 0x100883f0)
#define g_no_wave_volume G(uint32_t, 0x100883f4)
#define g_no_wave G(uint32_t, 0x100883f8)
#define g_no_midi G(uint32_t, 0x100883fc)
#define g_remap_drums G(uint32_t, 0x10088400)
#define g_wave_sync G(uint32_t, 0x10088404)
#define g_max_wave_buffers G(uint32_t, 0x10088408)
#define g_wave_caps ((WAVEOUTCAPSA *)ORIG_PTR(0x10088412))
#define g_midi_caps ((MIDIOUTCAPSA *)ORIG_PTR(0x10088446))
#define g_cd_open G(uint32_t, 0x1008847a)
#define g_cd_aux G(uint32_t, 0x1008847e)
#define g_cd_aux_volume G(DWORD, 0x10088496)
#define g_cd_owner G(HANDLE, 0x100884aa)
#define g_cd_aux_found G(uint32_t, 0x100884ae)
#define g_cd_profile G(uint32_t, 0x100884b2)


/* audio_play.c */
uint8_t *__stdcall midi_find(uint8_t *m);
void __stdcall midis_set_34(uint32_t v);
void __stdcall midi_notify_end(uint8_t *m);
void __stdcall midi_stop(uint8_t *m);
int __stdcall midi_start(uint8_t *m, const uint8_t *params);
uint8_t *__stdcall wave_find(uint8_t *s);
void __stdcall wave_sound_started(uint8_t *s);
void __stdcall wave_sound_schedule(uint8_t *s, uint32_t now);
void __stdcall wave_sound_ended(uint8_t *s);
void __stdcall wave_fade_out(uint8_t *s, uint32_t ms, uint8_t steps, uint8_t extend);
void __stdcall wave_sound_stop(uint8_t *s, const uint32_t *params);
void __stdcall wave_stop_all(const uint32_t *params);
uint32_t __stdcall wave_sound_ms(uint8_t *s);
void __stdcall wave_fade_in(uint8_t *s, uint32_t ms, uint8_t steps);
int __stdcall wave_needs_resample(uint8_t *s);
int __stdcall wave_sound_delay(uint8_t *s, const uint8_t *params);
void __cdecl wave_sounds_yield(void);
int __stdcall wave_sound_play(uint8_t *s, const uint8_t *params);
void __stdcall wave_report_error(const char *what);
void __cdecl wave_check_error(const char *what);
uint32_t __cdecl wave_close(void);
uint32_t __stdcall AudioResampleWave(void *dst_fmt, void *src_fmt, uint32_t len, void *src, void *dst);
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

/* not ported yet */
#define midi_stop_playing ORIG(void(__cdecl *)(void), 0x10044fa2)
#define midi_all_notes_off ORIG(void(__cdecl *)(void), 0x10043a33)
#define midi_channel_off ORIG(void(__stdcall *)(uint32_t), 0x10043a96)
#define wave_fade_all ORIG(void(__stdcall *)(uint32_t), 0x100406b7)
#define wave_set_device_volume ORIG(void(__stdcall *)(uint32_t), 0x10041101)
#define wave_buffers_free ORIG(void(__cdecl *)(void), 0x10040555)
#define resample_setup ORIG(uint32_t(__stdcall *)(void *, void *, void *, uint32_t), 0x100452e0)
#define wave_open ORIG(uint32_t(__stdcall *)(void *), 0x1004081f)
#define wave_pause ORIG(void(__stdcall *)(int), 0x10041036)
#define wave_apply_mute ORIG(void(__cdecl *)(void), 0x100407d5)
#define wave_sounds_free ORIG(void(__cdecl *)(void), 0x10041920)
#define wave_fade_step ORIG(void(__cdecl *)(void), 0x100447fb)
#define wave_mix ORIG(int(__cdecl *)(void), 0x100449a3)
#define wave_sound_timeout ORIG(void(__stdcall *)(void *), 0x10044d40)
#define wave_sound_refill ORIG(void(__stdcall *)(void *), 0x100459c5)
#define wave_sound_rewind ORIG(void(__stdcall *)(void *), 0x100458d5)
#define midi_close ORIG(uint32_t(__cdecl *)(void), 0x10043620)
#define midi_open ORIG(uint32_t(__cdecl *)(void), 0x1004380f)
#define midi_play_events ORIG(void(__stdcall *)(void *), 0x10044df0)
#define midi_pause ORIG(void(__stdcall *)(int), 0x100439c9)
#define midi_apply_mute ORIG(void(__cdecl *)(void), 0x100437bc)
#define midi_sounds_free ORIG(void(__cdecl *)(void), 0x100414b0)
#define cd_apply_mute ORIG(void(__cdecl *)(void), 0x10042cb0)
#define cd_device_stop ORIG(int(__cdecl *)(void), 0x10042d6d)
#define cd_close ORIG(void(__cdecl *)(void), 0x10042b13)
#define wave_reset ORIG(void(__cdecl *)(void), 0x1004117d)
#define resample ORIG(uint32_t(__stdcall *)(void *, void *, uint32_t, void *, uint32_t, void *), 0x10045394)
#endif
