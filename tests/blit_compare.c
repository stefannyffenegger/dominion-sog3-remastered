/* The sprite engine's assembly routines: our C (src/rtlib/blit.c) against the original,
 * side by side in one process on pseudo-random input. Windows program (runs under Wine):
 *   wine blit_compare.exe <unmodified Rtlib32.dll> */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../src/rtlib/blit.h"

static uint8_t *base;
static uint32_t seed = 12345;
static uint32_t rnd(uint32_t n) { seed = seed * 1103515245 + 12345; return (seed >> 8) % n; }
static int failures;

/* calls an original stdcall routine, keeping the registers it doesn't keep */
__attribute__((naked)) static uint32_t __cdecl call_orig(void *fn, uint32_t n, const uint32_t *args)
{
    (void)fn, (void)n, (void)args;
    __asm__ volatile("pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tpushl %ebp\n\t"
                     "movl 20(%esp), %eax\n\tmovl 24(%esp), %ecx\n\tmovl 28(%esp), %edx\n\t"
                     "1: testl %ecx, %ecx\n\tjz 2f\n\tdecl %ecx\n\tpushl (%edx,%ecx,4)\n\tjmp 1b\n\t"
                     "2: call *%eax\n\tpopl %ebp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret");
}
/* the same for cdecl routines: the caller removes the arguments */
__attribute__((naked)) static uint32_t __cdecl call_orig_cdecl(void *fn, uint32_t n, const uint32_t *args)
{
    (void)fn, (void)n, (void)args;
    __asm__ volatile("pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tpushl %ebp\n\tmovl %esp, %ebp\n\t"
                     "movl 20(%ebp), %eax\n\tmovl 24(%ebp), %ecx\n\tmovl 28(%ebp), %edx\n\t"
                     "1: testl %ecx, %ecx\n\tjz 2f\n\tdecl %ecx\n\tpushl (%edx,%ecx,4)\n\tjmp 1b\n\t"
                     "2: call *%eax\n\tmovl %ebp, %esp\n\tpopl %ebp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret");
}
#define ORIG_CDECL(addr, ...)                                                                      \
    ({                                                                                             \
        uint32_t a_[] = {__VA_ARGS__};                                                             \
        call_orig_cdecl(base + ((addr) - 0x10000000), sizeof a_ / 4, a_);                          \
    })
#define ORIG(addr, ...)                                                                            \
    ({                                                                                             \
        uint32_t a_[] = {__VA_ARGS__};                                                             \
        call_orig(base + ((addr) - 0x10000000), sizeof a_ / 4, a_);                                \
    })
#define U(p) ((uint32_t)(uintptr_t)(p))

static void check(const char *name, int ok, int i)
{
    if (!ok && failures++ < 20)
        printf("%s: case %d differs\n", name, i);
}

static void test_rects(void)
{
    for (int i = 0; i < 100000; i++) {
        int32_t a[4], b[4], o1[4], o2[4];
        for (int k = 0; k < 4; k++)
            a[k] = (int32_t)rnd(200) - 100, b[k] = (int32_t)rnd(200) - 100, o1[k] = o2[k] = (int32_t)rnd(1000);
        uint32_t r1 = ORIG(0x1005f6ac, U(o1), U(a), U(b));
        uint32_t r2 = (uint32_t)rect_intersect(o2, a, b);
        check("intersect", r1 == r2 && !memcmp(o1, o2, 16), i);
        r1 = ORIG(0x1005f70a, U(o1), U(a), U(b));
        r2 = (uint32_t)rect_union(o2, a, b);
        check("union", r1 == r2 && !memcmp(o1, o2, 16), i);
    }
    printf("rects: done\n");
}

static void test_fade(void)
{
    static uint8_t src[1024], d1[1024], d2[1024];
    for (int i = 0; i < 20000; i++) {
        for (int k = 0; k < 1024; k++)
            src[k] = (uint8_t)rnd(256);
        uint32_t steps = 1 + rnd(255), step = rnd(steps + 1), n = 1 + rnd(256);
        memset(d1, 0x55, sizeof d1), memset(d2, 0x55, sizeof d2);
        ORIG(0x10062ed8, U(d1), U(src), step, steps, n);
        palette_fade(d2, src, step, steps, n);
        check("fade", !memcmp(d1, d2, sizeof d1), i);
    }
    printf("fade: done\n");
}

