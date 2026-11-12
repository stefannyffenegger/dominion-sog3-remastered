/* The engine's audio library: the context layout and globals shared by audio*.c */
#ifndef DOMINION_RTLIB_AUDIO_INT_H
#define DOMINION_RTLIB_AUDIO_INT_H

#include "orig.h"
#include "midi_prepare.h"
#include "resample.h"

/* the audio context: 0x2fa bytes, fields at the original's (unaligned) offsets */
#define F(type, ctx, off) (*(type *)((uint8_t *)(ctx) + (off)))
#define A_WND 0x000           /* HWND for notifications */
#define A_SERVICE 0x004       /* AudioServiceProc: proc, then its argument */
#define A_SERVICE_ARG 0x008
#define A_MUTED 0x00c
#define A_PAUSED 0x010
#define A_BUFFERS_QUEUED 0x020 /* buffers handed to waveOut (short) */
#define A_WAVES_END 0x022     /* end of the wave sound list at 0xe6 */
#define A_WAVEOUT 0x026       /* HWAVEOUT, 0 = closed */
#define A_WAVE_COUNT 0x02a    /* wave sounds made (short) */
#define A_WAVES_ACTIVE 0x02c  /* short */
#define A_PRIORITY 0x05a
#define A_FORMAT 0x066
#define A_FORMAT_FLAG 0x060  /* the device format's WAVE_FORMAT_* flag (short) */
#define A_FADE 0x076
#define A_WAVES 0x0e6
#define A_MIDI_CURRENT 0x27a
#define A_MIDI_END 0x27e      /* end of the MIDI list at 0x2aa */
#define A_MIDIOUT 0x292
#define A_MIDI_PAUSED 0x2a2
#define A_SIZE 0x2fa
#define A_WAVE_ERROR 0x064    /* the last waveOut result (short) */
#define A_BYTES_PER_SEC 0x06e /* the device format's nAvgBytesPerSec (format at A_FORMAT) */
#define A_8BIT 0x07a          /* the device is 8-bit (else byte counts halve to samples) */
#define A_BUF_PLAY 0x014      /* buffer slots: playing, filling, end */
#define A_BUF_FILL 0x018
#define A_BUFFERS_END 0x01c
#define A_VOLUME_NOW 0x03e    /* the device volume (left, right words) */
#define A_FADE_FROM 0x042
#define A_FADE_TIME 0x046
#define A_VOLUME_SAVED 0x04a  /* while muted */
#define A_VOLUME_DEVICE 0x04e /* as found, restored at close */
#define A_DEVICE_ID 0x052
#define A_FADE_LEFT 0x056     /* per-step decrease (short) */
#define A_FADE_RIGHT 0x058
#define A_FADE_STEPS 0x05e
#define A_FORMATS 0x062       /* WAVE_FORMAT_* flags allowed (short) */
#define A_VOLUME_SET 0x07e
#define A_VOLUME 0x082
#define A_BUFFERS 0x086       /* 8 slots of 0xc bytes: WAVEHDR *, data, bytes used */
#define A_PAUSE_TIME 0x276
#define A_MIDIS 0x2aa         /* MIDI sound list, to A_MIDI_END */
#define A_MIDI_COUNT 0x29e    /* (short) */
#define A_MIDI_VOLUME_DEVICE 0x286 /* as found, restored at close */
#define A_MIDI_VOLUME_NOW 0x28a
#define A_MIDI_VOLUME_SAVED 0x28e /* while muted */
#define A_MIDI_DEVICE_ID 0x296
#define A_MIDI_TIMER 0x29a    /* the playing timer event */
#define A_MIDI_ERROR 0x2a0    /* the last midiOut result (short) */
#define A_MIDI_VOLUME_SET 0x2a6

/* a MIDI sound */
#define M_POS 0x04             /* the next event (short) */
#define M_COUNT 0x06           /* events (short) */
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
#define W_IN_MEMORY 0x86      /* read whole, else streamed */
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
uint32_t __stdcall AudioResampleWave(const WaveFormat *out_fmt, const WaveFormat *in_fmt, uint32_t len, uint8_t *dst,
                                     const uint8_t *src);
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

