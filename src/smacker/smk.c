/* Smacker decoder. Format notes (all integers little-endian):
 *
 *  0x00 "SMK2" / "SMK4"        0x14 flags (bit 0: extra "ring" frame for looping)
 *  0x04 width, 0x08 height     0x18 largest decoded audio chunk per track [7]
 *  0x0c frames                 0x34 size of the Huffman tree data
 *  0x10 frame rate             0x38 MMap, MClr, Full, Type tree sizes [4]
 *       (>0 ms, <0 10us units,  0x48 audio format per track [7]: bits 0-23 rate,
 *        0 = 10 fps)                 31 compressed, 30 present, 29 16-bit, 28 stereo,
 *                                    26-27 codec (0 = Huffman DPCM)
 *  0x68 frame sizes [frames] (low 2 bits are flags), frame types [frames] (bit 0:
 *       palette chunk, bit 1+t: audio track t), tree data, then the frames.
 *
 * Bitstreams are read least significant bit first. Video is coded in 4x4 blocks with
 * four "big" Huffman trees of 16-bit values; each big tree remembers its last three
 * values ("cache"), reset at every frame. */
#include "smk.h"

#include <string.h>

#define NODE 0x80000000u
#define MAX_SMALL 512

typedef struct Bits {
    const uint8_t *p;
    uint32_t bits; /* total */
    uint32_t pos;
    int overrun;
} Bits;

typedef struct SmallTree {
    uint32_t v[MAX_SMALL];
    uint32_t n;
} SmallTree;

typedef struct BigTree {
    uint32_t *v;
    uint32_t n, cap;
    uint32_t last[3]; /* cache slots */
} BigTree;

enum { T_MMAP, T_MCLR, T_FULL, T_TYPE };

typedef struct SmkPriv {
    SmkAlloc a;
    const uint8_t *data;
    size_t size;
    int v4;
    uint32_t total_frames;
    const uint8_t *frame_type;
    uint32_t *frame_off;
    uint32_t audio_fmt[SMK_TRACKS];
    uint32_t audio_cap[SMK_TRACKS];
    BigTree tree[4];
    SmallTree scratch[4]; /* audio trees */
} SmkPriv;

static const uint16_t block_runs[64] = {
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,  12,  13,  14,   15,   16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,  28,  29,  30,   31,   32,
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,  44,  45,  46,   47,   48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 128, 256, 512, 1024, 2048};

