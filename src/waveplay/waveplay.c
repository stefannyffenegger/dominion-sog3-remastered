/* WAVEPLAY.R32 replacement: sound effects (this file) and CD music (cdmusic.c).
 *
 * Sounds ("waves") are loaded into slots and played on channels:
 *   - from audio.rdf, an archive of 725 WAV files with a name index (rdf* calls);
 *     a loose WAV file with the same name takes precedence;
 *   - from loose WAV files (DSLoadWaveFile);
 *   - from the engine's resource file DOMINION.000 by resource id (DSLoadWave).
 * A wave is identified by a key: its archive index, the CRC32 of its file name, or
 * its resource id. Each channel has its own DirectSound buffer with the whole sound
 * (the original streamed sounds longer than ~2 s; the result sounds the same).
 * There are Dominion.ini [Dominion] SoundBuffers channels (8..20, default 8).
 *
 * When a sound stops (it ended, the game stopped it, or its channel was taken), the
 * message given to the play call is run at once (RtRunMsg), before the channel is
 * cleared. The script may call back into this module from that message.
 *
 * Volumes are 0..10000 (DirectSound volume + 10000, i.e. 1/100 dB below full);
 * pan is DirectSound's -10000..10000; frequency is in Hz.
 *
 * Behavior kept from the original on purpose is marked "original:". */
#include <string.h>
#include "rtlib32.h"
#include "waveplay.h"
#include <mmsystem.h>

WavePlay *g_wp;

/* ---- small helpers ---- */

void *wp_alloc(size_t n)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n ? n : 1);
}

void wp_free(void *p)
{
    if (p)
        HeapFree(GetProcessHeap(), 0, p);
}

void wp_debug(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    wvsprintfA(buf, fmt, ap);
    va_end(ap);
    OutputDebugStringA(buf);
}

static uint32_t rd32(const uint8_t *p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t crc_table[256];

uint32_t __cdecl calc_crc32(const uint8_t *p, uint32_t len)
{
    if (!crc_table[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (c >> 1) ^ 0xedb88320u : c >> 1;
            crc_table[i] = c;
        }
    uint32_t crc = 0xffffffffu; /* original: no final inversion */
    for (uint32_t i = 0; i < len; i++)
        crc = crc_table[(p[i] ^ crc) & 0xff] ^ crc >> 8;
    return crc;
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static int stricmp_ascii(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = lower((uint8_t)*a), y = lower((uint8_t)*b);
        if (x != y || !x)
            return x - y;
    }
}

/* ---- audio.rdf ---- */

#define RDF_REC 128

static int rdf_open(WavePlay *wp, const char *path)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    uint8_t hdr[0xcc];
    DWORD n;
    if (!ReadFile(f, hdr, sizeof hdr, &n, NULL) || n != sizeof hdr) {
        OutputDebugStringA("error reading in rdf header\n");
        CloseHandle(f);
        return 0;
    }
    uint16_t files = rd16(hdr + 0xc6);
    uint32_t index_at = rd32(hdr + 0xc8);
    uint8_t *index = wp_alloc((size_t)files * RDF_REC);
    SetFilePointer(f, (LONG)index_at, NULL, FILE_BEGIN);
    if (!index || !ReadFile(f, index, (DWORD)files * RDF_REC, &n, NULL) || n != (DWORD)files * RDF_REC) {
        OutputDebugStringA("error reading in index records\n");
        wp_free(index);
        CloseHandle(f);
        return 0;
    }
    OutputDebugStringA("rdf file opened...\n");
    wp->rdf = f;
    wp->rdf_files = files;
    wp->rdf_index = index;
    wp->rdf_open = 1;
    return 1;
}

static void rdf_close(WavePlay *wp)
{
    if (wp->rdf_index) {
        wp_free(wp->rdf_index);
        wp->rdf_index = NULL;
    }
    if (wp->rdf && wp->rdf != INVALID_HANDLE_VALUE)
        CloseHandle(wp->rdf);
    wp->rdf = NULL;
    wp->rdf_files = 0;
    wp->rdf_open = 0;
}

/* Index of name in the archive (case-insensitive), or -1. */
static int rdf_find(WavePlay *wp, const char *name)
{
    int lo = 0, hi = (int)wp->rdf_files - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const char *rec = (const char *)wp->rdf_index + mid * RDF_REC;
        char recname[0x77];
        memcpy(recname, rec, 0x76);
        recname[0x76] = 0;
        int c = stricmp_ascii(name, recname);
        if (c == 0)
            return rd16((const uint8_t *)rec + 0x76);
        if (c < 0)
            hi = mid - 1;
        else
            lo = mid + 1;
    }
    return -1;
}

