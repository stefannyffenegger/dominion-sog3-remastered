/* The engine's audio library, 0x10045480-0x10045f08: a wave sound's data as the mixer
 * takes it (refilled from the source, looped, unpacked from IMA ADPCM, converted to the
 * device format) and mixing one sound into a buffer, with its volume and fades.
 * Unused by Dominion. Rewritten from the original. */
#include "orig.h"
#include "utils.h"
#include "compress.h"
#include "audio.h"
#include "audio_int.h"

/* a wave sound's stream fields */
#define S_BUF 0x00       /* the data buffer */
#define S_AVAIL 0x04     /* bytes in it */
#define S_OFFSET 0x08    /* where they start */
#define S_SIZE 0x0c      /* its size */
#define S_PLAYED 0x18    /* bytes of this pass played */
#define S_READ 0x1c      /* bytes of this pass read */
#define S_CHUNK 0x20     /* the chunk taken: where, size */
#define S_CHUNK_LEN 0x24
#define S_END_READ 0x88  /* the source is at its end */
#define S_IMA 0x64       /* IMA ADPCM state (12 bytes) */
#define S_RESAMPLE 0x93  /* resampler state (9 bytes) */

typedef uint32_t(__stdcall *ReadFn)(void *arg, void *buf, uint32_t n);
typedef int(__stdcall *RewindFn)(void *arg);

/* 0x10045992 */
int __stdcall wave_source_rewind(uint8_t *s)
{
    return F(void *, s, 0x44) ? F(RewindFn, s, 0x44)(F(void *, s, 0x70)) : 0;
}

/* 0x100459c5: moves the unplayed bytes to the front and reads more */
int __stdcall wave_sound_refill(uint8_t *s)
{
    uint8_t *buf = F(uint8_t *, s, S_BUF);
    if (F(uint32_t, s, S_OFFSET)) {
        if (F(uint32_t, s, S_AVAIL))
            CopyHugeBytes(buf, buf + F(uint32_t, s, S_OFFSET), F(uint32_t, s, S_AVAIL));
        F(uint32_t, s, S_OFFSET) = 0;
    }
    uint32_t room = F(uint32_t, s, S_SIZE) - F(uint32_t, s, S_AVAIL);
    if (!room)
        return 0;
    if (F(uint32_t, s, W_LENGTH) - F(uint32_t, s, S_READ) <= room) {
        s[S_END_READ] = 1;
        room = F(uint32_t, s, W_LENGTH) - F(uint32_t, s, S_READ);
    }
    uint32_t got = F(ReadFn, s, 0x40)(F(void *, s, 0x70), buf + F(uint32_t, s, S_AVAIL), room);
    if (F(uint32_t, s, 0x4c))
        F(uint32_t, s, W_END_TIME) +=
            (got * 100 << (s[W_SHIFT] & 0x1f)) / (F(uint32_t, s, W_FORMAT + 8) / 10);
    if (!got) {
        if (!F(uint32_t, s, 0x4c))
            return 0;
        s[S_END_READ] = 1;
        F(uint32_t, s, W_LENGTH) = F(uint32_t, s, S_AVAIL) + F(uint32_t, s, S_PLAYED);
    }
    F(uint32_t, s, S_AVAIL) += got;
    F(uint32_t, s, S_READ) += got;
    if (!s[W_IN_MEMORY] && s[S_END_READ] && F(int16_t, s, W_LOOPS_LEFT)) {
        F(uint32_t, s, S_READ) = 0;
        s[S_END_READ] = 0;
        return wave_source_rewind(s);
    }
    return 1;
}

/* 0x100458d5: back to the start */
void __stdcall wave_sound_rewind(uint8_t *s)
{
    if (s[0x8d])
        memset(s + S_IMA, 0, 0xc);
    if (s[W_IN_MEMORY]) {
        F(uint32_t, s, S_AVAIL) = F(uint32_t, s, S_SIZE);
        F(uint32_t, s, S_PLAYED) = 0;
        F(uint32_t, s, S_OFFSET) = 0;
    } else if (F(uint32_t, s, S_PLAYED)) {
        F(uint32_t, s, S_READ) = 0;
        F(uint32_t, s, S_PLAYED) = 0;
        F(uint32_t, s, S_AVAIL) = 0;
        s[S_END_READ] = 0;
        wave_source_rewind(s);
        wave_sound_refill(s);
    }
}

