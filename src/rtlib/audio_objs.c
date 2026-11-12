/* The engine's audio library, 0x10041514-0x1004226a: making and freeing MIDI sounds (up
 * to 20) and wave sounds (up to 100). A sound reads its data through a source: three
 * callbacks (read, rewind, close, all stdcall) and their argument. A wave longer than
 * the buffers streams; a shorter one is read whole. Unused by Dominion. Rewritten from
 * the original. */
#include "orig.h"
#include "utils.h"
#include "audio.h"
#include "audio_int.h"

#define MIDI_MAX 20
#define WAVE_MAX 100

typedef void(__stdcall *CloseFn)(void *);

/* a source: read(arg, buf, n), rewind(arg), close(arg), two values, arg */
typedef struct Source {
    uint32_t(__stdcall *read)(void *arg, void *buf, uint32_t n);
    int(__stdcall *rewind)(void *arg);
    void(__stdcall *close)(void *arg);
    uint32_t v3, v4;
    void *arg;
} Source;

/* ---- MIDI sounds (0x50 bytes) ---- */

/* 0x1004166c */
int __stdcall midi_source_close(uint8_t *m)
{
    if (F(void *, m, 0x18)) {
        F(CloseFn, m, 0x18)(F(void *, m, 0x1c));
        F(void *, m, 0x18) = NULL;
    }
    return 1;
}

/* 0x1004154c: takes the sound out of the list and frees it */
uint8_t *__stdcall midi_sound_remove(uint8_t *m)
{
    uint32_t n = F(uint16_t, g_audio, A_MIDI_COUNT), i = 0;
    uint8_t **list = (uint8_t **)(g_audio + A_MIDIS);
    while (i < n && list[i] != m)
        i++;
    if (i < n) {
        midi_source_close(m);
        if (F(void *, m, 0))
            winfree(F(void *, m, 0));
        LocalFree(m);
        memmove(list + i, list + i + 1, (n - i) * 4); /* original: one entry past the end too */
        F(uint16_t, g_audio, A_MIDI_COUNT)--;
        F(uint8_t **, g_audio, A_MIDI_END) = list + F(uint16_t, g_audio, A_MIDI_COUNT);
    }
    return NULL;
}

/* 0x10041514 */
void __stdcall midi_sound_free(uint8_t *m)
{
    if (F(uint8_t *, g_audio, A_MIDI_CURRENT) == m)
        midi_stop(m);
    midi_sound_remove(m);
}

/* 0x1004177e */
uint8_t *__cdecl midi_sound_alloc(void)
{
    if (F(uint16_t, g_audio, A_MIDI_COUNT) >= MIDI_MAX)
        return NULL;
    uint8_t *m = LocalAlloc(LMEM_ZEROINIT, 0x50);
    if (!m)
        return NULL;
    ((uint8_t **)(g_audio + A_MIDIS))[F(uint16_t, g_audio, A_MIDI_COUNT)] = m;
    F(uint16_t, g_audio, A_MIDI_COUNT)++;
    F(uint8_t **, g_audio, A_MIDI_END) = (uint8_t **)(g_audio + A_MIDIS) + F(uint16_t, g_audio, A_MIDI_COUNT);
    return m;
}

/* 0x10041827: reads the whole (prepared) MIDI data; info: size, a value */
int __stdcall midi_sound_load(uint8_t *m, const Source *src, const uint32_t *info)
{
    F(void *, m, 0x1c) = src->arg;
    F(void *, m, 0x10) = (void *)src->read;
    F(void *, m, 0x14) = (void *)src->rewind;
    F(void *, m, 0x18) = (void *)src->close;
    F(uint32_t, m, 0x20) = info[1];
    F(uint32_t, m, 0x34) = 1;
    uint32_t size = info[0];
    F(uint16_t, m, 6) = (uint16_t)(size >> 2);
    F(void *, m, 0) = winmalloc(size, 0);
    return F(void *, m, 0) && src->read(src->arg, F(void *, m, 0), size) == size && midi_source_close(m);
}

uint8_t *__stdcall AudioCreateMidi(uint8_t *a, const Source *src, const uint32_t *info)
{
    g_audio = a;
    if (!src || !src->read)
        return NULL;
    uint8_t *m = midi_sound_alloc();
    if (!m && src->close)
        src->close(src->arg);
    if (m && !midi_sound_load(m, src, info))
        m = midi_sound_remove(m);
    return m;
}

