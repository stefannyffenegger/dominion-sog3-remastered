/* The game logic, 0x100787e3-0x10079568: wall segments between two of a player's wall posts:
 * a record (0x86 bytes, at 0xe123, 0xe11f of them) with the two posts, the direction and
 * up to 20 cells with their wall pieces. */
#include "iso_core.h"
#include "display.h"
#include "damage.h"
#include "intfwave.h"
#include "lineofsight.h"
#include "walls.h"

#define STEP ((const int8_t *)ORIG_PTR(0x1013b468))
#define SEGS I(uint8_t *, 0xe123)
#define SEG_N I(int32_t, 0xe11f)

#define posts_too_close ORIG(int(__cdecl *)(uint8_t *, uint8_t *), 0x100d1e0f)
#define units_between ORIG(int32_t(__cdecl *)(uint8_t *, uint8_t *, int32_t *), 0x100d2083)
#define route_neighbour ORIG(uint32_t(__cdecl *)(int16_t, int16_t), 0x10077b2b)
#define route_split ORIG(void(__cdecl *)(uint32_t, uint32_t, int), 0x10079c6c)
#define segment_find ORIG(uint32_t(__cdecl *)(int16_t, int16_t), 0x10079568)
#define wall_piece_at ORIG(int16_t(__cdecl *)(int32_t, uint32_t, uint16_t, uint32_t), 0x10079669)

static uint8_t *unit_ptr(int32_t id) { return I(uint8_t **, ISO_UNITS)[id]; }

/* 0x100787e3: whether a unit is a structure of types 0x40-0x4f */
int32_t __cdecl unit_is_post(int16_t unit)
{
    uint8_t *u = unit_ptr(unit);
    return u && P(int32_t, u, 0xc) >= 0x40 && P(int32_t, u, 0xc) <= 0x4f;
}

/* 0x10079430: a free segment record (in use 0, no posts), or a new one: the table grows by
 * 50. -1 if out of memory */
int16_t __cdecl wall_segment_alloc(void)
{
    int16_t k;
    for (k = 0; (int32_t)k < SEG_N; k++)
        if (!P(int32_t, SEGS, k * 0x86) && !P(int16_t, SEGS, k * 0x86 + 4) && !P(int16_t, SEGS, k * 0x86 + 6))
            return k;
    int32_t old = SEG_N;
    SEG_N += 0x32;
    SEGS = winrealloc(SEGS, (SIZE_T)(SEG_N * 0x86), 1);
    return SEGS ? (int16_t)old : -1;
}

/* 0x10078fa5: builds the wall segment from post `a` to post `b` along a clear straight line:
 * splits the posts' existing segments in that direction, reuses a matching one or takes a
 * new record, and fills in the cells with their wall pieces. The record, or -1 */
