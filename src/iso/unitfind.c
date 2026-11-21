/* The game logic, 0x100b72e6-0x100b7a66: finding a unit's way to a point and putting it
 * on that way (the found path is a chain of vectors, iso+0x1984, 0x12 bytes each: +4/+6
 * the point, +8 a count, +9 a direction, +0xa flags, +0xe the next; +0x115 a path found
 * ahead of time). */
#include "iso_core.h"
#include "sector.h"
#include "unitfind.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define F (P(uint32_t, u, 0x10e))
#define cell_lower ORIG(void(__cdecl *)(int32_t, int), 0x10067214)
#define vector_new ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t), 0x100e98de)
#define vector_free ORIG(void(__cdecl *)(uint32_t), 0x100e9bbc)
#define net_delay ORIG(int(__cdecl *)(void), 0x1003f390)
#define direction_between ORIG(int(__cdecl *)(int16_t, int16_t, int16_t, int16_t), 0x1009188a)
#define slot_offset ORIG(void(__cdecl *)(uint32_t, int16_t, int32_t *), 0x10091860)
#define path_find ORIG(int32_t(__cdecl *)(int32_t, int32_t, int32_t, int32_t, int16_t *, int, uint32_t, int), 0x100c70bc)
#define path_stop ORIG(int(__cdecl *)(uint32_t, int), 0x100d4947)
#define patrol_point ORIG(int32_t(__cdecl *)(uint32_t), 0x100ea9f5)
#define patrol_path_set ORIG(void(__cdecl *)(uint32_t, int32_t), 0x100eaa98)
#define unit_path_follow ORIG(void(__cdecl *)(uint32_t, int32_t, uint32_t), 0x100e9260)
#define unit_blocked ORIG(int(__cdecl *)(uint32_t), 0x100d2319)
#define unit_moved ORIG(void(__cdecl *)(uint32_t), 0x100cb2b2)
#define IsoUpdateFootprint ORIG(int32_t(__stdcall *)(uint8_t *, uint32_t, int, int32_t), 0x100ef91a)

/* puts waypoint `k` on the middle of its map cell (+0x20, +0x10), moved by the unit's
 * place in a formation (bits 10-11, for types with +0xb0) */
static void waypoint_centre(uint8_t *u, int16_t k, int32_t px, int32_t py)
{
    if (P(int32_t, P(uint8_t *, u, 0x32), 0xb0)) {
        int32_t off[2];
        slot_offset(0, (int16_t)(F >> 10 & 3), off);
        P(int16_t, VEC(k), 4) = (int16_t)((int16_t)off[0] + px + 0x20);
        P(int16_t, VEC(k), 6) = (int16_t)((int16_t)off[1] + py + 0x10);
    } else {
        P(int16_t, VEC(k), 4) = (int16_t)(px + 0x20);
        P(int16_t, VEC(k), 6) = (int16_t)(py + 0x10);
    }
}

/* 0x100b72e6: finds unit `unit`'s way to (tx, ty) and sets it going; `patrol` starts from
 * the patrol's point (for a unit on patrol, +0x113), `reuse` takes the path found ahead of
 * time (+0x115, otherwise dropped). `flag` goes on to the unit's path. Above 0 when it is
 * on its way, otherwise what the path finder returned; with -0x32 (no way) a formation
 * type still turns to face the point */
