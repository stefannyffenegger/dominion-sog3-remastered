/* The game logic, 0x100bdbeb-0x100bdd54: units carried by another (+0xf6 the next carried
 * unit, +0xf3 how much the carrier holds, each unit's type +0xe4 the room it takes). */
#include "iso_core.h"
#include "sector.h"
#include "carry.h"

#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])

/* 0x100bdbeb: moves the units carried by unit `unit` to its point; a carried unit that no
 * longer exists ends the chain */
void __cdecl carried_follow(uint32_t unit)
{
    int32_t prev = unit & 0xffff;
    int32_t k = P(uint16_t, UNIT(unit & 0xffff), 0xf6);
    if (k <= 0)
        return;
    int32_t room = P(uint8_t, UNIT(unit & 0xffff), 0xf3);
    while (room && k > 0) {
        if (!UNIT(k)) {
            P(uint16_t, UNIT(prev), 0xf6) = 0;
            return;
        }
        P(int16_t, UNIT(k), 0x52) = P(int16_t, UNIT(unit & 0xffff), 0x52);
        P(int16_t, UNIT(k), 0x54) = P(int16_t, UNIT(unit & 0xffff), 0x54);
        room -= P(int32_t, P(uint8_t *, UNIT(k), 0x32), 0xe4);
        prev = k;
        k = P(uint16_t, UNIT(k), 0xf6);
    }
}

/* ---- 0x100be084-0x100be4d2: the destinations units have claimed (iso+0x18e8, 11 bytes
 * each: +0 the unit, +2 the map cell, +6 bits 0-1 the formation slot, +7 the previous,
 * +9 the next; the list 0x1066-0x1068, the free ones from 0x1acb; a unit's own +0x15d) */

#define DEST(k) (I(uint8_t *, 0x18e8) + (k) * 11)

/* 0x100be084: claims the map cell at (x, y) as unit `unit`'s destination, or with `check`
 * only tells whether it is free for the unit (a cell another unit has claimed is free only
 * for a formation type (+0x74 == 1) in another slot). Only for the ground units' type
 * handler (0x1000a86b); 0 for the others, or when the cell is taken */
int32_t __cdecl dest_claim(uint32_t unit, uint32_t x, uint32_t y, int32_t check)
{
    uint8_t *u = UNIT(unit & 0xffff);
    if (G(uint32_t, 0x101545bc + P(int32_t, u, 0xc) * 0x44) != (uint32_t)(uintptr_t)ORIG_PTR(0x1000a86b))
        return 0;
    int vx = (int16_t)x, vy = (int16_t)y, sx, sy;
    int32_t cell = IsoVTilePos(g_iso, &vx, &vy, &sx, &sy);
    uint8_t slot = (uint8_t)(P(uint32_t, u, 0x10e) >> 10 & 3);
    if (check) {
        for (int16_t k = I(int16_t, 0x1066); k >= 0; k = P(int16_t, DEST(k), 9)) {
            if (P(uint16_t, DEST(k), 0) == (unit & 0xffff) || P(int32_t, DEST(k), 2) != cell)
                continue;
            if (P(int32_t, P(uint8_t *, UNIT(unit & 0xffff), 0x32), 0x74) != 1)
                return 0;
            if ((P(uint8_t, DEST(k), 6) & 3) == slot)
                return 0;
        }
        return 1;
    }
    int16_t own = P(int16_t, u, 0x15d);
    if (own >= 0) {
        P(int32_t, DEST(own), 2) = cell;
        P(uint8_t, DEST(own), 6) = (uint8_t)((P(uint8_t, DEST(own), 6) & 0xfc) | slot);
        return 1;
    }
    int16_t k = I(int16_t, 0x1acb);
    P(uint16_t, DEST(k), 0) = (uint16_t)unit;
    P(int32_t, DEST(k), 2) = cell;
    P(uint8_t, DEST(k), 6) = (uint8_t)(slot | (P(uint8_t, DEST(k), 6) & 0xfc));
    I(int16_t, 0x1acb) = P(int16_t, DEST(k), 9);
    P(int16_t, DEST(k), 9) = -1;
    P(int16_t, DEST(k), 7) = I(int16_t, 0x1068);
    P(int16_t, u, 0x15d) = k;
    if (I(int16_t, 0x1068) >= 0)
        P(int16_t, DEST(I(int16_t, 0x1068)), 9) = k;
    else
        I(int16_t, 0x1066) = k;
    I(int16_t, 0x1068) = k;
    return 1;
}

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define formation_spot                                                                       \
    ORIG(int32_t(__cdecl *)(uint32_t, uint32_t, int32_t, uint32_t, int32_t *, int32_t *, int32_t *, int32_t, int32_t), \
         0x100ba61f)
#define unit_move_order                                                                      \
    ORIG(int32_t(__cdecl *)(uint32_t, int32_t, int32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t,     \
                            uint32_t),                                                       \
         0x100b4ba4)