/* 0x10045764: at the end: again (loops left; negative = forever) or stop */
int __stdcall wave_sound_loop(uint8_t *s)
{
    if (!F(int16_t, s, W_LOOPS_LEFT)) {
        wave_sound_ended(s);
        return 0;
    }
    if (F(int16_t, s, W_LOOPS_LEFT) > 0)
        F(int16_t, s, W_LOOPS_LEFT)--;
    if (s[0x8d])
        memset(s + S_IMA, 0, 0xc);
    if (s[W_IN_MEMORY]) {
        F(uint32_t, s, S_AVAIL) = F(uint32_t, s, S_SIZE);
        F(uint32_t, s, S_OFFSET) = 0;
    }
    return 1;
}

/* 0x100457ff: how many bytes of the sound make `want` bytes for the device */
uint32_t __stdcall wave_fetch_size(uint8_t *s, uint32_t want)
{
    if (s[0x8d])
        want >>= s[W_SHIFT] & 0x1f;
    if (s[W_RESAMPLED]) {
        const uint8_t *st = s + S_RESAMPLE; /* in16, out16, in stereo, out stereo, down, up */
        want = ((((uint32_t)st[4] * want) / st[5] << (st[0] & 0x1f)) >> (st[1] & 0x1f) << (st[2] & 0x1f)) >>
               (st[3] & 0x1f);
    }
    if (F(uint32_t, s, S_AVAIL) < want)
        want = F(uint32_t, s, S_AVAIL);
    uint32_t left = F(uint32_t, s, W_LENGTH) - F(uint32_t, s, S_PLAYED);
    return left < want ? left : want;
}

/* 0x10045669: room to unpack n bytes */
int __stdcall wave_unpack_room(uint8_t *s, uint32_t n)
{
    uint32_t size = n << (s[W_SHIFT] & 0x1f);
    if (F(uint32_t, g_audio, 0x36) >= size)
        return 1;
    void *p = winrealloc(F(void *, g_audio, 0x2e), size, 0);
    if (!p)
        return 0;
    F(void *, g_audio, 0x2e) = p;
    F(uint32_t, g_audio, 0x36) = size;
    return 1;
}

/* 0x100456ef: room to convert n bytes */
int __stdcall wave_convert_room(uint8_t *s, uint32_t n)
{
    (void)s;
    if (F(uint32_t, g_audio, 0x3a) >= n)
        return 1;
    void *p = winrealloc(F(void *, g_audio, 0x32), n, 0);
    if (!p)
        return 0;
    F(void *, g_audio, 0x32) = p;
    F(uint32_t, g_audio, 0x3a) = n;
    return 1;
}

/* 0x10045480: the next chunk of the sound for the device, at S_CHUNK (0 bytes at the
 * end or on failure) */
void __stdcall wave_sound_fetch(uint8_t *s, uint32_t want)
{
    F(uint32_t, s, S_CHUNK_LEN) = 0;
    if (F(uint32_t, s, W_LENGTH) <= F(uint32_t, s, S_PLAYED)) {
        if (!wave_sound_loop(s))
            return;
        F(uint32_t, s, S_PLAYED) = 0;
    }
    uint32_t n;
    if ((!F(uint32_t, s, S_AVAIL) && !wave_sound_refill(s)) || !(n = wave_fetch_size(s, want)) ||
        (s[0x8d] && !wave_unpack_room(s, n)) || (s[W_RESAMPLED] && !wave_convert_room(s, want)))
        return;
    F(uint8_t *, s, S_CHUNK) = F(uint8_t *, s, S_BUF) + F(uint32_t, s, S_OFFSET);
    F(uint32_t, s, S_CHUNK_LEN) = n;
    F(uint32_t, s, S_AVAIL) -= n;
    F(uint32_t, s, S_PLAYED) += n;
    F(uint32_t, s, S_OFFSET) += n;
    if (s[0x8d]) {
        ima2pcm((ImaState *)(s + S_IMA), F(int16_t *, g_audio, 0x2e), F(uint8_t *, s, S_CHUNK), n,
                F(int16_t, s, W_FORMAT + 2) == 2);
        F(uint32_t, s, S_CHUNK_LEN) = n << (s[W_SHIFT] & 0x1f);
        F(uint8_t *, s, S_CHUNK) = F(uint8_t *, g_audio, 0x2e);
    }
    if (s[W_RESAMPLED]) {
        n = resample(s + S_RESAMPLE, F(uint8_t *, g_audio, 0x32), F(uint32_t, g_audio, 0x3a),
                     F(uint8_t *, s, S_CHUNK), F(uint32_t, s, S_CHUNK_LEN), NULL);
        F(uint32_t, s, S_CHUNK_LEN) = n & 0xffff;
        F(uint8_t *, s, S_CHUNK) = F(uint8_t *, g_audio, 0x32);
    }
}

/* ---- volume ---- */