/* Whole file `idx` of the archive, or NULL. */
static uint8_t *rdf_read(WavePlay *wp, int idx, uint32_t *size)
{
    if (!wp->rdf_open || idx < 0 || idx >= wp->rdf_files)
        return NULL;
    const uint8_t *rec = wp->rdf_index + idx * RDF_REC;
    uint32_t len = rd32(rec + 0x78), at = rd32(rec + 0x7c);
    uint8_t *buf = wp_alloc(len);
    DWORD n;
    if (!buf || SetFilePointer(wp->rdf, (LONG)at, NULL, FILE_BEGIN) != at ||
        !ReadFile(wp->rdf, buf, len, &n, NULL) || n != len) {
        wp_free(buf);
        return NULL;
    }
    *size = len;
    return buf;
}

static uint8_t *file_read(const char *path, uint32_t *size)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return NULL;
    DWORD len = GetFileSize(f, NULL), n;
    uint8_t *buf = len != INVALID_FILE_SIZE ? wp_alloc(len) : NULL;
    if (!buf || !ReadFile(f, buf, len, &n, NULL) || n != len) {
        wp_free(buf);
        buf = NULL;
    }
    CloseHandle(f);
    *size = len;
    return buf;
}

/* ---- waves ---- */

static int new_slot(WavePlay *wp)
{
    for (int i = 0; i < wp->n_waves; i++)
        if (!wp->waves[i].used)
            return i;
    Wave *w = wp_alloc((size_t)(wp->n_waves + 1) * sizeof *w);
    if (!w)
        return -1;
    if (wp->waves) {
        memcpy(w, wp->waves, (size_t)wp->n_waves * sizeof *w);
        wp_free(wp->waves);
    }
    wp->waves = w;
    w[wp->n_waves].queue_pos = -1;
    return wp->n_waves++;
}

/* Slot of a loaded wave with this key, or -1. */
static int find_wave(WavePlay *wp, uint32_t key, int from_resource)
{
    for (int i = 0; i < wp->n_waves; i++)
        if (wp->waves[i].used && wp->waves[i].key == key && wp->waves[i].from_resource == from_resource)
            return i;
    return -1;
}

/* Take the PCM data out of a RIFF/WAVE file in memory. Like the original, only
 * plain PCM with a 16-byte "fmt " chunk is accepted. */
static int parse_wav(Wave *w, const uint8_t *f, uint32_t size)
{
    if (size < 12 || memcmp(f, "RIFF", 4) || (memcmp(f + 8, "WAVE", 4) && memcmp(f + 8, "WAV7", 4)))
        return 0;
    int have_fmt = 0;
    for (uint32_t at = 12; at + 8 <= size;) {
        uint32_t len = rd32(f + at + 4);
        const uint8_t *body = f + at + 8;
        if (len > size - at - 8)
            len = size - at - 8;
        if (!memcmp(f + at, "fmt ", 4)) {
            if (len != 16 || rd16(body) != WAVE_FORMAT_PCM)
                return 0;
            memset(&w->fmt, 0, sizeof w->fmt);
            memcpy(&w->fmt, body, 16);
            have_fmt = 1;
        } else if (!memcmp(f + at, "data", 4)) {
            if (!have_fmt || !w->fmt.nBlockAlign)
                return 0;
            w->size = len - len % w->fmt.nBlockAlign;
            w->data = wp_alloc(w->size);
            if (!w->data)
                return 0;
            memcpy(w->data, body, w->size);
            return 1;
        }
        at += 8 + ((len + 1) & ~1u);
    }
    return 0;
}

static void unqueue(WavePlay *wp, int slot)
{
    for (int i = 0; i < wp->n_queue; i++) {
        if (wp->queue[i].wave != slot)
            continue;
        memmove(&wp->queue[i], &wp->queue[i + 1], (size_t)(wp->n_queue - i - 1) * sizeof wp->queue[0]);
        wp->n_queue--;
        break;
    }
    for (int i = 0; i < wp->n_queue; i++)
        wp->waves[wp->queue[i].wave].queue_pos = i;
    if (slot >= 0 && slot < wp->n_waves)
        wp->waves[slot].queue_pos = -1;
}

/* Unload after 10 s unless it is played again: move to the end of the queue. */
static void queue_for_unload(WavePlay *wp, int slot)
{
    if (wp->waves[slot].queue_pos >= 0)
        unqueue(wp, slot);
    if (wp->n_queue >= UNLOAD_QUEUE)
        return;
    wp->queue[wp->n_queue].wave = slot;
    wp->queue[wp->n_queue].since = timeGetTime();
    wp->waves[slot].queue_pos = wp->n_queue++;
}

static void free_wave(WavePlay *wp, int slot)
{
    Wave *w = &wp->waves[slot];
    wp_free(w->data);
    memset(w, 0, sizeof *w);
    w->queue_pos = -1;
}

/* ---- channels ---- */

int __cdecl DSStopWaveIdx(WavePlay *wp, int idx);