#define unit_dir_from ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t *), 0x100d9cb5)

/* 0x100be4d2: gives up unit `unit`'s claimed destination. With `push_off` (and unless the
 * unit is busy, +0x10e bits 4-5), the units of this side that are standing still with the
 * same cell as their destination (formation types: in the same slot) are sent to a free
 * cell nearby, at least 18 x 10 away from the unit. 0 when it had no destination */
int32_t __cdecl dest_release(uint32_t unit, int32_t push_off)
{
    int32_t first = 1;
    uint8_t *u = UNIT(unit & 0xffff);
    int16_t own = P(int16_t, u, 0x15d);
    if (own < 0)
        return 0;
    int16_t prev = P(int16_t, DEST(own), 7), next = P(int16_t, DEST(own), 9);
    if (prev >= 0) {
        P(int16_t, DEST(prev), 9) = next;
        if (next >= 0)
            P(int16_t, DEST(next), 7) = prev;
    } else {
        if (next >= 0)
            P(int16_t, DEST(next), 7) = -1;
        I(int16_t, 0x1066) = next;
    }
    if (next < 0)
        I(int16_t, 0x1068) = prev;
    int32_t cell = P(int32_t, DEST(P(int16_t, u, 0x15d)), 2);
    P(int16_t, DEST(P(int16_t, u, 0x15d)), 9) = I(int16_t, 0x1acb);
    P(int16_t, DEST(P(int16_t, u, 0x15d)), 7) = -1;
    I(int16_t, 0x1acb) = P(int16_t, u, 0x15d);
    P(int16_t, u, 0x15d) = -1;
    if (!push_off || (P(uint32_t, u, 0x10e) >> 4 & 1) || (P(uint32_t, u, 0x10e) >> 5 & 1))
        return 1;

    for (int16_t k = I(int16_t, 0x1066), nx; k >= 0; k = nx) {
        nx = P(int16_t, DEST(k), 9);
        uint16_t other = P(uint16_t, DEST(k), 0);
        if (!UNIT(other))
            continue;
        if (I(int32_t, 0x175) && !unit_is_local(other))
            continue;
        if (nx == k)
            break;
        if (P(int32_t, DEST(k), 2) != cell)
            continue;
        uint8_t *o = UNIT(other);
        uint32_t of = P(uint32_t, o, 0x10e);
        if ((of >> 4 & 1) || (of >> 6 & 1) || (of >> 5 & 1))
            continue;
        if (P(int16_t, o, 0x11b) > 0 && P(int16_t, VEC(P(int16_t, o, 0x11b)), 0xe) > 0)
            continue;
        if (P(uint8_t, o, 0x196) == 0x10 || P(uint8_t, o, 0x196) == 0x42)
            continue;
        if (P(int32_t, P(uint8_t *, u, 0x32), 0xb0)) {
            if ((P(uint8_t, DEST(k), 6) & 3) != (P(uint32_t, u, 0x10e) >> 10 & 3))
                continue;
            if (!unit_is_local(other))
                continue;
        }
        int32_t x = P(int16_t, o, 0x70), y = P(int16_t, o, 0x72), at, dir = 0;
        if (P(uint16_t, o, 0x17d)) {
            unit_dir_from(other, P(uint16_t, o, 0x17d), &dir);
            dir = G(int32_t, 0x10157350 + dir * 4);
        }
        uint32_t slot = P(uint32_t, o, 0x10e) >> 10 & 3;
        /* (the original also works out the unit's speed here and never uses it) */
        int done = 0;
        while (!done) {
            x = P(int16_t, o, 0x70);
            y = P(int16_t, o, 0x72);
            P(uint32_t, o, 0x10e) = (P(uint32_t, o, 0x10e) & 0xfffff3ff) | (slot & 3) << 10;
            uint32_t d = G(uint32_t, 0x10158c68 + dir * 4);
            if (!formation_spot(other, !first, 1, d, &x, &y, &at, 0, 1) &&
                !formation_spot(other, !first, 1, d, &x, &y, &at, 1, 1)) {
                done = 1;
                continue;
            }
            int dx = x - P(int16_t, u, 0x52), dy = y - P(int16_t, u, 0x54);
            if ((dx < 0 ? -dx : dx) < 0x12 && (dy < 0 ? -dy : dy) < 0xa)
                continue;
            first = 0;
            if (dest_claim(other, x, y, 1))
                done = 1;
        }
        unit_move_order(other, x, y, 1, 0, 0, 1, 0, 0, 1);
        if (P(int16_t, o, 0x119) > 0) {
            dest_claim(other, x, y, 0);
            P(uint32_t, o, 0x10e) |= 0x80000;
        }
    }
    if (!first)
        formation_spot(0, 2, 0, 0, 0, 0, 0, 0, 0);
    return 1;
}
