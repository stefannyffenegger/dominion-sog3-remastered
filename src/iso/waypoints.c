/* The game logic, 0x100b4683-0x100b4ba4: suspending and resuming a unit's waypoint path
 * (the waypoints are vectors, iso+0x1984: +4/+6 the point, +8 a count, +9 a byte, +0xa
 * flags; a unit's path: +0x119 its first, +0x11b the current, +0x11d/+0x11f the saved ones,
 * +0x10e bit 1 suspended). */
#include "iso_core.h"
#include "sector.h"
#include "waypoints.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define unit_path_set ORIG(void(__cdecl *)(uint32_t, int32_t, uint32_t, int), 0x100d559a)
#define unit_path_join ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t, int), 0x100d4bfa)
#define vector_new ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t), 0x100e98de)
#define vector_link ORIG(void(__cdecl *)(int32_t, int32_t, int), 0x100d4450)
#define unit_move ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t, int, int, int, int, int, int, uint32_t), 0x100b4ba4)

/* `suspend` 0 resumes the saved path (moving on to its current waypoint), else the path is
 * saved and the unit sent along a one-point path to its current waypoint. In a network game
 * only for the local player's units (or when `forced`). 1, or 0 when nothing was done */
int32_t __cdecl unit_path_suspend(uint32_t unit, int32_t suspend, int32_t forced)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    if (!u)
        return 0;
    if (I(int32_t, 0x175) && !unit_is_local(unit) && !forced)
        return 0;
    if (!suspend) {
        if (!(P(uint32_t, u, 0x10e) >> 1 & 1))
            return 1;
        if (P(int16_t, u, 0x11f) < 1) {
            P(uint32_t, u, 0x10e) &= 0xfffffffd;
            return 1;
        }
        P(uint32_t, u, 0x10e) &= 0xfffffffd;
        int go = P(int16_t, u, 0x11b) >= 1 && P(int16_t, u, 0x113) >= 1;
        int32_t at = P(int16_t, u, 0x113) < 1 ? -1 : P(int16_t, u, 0x11b);
        unit_path_set(unit, at, P(int16_t, u, 0x113) > 0, 0);
        int16_t was = P(int16_t, u, 0x11b);
        P(int16_t, u, 0x11b) = P(int16_t, u, 0x11d);
        P(int16_t, u, 0x119) = P(int16_t, u, 0x11f);
        P(int16_t, u, 0x11d) = 0;
        P(int16_t, u, 0x11f) = 0;
        if (was > 0) {
            unit_path_join(unit, P(int16_t, u, 0x11b), was, 1);
            P(int16_t, u, 0x11b) = was;
        }
        if (go && P(int16_t, u, 0x11b) < 1)
            P(int16_t, u, 0x11b) = P(int16_t, u, 0x119);
        if (go) {
            uint8_t *v = VEC(P(int16_t, u, 0x11b));
            unit_move(unit, P(int16_t, v, 4), P(int16_t, v, 6), 1, 0, 0, 1, 0, 0, P(uint16_t, v, 0xa) >> 8 & 1);
        } else if (P(int16_t, u, 0x11b) < 1) {
            if (P(int16_t, u, 0x119) > 0)
                P(uint32_t, u, 0x10e) |= 0x4000;
        } else {
            P(uint32_t, u, 0x10e) |= 0x2000000;
            P(uint32_t, u, 0x10e) |= 0x4000;
        }
        return 1;
    }
    if (P(uint32_t, u, 0x10e) >> 1 & 1)
        return 1;
    if (P(int16_t, u, 0x119) < 1) {
        P(uint32_t, u, 0x10e) &= 0xfffffffd;
        return 1;
    }
    P(uint32_t, u, 0x10e) |= 2;
    if (P(int16_t, u, 0x11b) < 1) {
        unit_path_set(unit, -1, 0, 0);
        P(int16_t, u, 0x11f) = P(int16_t, u, 0x119);
        P(int16_t, u, 0x11d) = -1;
        P(int16_t, u, 0x119) = -1;
        return 0;
    }
    /* the original passes the player byte with the record pointer's upper half above it */
    int16_t n = (int16_t)vector_new(((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint8_t, u, 0x14), P(uint32_t, u, 0xc));
    if (n > 0) {
        uint8_t *cur = VEC(P(int16_t, u, 0x11b));
        P(uint8_t, VEC(n), 8)++;
        P(int16_t, VEC(n), 4) = P(int16_t, cur, 4);
        P(int16_t, VEC(n), 6) = P(int16_t, cur, 6);
        P(uint8_t, VEC(n), 9) = P(uint8_t, cur, 9);
        vector_link(n, -1, 1);
        unit_path_set(unit, -1, 0, 0);
        P(int16_t, u, 0x11f) = P(int16_t, u, 0x119);
        P(int16_t, u, 0x11d) = P(int16_t, u, 0x11b);
        P(int16_t, u, 0x11b) = n;
        P(int16_t, u, 0x119) = P(int16_t, u, 0x11b);
    }
    return 1;
}
