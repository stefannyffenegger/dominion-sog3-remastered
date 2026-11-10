/* The engine's compression exports, on top of codec.c. Rewritten from the original
 * (0x10046250-0x10046f8c). The packer/unpacker states are private to these functions,
 * so they use our own layouts; the sound states keep the original's, since the audio
 * code calls ima2pcm with its own. */
#include "orig.h"
#include "codec.h"
#include "compress.h"
#include "utils.h"

#define g_pack_method G(int32_t, 0x10079d40) /* 0: RLE, 1: LZW 11-bit, 2: best of all */

typedef int(__stdcall *PackOut)(void *data, uint32_t n, uint32_t user);

/* ---- bitmaps ---- */

typedef struct BitmapUnpacker {
    LzwDecoder lzw;
} BitmapUnpacker;

void *__cdecl DecompressBitmapInit(void)
{
    return winmalloc(sizeof(BitmapUnpacker), 0);
}

int __stdcall DecompressBitmapExit(void *u)
{
    winfree(u);
    return 0;
}

int __stdcall DecompressBitmapData(void *u, const uint8_t *src, uint8_t *dst, uint32_t size)
{
    return bitmap_unpack(&((BitmapUnpacker *)u)->lzw, src, (size_t)-1, dst, size);
}

typedef struct BitmapPacker {
    LzwEncoder lzw;
    uint8_t *buf[3]; /* one output buffer per method tried */
    uint8_t *mem;
} BitmapPacker;

#define PACK_BUF 0x2800

void *__cdecl CompressBitmapInit(void)
{
    BitmapPacker *p = winmalloc(sizeof *p, 1);
    if (!p)
        return NULL;
    p->mem = winmalloc(3 * PACK_BUF, 0);
    if (!p->mem) {
        winfree(p);
        return NULL;
    }
    for (int i = 0; i < 3; i++)
        p->buf[i] = p->mem + i * PACK_BUF;
    return p;
}

void __stdcall CompressBitmapExit(void *p)
{
    winfree(((BitmapPacker *)p)->mem);
    winfree(p);
}

int __stdcall CompressBitmapWriteEOD(void *packer, PackOut out, uint32_t user)
{
    BitmapPacker *p = packer;
    uint16_t end = PACK_END;
    memcpy(p->buf[0], &end, 2);
    return out(p->buf[0], 2, user);
}

static int put_block(PackOut out, uint32_t user, uint8_t *buf, uint16_t head, uint32_t n)
{
    memcpy(buf, &head, 2);
    return out(buf, n + 2, user);
}

/* A block that didn't get smaller: header, then the bytes as they are */
static int put_raw(BitmapPacker *p, PackOut out, uint32_t user, const uint8_t *src, uint32_t n)
{
    uint16_t head = (uint16_t)n;
    memcpy(p->buf[0], &head, 2);
    out(p->buf[0], 2, user);
    return out((void *)src, n, user);
}

/* Packs n bytes in blocks of up to 0x1fff and hands each block to out(data, size, user).
 * Method from the original's setting (1 = LZW with 11-bit codes, never changed). */