/* a bitmap header plus pixels */
static uint8_t *dib(int w, int h, uint32_t flags)
{
    uint32_t pitch = (w + 3) & ~3u;
    uint8_t *p = calloc(1, 0x24 + pitch * h + 64);
    *(uint32_t *)p = 0x24, *(int32_t *)(p + 4) = w, *(uint32_t *)(p + 8) = pitch, *(int32_t *)(p + 0xc) = h;
    *(uint32_t *)(p + 0x10) = flags, *(uint8_t **)(p + 0x20) = p + 0x24;
    for (uint32_t k = 0; k < pitch * h; k++)
        p[0x24 + k] = (uint8_t)rnd(256);
    return p;
}

static void test_fill(void)
{
    for (int i = 0; i < 5000; i++) {
        int w = 1 + rnd(80), h = 1 + rnd(60);
        uint8_t *a = dib(w, h, rnd(2) ? 0x40 : 0);
        uint32_t size = 0x24 + *(uint32_t *)(a + 8) * h;
        uint8_t *b = malloc(size + 64);
        memcpy(b, a, size), *(uint8_t **)(b + 0x20) = b + 0x24;
        int x = rnd(w), y = rnd(h), fw = 1 + rnd(w - x), fh = 1 + rnd(h - y);
        uint32_t color = rnd(0x10000);
        ORIG(0x100630cc, U(a), x, y, fw, fh, color);
        dib_fill8(b, x, y, fw, fh, color);
        check("fill8", !memcmp(a + 0x24, b + 0x24, size - 0x24), i);
        free(a), free(b);
    }
    printf("fill8: done\n");
}

/* a run-length coded bitmap: row table of 16-bit offsets, then per row runs
 * (code n <= 0x7f, value) and literals (code 0xff - n, or 0xff, n, then n bytes) */
static uint8_t *rle_dib(int w, int h)
{
    uint8_t *p = calloc(1, 0x24 + 2 * h + h * (w * 3 + 8) + 64);
    *(int32_t *)(p + 4) = w, *(uint32_t *)(p + 8) = w, *(int32_t *)(p + 0xc) = h;
    *(uint32_t *)(p + 0x10) = 0x80, *(uint8_t **)(p + 0x20) = p + 0x24;
    uint8_t *bits = p + 0x24, *q = bits + 2 * h;
    for (int y = 0; y < h; y++) {
        ((uint16_t *)bits)[y] = (uint16_t)(q - bits);
        for (int left = w; left > 0;) {
            int n = 1 + rnd(left < 100 ? left : 100);
            if (rnd(2)) {
                *q++ = (uint8_t)n, *q++ = (uint8_t)rnd(256);
            } else {
                if (n <= 0x7f && rnd(2))
                    *q++ = (uint8_t)(0xff - n);
                else
                    *q++ = 0xff, *q++ = (uint8_t)n;
                for (int k = 0; k < n; k++)
                    *q++ = (uint8_t)rnd(256);
            }
            left -= n;
        }
        *q++ = 0, *q++ = 0;
    }
    return p;
}

static uint8_t *clone(const uint8_t *a)
{
    uint32_t size = 0x24 + *(uint32_t *)(a + 8) * *(uint32_t *)(a + 0xc);
    uint8_t *b = malloc(size + 64);
    memcpy(b, a, size), *(uint8_t **)(b + 0x20) = b + 0x24;
    return b;
}

