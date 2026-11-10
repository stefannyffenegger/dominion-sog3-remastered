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

int main(int argc, char **argv)
{
    if (argc < 2 || !(base = (uint8_t *)LoadLibraryA(argv[1]))) {
        printf("usage: blit_compare <unmodified Rtlib32.dll>\n");
        return 2;
    }
    test_rects();
    test_fade();
    test_fill();
    printf(failures ? "FAILED: %d differences\n" : "all identical\n", failures);
    return failures != 0;
}