static void apply_volume(Channel *c)
{
    int v = c->volume - 10000;
    IDirectSoundBuffer_SetVolume(c->buf, v < -10000 ? -10000 : v > 0 ? 0 : v);
}

static void apply_pan(Channel *c)
{
    IDirectSoundBuffer_SetPan(c->buf, c->pan < -10000 ? -10000 : c->pan > 10000 ? 10000 : c->pan);
}

/* Run the channel's stop message, if any. */
static void stop_message(WavePlay *wp, int idx)
{
    Channel *c = &wp->ch[idx];
    if (wp->unloading_all || !c->msg)
        return;
    uint8_t m[0x22] = {0};
    memcpy(m + 2, &c->msg, 4);
    m[9] = 1;
    memcpy(m + 10, &c->key, 4);
    RtRunMsg(wp->app, m);
}

static void release_channel(WavePlay *wp, int idx)
{
    Channel *c = &wp->ch[idx];
    IDirectSoundBuffer_Stop(c->buf);
    IDirectSoundBuffer_Release(c->buf);
    memset(c, 0, sizeof *c);
    wp->playing--;
}

/* First channel playing `key`, or -1. */
static int channel_of(WavePlay *wp, uint32_t key)
{
    for (int i = 0; i < wp->n_channels; i++)
        if (wp->ch[i].buf && wp->ch[i].key == key)
            return i;
    return -1;
}

/* A free channel; if none, the one with the lowest priority below `priority`.
 * original: a channel's priority is never stored, so every busy channel counts as
 * 0. When all are busy, priority-0 sounds are dropped and higher ones always take
 * channel 0. */
static int alloc_channel(WavePlay *wp, int16_t priority)
{
    int best = -1, i;
    int16_t lowest = 0x7fff;
    for (i = 0; i < wp->n_channels && wp->ch[i].buf; i++) {
        if (wp->ch[i].priority < priority && wp->ch[i].priority < lowest) {
            lowest = wp->ch[i].priority;
            best = i;
        }
    }
    if (i == wp->n_channels) {
        if (best < 0)
            return -1;
        i = best;
    }
    if (wp->ch[i].buf)
        DSStopWaveIdx(wp, i);
    return i;
}

/* Play wave `slot` on a channel. */
static int play(WavePlay *wp, int slot, int volume, int pan, int loop, int msg, int priority,
                int restart, int auto_unload)
{
    Wave *w = &wp->waves[slot];
    int16_t prio = priority ? (int16_t)priority : w->priority;
    int idx = channel_of(wp, w->key);
    if (idx >= 0 && restart) {
        IDirectSoundBuffer_Stop(wp->ch[idx].buf);
        IDirectSoundBuffer_SetCurrentPosition(wp->ch[idx].buf, 0);
        wp->playing--;
    } else {
        idx = alloc_channel(wp, prio);
        if (idx < 0)
            return -1;
        w = &wp->waves[slot]; /* a stop message may have loaded more waves */
        DSBUFFERDESC desc = {0};
        desc.dwSize = sizeof desc;
        desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLPAN |
                       DSBCAPS_CTRLVOLUME;
        desc.dwBufferBytes = w->size ? w->size : w->fmt.nBlockAlign;
        desc.lpwfxFormat = &w->fmt;
        IDirectSoundBuffer *buf;
        if (!w->used || IDirectSound_CreateSoundBuffer(wp->ds, &desc, &buf, NULL) != DS_OK)
            return -1;
        void *p1, *p2;
        DWORD n1, n2;
        if (IDirectSoundBuffer_Lock(buf, 0, desc.dwBufferBytes, &p1, &n1, &p2, &n2, 0) == DS_OK) {
            memcpy(p1, w->data, n1 < w->size ? n1 : w->size);
            IDirectSoundBuffer_Unlock(buf, p1, n1, p2, n2);
        }
        wp->ch[idx].buf = buf;
    }
    Channel *c = &wp->ch[idx];
    c->key = w->key;
    c->wave = slot;
    c->loop = loop & 1;
    c->msg = msg;
    c->priority = 0; /* original: never set */
    c->volume = volume;
    c->pan = pan;
    apply_volume(c);
    apply_pan(c);
    if (wp->audio_paused) {
        c->paused = 1;
        c->pause_pos = 0;
    } else {
        IDirectSoundBuffer_Play(c->buf, 0, 0, c->loop ? DSBPLAY_LOOPING : 0);
        c->paused = 0;
    }
    wp->playing++;
    if (auto_unload && !loop)
        queue_for_unload(wp, slot);
    return idx;
}

/* ---- life cycle ---- */