static void test_copy(void)
{
    for (int i = 0; i < 20000; i++) {
        int rle = rnd(2);
        int sw = 1 + rnd(120), sh = rle ? 1 : 1 + rnd(50);
        uint8_t *src = rle ? rle_dib(sw, sh) : dib(sw, sh, 0);
        int dw = sw + rnd(40), dh = sh + rnd(40);
        uint8_t *a = dib(dw, dh, rnd(2) ? 0x40 : 0), *b = clone(a);
        int w = 1 + rnd(sw), h = 1 + rnd(sh), sx = rnd(sw - w + 1), sy = rnd(sh - h + 1);
        int dx = rnd(dw - w + 1), dy = rnd(dh - h + 1);
        uint32_t flip = (rle && rnd(2) ? 1 : 0) | (rnd(2) ? 0x100 : 0);
        ORIG(0x100629f4, U(a), U(src), w, h, dx, dy, sx, sy, flip);
        dib_copy(b, src, w, h, dx, dy, sx, sy, flip);
        check(rle ? "copy rle" : "copy", !memcmp(a + 0x24, b + 0x24, *(uint32_t *)(a + 8) * dh), i);
        free(a), free(b), free(src);
    }
    /* raw source with a horizontal flip: one row, read from src.w - sx onwards */
    for (int i = 0; i < 5000; i++) {
        int sw = 2 + rnd(120), w = 1 + rnd(sw / 2), sx = w + rnd(sw - 2 * w + 1);
        uint8_t *src = dib(sw, 1 + rnd(4), 0);
        int sh = *(int32_t *)(src + 0xc), dw = sw + rnd(40), dh = 1 + rnd(40);
        uint8_t *a = dib(dw, dh, rnd(2) ? 0x40 : 0), *b = clone(a);
        int dx = rnd(dw - w + 1), dy = rnd(dh), sy = rnd(sh);
        uint32_t flip = 1 | (rnd(2) ? 0x100 : 0);
        ORIG(0x100629f4, U(a), U(src), w, 1, dx, dy, sx, sy, flip);
        dib_copy(b, src, w, 1, dx, dy, sx, sy, flip);
        check("copy flip", !memcmp(a + 0x24, b + 0x24, *(uint32_t *)(a + 8) * dh), i);
        free(a), free(b), free(src);
    }
    printf("copy: done\n");
}

/* a run-length coded sprite in the engine's segment format (rows bottom first) */
static uint8_t *seg_dib(int w, int h)
{
    uint8_t *p = calloc(1, 0x24 + h * (4 + w * 6) + 64);
    *(int32_t *)(p + 4) = w, *(uint32_t *)(p + 8) = w, *(int32_t *)(p + 0xc) = h;
    *(uint32_t *)(p + 0x10) = 0x80, *(uint8_t **)(p + 0x20) = p + 0x24;
    uint8_t *q = p + 0x24;
    for (int y = 0; y < h; y++) {
        uint8_t *count = q;
        q += 2;
        int nseg = 0;
        for (int left = w; left > 0; nseg++) {
            int n = 1 + rnd(left < 40 ? left : 40), type = rnd(4);
            *(uint16_t *)q = (uint16_t)(type << 11 | n);
            q += 2;
            if (type == 3) {
                for (int k = 0; k < n; k++)
                    *q++ = (uint8_t)rnd(256);
                if ((uintptr_t)q & 1)
                    *q++ = (uint8_t)rnd(256);
            }
            left -= n;
        }
        count[0] = (uint8_t)nseg, count[1] = (uint8_t)rnd(256);
    }
    return p;
}

/* destination bitmaps with a guard area in front, compared as a whole */
#define GUARD 8192
static uint8_t *gdib(int w, int h, uint32_t flags, uint32_t *size)
{
    uint32_t pitch = (w + 3) & ~3u;
    *size = GUARD + 0x24 + pitch * h + 64;
    uint8_t *blk = malloc(*size);
    for (uint32_t k = 0; k < *size; k++)
        blk[k] = (uint8_t)rnd(256);
    uint8_t *p = blk + GUARD;
    *(uint32_t *)p = 0x24, *(int32_t *)(p + 4) = w, *(uint32_t *)(p + 8) = pitch, *(int32_t *)(p + 0xc) = h;
    *(uint32_t *)(p + 0x10) = flags, *(uint8_t **)(p + 0x20) = p + 0x24;
    return blk;
}

