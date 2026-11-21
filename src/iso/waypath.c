/* The game logic, 0x100b54c7-0x100b5897: putting a new stretch of waypoints into a unit's
 * path, and dropping the waypoints of a saved path (the waypoints are vectors, iso+0x1984,
 * 0x12 bytes each: +4/+6 the point, +8 a count, +0xa flags, +0xc the spread, +0xe the
 * next; a unit's path: +0x119 its first, +0x11d/+0x11f the saved ones). */
#include "iso_core.h"
#include "waypath.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define path_build ORIG(int32_t(__cdecl *)(uint32_t, int32_t, int32_t, int32_t, int32_t, int, int, int), 0x1008fc3d)
#define path_put_after ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t), 0x100d51bb)
#define path_append ORIG(void(__cdecl *)(uint32_t, int32_t), 0x100d4e05)
#define cell_lower ORIG(void(__cdecl *)(int32_t, int), 0x10067214)
#define vector_free ORIG(void(__cdecl *)(uint32_t), 0x100e9bbc)
#define vector_link ORIG(void(__cdecl *)(int32_t, int32_t, int), 0x100d4450)

/* 0x100b54c7: builds the waypoints from (x, y) to waypoint `to`'s point and puts them into
 * unit `unit`'s path: `how` 1 after waypoint `after`, otherwise at the path's end (`how` 0
 * finds that end itself; 3 marks every waypoint "last"). The new waypoints take `spread`.
 * The first new waypoint, 0 when none were made, or -1 without a map (+0xe113) */
int32_t __cdecl path_insert(uint32_t unit, int32_t x, int32_t y, int32_t after, int32_t to, uint32_t how,
                            uint32_t spread)
{
    if (!I(int32_t, 0xe113))
        return -1;
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    if (!(how & 0xffff)) {
        int32_t k = P(int16_t, u, 0x119);
        while (k > 0 && P(int16_t, VEC(k), 0xe) > 0)
            k = P(int16_t, VEC(k), 0xe);
        after = k;
    }
    int32_t first = path_build(unit, x, y, P(uint16_t, VEC(to), 4), P(uint16_t, VEC(to), 6), (how & 0xffff) == 3, 0,
                               P(uint16_t, VEC(to), 0xa) >> 8 & 1);
    if (first > 0) {
        P(uint16_t, VEC(first), 0xc) = (uint16_t)spread;
        /* the original walks to the stretch's last waypoint and does nothing with it */
        int32_t k = first;
        while (P(uint16_t, VEC(k), 0xa) & 1)
            k = P(int16_t, VEC(k), 0xe);
        if ((how & 0xffff) == 1)
            path_put_after(unit, after, first);
        else
            path_append(unit, first);
    }
    return first > 0 ? first : 0;
}

/* 0x100b56aa: drops the waypoints of unit `unit`'s saved path (+0x11f), up to the first one
 * without a count; those flagged bit 1 only when `all`. A dropped waypoint gives back its
 * map cell's mark (unless flagged bit 9). With nothing left, +0x11d becomes -1 */
void __cdecl saved_path_drop(uint32_t unit, int32_t all)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    if (!u)
        return;
    int16_t kept = -1;
    int16_t k = P(int16_t, u, 0x11f);
    while (k > 0 && P(int8_t, VEC(k), 8) > 0) {
        int16_t cur = k;
        k = P(int16_t, VEC(cur), 0xe);
        if ((P(uint16_t, VEC(cur), 0xa) >> 1 & 1) && !all) {
            kept = cur;
            continue;
        }
        if (I(int32_t, 0xe113)) {
            int vx = P(int16_t, VEC(cur), 4), vy = P(int16_t, VEC(cur), 6), sx, sy;
            int32_t cell = IsoVTilePos(g_iso, &vx, &vy, &sx, &sy);
            if (!(P(uint16_t, VEC(cur), 0xa) >> 9 & 1))
                cell_lower(cell, 1);
        }
        /* the original passes the waypoint with stack leftovers above it */
        vector_free((uint16_t)cur);
        if (kept <= 0)
            P(int16_t, u, 0x11f) = k;
        else
            vector_link(kept, k, 1);
    }
    if (P(int16_t, u, 0x11f) <= 0)
        P(int16_t, u, 0x11d) = -1;
}