int __cdecl DSInitWavePlay(WavePlay *wp)
{
    if (wp->ds)
        return 1;
    wp->master_volume = 10000;
    int n = (int)GetPrivateProfileIntA("Dominion", "SoundBuffers", 8, "dominion.ini");
    wp->n_channels = n < 8 ? 8 : n > MAX_CHANNELS ? MAX_CHANNELS : n;
    memset(wp->ch, 0, sizeof wp->ch);
    if (DirectSoundCreate(NULL, &wp->ds, NULL) != DS_OK ||
        IDirectSound_SetCooperativeLevel(wp->ds, RtGetWindow(wp->app), DSSCL_PRIORITY) != DS_OK) {
        if (wp->ds)
            IDirectSound_Release(wp->ds);
        wp->ds = NULL;
        MessageBoxA(NULL,
                    "ERROR initializing DirectSound. Do you have another application that has "
                    "access to your sound hardware? Please close all other applications and verify "
                    "that your sound hardware is working properly.",
                    "Dominion: Storm Over Gift3", MB_ICONERROR);
        return 0;
    }
    /* The master volume and pan act on the primary buffer, like the original: this
     * covers everything played through this DirectSound object (effects, cutscene
     * sound and our music). The volume it had before is restored at the end. */
    DSBUFFERDESC pd = {0};
    pd.dwSize = sizeof pd;
    pd.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN;
    if (IDirectSound_CreateSoundBuffer(wp->ds, &pd, &wp->primary, NULL) == DS_OK) {
        IDirectSoundBuffer_GetVolume(wp->primary, &wp->saved_primary_volume);
        IDirectSoundBuffer_GetPan(wp->primary, &wp->saved_primary_pan);
    } else {
        wp->primary = NULL;
    }
    wp->primary_fmt.wFormatTag = WAVE_FORMAT_PCM;
    wp->primary_fmt.nChannels = 2;
    wp->primary_fmt.nSamplesPerSec = 22050;
    wp->primary_fmt.wBitsPerSample = 8;
    wp->n_queue = 0;
    music_init(wp);
    return 1;
}

int __cdecl DSDeInitWavePlay(WavePlay *wp)
{
    if (!wp || !wp->ds)
        return 1;
    music_shutdown();
    for (int i = 0; i < MAX_CHANNELS; i++)
        if (wp->ch[i].buf) {
            IDirectSoundBuffer_Stop(wp->ch[i].buf);
            IDirectSoundBuffer_Release(wp->ch[i].buf);
        }
    memset(wp->ch, 0, sizeof wp->ch);
    for (int i = 0; i < wp->n_waves; i++)
        wp_free(wp->waves[i].data);
    wp_free(wp->waves);
    wp->waves = NULL;
    wp->n_waves = 0;
    wp->n_queue = 0;
    wp->playing = 0;
    if (wp->primary) {
        IDirectSoundBuffer_SetVolume(wp->primary, wp->saved_primary_volume);
        IDirectSoundBuffer_SetPan(wp->primary, wp->saved_primary_pan);
        IDirectSoundBuffer_Release(wp->primary);
        wp->primary = NULL;
    }
    IDirectSound_Release(wp->ds);
    wp->ds = NULL;
    return 1;
}

/* The engine calls this regularly (its one audio service slot). */
void __stdcall DSAudioService(WavePlay *wp)
{
    music_service();
    DWORD now = timeGetTime();
    if (now - wp->last_service <= 4)
        return;
    wp->last_service = now;

    /* unload waves that have not been played for 10 s */
    for (int i = 0; i < wp->n_queue;) {
        UnloadEntry *e = &wp->queue[i];
        if (now - e->since < 10000)
            break;
        int slot = e->wave, keep = 0;
        for (int c = 0; c < wp->n_channels; c++) {
            if (!wp->ch[c].buf || wp->ch[c].key != wp->waves[slot].key)
                continue;
            DWORD status = 0;
            IDirectSoundBuffer_GetStatus(wp->ch[c].buf, &status);
            if ((status & DSBSTATUS_PLAYING) || wp->ch[c].loop) {
                e->since += 10000; /* still in use: try again later */
                keep = 1;
                break;
            }
            DSStopWaveIdx(wp, c);
        }
        if (keep) {
            i++;
            continue;
        }
        /* original: removed the first N queue entries afterwards, which dropped the
         * wrong ones when an earlier entry had been postponed */
        unqueue(wp, slot);
        free_wave(wp, slot);
    }

    /* channels whose sound has finished */
    for (int c = 0; c < wp->n_channels; c++) {
        Channel *ch = &wp->ch[c];
        if (!ch->buf || ch->paused || ch->loop)
            continue;
        DWORD status = 0;
        if (IDirectSoundBuffer_GetStatus(ch->buf, &status) == DS_OK && !(status & DSBSTATUS_PLAYING))
            DSStopWaveIdx(wp, c);
    }
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)inst, (void)reason, (void)reserved;
    return TRUE;
}