static void test_merge(void)
{
    static uint8_t hsl[0x1000];
    static uint32_t cmap[256];
    uint32_t *shadow = (uint32_t *)(base + 0x7b7e8);
    *(uint8_t **)(base + 0x89e24) = hsl;
    for (int i = 0; i < 30000; i++) {
        for (int k = 0; k < 256; k++)
            shadow[k] = rnd(256), cmap[k] = rnd(256);
        *(uint32_t *)(hsl + 0xb74) = rnd(2), *(uint32_t *)(hsl + 0xb68) = rnd(2);
        *(uint32_t *)(hsl + 0x8a4) = rnd(2) ? 640 : 800;
        MergeEnv e = {shadow, *(uint32_t *)(hsl + 0xb74) && *(uint32_t *)(hsl + 0x8a4) > 0x280,
                      *(uint32_t *)(hsl + 0xb74) && *(uint32_t *)(hsl + 0xb68)};
        int sw = 1 + rnd(60), sh = 1 + rnd(40);
        uint8_t *src = seg_dib(sw, sh);
        int full = rnd(3) == 0;
        int w = full ? sw : 1 + rnd(sw), h = full ? sh : 1 + rnd(sh);
        int sx = full ? 0 : rnd(sw - w + 1), sy = full ? 0 : rnd(sh - h + 1);
        int dw = sw + rnd(40), dh = sh + rnd(40);
        uint32_t size;
        uint8_t *ga = gdib(dw, dh, rnd(2) ? 0x40 : 0, &size), *gb = malloc(size);
        memcpy(gb, ga, size), *(uint8_t **)(gb + GUARD + 0x20) = gb + GUARD + 0x24;
        uint8_t *a = ga + GUARD, *b = gb + GUARD;
        int dx = rnd(dw - w + 1), dy = rnd(dh - h + 1);
        uint32_t flip = rnd(4), fl = (flip & 1) | (flip & 2 ? 0x100 : 0);
        uint32_t *cm = rnd(2) ? cmap : NULL;
        ORIG_CDECL(0x10059e74, U(a), U(src), w, h, dx, dy, sx, sy, fl, U(cm));
        dib_merge_rle(&e, b, src, w, h, dx, dy, sx, sy, fl, cm);
        *(uint8_t **)(b + 0x20) = *(uint8_t **)(a + 0x20); /* the only expected difference */
        int same = !memcmp(ga, gb, size);
        if (!same && failures < 5) {
            uint32_t k = 0;
            while (ga[k] == gb[k])
                k++;
            printf("  sw=%d sh=%d w=%d h=%d sx=%d sy=%d dw=%d dh=%d dx=%d dy=%d flip=%x cmap=%d env=%d,%d first diff at %d orig=%d ours=%d\n",
                   sw, sh, w, h, sx, sy, dw, dh, dx, dy, fl, cm != NULL, e.dither_slow, e.dither_fast,
                   (int)k - GUARD - 0x24, ga[k], gb[k]);
        }
        check(full ? "merge full" : "merge clipped", same, i);
        free(ga), free(gb), free(src);
    }
    printf("merge (run-length): done\n");
}

/* a run-length bitmap in the plain merge's format: 16-bit row table, rows stored from the
 * last to the first, each a list of runs, literals and effect pixels ended by a 0 */
static uint8_t *plain_dib(int w, int h)
{
    uint8_t *p = calloc(1, 0x24 + 2 * h + h * (w * 3 + 8) + 64);
    *(int32_t *)(p + 4) = w, *(uint32_t *)(p + 8) = w, *(int32_t *)(p + 0xc) = h;
    *(uint32_t *)(p + 0x10) = 0x80, *(uint8_t **)(p + 0x20) = p + 0x24;
    uint8_t *bits = p + 0x24, *q = bits + 2 * h;
    for (int y = h - 1; y >= 0; y--) {
        ((uint16_t *)bits)[y] = (uint16_t)(q - bits);
        for (int left = w; left > 0;) {
            int n = 1 + rnd(left < 100 ? left : 100), kind = rnd(3);
            if (kind == 0) {
                int v = rnd(3) ? (int)rnd(4) : (int)rnd(256);
                *q++ = (uint8_t)(n > 0x7f ? 0x7f : n), *q++ = (uint8_t)v;
                n = n > 0x7f ? 0x7f : n;
            } else if (kind == 1) {
                n = n > 0x7f ? 0x7f : n;
                *q++ = (uint8_t)(0xff - n);
                for (int k = 0; k < n; k++)
                    *q++ = (uint8_t)rnd(256);
            } else {
                *q++ = 0xff, *q++ = (uint8_t)n;
                for (int k = 0; k < n; k++)
                    *q++ = rnd(2) ? (uint8_t)rnd(4) : (uint8_t)rnd(256);
            }
            left -= n;
        }
        *q++ = 0;
    }
    return p;
}

