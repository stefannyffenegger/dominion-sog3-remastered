/* The game logic, 0x100772d9-0x10078453: whether a straight line between two units is clear
 * (for shooting), walking the map cells in one of the eight directions, and a few checks. */
#include "iso_core.h"
#include "display.h"
#include "lineofsight.h"

#define STEP ((const int8_t *)ORIG_PTR(0x1013b468)) /* per direction: dx, dy on even rows, then odd */
#define CELL(x, y) I(uint32_t *, 0x1934)[((uint32_t)(y) << (I(uint8_t, 0x1aa0) & 0x1f)) + (x)]
#define is_structure ORIG(int32_t(__cdecl *)(int16_t), 0x1007818a)

static uint8_t *unit_ptr(int32_t id) { return I(uint8_t **, ISO_UNITS)[id]; }

/* 0x100772d9: called with each cell a line visits; empty (a debug hook) */
void __cdecl line_visit(int32_t x, int32_t y)
{
    (void)x;
    (void)y;
}

/* 0x10077303: whether a unit is of types 0x40-0x43 (walls) */
int32_t __cdecl unit_is_wall(int16_t unit)
{
    uint8_t *u = unit_ptr(unit);
    return u && P(int32_t, u, 0xc) >= 0x40 && P(int32_t, u, 0xc) <= 0x43;
}

/* 0x10077784: whether a cell is on the map */
int32_t __cdecl map_cell_valid(int32_t x, int32_t y)
{
    return x >= 0 && x < I(int32_t, ISO_MAP_W) && y >= 0 && y < I(int32_t, ISO_MAP_H);
}

static int32_t route_kind(int16_t unit, int16_t dir, int32_t kind)
{
    int32_t r = P(int16_t, unit_ptr(unit), 0x372 + dir * 2);
    return r >= 0 && r < I(int32_t, 0xe11f) && P(int32_t, I(uint8_t *, 0xe123), r * 0x86) == kind;
}

/* 0x100778fb, 0x10077aa1: whether the unit's route record for a direction (+0x372) is of
 * kind 1 or 2 */
int32_t __cdecl unit_route_kind1(int16_t unit, int16_t dir) { return route_kind(unit, dir, 1); }
int32_t __cdecl unit_route_kind2(int16_t unit, int16_t dir) { return route_kind(unit, dir, 2); }

static uint32_t step_cell(int32_t x, uint32_t y, int16_t dir)
{
    uint16_t d = (uint16_t)dir & 7;
    int32_t nx = (int16_t)((int16_t)STEP[(y & 1) * 2 + (int16_t)d * 4] + (int16_t)x);
    int32_t ny = (int16_t)((int16_t)STEP[(y & 1) * 2 + (int16_t)d * 4 + 1] + (int16_t)y);
    return CELL(nx, ny);
}

/* 0x1007831e: whether the cells beside (x, y), turned one direction either way from `dir`,
 * are blocked (bit 14) */
int32_t __cdecl line_side_blocked(int32_t x, uint32_t y, int16_t dir)
{
    uint16_t d = (uint16_t)(dir + 1) & 7;
    int32_t dx = STEP[(y & 1) * 2 + (int16_t)d * 4], dy = STEP[(y & 1) * 2 + (int16_t)d * 4 + 1];
    line_visit(dx + x, (int32_t)(dy + y));
    if (CELL(dx + x, dy + y) >> 14 & 1)
        return 1;
    d = (uint16_t)(dir - 1) & 7;
    dx = STEP[(y & 1) * 2 + (int16_t)d * 4];
    dy = STEP[(y & 1) * 2 + (int16_t)d * 4 + 1];
    line_visit(dx + x, (int32_t)(dy + y));
    return CELL(dx + x, dy + y) >> 14 & 1;
}

/* 0x100781e6: walking from (x1, y1) in direction `dir` towards (x2, y2), whether a step
 * squeezes between blocked cells on both sides (up to 20 steps, stopping one short) */