WavePlay *__cdecl RtLibInit(void *app, void *info)
{
    WavePlay *wp = wp_alloc(sizeof *wp);
    if (!wp)
        return NULL;
    g_wp = wp;
    wp->app = app;
    wp->hres = RtGetHResfile(app);
    DSInitWavePlay(wp);
    *(void **)((uint8_t *)info + 0xc) = (void *)DSAudioService;
    ServicePair ours = {(void(__stdcall *)(void *))DSAudioService, wp};
    AudioSetServiceProc(RtGetHAL(app), &ours, &wp->saved_service);
    return wp;
}

void __cdecl RtLibFree(WavePlay *wp)
{
    AudioSetServiceProc(RtGetHAL(wp->app), &wp->saved_service, NULL);
    DSDeInitWavePlay(wp);
    rdf_close(wp);
    if (g_wp == wp)
        g_wp = NULL;
    wp_free(wp);
}

int __cdecl rdfInitWavePlay(WavePlay *wp, unsigned path_id)
{
    const char *path = RtGetString(wp->app, path_id);
    if (!path || !rdf_open(wp, path))
        return 0;
    return DSInitWavePlay(wp);
}

int __cdecl DSUnloadWave(WavePlay *wp, int key);

void __cdecl rdfDeInitWavePlay(WavePlay *wp)
{
    DSUnloadWave(wp, 0); /* original: key 0, i.e. only archive file 0 */
    rdf_close(wp);
    DSDeInitWavePlay(wp);
}

IDirectSound *__cdecl DSGetObject(WavePlay *wp)
{
    return wp->ds;
}

/* ---- loading ---- */

/* Load name: a loose file of that name, else the archive. Returns the slot, -1 for
 * a bad file, -2 if not found, -3 if out of memory. */
int __cdecl rdfLoadWave(WavePlay *wp, const char *name, int unused)
{
    (void)unused;
    if (!name)
        return -1;
    g_wp = wp;
    int idx = wp->rdf_open ? rdf_find(wp, name) : -1;
    uint32_t key = idx >= 0 ? (uint32_t)idx : calc_crc32((const uint8_t *)name, lstrlenA(name));
    int slot = find_wave(wp, key, 0);
    if (slot >= 0)
        return slot;
    uint32_t size = 0;
    uint8_t *file = file_read(name, &size);
    if (!file) {
        if (idx < 0)
            return -2;
        file = rdf_read(wp, idx, &size);
        if (!file)
            return -1;
    }
    slot = new_slot(wp);
    if (slot < 0) {
        wp_free(file);
        return -3;
    }
    Wave *w = &wp->waves[slot];
    memset(w, 0, sizeof *w);
    int ok = parse_wav(w, file, size);
    wp_free(file);
    if (!ok) {
        wp_free(w->data);
        memset(w, 0, sizeof *w);
        w->queue_pos = -1;
        return -1;
    }
    w->used = 1;
    w->key = key;
    w->queue_pos = -1;
    return slot;
}

int __cdecl rdfLoadWaveFile(WavePlay *wp, unsigned name_id, int unused)
{
    const char *name = RtGetString(wp->app, name_id);
    return name ? rdfLoadWave(wp, name, unused) : -1;
}

/* A loose WAV file, keyed by the CRC32 of its name. original: loaded again on
 * every call (no lookup). */
int __cdecl _LoadWaveFile(const char *name)
{
    WavePlay *wp = g_wp;
    if (!wp || !name)
        return -1;
    uint32_t size = 0;
    uint8_t *file = file_read(name, &size);
    if (!file)
        return -1;
    int slot = new_slot(wp);
    if (slot < 0) {
        wp_free(file);
        return -1;
    }
    Wave *w = &wp->waves[slot];
    memset(w, 0, sizeof *w);
    int ok = parse_wav(w, file, size);
    wp_free(file);
    if (!ok) {
        wp_free(w->data);
        memset(w, 0, sizeof *w);
        w->queue_pos = -1;
        return -1;
    }
    w->used = 1;
    w->key = calc_crc32((const uint8_t *)name, lstrlenA(name));
    w->queue_pos = -1;
    return slot;
}

/* original: returns 0 rather than -1 when the string is missing */
int __cdecl DSLoadWaveFile(WavePlay *wp, unsigned name_id)
{
    g_wp = wp;
    const char *name = RtGetString(wp->app, name_id);
    return name ? _LoadWaveFile(name) : 0;
}

/* Resource `id` from the engine's resource file: a 32-byte header (size, flags,
 * priority, wave format) followed by PCM or IMA ADPCM data. */