/* 0x10045bd1: v + d, kept within 0 and the limit (16-bit) */
uint32_t __stdcall volume_step16(uint32_t v, int16_t d, uint32_t limit)
{
    int32_t r = d + (int32_t)(v & 0xffff);
    if ((int32_t)(limit & 0xffff) < r)
        r = (int32_t)(limit & 0xffff);
    else if (r < 0)
        r = 0;
    return (uint32_t)r & 0xffff;
}

/* 0x10045b80: both channels of a device volume */
uint32_t __stdcall wave_volume_step(uint32_t v, int16_t dl, int16_t dr, uint32_t limit)
{
    uint32_t hi = volume_step16(v >> 16, dr, limit >> 16);
    uint32_t lo = volume_step16(v, dl, limit);
    return hi << 16 | (lo & 0xffff);
}

/* ---- mixing a sound into a buffer (index 1: the first, which copies) ---- */

/* one sample's fade step */
static void fade_step(uint8_t *s)
{
    if (s[0x8b]) { /* fading in */
        if (!--F(uint32_t, s, 0x2c)) {
            F(uint32_t, s, 0x2c) = F(uint32_t, s, 0x28);
            s[W_FADE_VOLUME]++;
            if (s[W_FADE_VOLUME] >= s[W_VOLUME])
                s[0x8b] = 0;
        }
    } else if (s[0x8c]) { /* fading out */
        if (!--F(uint32_t, s, 0x34)) {
            F(uint32_t, s, 0x34) = F(uint32_t, s, 0x30);
            if (!--s[W_FADE_VOLUME])
                s[0x8c] = 0;
        }
    }
}

static uint8_t clip8(int32_t v) { return v > 0x7f ? 0xff : v < -0x80 ? 0 : (uint8_t)(v + 0x80); }
static int16_t clip16(int32_t v) { return (int16_t)(v > 0x7fff ? 0x7fff : v < -0x7fff ? 0x8001 : v); }

/* 0x10045c30: returns the bytes written; the rest of a first sound's buffer is
 * silence. original: 8-bit samples are scaled by the volume with their 128 offset */
uint32_t __stdcall mix_sound8(uint8_t *s, uint8_t *out, uint32_t n, uint32_t index)
{
    uint32_t want = n;
    for (;;) {
        wave_sound_fetch(s, n);
        uint32_t k = F(uint32_t, s, S_CHUNK_LEN);
        if (!k)
            break;
        n -= k;
        const uint8_t *in = F(const uint8_t *, s, S_CHUNK);
        if (s[W_FADE_VOLUME] == 0x10 && !s[0x8b] && !s[0x8c]) {
            if (index == 1)
                for (; k; k--)
                    *out++ = *in++;
            else
                for (; k; k--, out++)
                    *out = clip8((int32_t)*in++ + *out - 0x100);
        } else {
            for (; k; k--) {
                int32_t v = *in++;
                if (s[W_FADE_VOLUME] != 0x10)
                    v = v * (int8_t)s[W_FADE_VOLUME] >> 4;
                if (index != 1)
                    v = clip8(v + *out - 0x100);
                *out++ = (uint8_t)v;
                fade_step(s);
            }
        }
        if (!n || !s[W_PLAYING])
            break;
    }
    if (index == 1)
        for (uint32_t k = n; k; k--)
            *out++ = 0x80;
    return want - n;
}

/* 0x10045d96 */
uint32_t __stdcall mix_sound16(uint8_t *s, int16_t *out, uint32_t n, uint32_t index)
{
    uint32_t want = n;
    for (;;) {
        wave_sound_fetch(s, n);
        uint32_t k = F(uint32_t, s, S_CHUNK_LEN);
        if (!k)
            break;
        n -= k;
        k >>= 1;
        const int16_t *in = F(const int16_t *, s, S_CHUNK);
        if (s[W_FADE_VOLUME] == 0x10 && !s[0x8b] && !s[0x8c]) {
            if (index == 1)
                for (; k; k--)
                    *out++ = *in++;
            else
                for (; k; k--, out++)
                    *out = clip16((int32_t)*in++ + *out);
        } else {
            for (; k; k--) {
                int32_t v = *in++;
                if (s[W_FADE_VOLUME] != 0x10)
                    v = v * (int8_t)s[W_FADE_VOLUME] >> 4;
                if (index != 1)
                    v = clip16(v + *out);
                *out++ = (int16_t)v;
                fade_step(s);
            }
        }
        if (!n || !s[W_PLAYING])
            break;
    }
    if (index == 1)
        for (uint32_t k = n >> 1; k; k--)
            *out++ = 0;
    return want - n;
}
