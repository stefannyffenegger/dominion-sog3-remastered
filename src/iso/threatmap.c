/* The game logic, 0x10066fac-0x10067abc: a byte per map cell (0-6) that structures raise
 * over the cells they stand on (the AI's "occupied" map), distances, a unit's screen box,
 * and the timed animation steps of the effects list. */
#include "iso_core.h"
#include "threatmap.h"

#define OCC 0xe117 /* the map: {int cells; short h, w; uint8_t *v} */
#define OCC_N (*I(int32_t *, OCC))
#define OCC_V P(uint8_t *, I(uint8_t *, OCC), 8)
#define ISO_CELL_FLAGS 0x1934
#define ISO_CELL_LIST 0x1984 /* 0x12-byte records: x (+4), y (+6), next (+0xe) */
#define ISO_EFFECTS 0xd6db   /* 0x44 bytes each */

#define g_hypot ORIG(long double(__cdecl *)(double, double), 0x100f4fc0)
#define EFFECT_STEPS ((uint8_t *)ORIG_PTR(0x10157154)) /* per effect kind, 0x18 apart */
#define EFFECT_MSG ((uint32_t *)ORIG_PTR(0x101572c0))

typedef int(__cdecl *effect_step_fn)(uint32_t, uint32_t);

static int32_t ftol(long double v) { return (int32_t)(int64_t)v; }

/* 0x100670e4: frees the map */
int32_t __cdecl occ_free(void)
{
    if (I(uint8_t *, OCC)) {
        winfree(OCC_V);
        OCC_V = 0;
        winfree(I(uint8_t *, OCC));
        I(uint8_t *, OCC) = 0;
    }
    return 1;
}

/* 0x10066fac: (re)creates the map, all 0, at the map's size. 0 if out of memory */
int32_t __cdecl occ_new(void)
{
    int32_t ok = 1;
    if (I(uint8_t *, OCC))
        occ_free();
    I(uint8_t *, OCC) = winmalloc(0xc, 0);
    if (I(uint8_t *, OCC)) {
        uint8_t *m = I(uint8_t *, OCC);
        P(int16_t, m, 4) = (int16_t)I(int32_t, ISO_MAP_H);
        P(int16_t, m, 6) = (int16_t)I(int32_t, ISO_MAP_W);
        P(int32_t, m, 0) = P(int16_t, m, 4) * P(int16_t, m, 6);
        P(uint8_t *, m, 8) = winmalloc((SIZE_T)P(int32_t, m, 0), 0);
        if (!P(uint8_t *, m, 8)) {
            OutputDebugStringA((const char *)ORIG_PTR(0x10140548));
            ok = 0;
        } else {
            memset(P(uint8_t *, m, 8), 0, (size_t)P(int32_t, m, 0));
        }
    }
    return ok;
}

static void occ_raise(int32_t cell, int16_t by)
{
    uint8_t *v = OCC_V + cell;
    if (*v + by < 7)
        *v = (uint8_t)(*v + by);
    else
        *v = 6;
}

static void occ_lower(int32_t cell, int16_t by)
{
    uint8_t *v = OCC_V + cell;
    if ((int32_t)*v - by <= 0)
        *v = 0;
    else
        *v = (uint8_t)(*v - by);
}

/* 0x10067151: raises a cell (not one flagged 1), to at most 6 */
void __cdecl occ_cell_raise(int32_t cell, int16_t by)
{
    if (cell >= 0 && cell < OCC_N && !(I(uint8_t *, ISO_CELL_FLAGS)[cell * 4] & 1))
        occ_raise(cell, by);
}

/* 0x10067214: lowers a cell, to at least 0 */
void __cdecl occ_cell_lower(int32_t cell, int16_t by)
{
    if (cell >= 0 && cell < OCC_N)
        occ_lower(cell, by);
}

/* 0x100672b3: a cell's value (the upper bits are what the original's registers held) */
uint32_t __cdecl occ_get(int32_t cell)
{
    if (!I(uint8_t *, OCC) || cell < 0)
        return (uint32_t)(uintptr_t)g_iso & 0xffff0000;
    if (OCC_N <= cell)
        return (uint32_t)(uintptr_t)I(uint8_t *, OCC) & 0xffff0000;
    return ((uint32_t)(uintptr_t)OCC_V & 0xffff0000) | OCC_V[cell];
}

static void unit_cells(int16_t id, int16_t by, int raise)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
    if (!u)
        return;
    if (raise) {
        int32_t type = P(int32_t, u, 0xc);
        if (!((type < 0 || type > 7) && (type < 0xc || type > 0x17)))
            return;
    }
    uint8_t *cells = I(uint8_t *, ISO_CELL_LIST);
    for (int16_t k = P(int16_t, u, 0x113); k > 0; k = P(int16_t, cells, k * 0x12 + 0xe)) {
        int x = P(int16_t, cells, k * 0x12 + 4);
        int y = P(int16_t, cells, k * 0x12 + 6);
        int tx, ty;
        int32_t cell = IsoVTilePos(g_iso, &x, &y, &tx, &ty);
        if (cell >= 0 && cell < OCC_N) {
            if (raise)
                occ_raise(cell, by);
            else
                occ_lower(cell, by);
        }
    }
}