int __cdecl DSLoadWave(WavePlay *wp, uint32_t id, int unused)
{
    (void)unused;
    int slot = find_wave(wp, id, 1);
    if (slot >= 0)
        return slot;
    uint8_t hdr[0x20];
    if (!ResReadResourceData(wp->hres, id, hdr, sizeof hdr, 0))
        return -1;
    uint32_t size = rd32(hdr);
    uint8_t *raw = wp_alloc(size);
    if (!raw || !ResReadResourceData(wp->hres, id, raw, size, 0x20)) {
        wp_free(raw);
        return -1;
    }
    slot = new_slot(wp);
    if (slot < 0) {
        wp_free(raw);
        return -1;
    }
    Wave *w = &wp->waves[slot];
    memset(w, 0, sizeof *w);
    w->priority = (int16_t)rd16(hdr + 8);
    memcpy(&w->fmt, hdr + 0x10, 14);
    w->fmt.wBitsPerSample = rd16(hdr + 0x1e);
    w->fmt.cbSize = 0;
    if (hdr[6]) { /* IMA ADPCM: decode with the engine's decoder */
        uint32_t out = size << (w->fmt.wBitsPerSample == 16 ? 2 : 1);
        uint8_t state[12] = {0};
        w->data = wp_alloc(out);
        if (!w->data) {
            wp_free(raw);
            memset(w, 0, sizeof *w);
            w->queue_pos = -1;
            return -1;
        }
        ima2pcm(state, w->data, raw, size, w->fmt.nChannels == 2);
        wp_free(raw);
        w->size = out;
        w->fmt.wFormatTag = WAVE_FORMAT_PCM;
        w->fmt.nBlockAlign = (WORD)(w->fmt.nChannels * w->fmt.wBitsPerSample / 8);
        w->fmt.nAvgBytesPerSec = w->fmt.nSamplesPerSec * w->fmt.nBlockAlign;
    } else {
        w->data = raw;
        w->size = size;
    }
    if (w->fmt.nBlockAlign)
        w->size -= w->size % w->fmt.nBlockAlign;
    w->used = 1;
    w->key = id;
    w->from_resource = 1;
    w->queue_pos = -1;
    return slot;
}

/* ---- playing ---- */

/* Play a loaded wave. Returns the channel or -1.
 *   loop: repeat until stopped     msg: script message when it stops
 *   priority: 0 = the wave's own   restart: if it is already playing, start that again
 *   auto_unload: unload 10 s after the last play */
int __cdecl DSPlayWaveFile(WavePlay *wp, int slot, int volume, int pan, int loop, int msg,
                           int priority, int restart, int auto_unload)
{
    if (slot < 0 || slot >= wp->n_waves || !wp->waves[slot].used)
        return -1;
    return play(wp, slot, volume, pan, loop, msg, priority, restart, auto_unload);
}

int __cdecl rdfPlayWave(WavePlay *wp, const char *name, int volume, int pan, int loop, int msg,
                        int priority, int restart, int unused, int auto_unload)
{
    (void)unused;
    int slot = rdfLoadWave(wp, name, 0);
    if (slot < 0)
        return slot;
    return DSPlayWaveFile(wp, slot, volume, pan, loop, msg, priority, restart, auto_unload);
}

int __cdecl rdfPlayWaveFile(WavePlay *wp, unsigned name_id, int volume, int pan, int loop, int msg,
                            int priority, int restart, int unused, int auto_unload)
{
    const char *name = RtGetString(wp->app, name_id);
    if (!name)
        return -1;
    return rdfPlayWave(wp, name, volume, pan, loop, msg, priority, restart, unused, auto_unload);
}

/* Same for a resource of the engine's resource file. */
int __cdecl DSPlayWave(WavePlay *wp, uint32_t id, int volume, int pan, int loop, int msg,
                       int priority, int restart, int unused, int auto_unload)
{
    int slot = DSLoadWave(wp, id, unused);
    if (slot < 0)
        return -1;
    return play(wp, slot, volume, pan, loop, msg, priority, restart, auto_unload);
}

/* ---- stopping ---- */

int __cdecl DSStopWaveIdx(WavePlay *wp, int idx)
{
    if (idx < 0 || idx >= wp->n_channels || !wp->ch[idx].buf)
        return 0;
    stop_message(wp, idx);
    if (!wp->ch[idx].buf) /* the message already stopped it */
        return 1;
    release_channel(wp, idx);
    return 1;
}

/* key 0 = every channel. */
int __cdecl DSStopWave(WavePlay *wp, uint32_t key)
{
    if (key == 0) {
        for (int i = 0; i < wp->n_channels; i++) {
            if (!wp->ch[i].buf)
                continue;
            stop_message(wp, i);
            if (!wp->ch[i].buf)
                return 0; /* original */
            release_channel(wp, i);
        }
        return 1;
    }
    int idx = channel_of(wp, key);
    if (idx < 0)
        return 0;
    stop_message(wp, idx);
    if (!wp->ch[idx].buf)
        return 0;
    release_channel(wp, idx);
    return 1;
}

static uint32_t name_key(WavePlay *wp, const char *name)
{
    int idx = wp->rdf_open ? rdf_find(wp, name) : -1;
    return idx >= 0 ? (uint32_t)idx : calc_crc32((const uint8_t *)name, lstrlenA(name));
}

