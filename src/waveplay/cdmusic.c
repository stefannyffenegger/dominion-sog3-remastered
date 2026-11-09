/* CD music from files. The original played the CD's audio tracks through MCI; we
 * play WAV files ripped from the discs (tools/rip_music.sh):
 *     <game>\music\DOMINION1\track02.wav ... track06.wav   (or trackNN.cdda.wav)
 *     <game>\music\DOMINION2\...
 * "The disc in the drive" is whichever disc the game last asked for with
 * DSCheckCDIdentity (Human/Darken missions: disc 1, Merc/Scorpion: disc 2).
 *
 * Music streams from a background thread into a DirectSound buffer, so it keeps
 * playing while the game loads. The "track finished" message is sent from the
 * engine's audio service call, on the game's own thread. */
#include <string.h>
#include "rtlib32.h"
#include "waveplay.h"
#include <mmsystem.h>

typedef struct Music {
    WavePlay *wp;
    char base[MAX_PATH]; /* <game>\music\ */
    char disc[32];       /* "DOMINION1" / "DOMINION2" */
    int volume;          /* 0..10000 as the game sets it */

    CRITICAL_SECTION lock;
    int lock_ready;
    HANDLE thread, wake;
    volatile LONG quit;

    /* current track (guarded by lock) */
    int track; /* 0 = none */
    int paused;
    int msg;   /* message to send when the track ends */
    volatile LONG ended;
    HANDLE file;
    uint32_t data_left;
    IDirectSoundBuffer *buf;
    uint32_t buf_size, write_pos, last_play, block;
    uint64_t written_real, written_total, played;
} Music;

static Music music;

/* 2000 * log10(percent / 100): DirectSound volume in 1/100 dB */
static const int16_t db_table[101] = {
    -10000, -4000, -3398, -3046, -2796, -2602, -2444, -2310, -2194, -2092, -2000, -1917, -1842,
    -1772,  -1708, -1648, -1592, -1539, -1489, -1442, -1398, -1356, -1315, -1277, -1240, -1204,
    -1170,  -1137, -1106, -1075, -1046, -1017, -990,  -963,  -937,  -912,  -887,  -864,  -840,
    -818,   -796,  -774,  -754,  -733,  -713,  -694,  -674,  -656,  -638,  -620,  -602,  -585,
    -568,   -551,  -535,  -519,  -504,  -488,  -473,  -458,  -444,  -429,  -415,  -401,  -388,
    -374,   -361,  -348,  -335,  -322,  -310,  -297,  -285,  -273,  -262,  -250,  -238,  -227,
    -216,   -205,  -194,  -183,  -172,  -162,  -151,  -141,  -131,  -121,  -111,  -101,  -92,
    -82,    -72,   -63,   -54,   -45,   -35,   -26,   -18,   -9,    0};

static void apply_volume(void)
{
    if (!music.buf)
        return;
    int v = music.volume < 0 ? 0 : music.volume > 10000 ? 10000 : music.volume;
    IDirectSoundBuffer_SetVolume(music.buf, db_table[v / 100]);
}

/* ---- WAV files ---- */

typedef struct WavInfo {
    WAVEFORMATEX fmt;
    uint32_t data_size;
} WavInfo;

static HANDLE track_open(int track, WavInfo *wi)
{
    static const char *const patterns[] = {"%s%s\\track%02d.wav", "%s%s\\track%02d.cdda.wav"};
    char path[MAX_PATH];
    HANDLE f = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 2 && f == INVALID_HANDLE_VALUE; i++) {
        wsprintfA(path, patterns[i], music.base, music.disc, track);
        f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    }
    if (f == INVALID_HANDLE_VALUE)
        return NULL;

    uint8_t hdr[12];
    DWORD n;
    int have_fmt = 0;
    if (!ReadFile(f, hdr, 12, &n, NULL) || n != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4))
        goto fail;
    for (;;) {
        uint8_t ck[8];
        if (!ReadFile(f, ck, 8, &n, NULL) || n != 8)
            goto fail;
        uint32_t size = ck[4] | ck[5] << 8 | ck[6] << 16 | (uint32_t)ck[7] << 24;
        if (!memcmp(ck, "fmt ", 4)) {
            uint8_t fm[16];
            if (size < 16 || !ReadFile(f, fm, 16, &n, NULL) || n != 16)
                goto fail;
            memset(&wi->fmt, 0, sizeof wi->fmt);
            memcpy(&wi->fmt, fm, 16);
            have_fmt = 1;
            SetFilePointer(f, (size - 16 + 1) & ~1u, NULL, FILE_CURRENT);
        } else if (!memcmp(ck, "data", 4)) {
            if (!have_fmt || wi->fmt.wFormatTag != WAVE_FORMAT_PCM || !wi->fmt.nBlockAlign)
                goto fail;
            wi->data_size = size - size % wi->fmt.nBlockAlign;
            return f;
        } else {
            SetFilePointer(f, (size + 1) & ~1u, NULL, FILE_CURRENT);
        }
    }