uint32_t __stdcall AudioDestroyMidi(uint8_t *a, uint8_t *m)
{
    g_audio = a;
    if (m)
        midi_sound_free(m);
    return 0;
}

/* ---- wave sounds (0x9c bytes) ---- */

/* 0x10041ae7 */
void __stdcall wave_source_close(uint8_t *s)
{
    if (F(void *, s, 0x48)) {
        F(CloseFn, s, 0x48)(F(void *, s, 0x70));
        F(void *, s, 0x48) = NULL;
    }
}

/* 0x100419ba */
uint8_t *__stdcall wave_sound_remove(uint8_t *s)
{
    uint32_t n = F(uint16_t, g_audio, A_WAVE_COUNT);
    uint16_t i = 0;
    uint8_t **list = (uint8_t **)(g_audio + A_WAVES);
    while (i < n && list[i] != s)
        i++;
    if (i < n) {
        wave_source_close(s);
        if (F(void *, s, 0))
            winfree(F(void *, s, 0));
        LocalFree(s);
        memmove(list + i, list + i + 1, (n - i) * 4); /* original: one entry past the end too */
        F(uint16_t, g_audio, A_WAVE_COUNT)--;
        F(uint8_t **, g_audio, A_WAVES_END) = list + F(uint16_t, g_audio, A_WAVE_COUNT);
    }
    return NULL;
}

/* 0x1004197e */
void __stdcall wave_sound_free(uint8_t *s)
{
    if (s[W_PLAYING]) {
        F(int16_t, g_audio, A_WAVES_ACTIVE)--;
        wave_reset();
    }
    wave_sound_remove(s);
}

/* 0x10041920 */
void __cdecl wave_sounds_free(void)
{
    for (uint8_t **p = (uint8_t **)(g_audio + A_WAVES); p < F(uint8_t **, g_audio, A_WAVES_END); p++)
        wave_sound_free(*p);
    if (F(HWAVEOUT, g_audio, A_WAVEOUT))
        wave_close();
}

/* 0x10041c0b */
uint8_t *__cdecl wave_sound_alloc(void)
{
    if (F(uint16_t, g_audio, A_WAVE_COUNT) >= WAVE_MAX)
        return NULL;
    uint8_t *s = LocalAlloc(LMEM_ZEROINIT, 0x9c);
    if (!s)
        return NULL;
    ((uint8_t **)(g_audio + A_WAVES))[F(uint16_t, g_audio, A_WAVE_COUNT)] = s;
    F(uint16_t, g_audio, A_WAVE_COUNT)++;
    F(uint8_t **, g_audio, A_WAVES_END) = (uint8_t **)(g_audio + A_WAVES) + F(uint16_t, g_audio, A_WAVE_COUNT);
    return s;
}

/* 0x10041ca8. info: length (+0), in memory (+4), no priority (+5), 8-bit data played as
 * 16 (+6), priority (+8), a value (+0xc), the format (+0x10, 16 bytes) */
int __stdcall wave_sound_load(uint8_t *s, const Source *src, const uint8_t *info)
{
    F(void *, s, 0x70) = src->arg;
    F(void *, s, 0x40) = (void *)src->read;
    F(void *, s, 0x44) = (void *)src->rewind;
    F(void *, s, 0x48) = (void *)src->close;
    F(uint32_t, s, 0x4c) = src->v3;
    if (F(uint32_t, s, 0x4c))
        F(uint32_t, s, W_END_TIME) = 0;
    F(uint32_t, s, 0x50) = src->v4;
    F(uint32_t, s, 0x74) = F(uint32_t, info, 0xc);
    F(uint32_t, s, W_LENGTH) = F(uint32_t, info, 0);
    memcpy(s + W_FORMAT, info + 0x10, 16);
    s[W_FADE_VOLUME] = 0x10;
    s[W_VOLUME] = 0x10;
    s[0x82] = 1;
    s[W_IN_MEMORY] = info[4] != 0;
    F(int16_t, s, W_PRIORITY) = (int16_t)F(uint32_t, info, 8);
    s[W_NO_PRIORITY] = info[5] != 0;
    s[0x8d] = info[6] != 0;
    if (s[0x8d])
        s[W_SHIFT] = F(int16_t, s, W_FORMAT + 0xe) == 16 ? 2 : 1;
    uint32_t room = (uint32_t)g_max_wave_buffers << 13;
    if (F(uint32_t, s, W_LENGTH) << (s[W_SHIFT] & 0x1f) <= room)
        s[W_IN_MEMORY] = 1;
    if (!s[W_IN_MEMORY]) {
        F(uint32_t, s, 0xc) = room >> (s[W_SHIFT] & 0x1f);
        F(uint32_t, s, 0x10) = F(uint32_t, s, 0xc) >> 1;
    } else {
        F(uint32_t, s, 0xc) = F(uint32_t, s, W_LENGTH);
    }
    F(void *, s, 0) = winmalloc(F(uint32_t, s, 0xc), 0);
    if (!F(void *, s, 0) || !wave_sound_refill(s))
        return 0;
    if (s[W_IN_MEMORY])
        wave_source_close(s);
    return 1;
}