int __cdecl rdfStopWave(WavePlay *wp, const char *name)
{
    return name ? DSStopWave(wp, name_key(wp, name)) : 0;
}

int __cdecl rdfStopWaveFile(WavePlay *wp, unsigned name_id)
{
    const char *name = RtGetString(wp->app, name_id);
    return name ? rdfStopWave(wp, name) : -1;
}

void __cdecl rdfStopWaveIdx(WavePlay *wp, int idx)
{
    DSStopWaveIdx(wp, idx);
}

void __cdecl rdfStopAll(WavePlay *wp)
{
    DSStopWave(wp, 0);
}

void __cdecl DSPauseCDTrack(WavePlay *wp, int pause);

/* Pause or resume every channel (and the music if `cd`). Sounds started while
 * paused wait too. */
void __cdecl DSPauseAudio(WavePlay *wp, int pause, int cd)
{
    wp->audio_paused = pause;
    for (int i = 0; i < wp->n_channels; i++) {
        Channel *c = &wp->ch[i];
        if (!c->buf)
            continue;
        if (pause && !c->paused) {
            DWORD write;
            IDirectSoundBuffer_GetCurrentPosition(c->buf, &c->pause_pos, &write);
            c->paused = 1;
            IDirectSoundBuffer_Stop(c->buf);
        } else if (!pause && c->paused) {
            IDirectSoundBuffer_SetCurrentPosition(c->buf, c->pause_pos);
            c->paused = 0;
            IDirectSoundBuffer_Play(c->buf, 0, 0, c->loop ? DSBPLAY_LOOPING : 0);
        }
    }
    if (cd)
        DSPauseCDTrack(wp, pause ? 1 : 0);
}

/* ---- unloading ---- */

static void stop_wave_channels(WavePlay *wp, uint32_t key)
{
    for (int c = 0; c < wp->n_channels; c++)
        if (wp->ch[c].buf && wp->ch[c].key == key)
            DSStopWaveIdx(wp, c);
}

static void unload_all(WavePlay *wp)
{
    wp->unloading_all = 1;
    wp->n_queue = 0;
    for (int i = 0; i < wp->n_waves; i++)
        if (wp->waves[i].used)
            stop_wave_channels(wp, wp->waves[i].key);
    for (int i = 0; i < wp->n_waves; i++)
        wp_free(wp->waves[i].data);
    wp_free(wp->waves);
    wp->waves = NULL;
    wp->n_waves = 0;
    wp->unloading_all = 0;
}

/* Unload by key; UNLOAD_ALL_KEY = everything (without stop messages). */
int __cdecl DSUnloadWave(WavePlay *wp, int key)
{
    if (key == UNLOAD_ALL_KEY) {
        unload_all(wp);
        return 1;
    }
    int slot = -1;
    for (int i = 0; i < wp->n_waves && slot < 0; i++)
        if (wp->waves[i].used && wp->waves[i].key == (uint32_t)key)
            slot = i;
    if (slot < 0)
        return 0;
    stop_wave_channels(wp, (uint32_t)key);
    unqueue(wp, slot);
    free_wave(wp, slot);
    return 1;
}

/* Unload by slot; UNLOAD_ALL_KEY = everything. */
int __cdecl DSUnloadWaveFile(WavePlay *wp, int slot)
{
    if (slot == UNLOAD_ALL_KEY) {
        unload_all(wp);
        return 1;
    }
    if (slot < 0 || slot >= wp->n_waves || !wp->waves[slot].used)
        return 0;
    stop_wave_channels(wp, wp->waves[slot].key);
    unqueue(wp, slot);
    free_wave(wp, slot);
    return 1;
}

int __cdecl rdfUnloadWave(WavePlay *wp, const char *name)
{
    return name ? DSUnloadWave(wp, (int)name_key(wp, name)) : 0;
}

int __cdecl rdfUnloadWaveFile(WavePlay *wp, int name_id)
{
    if (name_id == UNLOAD_ALL_KEY)
        return DSUnloadWave(wp, UNLOAD_ALL_KEY);
    const char *name = RtGetString(wp->app, (unsigned)name_id);
    return name ? rdfUnloadWave(wp, name) : 0;
}

/* original: unloads key 0 only (archive file 0), not everything */
int __cdecl rdfUnloadAll(WavePlay *wp)
{
    return DSUnloadWave(wp, 0);
}

/* ---- volume, pan, frequency ---- */

static Channel *channel(WavePlay *wp, int idx)
{
    return idx >= 0 && idx < wp->n_channels && wp->ch[idx].buf ? &wp->ch[idx] : NULL;
}

int __cdecl DSSetVolumeIdx(WavePlay *wp, int idx, int volume)
{
    Channel *c = channel(wp, idx);
    if (!c)
        return 0;
    c->volume = volume;
    apply_volume(c);
    return 1;
}