/* audio_wave.c */
uint32_t __cdecl wave_buffers_free(void);
void __stdcall wave_set_device_volume(uint32_t v);
void __stdcall wave_fade_all(uint32_t ms);
void __cdecl wave_apply_mute(void);
int16_t __stdcall wave_format_index(const WAVEFORMATEX *f, uint16_t mask);
uint16_t __stdcall wave_find_format(const WAVEFORMATEX *f);
int __stdcall wave_pick_format(const WAVEFORMATEX *f);
uint32_t __cdecl wave_buffers_alloc(void);
uint32_t __stdcall wave_device_open(const WAVEFORMATEX *f);
uint32_t __stdcall wave_open(const WAVEFORMATEX *f);
void __stdcall wave_pause(int pause);
void __cdecl wave_reset(void);
void __stdcall AudioCloseWave(uint8_t *a);
uint32_t __stdcall wave_device_format(uint8_t *unused, uint32_t *out);
uint32_t __stdcall AudioGetWaveVolume(uint8_t *a);
void __stdcall AudioOpenWave(uint8_t *a, const WAVEFORMATEX *f);
void __stdcall AudioSetWaveFormats(uint8_t *a, uint16_t formats);
void __stdcall AudioSetWaveVolume(uint8_t *a, uint32_t v);
void __cdecl midi_sounds_free(void);

/* audio_objs.c */
struct Source;
/* a file source (0x14 bytes): the file, position, packed (WAV7), in memory, the data */
typedef struct FileSource {
    HMMIO f;
    uint32_t pos, packed, in_memory;
    uint8_t *data;
} FileSource;
int __stdcall midi_source_close(uint8_t *m);
uint8_t *__stdcall midi_sound_remove(uint8_t *m);
void __stdcall midi_sound_free(uint8_t *m);
uint8_t *__cdecl midi_sound_alloc(void);
int __stdcall midi_sound_load(uint8_t *m, const struct Source *src, const uint32_t *info);
uint8_t *__stdcall AudioCreateMidi(uint8_t *a, const struct Source *src, const uint32_t *info);
uint32_t __stdcall AudioDestroyMidi(uint8_t *a, uint8_t *m);
void __stdcall wave_source_close(uint8_t *s);
uint8_t *__stdcall wave_sound_remove(uint8_t *s);
void __stdcall wave_sound_free(uint8_t *s);
void __cdecl wave_sounds_free(void);
uint8_t *__cdecl wave_sound_alloc(void);
int __stdcall wave_sound_load(uint8_t *s, const struct Source *src, const uint8_t *info);
uint8_t *__stdcall AudioCreateWave(uint8_t *a, const struct Source *src, const uint8_t *info);
uint32_t __stdcall AudioDestroyWave(uint8_t *a, uint8_t *s);
void *__stdcall file_source_close(FileSource *s);
void __stdcall file_source_close_cb(FileSource *s);
uint32_t __stdcall file_source_read(FileSource *s, void *buf, uint32_t n);
int __stdcall file_source_rewind(FileSource *s);
FileSource *__stdcall file_source_open(LPSTR path);
LONG __stdcall mmio_size(HMMIO f);
uint8_t *__stdcall midi_create_from_file(uint8_t *a, LPSTR path, uint32_t unused);

/* audio_files.c */
int __stdcall wave_file_riff(FileSource *fs);
int __stdcall wave_file_format(HMMIO f, WAVEFORMAT *fmt);
DWORD __stdcall wave_file_data(HMMIO f);
int __stdcall wave_file_info(FileSource *fs, uint8_t *info);
uint8_t *__stdcall AudioCreateWaveFileSound(uint8_t *a, LPSTR path, uint8_t flags);
int __stdcall wave_file_unpack(HMMIO f, int16_t *out, const uint8_t *fmt);
uint8_t *__stdcall wave_file_read(HMMIO f, int packed, uint8_t *out);
uint8_t *__stdcall wave_file_load(LPSTR path, uint8_t *out);
int __stdcall wave_file_write_cb(uint32_t f, void *buf, uint32_t n);
int __stdcall wave_file_write_packed(HMMIO f, const int16_t *data, uint32_t n, const uint8_t *fmt);
int __stdcall wave_file_save(LPSTR path, const uint8_t *fmt, const int16_t *data, uint32_t size, int pack);

