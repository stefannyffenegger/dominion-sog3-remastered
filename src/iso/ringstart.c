/* The game logic, 0x100dd5db-0x100dd9d1: a unit's mark on the map, and where on a ring of
 * cells to start looking. */
#include "iso_core.h"
#include "ringstart.h"

#define RING_COUNT(n) (((int32_t *)ORIG_PTR(0x10181328))[(n) * 4 + 1])

/* 0x100dd5db: clears bit 28 of the map cell in unit `k`'s +0x3a5, if on the map, and the field */
void __cdecl unit_free_links(int32_t k)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[k];
    int32_t c = P(int32_t, u, 0x3a5);
    if (c >= 0 && I(int32_t, 0x1a98) * I(int32_t, 0x1a9c) > c)
        I(uint32_t *, 0x1934)[c] &= 0xefffffff;
    P(int32_t, u, 0x3a5) = 0;
}

/* `eighth` scaled by a / b, the ratio going through memory as a float */
static int32_t part(int32_t eighth, int32_t a, int32_t b)
{
    volatile float f = (float)((long double)a / (long double)b);
    return (int32_t)((long double)eighth * f);
}

/* 0x100dd662: the index on ring `n` in the direction from (cx, cy) to (x, y), the ring
 * running through the four quadrants with y counting double */
int32_t __cdecl ring_start(int32_t x, int32_t y, int32_t cx, int32_t cy, int32_t n)
{
    int32_t dx = x - cx, dy = y - cy, count = RING_COUNT(n);
    int32_t e = count / 8, q = count / 4;
    int32_t ax = dx < 0 ? -dx : dx, ay = (dy < 0 ? -dy : dy) * 2;
    if (dx >= 0 && dy >= 0)
        return ay == ax ? e : ay < ax ? part(e, ay, ax) : q - part(e, ax, ay);
    if (dx >= 0)
        return ay == ax ? e * 7 : ay < ax ? count - 1 - part(e, ay, ax) : part(e, ax, ay) + q * 3;
    if (dy >= 0)
        return ay == ax ? e * 3 : ay < ax ? q * 2 - part(e, ay, ax) : q + part(e, ax, ay);
    return ay == ax ? e * 5 : ay < ax ? part(e, ay, ax) + q * 2 : q * 3 - part(e, ax, ay);
}
