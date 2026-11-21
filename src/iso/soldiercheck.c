/* The game logic, 0x100d0e81-0x100d1afa: whether a soldier's patrol ends somewhere it can
 * stand. Legs: 25 bytes each at +0x1e9 (start x, y, end x, y as floats, the direction). */
#include "iso_core.h"
#include "soldiercheck.h"

#define LEG(u, i) ((u) + 0x1e9 + (i) * 25)
#define OCC (I(uint32_t *, 0x1934))
#define OBJ(c) ((c) >> 16 & 0xfff)
#define fn_100d27d8 ORIG(int(__cdecl *)(uint32_t), 0x100d27d8)
#define NEIGHBOUR(table, row) G(int32_t, (table) + ((row) & 1) * 4)

/* a cell a soldier can stand on: bit 14, or bits 13, 12, 0 and the object clear */
static int open_cell(uint32_t c)
{
    if (c >> 14 & 1)
        return 1;
    return !(c >> 13 & 1) && !(c >> 12 & 1) && !(c & 1) && !OBJ(c);
}

/* the cell of the last leg's end, moved by (dx, dy) */
static int32_t end_cell(uint8_t *u, int dx, int dy, int *row)
{
    uint8_t *last = LEG(u, P(int16_t, u, 0x1e7) - 1);
    int x = (int32_t)(long double)P(float, last, 8) + dx, y = (int32_t)(long double)P(float, last, 0xc) + dy, a, b;
    int32_t cell = IsoVTilePos(g_iso, &x, &y, &a, &b);
    *row = y;
    return cell;
}

/* 0x100d0e81: whether soldier `u` has to plan its patrol again (1) because its last leg ends
 * on a cell it cannot stand on: one taken by an impassable unit (not a kind-3 unit; a
 * patrolling one there also sets +0x1cd bit 8) or by a passable object on a taken cell
 * (bit 3), or a blocked cell boxed in ahead and on both sides. Such an end may still do
 * when the cell next to it for the soldier's formation slot (+0x10e bits 10-11; tables
 * 0x1017c538-0x1017c570, or 32 / 16 further along) is open. The floats become ints by
 * truncation (the original's _ftol). */
int32_t __cdecl soldier_patrol_check(uint8_t *u)
{
    if (!P(int16_t, u, 0x1e7))
        return 0;
    int32_t r = 0, row;
    uint8_t *last = LEG(u, P(int16_t, u, 0x1e7) - 1);
    int32_t cell = end_cell(u, 0, 0, &row);
    if (P(int32_t, u, 0x56) != cell) {
        uint32_t c = OCC[cell];
        if (!(c >> 13 & 1) && !(c >> 12 & 1) && !(c >> 14 & 1)) {
            uint16_t obj = (uint16_t)OBJ(c);
            if (obj) {
                if (!fn_100d27d8(obj)) {
                    if ((c >> 3 & 1) && (int16_t)obj > 0 && I(uint8_t **, ISO_UNITS)[(int16_t)obj] &&
                        unit_kind_of(obj) != 3) {
                        uint8_t *o = I(uint8_t **, ISO_UNITS)[(int16_t)obj];
                        if ((c >> 3 & 1) && o && (P(uint16_t, o, 0x1cd) >> 4 & 1))
                            P(uint16_t, u, 0x1cd) |= 0x100;
                        r = 1;
                    }
                } else if (c >> 3 & 1) {
                    r = 1;
                }
            } else {
                uint8_t dir = P(uint8_t, last, 0x10);
                if ((c & 1) && (OCC[G(int32_t, 0x1017c538 + dir * 8 + (row & 1) * 4) + cell] & 1)) {
                    uint8_t d1 = (uint8_t)((dir + 1) & 7), d2 = (uint8_t)((dir - 1) & 7);
                    if ((OCC[G(int32_t, 0x1017c538 + d1 * 8 + (row & 1) * 4) + cell] & 1) &&
                        (OCC[G(int32_t, 0x1017c538 + d2 * 8 + (row & 1) * 4) + cell] & 1))
                        return 1;
                }
            }
        }
    }
    if (r != 1)
        return r;

    cell = end_cell(u, 0, 0, &row);
    uint8_t dir = P(uint8_t, last, 0x10);
    if (P(int32_t, u, 0x56) == cell)
        return r;
    uint32_t c = OCC[cell];
    if (!(c >> 13 & 1) && !(c >> 12 & 1) && !(c & 1) && !OBJ(c))
        return r;
    uint32_t slot = P(uint32_t, u, 0x10e) >> 10 & 3;
    int32_t next = -1;
    switch (dir) {
    case 0:
        if (slot == 1)
            next = NEIGHBOUR(0x1017c570, row) + cell;
        else if (slot == 3)
            next = NEIGHBOUR(0x1017c540, row) + cell;
        else
            next = end_cell(u, 0x20, 0, &row);
        break;
    case 4:
        if (slot == 1)
            next = NEIGHBOUR(0x1017c560, row) + cell;
        else if (slot == 3)
            next = NEIGHBOUR(0x1017c550, row) + cell;
        else
            next = end_cell(u, -0x20, 0, &row);
        break;
    case 2:
        if (slot == 0)
            next = NEIGHBOUR(0x1017c550, row) + cell;
        else if (slot == 2)
            next = NEIGHBOUR(0x1017c540, row) + cell;
        else
            next = end_cell(u, 0, 0x10, &row);
        break;
    case 6:
        if (slot == 0)
            next = NEIGHBOUR(0x1017c560, row) + cell;
        else if (slot == 2)
            next = NEIGHBOUR(0x1017c570, row) + cell;
        else
            next = end_cell(u, 0, -0x10, &row);
        break;
    default:
        return r;
    }
    if (open_cell(OCC[next]))
        r = 0;
    return r;
}

/* 0x100d1aaa: the kind (+0x28) of unit `id` */
int32_t __cdecl unit_kind_of(uint32_t id)
{
    return unit_kind((uint8_t *)I(uint8_t **, ISO_UNITS)[(int16_t)id]);
}

/* 0x100d1adf: a unit's kind (+0x28) */
int32_t __cdecl unit_kind(uint8_t *u)
{
    return P(uint16_t, u, 0x28);
}