/* 0x10067316: raises the cells a unit stands on (not for unit types 0-7 and 0xc-0x17) */
void __cdecl occ_unit_raise(int16_t id, int16_t by) { unit_cells(id, by, 1); }

/* 0x100674b3: lowers the cells a unit stands on */
void __cdecl occ_unit_lower(int16_t id, int16_t by) { unit_cells(id, by, 0); }

/* 0x10067620: the distance between two points, truncated */
int64_t __cdecl point_distance(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    return (int64_t)g_hypot((double)(x2 - x1), (double)(y2 - y1));
}

/* 0x10067664: the distance in steps along the axes */
int32_t __cdecl point_distance_axes(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    int32_t dx = x2 - x1, dy = y2 - y1;
    return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

/* 0x10067690: a unit's box on the screen, from its sprite's first (or, for types
 * 0x88-0x9f, second) frame; turrets (types 0x30-0x37) are raised by their height */
void __cdecl unit_screen_box(uint32_t id, int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    if (!u)
        return;
    uint8_t *s = P(uint8_t *, u, 4);
    uint8_t **frame = 0;
    int32_t raise = 0;
    int32_t type = P(int32_t, u, 0xc);
    if (type >= 0x30 && type <= 0x37)
        raise = ftol(P(float, P(uint8_t *, u, 0x39d), 0x20));
    if (SpriteGetNumCels(s) >= 2) {
        if (type < 0x88 || type > 0x9f)
            frame = (uint8_t **)(P(uint8_t *, s, 0x24) + 4);
        else
            frame = P(uint8_t **, s, 0x24);
    }
    if (!frame) {
        *x = P(int16_t, u, 0x52);
        *y = P(int16_t, u, 0x54) - 10 - raise;
        *w = 0x14;
        *h = 0x14;
    } else {
        uint8_t *f = *frame;
        *x = P(int16_t, u, 0x52) + P(int32_t, f, 0x14);
        *y = P(int16_t, u, 0x54) + P(int32_t, f, 0x18) - raise;
        *w = P(int32_t, f, 8);
        *h = P(int32_t, f, 0xc);
    }
}

/* 0x100677e6: counts down an effect's timer; at 0 its kind's step runs, and if that returns
 * nonzero, the timer starts again */
void __cdecl effect_timer(uint32_t id, uint32_t arg)
{
    uint8_t *e = I(uint8_t *, ISO_EFFECTS) + (id & 0xffff) * 0x44;
    P(uint8_t, e, 0x42)--;
    if (P(uint8_t, e, 0x42) == 0) {
        effect_step_fn step = P(effect_step_fn, EFFECT_STEPS, P(int32_t, P(uint8_t *, e, 0x16), 0x50) * 0x18);
        if (step && step(id, arg))
            P(uint8_t, e, 0x42) = P(uint8_t, e, 0x41);
    }
}

static void effect_sprite_show(uint8_t *sprite, uint32_t cel, int x, int y)
{
    SpriteSetRedraw(I(void *, ISO_HSL), sprite, 0, 0);
    SpriteReplaceCellRange(I(void *, ISO_HSL), I(void *, 0x1822), sprite, cel, 0, 1);
    SpriteSetPosition(I(void *, ISO_HSL), sprite, x, y, 0);
    P(uint32_t, sprite, 0xec) = 0;
    EFFECT_MSG[1] = 0;
    EFFECT_MSG[2] = 0;
    EFFECT_MSG[3] = 0;
    SpriteSendMessage(I(void *, ISO_HSL), sprite, 4, EFFECT_MSG);
}

/* 0x10067883: an effect's next frame (counting down from its kind's frame count), and its
 * second sprite's */
void __cdecl effect_next_frame(uint32_t id)
{
    uint8_t *e = I(uint8_t *, ISO_EFFECTS) + (id & 0xffff) * 0x44;
    uint8_t *kind = P(uint8_t *, e, 0x16);
    if ((P(uint8_t, e, 0x43) >> 3 & 1) || !P(int32_t, kind, 0x44))
        return;
    SpriteSetRedraw(I(void *, ISO_HSL), P(void *, e, 0), 0, 0);
    if (P(uint8_t, e, 0x34))
        P(uint8_t, e, 0x34)--;
    SpriteReplaceCellRange(I(void *, ISO_HSL), I(void *, 0x1822), P(void *, e, 0),
                           (uint32_t)(P(int32_t, kind, 0) + P(uint8_t, e, 0x34) + P(int16_t, e, 0x32)), 0, 1);
    SpriteSetPosition(I(void *, ISO_HSL), P(void *, e, 0), P(int16_t, e, 0x22), P(int16_t, e, 0x24), 0);
    P(uint32_t, P(uint8_t *, e, 0), 0xec) = 0;
    EFFECT_MSG[1] = 0;
    EFFECT_MSG[2] = 0;
    EFFECT_MSG[3] = 0;
    SpriteSendMessage(I(void *, ISO_HSL), P(void *, e, 0), 4, EFFECT_MSG);
    if (!P(uint8_t, e, 0x34))
        P(uint8_t, e, 0x34) = P(uint8_t, kind, 0x44);
    if (P(uint8_t *, e, 4))
        effect_sprite_show(P(uint8_t *, e, 4), P(uint32_t, kind, 0x14), P(int16_t, e, 0x2a), P(int16_t, e, 0x2c));
}