fail:
    CloseHandle(f);
    return NULL;
}

static uint32_t track_length_ms(int track)
{
    WavInfo wi;
    HANDLE f = track_open(track, &wi);
    if (!f)
        return 0;
    CloseHandle(f);
    return (uint32_t)((uint64_t)wi.data_size * 1000 / wi.fmt.nAvgBytesPerSec);
}

/* ---- streaming (lock held) ---- */

static void stream_close(void)
{
    if (music.buf) {
        IDirectSoundBuffer_Stop(music.buf);
        IDirectSoundBuffer_Release(music.buf);
        music.buf = NULL;
    }
    if (music.file) {
        CloseHandle(music.file);
        music.file = NULL;
    }
    music.track = 0;
    music.paused = 0;
}

/* Write n bytes at write_pos: from the file while it lasts, then silence. */
static void stream_write(uint32_t n)
{
    void *p[2];
    DWORD len[2];
    if (IDirectSoundBuffer_Lock(music.buf, music.write_pos, n, &p[0], &len[0], &p[1], &len[1], 0) != DS_OK)
        return;
    for (int i = 0; i < 2; i++) {
        if (!p[i])
            continue;
        DWORD got = 0;
        uint32_t want = len[i] < music.data_left ? len[i] : music.data_left;
        if (want && ReadFile(music.file, p[i], want, &got, NULL)) {
            music.data_left -= got;
            music.written_real += got;
        }
        if (got < len[i])
            memset((uint8_t *)p[i] + got, 0, len[i] - got);
    }
    IDirectSoundBuffer_Unlock(music.buf, p[0], len[0], p[1], len[1]);
    music.write_pos = (music.write_pos + n) % music.buf_size;
    music.written_total += n;
}

static void stream_pump(void)
{
    if (!music.buf || music.paused || music.ended)
        return;
    DWORD play, write;
    if (IDirectSoundBuffer_GetCurrentPosition(music.buf, &play, &write) != DS_OK)
        return;
    music.played += (play + music.buf_size - music.last_play) % music.buf_size;
    music.last_play = play;
    if (music.played >= music.written_real && music.data_left == 0) {
        IDirectSoundBuffer_Stop(music.buf);
        InterlockedExchange(&music.ended, 1);
        return;
    }
    uint32_t queued = (uint32_t)(music.written_total - music.played);
    uint32_t space = music.buf_size > queued ? music.buf_size - queued : 0;
    space -= space % music.block;
    if (space >= music.buf_size / 4)
        stream_write(space);
}

static int stream_open(int track)
{
    WavInfo wi;
    HANDLE f = track_open(track, &wi);
    if (!f)
        return 0;
    if (!music.wp || !music.wp->ds) {
        CloseHandle(f);
        return 0;
    }
    DSBUFFERDESC desc = {0};
    desc.dwSize = sizeof desc;
    desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME;
    music.block = wi.fmt.nBlockAlign;
    music.buf_size = wi.fmt.nAvgBytesPerSec; /* 1 second */
    music.buf_size -= music.buf_size % music.block;
    desc.dwBufferBytes = music.buf_size;
    desc.lpwfxFormat = &wi.fmt;
    if (IDirectSound_CreateSoundBuffer(music.wp->ds, &desc, &music.buf, NULL) != DS_OK) {
        music.buf = NULL;
        CloseHandle(f);
        return 0;
    }
    music.file = f;
    music.data_left = wi.data_size;
    music.write_pos = music.last_play = 0;
    music.written_real = music.written_total = music.played = 0;
    music.ended = 0;
    music.paused = 0;
    music.track = track;
    apply_volume();
    stream_write(music.buf_size);
    IDirectSoundBuffer_Play(music.buf, 0, 0, DSBPLAY_LOOPING);
    return 1;
}

static DWORD WINAPI stream_thread(LPVOID unused)
{
    (void)unused;
    while (!music.quit) {
        WaitForSingleObject(music.wake, 50);
        EnterCriticalSection(&music.lock);
        stream_pump();
        LeaveCriticalSection(&music.lock);
    }
    return 0;
}

/* ---- life cycle (called from waveplay.c) ---- */

