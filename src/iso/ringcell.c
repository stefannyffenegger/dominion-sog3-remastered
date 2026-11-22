/* The game logic, 0x100dd34c-0x100dd546: the helpers of the ring builder. */
#include "iso_core.h"
#include "ringcell.h"
#include "ringinit.h"

#define TMP_CELL(k) P(int32_t, (uint8_t *)ORIG_PTR(0x10180ce8), (k) * 8)
#define vector_to ORIG(void(__cdecl *)(float *, float *, float *), 0x1007e720)
#define crt_sqrt ORIG(long double(__cdecl *)(double), 0x100f4dd0)

/* 0x100dd34c: the cell of (*x, *y), moved to its tile; 0 when among the first `count` kept */
int32_t __cdecl ring_add(int32_t *x, int32_t *y, int32_t count)
{
    int32_t row, cell = tile_corner(x, y, &row);
    for (int32_t i = 0; i < count; i++)
        if (TMP_CELL(i) == cell)
            return 0;
    return cell;
}

/* 0x100dd3b3: whether (x, y) is within `r` of (cx, cy), measured along the axes */
int32_t __cdecl ring_clear(int32_t x, int32_t y, int32_t cx, int32_t cy, int32_t r)
{
    int32_t dy = y - cy, dx = x - cx;
    return (dy < 0 ? -dy : dy) + (dx < 0 ? -dx : dx) <= r;
}

/* 0x100dd404: moves (*x, *y) towards (cx, cy) in steps from 4 until its tile is within `r`;
 * that tile's cell, or 0 when none is */
int32_t __cdecl ring_fix(int32_t *x, int32_t *y, int32_t cx, int32_t cy, int32_t r)
{
    float a[2] = {(float)*x, (float)*y}, b[2] = {(float)cx, (float)cy}, v[2];
    vector_to(a, b, v);
    long double ady = (long double)a[1] - b[1], adx = (long double)a[0] - b[0];
    crt_sqrt((double)(ady * ady + adx * adx));
    volatile float t = 4.0f;
    /* (as the x87 compare: unordered goes on) */
    while (!(t >= G(float, 0x10113328))) {
        int32_t px = (int32_t)((long double)v[0] * t + a[0]);
        int32_t py = (int32_t)((long double)v[1] * t + a[1]);
        int32_t row, cell = tile_corner(&px, &py, &row);
        if (ring_clear(*x, *y, px, py, r) == 1) {
            *x = px;
            *y = py;
            return cell;
        }
        t = (float)((long double)t + G(float, 0x10113324));
    }
    return 0;
}
