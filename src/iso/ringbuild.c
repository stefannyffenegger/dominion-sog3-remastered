/* The game logic, 0x100dcf34-0x100dd34c: building one ring of cells around a tile. */
#include "iso_core.h"
#include "aim.h"
#include "ringbuild.h"
#include "ringinit.h"

#define RING(n, k) (((int32_t *)ORIG_PTR(0x10181328))[(n) * 4 + (k)])
#define COUNT G(int32_t, 0x10159458)
#define TMP(k) ((uint8_t *)ORIG_PTR(0x10180ce8) + (k) * 8)
#define orig_malloc ORIG(void *(__stdcall *)(uint32_t, int32_t), 0x100f4976)
#define ring_add ORIG(int32_t(__cdecl *)(int32_t *, int32_t *, int32_t), 0x100dd34c)
#define ring_clear ORIG(int32_t(__cdecl *)(int32_t, int32_t, int32_t, int32_t, int32_t), 0x100dd3b3)
#define ring_fix ORIG(int32_t(__cdecl *)(int32_t *, int32_t *, int32_t, int32_t, int32_t), 0x100dd404)

static void keep(int32_t cell, int32_t px, int32_t py)
{
    P(int32_t, TMP(COUNT), 0) = cell;
    P(int16_t, TMP(COUNT), 4) = (int16_t)px;
    P(int16_t, TMP(COUNT), 6) = (int16_t)py;
    COUNT++;
}

/* copies the ring to a new block; `x`, `y` is the tile the offsets are relative to */
static int32_t *ring_copy(int32_t x, int32_t y)
{
    int32_t *out = orig_malloc(COUNT * 8, 0);
    if (!out)
        return 0;
    for (int32_t i = 0; i < COUNT; i++) {
        out[i * 2] = P(int32_t, TMP(i), 0);
        out[i * 2 + 1] = P(int32_t, TMP(i), 4);
        int32_t px = P(int16_t, TMP(i), 4) + x, py = P(int16_t, TMP(i), 6) + y, a, b;
        IsoVTilePos(g_iso, &px, &py, &a, &b);
    }
    return out;
}

/* 0x100dcf34: walks a circle of radius `n` tiles (64 px each, y at half scale) in half
 * degrees around the tile of (x, y), and keeps each new cell on the way as offsets from that
 * tile, for tiles in an even row (`even` 1, +8) or an odd one (+0xc) */
void __cdecl ring_build(int32_t x, int32_t y, int32_t n, int32_t even)
{
    COUNT = 0;
    int32_t radius = n << 6, step = 2, row;
    int32_t base = tile_corner(&x, &y, &row);
    for (int32_t i = 0; i < step * 360; i++) {
        volatile float a = (float)((long double)i / (long double)step * (long double)G(float, 0x10113318) /
                                   (long double)G(float, 0x1011331c));
        int32_t px = x + (int32_t)(aim_cos2(a) * (long double)radius);
        int32_t py = y + (int32_t)(aim_sin2(a) * (long double)radius * (long double)G(float, 0x10113320));
        int32_t cell = ring_add(&px, &py, COUNT);
        if (cell <= 0)
            continue;
        if (ring_clear(px, py, x, y, radius) == 1) {
            keep(cell, px, py);
        } else if (ring_fix(&px, &py, x, y, radius) > 0) {
            cell = ring_add(&px, &py, COUNT);
            if (cell > 0)
                keep(cell, px, py);
        }
    }
    for (int32_t i = 0; i < COUNT; i++) {
        P(int32_t, TMP(i), 0) -= base;
        P(int16_t, TMP(i), 4) = (int16_t)(P(int16_t, TMP(i), 4) - x);
        P(int16_t, TMP(i), 6) = (int16_t)(P(int16_t, TMP(i), 6) - y);
    }
    if (even == 1) {
        RING(n, 0) = n;
        RING(n, 1) = COUNT;
        int32_t *p = ring_copy(x, y);
        RING(n, 2) = (int32_t)(uintptr_t)p;
        if (!p)
            OutputDebugStringA((const char *)ORIG_PTR(0x10159460));
    } else {
        int32_t *p = ring_copy(x, y);
        RING(n, 3) = (int32_t)(uintptr_t)p;
        if (!p)
            OutputDebugStringA((const char *)ORIG_PTR(0x1015949c));
    }
}