int __stdcall CompressBitmapData(void *packer, uint32_t n, const uint8_t *src, PackOut out,
                                 uint32_t user)
{
    BitmapPacker *p = packer;
    int ok = 1;
    while (ok && n) {
        uint32_t len = n < 0x2000 ? n : 0x1fff;
        n = n < 0x2000 ? 0 : n - 0x1fff;
        if (g_pack_method == 0) {
            uint32_t r = (uint32_t)rle_pack(src, (int)len, p->buf[0] + 2);
            ok = r < len ? put_block(out, user, p->buf[0], (uint16_t)(r | PACK_RLE), r)
                         : put_raw(p, out, user, src, len);
        } else if (g_pack_method == 1) {
            uint32_t l = (uint32_t)lzw_pack(&p->lzw, 11, src, len, p->buf[1] + 2);
            ok = l < len ? put_block(out, user, p->buf[1], (uint16_t)(l | PACK_LZW11), l)
                         : put_raw(p, out, user, src, len);
        } else {
            /* the smallest of RLE and LZW with 10, 11 and 12 bits; the order of the
             * tries and the ties are the original's */
            uint32_t r = (uint32_t)rle_pack(src, (int)len, p->buf[0] + 2);
            uint32_t best = (uint32_t)lzw_pack(&p->lzw, 11, src, len, p->buf[1] + 2), other;
            uint16_t method;
            if (len < 0x1001) {
                other = (uint32_t)lzw_pack(&p->lzw, 10, src, len, p->buf[2] + 2);
                if (other < best) {
                    method = PACK_LZW10;
                    best = other;
                } else {
                    other = (uint32_t)lzw_pack(&p->lzw, 12, src, len, p->buf[2] + 2);
                    if (best < other)
                        method = PACK_LZW11;
                    else {
                        method = PACK_LZW12;
                        best = other;
                    }
                }
            } else {
                other = (uint32_t)lzw_pack(&p->lzw, 12, src, len, p->buf[2] + 2);
                if (other < best) {
                    method = PACK_LZW12;
                    best = other;
                } else {
                    other = (uint32_t)lzw_pack(&p->lzw, 10, src, len, p->buf[2] + 2);
                    if (best < other)
                        method = PACK_LZW11;
                    else {
                        method = PACK_LZW10;
                        best = other;
                    }
                }
            }
            if (r < len || best < len) {
                if (r < 0x1fff && r < best)
                    ok = put_block(out, user, p->buf[0], (uint16_t)(r | PACK_RLE), r);
                else if (method == PACK_LZW11)
                    ok = put_block(out, user, p->buf[1], (uint16_t)(best | PACK_LZW11), best);
                else /* the last try's buffer holds the winner */
                    ok = put_block(out, user, p->buf[2], (uint16_t)(method | other), other);
            } else {
                ok = put_raw(p, out, user, src, len);
            }
        }
        if (n)
            src += 0x1fff;
    }
    return ok;
}

/* ---- sound ---- */

typedef int(__stdcall *WaveRead)(uint32_t user, void *buf, uint32_t n);
typedef int(__stdcall *WaveWrite)(uint32_t user, void *buf, uint32_t n);

#pragma pack(push, 1)
typedef struct WaveHeader { /* 10 bytes before the IMA data */
    uint32_t pcm_bytes;     /* multiple of 4 */
    uint32_t ima_bytes;
    uint16_t version;       /* 1 */
} WaveHeader;
#pragma pack(pop)

struct WaveUnpacker { /* original layout */
    ImaState s;
    uint32_t stereo;
    uint32_t user;
    WaveRead read;
    uint8_t *buf;
    uint32_t buf_size;
    uint32_t pos;
    uint32_t total;    /* IMA bytes in the stream */
    uint32_t consumed; /* IMA bytes read so far */
    uint32_t avail;    /* unused bytes in buf */
};
typedef struct WaveUnpacker WaveUnpacker;
_Static_assert(sizeof(WaveUnpacker) == 0x30, "WaveUnpacker");

struct WavePacker { /* original layout */
    ImaState s;
    uint32_t stereo;
    uint32_t user;
    uint8_t *buf;
    WaveWrite write;
};
typedef struct WavePacker WavePacker;
_Static_assert(sizeof(WavePacker) == 0x1c, "WavePacker");

uint32_t __stdcall ima2pcm(ImaState *s, int16_t *out, const uint8_t *in, uint32_t n, int stereo)
{
    ima_unpack(s, out, in, n, stereo);
    return n;
}

uint32_t __stdcall pcm2ima(ImaState *s, uint8_t *out, const int16_t *in, uint32_t n, int stereo)
{
    ima_pack(s, out, in, n, stereo);
    return n;
}

int __stdcall wave_unpacker_free(WaveUnpacker *u)
{
    if (u->buf)
        winfree(u->buf);
    LocalFree(u);
    return 0;
}

WaveUnpacker *__stdcall wave_unpacker_new(int16_t channels, uint32_t buf_size)
{
    WaveUnpacker *u = LocalAlloc(LPTR, sizeof *u);
    if (!u)
        return NULL;
    u->stereo = channels == 2;
    u->buf_size = buf_size & ~3u;
    u->buf = winmalloc(u->buf_size, 0);
    if (!u->buf)
        return (WaveUnpacker *)(uintptr_t)wave_unpacker_free(u);
    return u;
}

