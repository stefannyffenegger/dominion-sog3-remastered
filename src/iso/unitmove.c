/* The game logic: a unit's move order (0x100b4ba4-0x100b54c7). The target point becomes a
 * new waypoint (a vector, iso+0x1984: +4/+6 the point, +8 a count, +0xa flags, +0xc the
 * spread, +0xe the next); `how` 0 replaces the path, 1 adds the point after the current
 * waypoint, 2 starts a patrol, 3 queues the point at the path's end. Units with a patrol
 * (+0x113) get it through 0x100b7a66 instead. 1 when the order was taken. */
#include "iso_core.h"
#include "sector.h"
#include "waypoints.h"
#include "unitmove.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define vector_new ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t), 0x100e98de)
#define vector_free ORIG(void(__cdecl *)(uint32_t), 0x100e9bbc)
#define patrol_order ORIG(int32_t(__cdecl *)(uint32_t, uint32_t, int, int, int, int, int32_t, int), 0x100b7a66)
#define path_insert ORIG(uint32_t(__cdecl *)(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t), 0x100b54c7)
#define path_append ORIG(void(__cdecl *)(uint32_t, int32_t), 0x100d4e05)
#define path_after ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t), 0x100d4f51)
#define unit_path_set ORIG(void(__cdecl *)(uint32_t, int32_t, uint32_t, int), 0x100d559a)
#define path_start ORIG(int32_t(__cdecl *)(uint32_t, uint32_t, int32_t, int), 0x100b5897)

static void copy_flag(uint8_t *to, const uint8_t *from, int bit)
{
    P(uint16_t, to, 0xa) = (uint16_t)(((P(uint16_t, from, 0xa) >> bit & 1) << bit) | (P(uint16_t, to, 0xa) & ~(1u << bit)));
}

int32_t __cdecl unit_move_order(uint32_t unit, int16_t x, int16_t y, uint32_t how_arg, uint32_t spread_arg, uint32_t a6,
                                uint32_t a7, int32_t a8, int32_t forced, uint16_t flag)
{
    /* compared as 16-bit values, but passed on whole as the original does */
    int16_t how = (int16_t)how_arg, spread = (int16_t)spread_arg;
    (void)a6;
    (void)a7;
    int16_t result = -1;
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    if (!u || (P(int32_t, u, 0xc) >= 0x30 && P(int32_t, u, 0xc) <= 0x37))
        return 0;
    if (I(int32_t, 0x175) && !unit_is_local(unit) && !forced)
        return 1;
    if ((P(uint32_t, u, 0x10e) >> 12 & 1) || (P(uint32_t, u, 0x10e) >> 4 & 1) || (P(uint32_t, u, 0x10e) >> 20 & 1))
        return 0;
    if ((P(uint32_t, u, 0x10e) >> 1 & 1) &&
        (P(uint8_t, u, 0x196) == 0xf || P(uint8_t, u, 0x196) == 0xa || P(uint8_t, u, 0x196) == 0xd))
        unit_path_suspend(unit, 0, 0);
    /* the original passes the player byte with the record pointer's upper half above it */
    int16_t v = (int16_t)vector_new(((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint8_t, u, 0x14), P(uint32_t, u, 0xc));
    if (v == -1)
        return 0;
    P(uint8_t, VEC(v), 8)++;
    P(int16_t, VEC(v), 4) = x;
    P(int16_t, VEC(v), 6) = y;
    P(uint16_t, VEC(v), 0xa) = (uint16_t)((P(uint16_t, VEC(v), 0xa) & 0xfeff) | (flag & 1) << 8);
    if (P(int16_t, u, 0x113) > 0 && how && how != 3) {
        /* a unit on patrol */
        if (!patrol_order(unit, (uint16_t)v, how == 2, 0, 0, 0, forced, how == 2)) {
            vector_free((uint16_t)v);
            return 0;
        }
        return 1;
    }
    if (P(int16_t, u, 0x119) < 1 || (how && how != 3)) {
        if (how == 2)
            unit_path_set(unit, -1, 1, 0);
        if (how == 1) {
            int16_t cur = P(int16_t, u, 0x11b);
            if (cur > 0) {
                copy_flag(VEC(v), VEC(cur), 6);
                copy_flag(VEC(v), VEC(cur), 1);
                copy_flag(VEC(v), VEC(cur), 8);
            }
            P(int16_t, VEC(v), 0xc) = spread;
            path_after(unit, P(int16_t, u, 0x11b), v);
            if (P(int16_t, u, 0x11b) > 0)
                P(uint32_t, u, 0x10e) |= 0x2000000;
        } else {
            /* from the end of the current path (or the unit) to the point */
            int16_t last = P(int16_t, u, 0x11b), n = last, px, py;
            while (n > 0) {
                last = n;
                n = P(int16_t, VEC(n), 0xe);
            }
            if (last < 1) {
                px = P(int16_t, u, 0x52);
                py = P(int16_t, u, 0x54);
            } else {
                px = P(int16_t, VEC(last), 4);
                py = P(int16_t, VEC(last), 6);
            }
            result = (int16_t)path_insert(unit, px, py, last, v, how_arg, spread_arg);
            if (result < 1) {
                if (result < 0) {
                    path_append(unit, v);
                    P(int16_t, VEC(v), 0xc) = spread;
                }
            } else {
                vector_free((uint16_t)v);
                v = result;
            }
        }
        if (P(int16_t, u, 0x119) < 1)
            P(int16_t, u, 0x119) = v;
        if (how == 3)
            P(uint16_t, VEC(v), 0xa) |= 2;
        if (P(int16_t, u, 0x113) < 1)
            P(uint32_t, u, 0x10e) |= 0x4000;
    } else {
        /* queued after the path's last waypoint */
        int16_t last = P(int16_t, u, 0x119), n;
        while ((n = P(int16_t, VEC(last), 0xe)) > 0)
            last = n;
        result = (int16_t)path_insert(unit, P(int16_t, VEC(last), 4), P(int16_t, VEC(last), 6), P(int16_t, u, 0x11b), v,
                                      how_arg, spread_arg);
        if (result < 1) {
            if (result < 0) {
                if (how == 3)
                    P(uint16_t, VEC(v), 0xa) |= 2;
                P(int16_t, VEC(v), 0xc) = spread;
                path_append(unit, v);
                result = v;
            }
        } else {
            vector_free((uint16_t)v);
        }
    }
    if (!I(uint8_t *, 0xe113)) {
        if ((P(uint32_t, u, 0x10e) >> 14 & 1) && !spread && path_start(unit, how == 1, a8, 0) < 1)
            return 0;
    } else {
        if (result < 1 && how != 1) {
            if (P(int16_t, u, 0x119) > 0)
                vector_free(P(uint16_t, u, 0x119));
            P(int16_t, u, 0x119) = 0;
            return 0;
        }
        if ((P(uint32_t, u, 0x10e) >> 14 & 1) && !spread && path_start(unit, how == 1, a8, 0) < 1)
            return 0;
    }
    return 1;
}