uint8_t *__stdcall AudioCreateWave(uint8_t *a, const Source *src, const uint8_t *info)
{
    g_audio = a;
    if (!src || !src->read)
        return NULL;
    uint8_t *s = wave_sound_alloc();
    if (!s && src->close)
        src->close(src->arg);
    if (s && !wave_sound_load(s, src, info))
        s = wave_sound_remove(s);
    return s;
}

uint32_t __stdcall AudioDestroyWave(uint8_t *a, uint8_t *s)
{
    g_audio = a;
    if (s)
        wave_sound_free(s);
    return 0;
}

/* ---- a MIDI sound from a file ---- */

/* 0x100420e5 */
void *__stdcall file_source_close(FileSource *s)
{
    if (s) {
        if (s->data)
            winfree(s->data);
        if (s->f)
            mmioClose(s->f, 0);
        LocalFree(s);
    }
    return NULL;
}

/* 0x10042147 */
void __stdcall file_source_close_cb(FileSource *s) { file_source_close(s); }

/* 0x10042162 */
uint32_t __stdcall file_source_read(FileSource *s, void *buf, uint32_t n)
{
    if (!s->in_memory)
        return (uint32_t)mmioRead(s->f, buf, (LONG)n);
    CopyHugeBytes(buf, s->data + s->pos, n);
    s->pos += n;
    return n;
}

/* 0x100421c0 */
int __stdcall file_source_rewind(FileSource *s)
{
    if (!s->in_memory)
        return mmioSeek(s->f, (LONG)s->pos, SEEK_SET) == (LONG)s->pos;
    s->pos = 0;
    return 1;
}

/* 0x10042082 */
FileSource *__stdcall file_source_open(LPSTR path)
{
    FileSource *s = LocalAlloc(LMEM_ZEROINIT, sizeof *s);
    if (!s)
        return NULL;
    s->f = mmioOpenA(path, NULL, 0);
    if (!s->f)
        s = file_source_close(s);
    return s;
}

/* 0x10042220 */
LONG __stdcall mmio_size(HMMIO f)
{
    LONG at = mmioSeek(f, 0, SEEK_CUR), end = mmioSeek(f, 0, SEEK_END);
    mmioSeek(f, at, SEEK_SET);
    return end;
}

/* 0x10041f60: a MIDI sound from a .mid file (no caller) */
uint8_t *__stdcall midi_create_from_file(uint8_t *a, LPSTR path, uint32_t unused)
{
    (void)unused;
    FileSource *fs = file_source_open(path);
    if (!fs)
        return NULL;
    fs->in_memory = 1;
    uint32_t size = (uint32_t)mmio_size(fs->f);
    uint8_t *raw = winmalloc(size, 0);
    if (!raw)
        return file_source_close(fs);
    mmioRead(fs->f, (HPSTR)raw, (LONG)size);
    mmioClose(fs->f, 0);
    fs->f = NULL;
    uint32_t info[2] = {0, 0};
    fs->data = (uint8_t *)AudioPrepareMidiData(raw, size, info);
    winfree(raw);
    if (!fs->data)
        return file_source_close(fs);
    Source src = {0};
    src.read = (uint32_t(__stdcall *)(void *, void *, uint32_t))file_source_read;
    src.rewind = (int(__stdcall *)(void *))file_source_rewind;
    src.close = (void(__stdcall *)(void *))file_source_close_cb;
    src.arg = fs;
    return AudioCreateMidi(a, &src, info);
}