void music_init(WavePlay *wp)
{
    music.wp = wp;
    if (!music.lock_ready) {
        char *p, path[MAX_PATH];
        HMODULE self;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)music_init, &self);
        GetModuleFileNameA(self, path, MAX_PATH);
        p = path + lstrlenA(path);
        while (p > path && p[-1] != '\\' && p[-1] != '/')
            p--;
        *p = 0;
        wsprintfA(music.base, "%smusic\\", path);
        lstrcpyA(music.disc, "DOMINION1");
        music.volume = 9960; /* what the original reported for a full CD volume */
        InitializeCriticalSection(&music.lock);
        music.lock_ready = 1;
    }
    if (!music.thread) {
        music.wake = CreateEventA(NULL, FALSE, FALSE, NULL);
        music.quit = 0;
        music.thread = CreateThread(NULL, 0, stream_thread, NULL, 0, NULL);
    }
}

void music_shutdown(void)
{
    if (!music.lock_ready)
        return;
    if (music.thread) {
        InterlockedExchange(&music.quit, 1);
        SetEvent(music.wake);
        WaitForSingleObject(music.thread, 2000);
        CloseHandle(music.thread);
        CloseHandle(music.wake);
        music.thread = NULL;
    }
    EnterCriticalSection(&music.lock);
    stream_close();
    LeaveCriticalSection(&music.lock);
    music.wp = NULL;
}

/* From the engine's audio service: send the end-of-track message. */
void music_service(void)
{
    if (!music.ended || !music.wp)
        return;
    EnterCriticalSection(&music.lock);
    int msg = music.track ? music.msg : 0;
    stream_close();
    music.ended = 0;
    music.msg = 0;
    LeaveCriticalSection(&music.lock);
    wp_debug("WAVEPLAY: track ended, msg %d\n", msg);
    if (msg) { /* like the original: message id at +2, everything else 0 */
        uint8_t m[0x22] = {0};
        memcpy(m + 2, &msg, 4);
        RtRunMsg(music.wp->app, m);
    }
}

/* ---- exports ---- */

/* "Is disc <label> in the drive?" Always yes; remember which disc the game wants. */
int __cdecl DSCheckCDIdentity(WavePlay *wp, unsigned label_id, int by_identity)
{
    (void)by_identity;
    const char *label = RtGetString(wp->app, label_id);
    if (label && lstrlenA(label) < (int)sizeof music.disc)
        lstrcpyA(music.disc, label);
    wp_debug("WAVEPLAY: disc %s\n", music.disc);
    return 1;
}

/* Tracks on "the disc": track 1 is data, music starts at track 2. 0 = no music files. */
int __cdecl DSNumCDTracks(WavePlay *wp)
{
    (void)wp;
    int n = 1;
    while (n < 99 && track_length_ms(n + 1))
        n++;
    return n > 1 ? n : 0;
}

uint32_t __cdecl DSCDTrackLen(WavePlay *wp, int track)
{
    (void)wp;
    return track >= 2 ? track_length_ms(track) : 0;
}

/* Play one track once; when it ends, message `msg` (if not 0) goes to the script.
 * Original: the end was timed from the start, so a pause made it "end" early. */
void __cdecl DSPlayCDTrack(WavePlay *wp, int track, int msg)
{
    int tracks = DSNumCDTracks(wp);
    wp_debug("WAVEPLAY: play track %d of %s (msg %d)\n", track, music.disc, msg);
    if (track < 2 || track > tracks)
        return;
    EnterCriticalSection(&music.lock);
    if (music.track == track && !music.ended) {
        LeaveCriticalSection(&music.lock);
        return;
    }
    stream_close();
    music.ended = 0;
    if (stream_open(track))
        music.msg = msg;
    LeaveCriticalSection(&music.lock);
}

void __cdecl DSStopCDTrack(WavePlay *wp)
{
    (void)wp;
    if (!music.lock_ready)
        return;
    EnterCriticalSection(&music.lock);
    stream_close();
    music.ended = 0;
    music.msg = 0;
    LeaveCriticalSection(&music.lock);
}

void __cdecl DSPauseCDTrack(WavePlay *wp, int pause)
{
    (void)wp;
    if (!music.lock_ready)
        return;
    EnterCriticalSection(&music.lock);
    if (music.track && music.buf && music.paused != (pause != 0)) {
        music.paused = pause != 0;
        if (music.paused)
            IDirectSoundBuffer_Stop(music.buf);
        else
            IDirectSoundBuffer_Play(music.buf, 0, 0, DSBPLAY_LOOPING);
    }
    LeaveCriticalSection(&music.lock);
}

/* Music volume, 0..10000 (the original set the CD drive's analog output; at 10000
 * its byte value wrapped to 0 = silent). */
int __cdecl DSSetCDVolume(WavePlay *wp, uint32_t volume)
{
    (void)wp;
    if (volume > 0xffff || !music.lock_ready)
        return 0;
    EnterCriticalSection(&music.lock);
    music.volume = (int)volume;
    apply_volume();
    LeaveCriticalSection(&music.lock);
    return 1;
}

uint32_t __cdecl DSGetCDVolume(void)
{
    return music.lock_ready ? (uint32_t)music.volume : 9960;
}