/* Reads the 10-byte header through read(user, buf, n); *info gets a copy. */
WaveUnpacker *__stdcall DecompressWaveInit(int16_t channels, uint32_t buf_size, WaveRead read,
                                           uint32_t user, void *out_info)
{
    WaveHeader *info = out_info;
    WaveUnpacker *u;
    WaveHeader h;
    if (!read || (u = wave_unpacker_new(channels, buf_size)) == NULL)
        return NULL;
    u->read = read;
    u->user = user;
    if (!read(user, &h, sizeof h) || h.version != 1)
        return (WaveUnpacker *)(uintptr_t)wave_unpacker_free(u);
    u->total = h.ima_bytes;
    if (info)
        *info = h;
    return u;
}

/* Fills out with up to n bytes of 16-bit samples; returns the bytes written. */
uint32_t __stdcall DecompressWaveData(WaveUnpacker *u, int16_t *out, uint32_t n)
{
    uint32_t done = 0;
    if (!u)
        return 0;
    for (n >>= 2; n;) {
        uint32_t k;
        if (!u->avail) {
            uint32_t left = u->total - u->consumed;
            u->avail = left < u->buf_size ? left : u->buf_size;
            if (!u->avail)
                break; /* original: loops forever when asked for more than there is */
            if (!u->read(u->user, u->buf, u->avail))
                return 0;
            u->consumed += u->avail;
            u->pos = 0;
        }
        k = n < u->avail ? n : u->avail;
        ima2pcm(&u->s, out, u->buf + u->pos, k, (int)u->stereo);
        u->pos += k;
        u->avail -= k;
        n -= k;
        out += k * 2;
        done += k * 4;
    }
    return done;
}

/* Back to the start of a stream whose reader has been rewound: skips the header again. */
int __stdcall DecompressWaveReset(WaveUnpacker *u)
{
    WaveHeader h;
    if (!u)
        return 0;
    u->s.index[1] = u->s.index[0] = 0;
    u->avail = u->consumed = 0;
    u->s.sample[1] = u->s.sample[0] = 0;
    return u->read(u->user, &h, sizeof h);
}

void __stdcall DecompressWaveExit(WaveUnpacker *u)
{
    if (u)
        wave_unpacker_free(u);
}

int __stdcall wave_packer_free(WavePacker *p)
{
    if (p->buf)
        winfree(p->buf);
    LocalFree(p);
    return 0;
}

WavePacker *__stdcall wave_packer_new(int16_t channels)
{
    WavePacker *p = LocalAlloc(LPTR, sizeof *p);
    if (!p)
        return NULL;
    p->stereo = channels == 2;
    p->buf = winmalloc(0x1000, 0);
    if (!p->buf)
        return (WavePacker *)(uintptr_t)wave_packer_free(p);
    return p;
}

/* If header is set, writes the 10-byte header for pcm_bytes of samples first. */
WavePacker *__stdcall CompressWaveInit(int16_t channels, uint32_t pcm_bytes, WaveWrite write,
                                       uint32_t user, int header)
{
    WavePacker *p;
    if (!write || (p = wave_packer_new(channels)) == NULL)
        return NULL;
    p->write = write;
    p->user = user;
    if (header) {
        WaveHeader h = {pcm_bytes & ~3u, pcm_bytes >> 2, 1};
        if (!write(user, &h, sizeof h))
            return (WavePacker *)(uintptr_t)wave_packer_free(p);
    }
    return p;
}

int __stdcall CompressWaveData(WavePacker *p, const int16_t *in, uint32_t n)
{
    if (!p)
        return 0;
    while (n) {
        uint32_t k = n < 0x4000 ? n : 0x4000;
        pcm2ima(&p->s, p->buf, in, k, (int)p->stereo);
        if (!p->write(p->user, p->buf, k >> 2))
            return 0;
        in = (const int16_t *)((const uint8_t *)in + k);
        n -= k;
    }
    return 1;
}

void __stdcall CompressWaveExit(WavePacker *p)
{
    if (p)
        wave_packer_free(p);
}
