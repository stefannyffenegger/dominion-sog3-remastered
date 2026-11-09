/* Shared state of our WAVEPLAY.R32 (waveplay.c: sound effects, cdmusic.c: music). */
#ifndef DOMINION_WAVEPLAY_H
#define DOMINION_WAVEPLAY_H

#include <stdint.h>
#define COBJMACROS
#include <windows.h>
#include <dsound.h>

#define MAX_CHANNELS 20
#define UNLOAD_QUEUE 64
#define UNLOAD_ALL_KEY (-666) /* "unload everything" in DSUnloadWave(File) */

typedef struct Wave {
    int used;
    uint32_t key;      /* archive index, CRC32 of a loose file name, or resource id */
    int from_resource; /* loaded with DSLoadWave from the engine's resource file */
    int16_t priority;
    WAVEFORMATEX fmt;
    uint8_t *data;
    uint32_t size;
    int queue_pos;     /* position in the unload queue, -1 = not queued */
} Wave;

typedef struct Channel {
    IDirectSoundBuffer *buf; /* NULL = free */
    uint32_t key;
    int wave;          /* slot in Waves */
    int loop;
    int msg;           /* script message when the sound stops (0 = none) */
    int16_t priority;  /* original: never set, always 0 (see alloc_channel) */
    int volume;        /* 0..10000 as given by the game */
    int pan;
    int paused;        /* stopped by DSPauseAudio */
    DWORD pause_pos;
} Channel;

typedef struct UnloadEntry {
    int wave;
    DWORD since;
} UnloadEntry;

typedef struct ServicePair {
    void(__stdcall *proc)(void *ctx);
    void *ctx;
} ServicePair;

typedef struct WavePlay {
    void *app;
    void *hres;        /* engine resource file (DOMINION.000) */
    IDirectSound *ds;
    IDirectSoundBuffer *primary;
    LONG saved_primary_volume, saved_primary_pan;
    int master_volume; /* 0..10000, on the primary buffer */
    int master_pan;
    WAVEFORMATEX primary_fmt;
    int n_channels;    /* Dominion.ini SoundBuffers, 8..20 */
    Channel ch[MAX_CHANNELS];
    Wave *waves;
    int n_waves;
    int playing;       /* channels started */
    int audio_paused;
    int unloading_all; /* no stop messages while everything is unloaded */
    UnloadEntry queue[UNLOAD_QUEUE];
    int n_queue;
    DWORD last_service;

    /* audio.rdf */
    int rdf_open;
    HANDLE rdf;
    uint16_t rdf_files;
    uint8_t *rdf_index; /* 128-byte records sorted by name */

    ServicePair saved_service;
} WavePlay;

extern WavePlay *g_wp;

/* engine (Rtlib32.dll) */
void *__cdecl RtGetHAL(void *app);
void *__cdecl RtGetHResfile(void *app);
int __cdecl RtRunMsg(void *app, void *msg);
void __stdcall AudioSetServiceProc(void *hal, const ServicePair *set, ServicePair *old);
int __stdcall ResReadResourceData(void *hres, uint32_t id, void *buf, uint32_t size, uint32_t offset);
void __stdcall ima2pcm(void *state, void *out, const void *in, uint32_t in_bytes, int stereo);

/* cdmusic.c */
void music_init(WavePlay *wp);
void music_shutdown(void);
void music_service(void);

void wp_debug(const char *fmt, ...);
void *wp_alloc(size_t n);
void wp_free(void *p);

#endif