int16_t __cdecl wall_segment_build(uint32_t a, uint32_t b)
{
    if (!unit_fire_line_clear(a, b))
        return -1;
    uint8_t *ua = unit_ptr((int16_t)a), *ub = unit_ptr((int16_t)b);
    uint16_t dir = (uint16_t)unit_direction(a, b);
    uint16_t back = (uint16_t)(dir - 4) & 7;
    int16_t seg;
    if (unit_route_kind2((int16_t)a, (int16_t)dir) == 1 || unit_route_kind2((int16_t)b, (int16_t)back) == 1) {
        int16_t n = (int16_t)route_neighbour((int16_t)a, (int16_t)dir);
        if (n > 0 && (int16_t)b != n)
            route_split(a, (uint16_t)n, 1);
        n = (int16_t)route_neighbour((int16_t)b, (int16_t)back);
        if (n > 0 && (int16_t)a != n)
            route_split(b, (uint16_t)n, 1);
        seg = (int16_t)segment_find((int16_t)a, (int16_t)b);
        if (seg < 0 && (seg = wall_segment_alloc()) < 0)
            return -1;
    } else if ((seg = wall_segment_alloc()) < 0) {
        return -1;
    }
    uint8_t *r = SEGS + seg * 0x86;
    P(int16_t, r, 4) = (int16_t)a;
    P(int16_t, r, 6) = (int16_t)b;
    P(int32_t, r, 10) = 0;
    int x = P(int16_t, ua, 0x52), tx, ty;
    uint32_t y = (uint32_t)(int32_t)P(int16_t, ua, 0x54);
    IsoVTilePos(g_iso, &x, (int *)&y, &tx, &ty);
    int x2 = P(int16_t, ub, 0x52);
    uint32_t y2 = (uint32_t)(int32_t)P(int16_t, ub, 0x54);
    IsoVTilePos(g_iso, &x2, (int *)&y2, &tx, &ty);
    int32_t n = 0;
    int16_t d = (int16_t)dir;
    int32_t sx = STEP[d * 4 + (y & 1) * 2], sy = STEP[d * 4 + (y & 1) * 2 + 1];
    x += sx;
    y += sy;
    uint32_t owner = ((uint32_t)(uintptr_t)ua & 0xffff0000) | P(uint8_t, ua, 0x14);
    do {
        if (x2 == x && y2 == y)
            break;
        int16_t piece = wall_piece_at(x, y, dir, owner);
        if (piece < 0)
            break;
        r = SEGS + seg * 0x86;
        P(int16_t, r, 0xe + n * 6) = piece;
        P(int16_t, r, 0x10 + n * 6) = (int16_t)x;
        P(int16_t, r, 0x12 + n * 6) = (int16_t)y;
        sx = STEP[d * 4 + (y & 1) * 2];
        sy = STEP[d * 4 + (y & 1) * 2 + 1];
        x += sx;
        y += sy;
        n++;
    } while (n < 0x14);
    r = SEGS + seg * 0x86;
    if (n < 1) {
        memset(r, 0, 0x86);
    } else {
        P(int32_t, r, 0) = 1;
        P(uint16_t, r, 8) = dir;
        P(int32_t, r, 10) = n;
        P(int16_t, ua, 0x372 + d * 2) = seg;
        P(int16_t, ub, 0x372 + (int16_t)back * 2) = seg;
    }
    return seg;
}

/* 0x10078f1e: the units standing between two posts take 50% damage from the first's
 * player */
void __cdecl wall_crush_between(uint8_t *a, uint8_t *b)
{
    int32_t ids[20];
    int32_t n = units_between(a, b, ids);
    for (int32_t k = 0; k < n; k++) {
        uint8_t *u = unit_ptr(ids[k]);
        if (u)
            unit_take_damage(u, ((uint32_t)(uintptr_t)a & 0xffff0000) | P(uint8_t, a, 0x14), 0x32);
    }
}

/* 0x10078dd5: connects two of a player's wall posts (both with walls, +0x42, and not
 * flagged +0x3e bit 12); with `check`, not when they are too close. The units in between
 * are crushed, and in mission state 4 the player hears message 0x26. 1 if connected */
int32_t __cdecl walls_connect(uint32_t a, uint32_t b, uint32_t unused, int check)
{
    (void)unused;
    uint8_t *ua = unit_ptr(a & 0xffff), *ub = unit_ptr(b & 0xffff);
    if (!ua || !ub || P(uint8_t, ub, 0x14) != P(uint8_t, ua, 0x14))
        return 0;
    if (!P(int16_t, ua, 0x42) || (P(uint32_t, ua, 0x3e) >> 12 & 1) || !P(int16_t, ub, 0x42) ||
        (P(uint32_t, ub, 0x3e) >> 12 & 1))
        return 0;
    if (check == 1 && posts_too_close(ua, ub) == 1)
        return 0;
    if (wall_segment_build(a, b) >= 0) {
        wall_crush_between(ua, ub);
        if (I(int32_t, 0x17f5) == 4)
            player_announce(P(uint8_t, ua, 0x14), 0x26, 1);
    }
    return 1;
}