int32_t __cdecl unit_find_path(uint32_t unit, int32_t tx, int32_t ty, int32_t patrol, int32_t reuse, int32_t a6,
                               uint32_t flag)
{
    (void)a6;
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    if (!u)
        return 0;
    if (!reuse && P(int16_t, u, 0x115) > 0) {
        int16_t k = P(int16_t, u, 0x115);
        while (k > 0 && P(int8_t, VEC(k), 8) > 0) {
            if (I(int32_t, 0xe113)) {
                int vx = P(int16_t, VEC(k), 4), vy = P(int16_t, VEC(k), 6), sx, sy;
                int32_t cell = IsoVTilePos(g_iso, &vx, &vy, &sx, &sy);
                if (!(P(uint16_t, VEC(k), 0xa) >> 9 & 1))
                    cell_lower(cell, 2);
            }
            int16_t done = k;
            k = P(int16_t, VEC(done), 0xe);
            /* the original passes the vector with stack leftovers above it */
            vector_free((uint16_t)done);
        }
        P(int16_t, u, 0x115) = 0;
    }
    int32_t sx, sy;
    if (patrol && P(int16_t, u, 0x113) > 0) {
        int32_t r = patrol_point(unit);
        sx = P(int16_t, VEC(r), 4);
        sy = P(int16_t, VEC(r), 6);
    } else {
        sx = P(int16_t, u, 0x52);
        sy = P(int16_t, u, 0x54);
    }
    if (!patrol && P(int16_t, u, 0x113) > 0)
        path_stop(unit, -1);

    int16_t k = 0;
    int32_t result;
    if (reuse && P(int16_t, u, 0x115) > 0) {
        k = P(int16_t, u, 0x115);
        P(int16_t, u, 0x115) = 0;
        result = 1;
        P(int16_t, u, 0x370) = -1;
    } else {
        /* a digging unit's own building footprint is lifted while the way is found */
        int lift = P(uint16_t, u, 0x17d) > 0 && (P(uint8_t, u, 0x196) == 0x42 || P(uint8_t, u, 0x196) == 0x41);
        if (lift)
            IsoUpdateFootprint(g_iso, P(uint16_t, u, 0x17d), 0, 0);
        result = path_find(sx, sy, tx, ty, &k, 1, unit, 0);
        if (P(uint16_t, u, 0x17d) > 0 && (P(uint8_t, u, 0x196) == 0x42 || P(uint8_t, u, 0x196) == 0x41))
            IsoUpdateFootprint(g_iso, P(uint16_t, u, 0x17d), 1, 0);
    }

    if (result > 0) {
        for (int16_t c = k; c > 0; c = P(int16_t, VEC(c), 0xe)) {
            int vx = P(int16_t, VEC(c), 4), vy = P(int16_t, VEC(c), 6), px, py;
            IsoVTilePos(g_iso, &vx, &vy, &px, &py);
            waypoint_centre(u, c, px, py);
        }
    } else if (P(int32_t, P(uint8_t *, u, 0x32), 0xb0) && result == -0x32) {
        int vx = tx, vy = ty, px, py;
        IsoVTilePos(g_iso, &vx, &vy, &px, &py);
        k = (int16_t)vector_new(((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint8_t, u, 0x14), P(uint32_t, u, 0xc));
        if (k == -1)
            return result;
        waypoint_centre(u, k, px, py);
        if (P(int16_t, VEC(k), 4) == sx && P(int16_t, VEC(k), 6) == sy) {
            P(uint8_t, VEC(k), 8)++;
            vector_free((uint16_t)k);
            return result;
        }
        P(uint8_t, VEC(k), 9) = (uint8_t)direction_between((int16_t)sx, (int16_t)sy, P(int16_t, VEC(k), 4),
                                                           P(int16_t, VEC(k), 6));
        P(uint16_t, VEC(k), 0xa) |= 0x20;
        result = 1;
    }

    if (result > 0) {
        if (!patrol || P(int16_t, u, 0x113) <= 0) {
            if (I(int32_t, 0x175))
                P(uint32_t, VEC(k), 0) = net_delay() + I(uint32_t, 0x17dd);
            unit_path_follow(unit, k, flag);
            if (!I(int32_t, 0x175) || unit_is_local(unit)) {
                P(uint16_t, u, 0x1cd) &= 0xfffe;
                if (unit_blocked(unit))
                    result = 0;
            }
        } else {
            patrol_path_set(unit, k);
        }
        if (result > 0)
            unit_moved(unit & 0xffff);
    }
    return result;
}
