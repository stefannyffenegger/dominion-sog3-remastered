/* The utility library's floating-point math: our C (src/rtlib/math87.c) against the
 * original, under two FPU precisions. Windows program (runs under Wine):
 *   wine math_compare.exe <unmodified Rtlib32.dll> */
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include "../src/rtlib/math87.h"

typedef int(__stdcall *Fn1)(int);
typedef int(__stdcall *Fn2)(int, int);
typedef int(__stdcall *Fn4)(int, int, int, int);
typedef void(__stdcall *FnCollide)(int, int, int, int, int *, int *, int *, int *);
typedef int(__stdcall *FnIsect)(int *, int, int, int, int, int, int, int, int);

static Fn1 o_sqrt, o_norm, o_sin, o_cos;
static Fn2 o_angle;
static Fn4 o_dist;
static FnCollide o_collide;
static FnIsect o_isect;

/* math87.c's integer trigonometry comes from the original here */
int __stdcall NormalizeAngle(int a) { return o_norm(a); }
int __stdcall Sine(int a) { return o_sin(a); }
int __stdcall Cosine(int a) { return o_cos(a); }

static uint32_t seed = 4711;
static uint32_t rnd32(void)
{
    seed = seed * 1103515245 + 12345;
    uint32_t a = seed >> 8;
    seed = seed * 1103515245 + 12345;
    return a << 16 ^ seed >> 8;
}
static int rnd_in(int lo, int hi) { return lo + (int)(rnd32() % (uint32_t)(hi - lo + 1)); }
/* small, medium, large or extreme numbers */
static int rnd_int(void)
{
    switch (rnd32() % 4) {
    case 0: return rnd_in(-20, 20);
    case 1: return rnd_in(-5000, 5000);
    case 2: return rnd_in(-1000000, 1000000);
    default: return (int)rnd32();
    }
}

static int failures;
static void report(const char *name, int ok, const char *fmt, int a, int b, int c, int d)
{
    if (!ok && failures++ < 20) {
        printf("  %s differs: ", name);
        printf(fmt, a, b, c, d);
        printf("\n");
    }
}

static void set_cw(uint16_t cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }

static void run(uint16_t cw)
{
    set_cw(cw);
    for (int y = -1024; y < 1024; y++) /* every small vector */
        for (int x = -1024; x < 1024; x++) {
            int a = o_angle(x, y), b = GetAngle(x, y);
            if (a != b)
                report("GetAngle", 0, "(%d, %d): %d / %d", x, y, a, b);
        }
    for (int i = 0; i < 2000000; i++) {
        int x = rnd_int(), y = rnd_int();
        int a = o_angle(x, y), b = GetAngle(x, y);
        if (a != b)
            report("GetAngle", 0, "(%d, %d): %d / %d", x, y, a, b);
    }
    for (int v = -70000; v < 3000000; v++) {
        int a = o_sqrt(v), b = SquareRoot(v);
        if (a != b)
            report("SquareRoot", 0, "(%d): %d / %d", v, a, b, 0);
    }
    for (int i = 0; i < 1000000; i++) {
        int v = (int)rnd32(), a = o_sqrt(v), b = SquareRoot(v);
        if (a != b)
            report("SquareRoot", 0, "(%d): %d / %d", v, a, b, 0);
        int p[4] = {rnd_int(), rnd_int(), rnd_int(), rnd_int()};
        a = o_dist(p[0], p[1], p[2], p[3]), b = Distance(p[0], p[1], p[2], p[3]);
        if (a != b)
            report("Distance", 0, "(%d, %d, ...): %d / %d", p[0], p[1], a, b);
    }
    for (int i = 0; i < 1000000; i++) {
        int small = rnd32() % 2, c[8];
        for (int k = 0; k < 8; k++)
            c[k] = small ? rnd_in(-6, 6) : rnd_int();
        int oa[2] = {0x5a5a, 0x5a5a}, ob[2] = {0x5a5a, 0x5a5a};
        int ra = o_isect(oa, c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]);
        int rb = GetIntersect(ob, c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]);
        if (ra != rb || oa[0] != ob[0] || oa[1] != ob[1])
            report("GetIntersect", 0, "(%d, %d, %d, %d, ...)", c[0], c[1], c[2], c[3]);
    }
    for (int i = 0; i < 1000000; i++) {
        int x1 = rnd_int(), y1 = rnd32() % 8 ? rnd_int() : 30000, x2 = rnd_int(), y2 = rnd_int();
        int s1 = rnd32() % 5 ? rnd_in(0, 3000) : 0, a1 = rnd_in(-4000, 8000);
        int s2 = rnd32() % 5 ? rnd_in(0, 3000) : 0, a2 = rnd_in(-4000, 8000);
        if (rnd32() % 10 == 0)
            s1 = rnd_int(), s2 = rnd_int();
        int os1 = s1, oa1 = a1, os2 = s2, oa2 = a2;
        o_collide(x1, y1, x2, y2, &os1, &oa1, &os2, &oa2);
        Collide(x1, y1, x2, y2, &s1, &a1, &s2, &a2);
        if (os1 != s1 || oa1 != a1 || os2 != s2 || oa2 != a2)
            report("Collide", 0, "(%d, %d, %d, %d)", x1, y1, x2, y2);
    }
    printf("control word %04x: done\n", cw);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    HMODULE m = argc > 1 ? LoadLibraryA(argv[1]) : NULL;
    if (!m) {
        printf("usage: math_compare <unmodified Rtlib32.dll>\n");
        return 2;
    }
    o_angle = (Fn2)GetProcAddress(m, "GetAngle");
    o_sqrt = (Fn1)GetProcAddress(m, "SquareRoot");
    o_dist = (Fn4)GetProcAddress(m, "Distance");
    o_collide = (FnCollide)GetProcAddress(m, "Collide");
    o_isect = (FnIsect)GetProcAddress(m, "GetIntersect");
    o_norm = (Fn1)GetProcAddress(m, "NormalizeAngle");
    o_sin = (Fn1)GetProcAddress(m, "Sine");
    o_cos = (Fn1)GetProcAddress(m, "Cosine");
    run(0x27f);
    run(0x37f);
    printf(failures ? "FAILED: %d differences\n" : "all identical\n", failures);
    return failures != 0;
}
