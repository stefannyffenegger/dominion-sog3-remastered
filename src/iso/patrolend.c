/* The game logic, 0x100cc55c-0x100cc9ca: whether a vehicle's patrol still ends somewhere it
 * can go. Legs: 25 bytes each at +0x1e9 (start x, y, end x, y as floats, the direction). */
#include "iso_core.h"
#include "patrolend.h"

#define LEG(u, i) ((u) + 0x1e9 + (i) * 25)
#define leg_vector ORIG(void(__cdecl *)(uint32_t, float *), 0x100cba36)
#define leg_step ORIG(long double(__cdecl *)(uint32_t), 0x100cc0b9)
#define fn_100d27d8 ORIG(int(__cdecl *)(int16_t), 0x100d27d8)
#define OCC (I(uint32_t *, 0x1934))

/* the cell a step of `dir` (scaled by 0x101132c8) beyond (x, y) */
static uint32_t *cell_beyond(uint8_t *u, int32_t x, int32_t y, uint32_t dir, int32_t *cell)
{
    float v[2];
    int tx = x, ty = y, a, b;
    leg_vector(dir, v);
    tx += (int32_t)((long double)v[0] * G(float, 0x101132c8));
    ty += (int32_t)((long double)v[1] * G(float, 0x101132c8));
    *cell = IsoVTilePos(g_iso, &tx, &ty, &a, &b);
    (void)u;
    return &OCC[*cell];
}

/* a cell a vehicle can drive onto: not bit 14, and bits 13, 12, 0 and the object clear */
static int open_cell(uint32_t c)
{
    if (c >> 14 & 1)
        return 1;
    return !(c >> 13 & 1) && !(c >> 12 & 1) && !(c & 1) && !(c >> 16 & 0xfff);
}

/* 0x100cc696: whether the cell at (x, y) blocks the end of vehicle `u`'s patrol (1): not
 * when it is the vehicle's own cell or a cell with bits 12-14. A cell with an object that
 * is not passable (0x100d27d8) blocks unless the next cell in direction `dir` is open (a
 * patrolling unit on a taken cell, +0x1cd bit 4, also sets the vehicle's +0x1cd bit 8); a
 * passable object blocks on a taken cell (bit 3). A blocked cell without an object (bit 0,
 * unless +0x10e bit 9) blocks unless the next cell is open. */
int32_t __cdecl patrol_end_free(uint8_t *u, int32_t x, int32_t y, uint32_t dir)
{
    int32_t r = 0, cell;
    int tx = x, ty = y, a, b;
    cell = IsoVTilePos(g_iso, &tx, &ty, &a, &b);
    if (P(int32_t, u, 0x56) == cell)
        return 0;
    uint32_t c = OCC[cell];
    if ((c >> 13 & 1) || (c >> 12 & 1) || (c >> 14 & 1))
        return 0;
    uint16_t obj = (uint16_t)(c >> 16 & 0xfff);
    if (obj) {
        if (!fn_100d27d8((int16_t)obj)) {
            if ((c >> 3 & 1) && (int16_t)obj > 0 && I(uint8_t **, ISO_UNITS)[(int16_t)obj] &&
                (P(uint16_t, I(uint8_t **, ISO_UNITS)[(int16_t)obj], 0x1cd) >> 4 & 1))
                P(uint16_t, u, 0x1cd) |= 0x100;
            r = 1;
            uint32_t *n = cell_beyond(u, x, y, dir, &cell);
            if (P(int32_t, u, 0x56) != cell && open_cell(*n))
                r = 0;
        } else if (c >> 3 & 1) {
            r = 1;
        }
        return r;
    }
    if (!(c & 1) || (P(uint32_t, u, 0x10e) >> 9 & 1))
        return 0;
    r = 1;
    uint32_t *n = cell_beyond(u, x, y, dir, &cell);
    if (P(int32_t, u, 0x56) != cell && open_cell(*n))
        r = 0;
    return r;
}

/* 0x100cc55c: whether vehicle `u` has to plan its patrol again: 1 without legs (or more than
 * 15), else what 0x100cc696 says about the last leg's end, tried again one step back when
 * it says 0. The floats become ints by truncation (the original's _ftol). */
int32_t __cdecl vehicle_patrol_check(uint8_t *u)
{
    int16_t n = P(int16_t, u, 0x1e7);
    if (n <= 0 || n > 0xf)
        return 1;
    uint8_t *last = LEG(u, n - 1);
    int32_t x = (int32_t)(long double)P(float, last, 8);
    int32_t y = (int32_t)(long double)P(float, last, 0xc);
    uint8_t dir = P(uint8_t, last, 0x10);
    int32_t r = patrol_end_free(u, x, y, dir);
    if (r)
        return r;
    volatile float step = (float)leg_step(dir);
    float vec[2];
    leg_vector((uint8_t)((dir - 4) & 7), vec);
    x += (int32_t)((long double)vec[0] * step);
    y += (int32_t)((long double)vec[1] * step);
    return patrol_end_free(u, x, y, dir);
}