int32_t __cdecl line_squeezed(int32_t x1, uint32_t y1, int32_t x2, uint32_t y2, int16_t dir)
{
    uint16_t back = (uint16_t)(dir - 4) & 7;
    int32_t x = x1 + STEP[dir * 4 + (y1 & 1) * 2];
    uint32_t y = y1 + STEP[dir * 4 + (y1 & 1) * 2 + 1];
    if (x == x2 && y2 == y)
        return 1;
    int8_t bx = STEP[(int16_t)back * 4 + (y2 & 1) * 2], by = STEP[(int16_t)back * 4 + (y2 & 1) * 2 + 1];
    for (int k = 2; k < 0x14; k++) {
        if (x == x2 + bx && y2 + by == y)
            break;
        if (line_side_blocked(x, y, dir) == 1)
            return 1;
        int32_t sx = STEP[dir * 4 + (y & 1) * 2], sy = STEP[dir * 4 + (y & 1) * 2 + 1];
        x += sx;
        y += sy;
    }
    return 0;
}

/* 0x10077c54: whether `unit` has a clear straight line to `target` (one of the eight
 * directions, at most 20 cells). Along an axis no cell may be blocked (bits 14, 0) or have
 * a structure on either side; diagonally, raised cells (bit 15) are stepped over. 1 only if
 * the target is reached after at least one step */
int32_t __cdecl unit_fire_line_clear(uint32_t unit, uint32_t target)
{
    uint32_t dir32 = (uint32_t)(uint16_t)unit_direction(unit, target);
    int16_t dir = (int16_t)dir32;
    int16_t u16 = (int16_t)unit;
    int32_t tid = (int16_t)target;
    if (unit_route_kind1(u16, dir) == 1)
        return 0;
    uint8_t *u = unit_ptr(u16), *t = unit_ptr((int16_t)target);
    int x = P(int16_t, u, 0x52), tx2, ty2;
    uint32_t y = (uint32_t)(int32_t)P(int16_t, u, 0x54);
    IsoVTilePos(g_iso, &x, (int *)&y, &tx2, &ty2);
    int x2 = P(int16_t, t, 0x52);
    uint32_t y2 = (uint32_t)(int32_t)P(int16_t, t, 0x54);
    IsoVTilePos(g_iso, &x2, (int *)&y2, &tx2, &ty2);
    int32_t n = 0;
    if (!(dir32 & 1)) {
        if (line_squeezed(x, y, x2, y2, dir))
            return 0;
        do {
            int32_t sx = STEP[dir * 4 + (y & 1) * 2], sy = STEP[dir * 4 + (y & 1) * 2 + 1];
            x += sx;
            y += sy;
            line_visit(x, (int32_t)y);
            uint32_t c = CELL(x, y);
            if ((int32_t)(c >> 16 & 0xfff) == tid)
                return n >= 1;
            if ((c >> 14 & 1) || (c & 1))
                return 0;
            for (int side = 1; side >= -1; side -= 2) {
                uint32_t s = step_cell(x, y, (int16_t)(dir + side));
                uint32_t who = s >> 16 & 0xfff;
                if (who && (int32_t)who != tid && is_structure((int16_t)((uint16_t)(s >> 16) & 0xfff)) == 1)
                    return 0;
            }
            n++;
        } while (n < 0x14 && (x != x2 || y != y2));
    } else {
        if (!unit_is_wall(u16)) {
            int32_t sx = STEP[dir * 4 + (y & 1) * 2], sy = STEP[dir * 4 + (y & 1) * 2 + 1];
            x += sx;
            y += sy;
            n++;
        }
        do {
            int32_t sx = STEP[dir * 4 + (y & 1) * 2], sy = STEP[dir * 4 + (y & 1) * 2 + 1];
            x += sx;
            y += sy;
            line_visit(x, (int32_t)y);
            uint32_t c = CELL(x, y);
            if (!(c >> 15 & 1)) {
                if ((int32_t)(c >> 16 & 0xfff) == tid)
                    return n >= 1;
                if ((c >> 14 & 1) || (c & 1))
                    return 0;
            } else {
                sx = STEP[dir * 4 + (y & 1) * 2];
                sy = STEP[dir * 4 + (y & 1) * 2 + 1];
                x += sx;
                y += sy;
                line_visit(x, (int32_t)y);
                c = CELL(x, y);
                if ((int32_t)(c >> 16 & 0xfff) == tid)
                    return 1;
            }
            n++;
        } while (n < 0x14 && (x != x2 || y != y2));
    }
    return 0;
}