static uint32_t rd32(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void bits_init(Bits *b, const uint8_t *p, size_t bytes)
{
    b->p = p;
    b->bits = bytes > 0x1fffffff ? 0xfffffff8u : (uint32_t)bytes * 8;
    b->pos = 0;
    b->overrun = 0;
}

static unsigned get_bit(Bits *b)
{
    if (b->pos >= b->bits) {
        b->overrun = 1;
        return 0;
    }
    unsigned v = (b->p[b->pos >> 3] >> (b->pos & 7)) & 1;
    b->pos++;
    return v;
}

static unsigned get_bits(Bits *b, int n)
{
    unsigned v = 0;
    for (int i = 0; i < n; i++)
        v |= get_bit(b) << i;
    return v;
}

/* ---- Huffman trees ----
 * Both kinds are stored as arrays in prefix order: a node holds NODE | size of its
 * left subtree, followed by the left subtree, then the right one. Bit 0 = left. */

static int small_decode(Bits *b, SmallTree *t, int depth)
{
    if (depth > 32 || t->n >= MAX_SMALL || b->overrun)
        return 0;
    if (!get_bit(b)) {
        t->v[t->n++] = get_bits(b, 8);
        return 1;
    }
    uint32_t node = t->n++;
    if (!small_decode(b, t, depth + 1))
        return 0;
    t->v[node] = NODE | (t->n - node - 1);
    return small_decode(b, t, depth + 1);
}

/* Read an optional small tree (presence bit, tree, end bit). Absent = always 0. */
static int small_read(Bits *b, SmallTree *t)
{
    t->n = 0;
    if (!get_bit(b)) {
        t->v[0] = 0;
        t->n = 1;
        return 1;
    }
    if (!small_decode(b, t, 0))
        return 0;
    get_bit(b);
    return !b->overrun;
}

static uint32_t small_get(Bits *b, const SmallTree *t)
{
    uint32_t i = 0;
    while (t->v[i] & NODE) {
        if (get_bit(b))
            i += t->v[i] & ~NODE;
        i++;
    }
    return t->v[i];
}

typedef struct BigCtx {
    SmallTree lo, hi;
    uint32_t esc[3];
    int have_last[3];
} BigCtx;

static int big_decode(Bits *b, BigTree *t, BigCtx *c, int depth)
{
    if (depth > 64 || t->n + 3 >= t->cap || b->overrun)
        return 0;
    if (!get_bit(b)) {
        uint32_t lo = small_get(b, &c->lo);
        uint32_t hi = small_get(b, &c->hi);
        uint32_t val = lo | hi << 8;
        for (int i = 0; i < 3; i++) {
            if (val == c->esc[i]) {
                t->last[i] = t->n;
                c->have_last[i] = 1;
                val = 0;
                break;
            }
        }
        t->v[t->n++] = val;
        return 1;
    }
    uint32_t node = t->n++;
    if (!big_decode(b, t, c, depth + 1))
        return 0;
    t->v[node] = NODE | (t->n - node - 1);
    return big_decode(b, t, c, depth + 1);
}

static int big_read(Bits *b, BigTree *t, uint32_t size_bytes, const SmkAlloc *a)
{
    t->cap = (size_bytes + 3) / 4 + 4;
    if (t->cap < 8)
        t->cap = 8;
    t->v = a->alloc(t->cap * sizeof *t->v);
    if (!t->v)
        return 0;
    t->n = 0;
    if (!get_bit(b)) { /* no tree: every lookup returns 0 */
        t->v[0] = 0;
        t->v[1] = 0;
        t->n = 2;
        t->last[0] = t->last[1] = t->last[2] = 1;
        return 1;
    }
    static BigCtx c; /* large; decoding is single-threaded */
    memset(&c, 0, sizeof c);
    if (!small_read(b, &c.lo) || !small_read(b, &c.hi))
        return 0;
    for (int i = 0; i < 3; i++)
        c.esc[i] = get_bits(b, 16);
    if (!big_decode(b, t, &c, 0))
        return 0;
    get_bit(b);
    for (int i = 0; i < 3; i++) /* escapes that never occur still need a cache slot */
        if (!c.have_last[i])
            t->last[i] = t->n++;
    return !b->overrun;
}

static uint32_t big_get(Bits *b, BigTree *t)
{
    uint32_t i = 0;
    while (t->v[i] & NODE) {
        if (get_bit(b))
            i += t->v[i] & ~NODE;
        i++;
    }
    uint32_t val = t->v[i];
    if (val != t->v[t->last[0]]) {
        t->v[t->last[2]] = t->v[t->last[1]];
        t->v[t->last[1]] = t->v[t->last[0]];
        t->v[t->last[0]] = val;
    }
    return val;
}

static void big_reset(BigTree *t)
{
    for (int i = 0; i < 3; i++)
        t->v[t->last[i]] = 0;
}

/* ---- open / close ---- */

void smk_close(Smk *s)
{
    if (!s)
        return;
    SmkPriv *p = s->priv;
    void (*fr)(void *) = p->a.free;
    for (int i = 0; i < 4; i++)
        if (p->tree[i].v)
            fr(p->tree[i].v);
    for (int t = 0; t < SMK_TRACKS; t++)
        if (s->track[t].pcm)
            fr(s->track[t].pcm);
    if (p->frame_off)
        fr(p->frame_off);
    if (s->image)
        fr(s->image);
    fr(p);
    fr(s);
}

Smk *smk_open(const uint8_t *data, size_t size, const SmkAlloc *alloc)
{
    if (size < 0x68 || memcmp(data, "SMK", 3) != 0 || (data[3] != '2' && data[3] != '4'))
        return NULL;
    uint32_t w = rd32(data + 4), h = rd32(data + 8), frames = rd32(data + 0xc);
    int32_t rate = (int32_t)rd32(data + 0x10);
    uint32_t flags = rd32(data + 0x14), trees_size = rd32(data + 0x34);
    if (w == 0 || h == 0 || w > 4096 || h > 4096 || frames == 0 || frames > 1000000)
        return NULL;

    Smk *s = alloc->alloc(sizeof *s);
    SmkPriv *p = alloc->alloc(sizeof *p);
    if (!s || !p) {
        if (s)
            alloc->free(s);
        if (p)
            alloc->free(p);
        return NULL;
    }
    s->priv = p;
    p->a = *alloc;
    p->data = data;
    p->size = size;
    p->v4 = data[3] == '4';
    p->total_frames = frames + (flags & 1);
    s->width = w;
    s->height = h;
    s->frames = frames;
    s->frame_us = rate > 0 ? (uint32_t)rate * 1000 : rate < 0 ? (uint32_t)-rate * 10 : 100000;
    s->cur_frame = UINT32_MAX;

    size_t tables = 0x68 + (size_t)p->total_frames * 5;
    if (tables + trees_size > size)
        goto fail;
    p->frame_type = data + 0x68 + (size_t)p->total_frames * 4;

    p->frame_off = alloc->alloc((p->total_frames + 1) * sizeof(uint32_t));
    s->image = alloc->alloc((size_t)w * h);
    if (!p->frame_off || !s->image)
        goto fail;
    size_t off = tables + trees_size;
    for (uint32_t i = 0; i < p->total_frames; i++) {
        p->frame_off[i] = (uint32_t)off;
        off += rd32(data + 0x68 + i * 4) & ~3u;
        if (off > size)
            goto fail;
    }
    p->frame_off[p->total_frames] = (uint32_t)off;

    for (int t = 0; t < SMK_TRACKS; t++) {
        uint32_t fmt = rd32(data + 0x48 + t * 4);
        uint32_t cap = rd32(data + 0x18 + t * 4);
        p->audio_fmt[t] = fmt;
        if (!(fmt & 0x40000000) || cap == 0 || cap > (1u << 24))
            continue;
        SmkTrack *tr = &s->track[t];
        tr->rate = fmt & 0xffffff;
        tr->channels = (fmt & 0x10000000) ? 2 : 1;
        tr->bits = (fmt & 0x20000000) ? 16 : 8;
        tr->present = ((fmt >> 26) & 3) == 0; /* Bink audio codecs are not supported */
        p->audio_cap[t] = cap;
        tr->pcm = alloc->alloc(cap);
        if (!tr->pcm)
            goto fail;
    }

    Bits b;
    bits_init(&b, data + tables, trees_size);
    for (int i = 0; i < 4; i++)
        if (!big_read(&b, &p->tree[i], rd32(data + 0x38 + i * 4), alloc))
            goto fail;
    return s;
fail:
    smk_close(s);
    return NULL;
}

void smk_rewind(Smk *s)
{
    s->cur_frame = UINT32_MAX;
    memset(s->image, 0, (size_t)s->width * s->height);
    memset(s->palette, 0, sizeof s->palette);
}

/* ---- per-frame decoding ---- */

static uint8_t pal6(unsigned c)
{
    c &= 0x3f;
    return (uint8_t)(c << 2 | c >> 4);
}

static void decode_palette(Smk *s, const uint8_t *q, const uint8_t *end)
{
    uint8_t old[768];
    memcpy(old, s->palette, sizeof old);
    uint8_t *pal = s->palette;
    unsigned n = 0;
    while (n < 256 && q < end) {
        unsigned t = *q++;
        if (t & 0x80) { /* keep the next (t & 0x7f) + 1 entries */
            n += (t & 0x7f) + 1;
        } else if (t & 0x40) { /* copy entries from the previous palette */
            if (q >= end)
                break;
            unsigned src = *q++, count = (t & 0x3f) + 1;
            if (src + count > 256)
                break;
            while (count-- && n < 256) {
                memcpy(pal + n * 3, old + src * 3, 3);
                n++;
                src++;
            }
        } else { /* a new color, 6 bits per component */
            if (end - q < 2)
                break;
            pal[n * 3] = pal6(t);
            pal[n * 3 + 1] = pal6(q[0]);
            pal[n * 3 + 2] = pal6(q[1]);
            q += 2;
            n++;
        }
    }
    s->palette_changed = 1;
}

static void decode_audio(Smk *s, int t, const uint8_t *q, uint32_t len)
{
    SmkPriv *p = s->priv;
    SmkTrack *tr = &s->track[t];
    tr->pcm_size = 0;
    if (!tr->present || len < 4)
        return;
    uint32_t cap = p->audio_cap[t];
    if (!(p->audio_fmt[t] & 0x80000000)) { /* uncompressed PCM */
        uint32_t n = len < cap ? len : cap;
        memcpy(tr->pcm, q, n);
        tr->pcm_size = n;
        return;
    }
    uint32_t unpacked = rd32(q);
    Bits b;
    bits_init(&b, q + 4, len - 4);
    if (!get_bit(&b)) /* no sound in this frame */
        return;
    int stereo = get_bit(&b);
    int is16 = get_bit(&b);
    if (stereo != (tr->channels == 2) || is16 != (tr->bits == 16))
        return;
    if (unpacked > cap)
        unpacked = cap;
    SmallTree *tree = p->scratch;
    for (int i = 0; i < (1 << (is16 + stereo)); i++)
        if (!small_read(&b, &tree[i]))
            return;

    if (is16) {
        uint16_t pred[2];
        int16_t *out = (int16_t *)tr->pcm;
        uint32_t samples = unpacked / 2;
        for (int i = stereo; i >= 0; i--) {
            unsigned v = get_bits(&b, 16);
            pred[i] = (uint16_t)((v & 0xff) << 8 | v >> 8);
        }
        uint32_t i = 0;
        for (; i <= (uint32_t)stereo && i < samples; i++)
            out[i] = (int16_t)pred[i];
        for (; i < samples && !b.overrun; i++) {
            unsigned ch = i & stereo;
            uint32_t lo = small_get(&b, &tree[2 * ch]);
            uint32_t hi = small_get(&b, &tree[2 * ch + 1]);
            pred[ch] = (uint16_t)(pred[ch] + (lo | hi << 8));
            out[i] = (int16_t)pred[ch];
        }
        tr->pcm_size = i * 2;
    } else {
        uint8_t pred[2];
        uint8_t *out = tr->pcm;
        for (int i = stereo; i >= 0; i--)
            pred[i] = (uint8_t)get_bits(&b, 8);
        uint32_t i = 0;
        for (; i <= (uint32_t)stereo && i < unpacked; i++)
            out[i] = pred[i];
        for (; i < unpacked && !b.overrun; i++) {
            unsigned ch = i & stereo;
            pred[ch] = (uint8_t)(pred[ch] + small_get(&b, &tree[ch]));
            out[i] = pred[ch];
        }
        tr->pcm_size = i;
    }
}

static void decode_video(Smk *s, const uint8_t *q, size_t len)
{
    SmkPriv *p = s->priv;
    for (int i = 0; i < 4; i++)
        big_reset(&p->tree[i]);
    Bits b;
    bits_init(&b, q, len);

    uint32_t bw = s->width / 4, bh = s->height / 4, blocks = bw * bh, blk = 0;
    size_t stride = s->width;
    while (blk < blocks && !b.overrun) {
        uint32_t type = big_get(&b, &p->tree[T_TYPE]);
        uint32_t run = block_runs[(type >> 2) & 0x3f];
        int mode = 0;
        if ((type & 3) == 1 && p->v4) {
            if (get_bit(&b))
                mode = 1;
            else if (get_bit(&b))
                mode = 2;
        }
        for (; run && blk < blocks; run--, blk++) {
            uint8_t *out = s->image + (blk / bw) * stride * 4 + (blk % bw) * 4;
            switch (type & 3) {
            case 0: { /* two colors and a 16-bit mask */
                uint32_t clr = big_get(&b, &p->tree[T_MCLR]);
                uint32_t map = big_get(&b, &p->tree[T_MMAP]);
                uint8_t hi = (uint8_t)(clr >> 8), lo = (uint8_t)clr;
                for (int y = 0; y < 4; y++, out += stride, map >>= 4)
                    for (int x = 0; x < 4; x++)
                        out[x] = (map >> x) & 1 ? hi : lo;
                break;
            }
            case 1: /* full block */
                if (mode == 0) {
                    for (int y = 0; y < 4; y++, out += stride) {
                        uint32_t pix = big_get(&b, &p->tree[T_FULL]);
                        out[2] = (uint8_t)pix;
                        out[3] = (uint8_t)(pix >> 8);
                        pix = big_get(&b, &p->tree[T_FULL]);
                        out[0] = (uint8_t)pix;
                        out[1] = (uint8_t)(pix >> 8);
                    }
                } else if (mode == 1) { /* SMK4: 2x2 pixels */
                    for (int half = 0; half < 2; half++) {
                        uint32_t pix = big_get(&b, &p->tree[T_FULL]);
                        for (int y = 0; y < 2; y++, out += stride) {
                            out[0] = out[1] = (uint8_t)pix;
                            out[2] = out[3] = (uint8_t)(pix >> 8);
                        }
                    }
                } else { /* SMK4: lines doubled */
                    for (int half = 0; half < 2; half++) {
                        uint32_t pix2 = big_get(&b, &p->tree[T_FULL]);
                        uint32_t pix1 = big_get(&b, &p->tree[T_FULL]);
                        for (int y = 0; y < 2; y++, out += stride) {
                            out[0] = (uint8_t)pix1;
                            out[1] = (uint8_t)(pix1 >> 8);
                            out[2] = (uint8_t)pix2;
                            out[3] = (uint8_t)(pix2 >> 8);
                        }
                    }
                }
                break;
            case 2: /* unchanged */
                break;
            case 3: /* one color */
                for (int y = 0; y < 4; y++, out += stride)
                    memset(out, (int)(type >> 8), 4);
                break;
            }
        }
    }
}

int smk_next_frame(Smk *s)
{
    SmkPriv *p = s->priv;
    uint32_t n = s->cur_frame + 1; /* wraps to 0 from UINT32_MAX */
    if (n >= p->total_frames)
        return 0;
    if (n == 0) {
        memset(s->image, 0, (size_t)s->width * s->height);
        memset(s->palette, 0, sizeof s->palette);
    }
    s->cur_frame = n;
    s->palette_changed = 0;
    for (int t = 0; t < SMK_TRACKS; t++)
        s->track[t].pcm_size = 0;

    const uint8_t *q = p->data + p->frame_off[n];
    const uint8_t *end = p->data + p->frame_off[n + 1];
    uint8_t type = p->frame_type[n];

    if (type & 1) {
        if (q >= end)
            return 0;
        size_t len = (size_t)q[0] * 4;
        if (len == 0 || len > (size_t)(end - q))
            return 0;
        decode_palette(s, q + 1, q + len);
        q += len;
    }
    for (int t = 0; t < SMK_TRACKS; t++) {
        if (!(type & (2 << t)))
            continue;
        if (end - q < 4)
            return 0;
        uint32_t len = rd32(q);
        if (len < 4 || len > (size_t)(end - q))
            return 0;
        decode_audio(s, t, q + 4, len - 4);
        q += len;
    }
    decode_video(s, q, (size_t)(end - q));
    return 1;
}
