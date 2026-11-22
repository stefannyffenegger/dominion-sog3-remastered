/* The game logic, 0x100dd9d1-0x100dde66: finding a free cell on a ring around a point. */
#include "iso_core.h"
#include "ringfind.h"
#include "ringinit.h"
#include "ringstart.h"

#define RING(n, k) (((int32_t *)ORIG_PTR(0x10181328))[(n) * 4 + (k)])
#define REC_CELL(t, i) P(int32_t, (t), (i) * 8)
#define REC_X(t, i) P(int16_t, (t), (i) * 8 + 4)
#define REC_Y(t, i) P(int16_t, (t), (i) * 8 + 6)
#define cell_taken ORIG(int32_t(__cdecl *)(int32_t), 0x100ddeda)
#define point_free ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100dde66)

/* whether entry `i` of the ring is free to stand on */
static int entry_free(const uint8_t *t, int32_t i, int32_t base, int32_t ox, int32_t oy, int32_t *cell)
{
    *cell = REC_CELL(t, i) + base;
    int32_t px = REC_X(t, i) + ox, py = REC_Y(t, i) + oy;
    return !cell_taken(*cell) && point_free(px, py) == 1;
}

/* 0x100dd9d1: a free cell on ring `n` around (*x, *y), for unit `k` coming from (ux, uy):
 * the entry facing the unit, else the nearest to it either way round the ring, else the next
 * smaller ring (down to 2). The point moves to the cell (from this ring's entry, also when a
 * smaller ring found it). With `mode` 3 the cell is marked as the unit's (+0x3a5, bit 28).
 * Returns the cell, or 0 or less */
int32_t __cdecl ring_find_cell(int32_t k, int32_t ux, int32_t uy, int32_t *x, int32_t *y, int32_t n, int32_t mode)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[k];
    unit_free_links(k);
    int32_t ox = *x, oy = *y, row;
    int32_t base = tile_corner(x, y, &row);
    const uint8_t *t = (const uint8_t *)(uintptr_t)RING(n, (row & 1) ? 3 : 2);
    int32_t idx = ring_start(ux, uy, *x, *y, n);
    int32_t count = RING(n, 1);
    int32_t cell = REC_CELL(t, idx) + base;
    if (cell_taken(cell) == 1) {
        int32_t start = idx, found = 0;
        for (int32_t s = 1; count / 2 > s; s++) {
            idx = start + s;
            if (idx >= count)
                idx -= count;
            if (entry_free(t, idx, base, ox, oy, &cell)) {
                found = 1;
                break;
            }
            idx = start - s;
            if (idx < 0)
                idx += count;
            if (entry_free(t, idx, base, ox, oy, &cell)) {
                found = 1;
                break;
            }
        }
        if (!found)
            cell = -1;
    }
    if (cell <= 0 && n > 2)
        cell = ring_find_cell(k, ux, uy, x, y, n - 1, mode);
    if (cell > 0) {
        *x = REC_X(t, idx) + ox;
        *y = REC_Y(t, idx) + oy;
        if (mode == 3) {
            P(int32_t, u, 0x3a5) = cell;
            if (I(int32_t, 0x1a98) * I(int32_t, 0x1a9c) > cell)
                I(uint32_t *, 0x1934)[cell] |= 0x10000000;
        }
    }
    return cell;
}