/* key 0 = master volume (primary buffer). */
int __cdecl DSSetVolume(WavePlay *wp, uint32_t key, int volume)
{
    if (key == 0) {
        if (!wp->primary)
            return 0;
        wp->master_volume = volume;
        return IDirectSoundBuffer_SetVolume(wp->primary, volume - 10000) == DS_OK;
    }
    return DSSetVolumeIdx(wp, channel_of(wp, key), volume);
}

int __cdecl DSGetVolumeIdx(WavePlay *wp, int idx)
{
    Channel *c = channel(wp, idx);
    return c ? c->volume : 0;
}

int __cdecl DSGetVolume(WavePlay *wp, uint32_t key)
{
    if (key == 0)
        return wp->primary ? wp->master_volume : -1;
    int idx = channel_of(wp, key);
    return idx >= 0 ? wp->ch[idx].volume : 0;
}

int __cdecl DSSetPanIdx(WavePlay *wp, int idx, int pan)
{
    Channel *c = channel(wp, idx);
    if (!c)
        return 0;
    c->pan = pan;
    apply_pan(c);
    return 1;
}

int __cdecl DSSetPan(WavePlay *wp, uint32_t key, int pan)
{
    if (key == 0) {
        if (!wp->primary)
            return 0;
        wp->master_pan = pan;
        return IDirectSoundBuffer_SetPan(wp->primary, pan) == DS_OK;
    }
    return DSSetPanIdx(wp, channel_of(wp, key), pan);
}

int __cdecl DSGetPanIdx(WavePlay *wp, int idx)
{
    Channel *c = channel(wp, idx);
    return c ? c->pan : 0;
}

int __cdecl DSGetPan(WavePlay *wp, uint32_t key)
{
    if (key == 0)
        return wp->primary ? wp->master_pan : -10001;
    int idx = channel_of(wp, key);
    return idx >= 0 ? wp->ch[idx].pan : 0;
}

int __cdecl DSSetFrequencyIdx(WavePlay *wp, int idx, uint32_t hz)
{
    Channel *c = channel(wp, idx);
    return c && IDirectSoundBuffer_SetFrequency(c->buf, hz) == DS_OK;
}

int __cdecl DSSetFrequency(WavePlay *wp, uint32_t key, uint32_t hz)
{
    return DSSetFrequencyIdx(wp, channel_of(wp, key), hz);
}

uint32_t __cdecl DSGetFrequencyIdx(WavePlay *wp, int idx)
{
    Channel *c = channel(wp, idx);
    DWORD hz = 0;
    if (!c || IDirectSoundBuffer_GetFrequency(c->buf, &hz) != DS_OK)
        return 0;
    return hz;
}

uint32_t __cdecl DSGetFrequency(WavePlay *wp, uint32_t key)
{
    return DSGetFrequencyIdx(wp, channel_of(wp, key));
}

/* ---- formats and channel count ---- */

int __cdecl DSGetFormat(WavePlay *wp, uint32_t key, uint32_t *channels, uint32_t *rate, uint32_t *bits)
{
    for (int i = 0; i < wp->n_waves; i++) {
        Wave *w = &wp->waves[i];
        if (!w->used || w->key != key)
            continue;
        *channels = w->fmt.nChannels;
        *rate = w->fmt.nSamplesPerSec;
        *bits = w->fmt.wBitsPerSample;
        return 1;
    }
    return 0;
}

/* The game asks for 22 kHz 8-bit. Modern systems mix at their own rate, so we only
 * remember it; applying it under Wine would lower the quality of everything. */
int __cdecl DSSetFormatPrimary(WavePlay *wp, int channels, int rate, int bits)
{
    wp->primary_fmt.nChannels = (WORD)channels;
    wp->primary_fmt.nSamplesPerSec = (DWORD)rate;
    wp->primary_fmt.wBitsPerSample = (WORD)bits;
    return 1;
}

int __cdecl DSGetFormatPrimary(WavePlay *wp, uint32_t *channels, uint32_t *rate, uint32_t *bits)
{
    *channels = wp->primary_fmt.nChannels;
    *rate = wp->primary_fmt.nSamplesPerSec;
    *bits = wp->primary_fmt.wBitsPerSample;
    return 1;
}

int __cdecl DSGetMaxBuffers(WavePlay *wp)
{
    return wp->n_channels;
}

/* original: when lowering the count it also stops channel n-1 */
void __cdecl DSSetMaxBuffers(WavePlay *wp, uint32_t n)
{
    if (n > MAX_CHANNELS)
        return;
    if ((int)n < wp->n_channels)
        for (int i = n ? (int)n - 1 : 0; i < wp->n_channels; i++)
            DSStopWaveIdx(wp, i);
    wp->n_channels = (int)n;
}
