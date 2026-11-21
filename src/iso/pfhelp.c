/* The game logic, 0x100c9a00-0x100c9cf0: the path finder's helpers (the rotated grid's
 * current cell 0x1017e408/40c and step direction 0x10158d74 are the original's globals). */
#include "iso_core.h"
#include "pfhelp.h"

#define CX G(int32_t, 0x1017e408)
#define CY G(int32_t, 0x1017e40c)
#define DIR G(int32_t, 0x10158d74)
#define crt_abs ORIG(int(__cdecl *)(int), 0x100f4f80)

/* 0x100c9a00: steps to the next of the current cell's 8 neighbours (tables 0x10158d08 for
 * the step, 0x10158d48 for the next direction) */
void __cdecl pf_step(void)
{
    CX += G(int32_t, 0x10158d08 + DIR * 8);
    CY += G(int32_t, 0x10158d0c + DIR * 8);
    DIR = G(int32_t, 0x10158d48 + DIR * 4);
}

/* 0x100c9a50: the estimated cost between two cells of the rotated grid (their distance in
 * steps along the axes) */
int32_t __cdecl pf_cost(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    int32_t dy = crt_abs(y0 - y1);
    return crt_abs(x0 - x1) + dy;
}

/* 0x100c9aa0: a map cell's column in the rotated grid */
int32_t __cdecl pf_rotate(int32_t col, int32_t row)
{
    int32_t r = (row >> 1) + col;
    if (row % 2)
        r++;
    return r;
}

/* 0x100c9ae0: whether map cell `cell` (at `col`, `row`) is closed to unit `unit` (0: any):
 * off the map; blocked (bit 0, except for hovercraft 0x30-0x37; bit 1); taken by another
 * object (bits 16-27), unless that is a wall piece (types 0x88-0x9c); bit 14 cells only for
 * computer units (+0x1cd bit 5); bit 12 cells never */
int32_t __cdecl pf_blocked(int32_t cell, int32_t col, int32_t row, uint32_t unit)
{
    int32_t hover = 0, ai = 0;
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    if (u) {
        hover = P(int32_t, u, 0xc) >= 0x30 && P(int32_t, u, 0xc) <= 0x37;
        ai = P(uint16_t, u, 0x1cd) >> 5 & 1;
    }
    if (col <= 0 || I(int32_t, 0x1a9c) <= col)
        return 1;
    if (row <= 0 || I(int32_t, 0x1a98) <= row)
        return 1;
    uint32_t c = I(uint32_t *, 0x1934)[cell];
    if (c >> 12 & 1)
        return 0;
    uint32_t obj = c >> 16 & 0xfff;
    if ((!(c & 1) || hover) && !(c >> 1 & 1) && (!obj || obj == (unit & 0xffff)))
        return 0;
    if ((c >> 14 & 1) && !ai)
        return 0;
    if (c & 1)
        return 1;
    if (!obj || !I(uint8_t **, ISO_UNITS)[obj])
        return 1;
    int32_t kind = P(int32_t, I(uint8_t **, ISO_UNITS)[obj], 0x10);
    if (kind < 0x88)
        return 1;
    return kind <= 0x9c ? 0 : 1;
}