/* audio_cd.c */
int __stdcall cd_command(UINT msg, DWORD_PTR flags, DWORD_PTR parms);
uint32_t __cdecl cd_close(void);
DWORD __cdecl cd_get_volume(void);
int __stdcall cd_set_volume(DWORD v);
void __cdecl cd_apply_mute(void);
int __cdecl cd_device_stop(void);
uint32_t __cdecl cd_position(void);
uint32_t __stdcall AudioGetCDPosition(uint8_t *a);
int __cdecl cd_open(void);
uint32_t __stdcall AudioGetCDTracks(uint8_t *a);
uint32_t __stdcall AudioGetCDVolume(uint8_t *a);
uint32_t __stdcall cd_track_length(uint8_t track);
uint32_t __stdcall AudioGetCDTrackLengths(uint8_t *a, uint8_t from, uint8_t to, uint32_t *out);
int __stdcall AudioIsCDPlaying(uint8_t *a);
void __cdecl cd_sync(void);
void __cdecl cd_profile(void);
uint32_t __stdcall cd_play(uint32_t from, uint32_t to);
int __stdcall AudioPlayCD(uint8_t *a, uint32_t from, uint32_t to);
uint32_t __stdcall cd_play_tracks(uint8_t from, uint8_t to);
int __stdcall AudioPlayCDTrack(uint8_t *a, uint8_t track);
int __stdcall AudioSetCDVolume(uint8_t *a, DWORD v);

/* audio_midi.c */
void __stdcall midi_report_error(const char *what);
void __cdecl midi_check_error(const char *what);
void __stdcall midi_set_device_volume(DWORD v);
void __stdcall midi_channel_off(uint8_t ch);
void __cdecl midi_all_notes_off(void);
uint32_t __cdecl midi_close(void);
int __cdecl midi_device_open(void);
uint32_t __stdcall AudioGetMidiVolume(uint8_t *a);
void __cdecl midi_apply_mute(void);
uint32_t __cdecl midi_open(void);
void __stdcall midi_pause(int pause);
HMIDIOUT __stdcall AudioOpenMidiDevice(uint8_t *a, uint32_t *remap_drums);
void __stdcall AudioSetMidiVolume(uint8_t *a, DWORD v);

/* audio_mix.c */
void __cdecl wave_fade_up(void);
void __cdecl wave_fade_down(void);
void __cdecl wave_fade_step(void);
void __cdecl wave_reclaim(void);
uint32_t __cdecl wave_mix16(void);
uint16_t __cdecl wave_mix8(void);
uint32_t __cdecl wave_mix(void);
void __stdcall wave_sound_timeout(uint8_t *s);
int __stdcall midi_loop(uint8_t *m);
void __cdecl midi_stop_playing(void);
void __stdcall midi_play_events(uint8_t *m);
void CALLBACK AudioTimerProc(UINT id, UINT msg, DWORD_PTR user, DWORD_PTR dw1, DWORD_PTR dw2);

/* not ported yet */
#define wave_volume_step ORIG(uint32_t(__stdcall *)(uint32_t, int, int, uint32_t), 0x10045b80)
#define mix_sound16 ORIG(uint32_t(__stdcall *)(uint8_t *, int16_t *, uint32_t, uint32_t), 0x10045d96)
#define mix_sound8 ORIG(uint32_t(__stdcall *)(uint8_t *, uint8_t *, uint32_t, uint32_t), 0x10045c30)
#define wave_sound_refill ORIG(int(__stdcall *)(void *), 0x100459c5)
#define wave_sound_rewind ORIG(void(__stdcall *)(void *), 0x100458d5)
#endif