static void test_plain(void)
{
    static uint32_t cmap[256];
    uint32_t *shadow = (uint32_t *)(base + 0x7b7e8);
    uint8_t *hilite = base + 0x7bbe8, *spot = base + 0x89e1c;
    for (int i = 0; i < 30000; i++) {
        for (int k = 0; k < 256; k++)
            shadow[k] = rnd(256), hilite[k] = (uint8_t)rnd(256), cmap[k] = rnd(256);
        *spot = (uint8_t)rnd(256);
        PlainEnv e = {shadow, hilite, *spot};
        int raw = rnd(2), sw = 1 + rnd(60), sh = 1 + rnd(40);
        uint8_t *src = raw ? dib(sw, sh, 0) : plain_dib(sw, sh);
        if (raw) /* plenty of the special values 0-3 */
            for (uint32_t k = 0; k < (uint32_t)((sw + 3) & ~3) * sh; k++)
                if (rnd(2))
                    src[0x24 + k] = (uint8_t)rnd(4);
        int full = rnd(3) == 0;
        int w = full ? sw : 1 + rnd(sw), h = full ? sh : 1 + rnd(sh);
        int sx = full ? 0 : rnd(sw - w + 1), sy = full ? 0 : rnd(sh - h + 1);
        int dw = sw + rnd(40), dh = sh + rnd(40);
        uint32_t size;
        uint8_t *ga = gdib(dw, dh, rnd(2) ? 0x40 : 0, &size), *gb = malloc(size);
        memcpy(gb, ga, size), *(uint8_t **)(gb + GUARD + 0x20) = gb + GUARD + 0x24;
        uint8_t *a = ga + GUARD, *b = gb + GUARD;
        int dx = rnd(dw - w + 1), dy = rnd(dh - h + 1);
        uint32_t flip = rnd(4), fl = (flip & 1) | (flip & 2 ? 0x100 : 0);
        uint32_t *cm = rnd(2) ? cmap : NULL;
        if (raw && cm) /* only the first row is right with a color map (original bug) */
            h = 1, sy = full ? sh - 1 : sy, full = 0;
        ORIG_CDECL(0x10060c14, U(a), U(src), w, h, dx, dy, sx, sy, fl, U(cm));
        dib_merge_plain(&e, b, src, w, h, dx, dy, sx, sy, fl, cm);
        *(uint8_t **)(b + 0x20) = *(uint8_t **)(a + 0x20);
        int same = !memcmp(ga, gb, size);
        if (!same && failures < 8) {
            uint32_t k = 0;
            while (ga[k] == gb[k])
                k++;
            printf("  sw=%d sh=%d w=%d h=%d sx=%d sy=%d dw=%d dh=%d dx=%d dy=%d flip=%x cmap=%d first diff at %d orig=%d ours=%d\n",
                   sw, sh, w, h, sx, sy, dw, dh, dx, dy, fl, cm != NULL, (int)k - GUARD - 0x24, ga[k], gb[k]);
        }
        check(raw ? "plain raw" : full ? "plain rle full" : "plain rle clipped", same, i);
        free(ga), free(gb), free(src);
    }
    printf("plain merge: done\n");
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2 || !(base = (uint8_t *)LoadLibraryA(argv[1]))) {
        printf("usage: blit_compare <unmodified Rtlib32.dll>\n");
        return 2;
    }
    test_rects();
    test_fade();
    test_fill();
    test_copy();
    test_merge();
    test_plain();
    printf(failures ? "FAILED: %d differences\n" : "all identical\n", failures);
    return failures != 0;
}
