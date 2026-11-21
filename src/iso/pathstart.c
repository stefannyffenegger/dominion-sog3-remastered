/* The game logic, 0x100b5897-0x100b6e66: moving a unit on to its next waypoint (the
 * original's names: +0x119 m_firstWayPoint, +0x11b m_curWayPoint, +0x177 pCrashCount,
 * +0x16f the retry time; a waypoint's +0 dwTimeStamp; iso+0x17dd GameClock). The waypoints
 * are vectors, iso+0x1984, 0x12 bytes each: +4/+6 the point, +9 a direction, +0xa flags,
 * +0xc a wait, +0xe the next. */
#include "iso_core.h"
#include "sector.h"
#include "status.h"
#include "pathstart.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define F (P(uint32_t, u, 0x10e))
#define CUR (P(int16_t, u, 0x11b))
#define FIRST (P(int16_t, u, 0x119))
#define cell_lower ORIG(void(__cdecl *)(int32_t, int), 0x10067214)
#define vector_new ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t), 0x100e98de)
#define vector_free ORIG(void(__cdecl *)(uint32_t), 0x100e9bbc)
#define vector_link ORIG(void(__cdecl *)(int32_t, int32_t, int), 0x100d4450)
#define net_delay ORIG(int(__cdecl *)(void), 0x1003f390)
#define direction_between ORIG(int(__cdecl *)(int16_t, int16_t, int16_t, int16_t), 0x1009188a)
#define unit_path_follow ORIG(void(__cdecl *)(uint32_t, int32_t, uint32_t), 0x100e9260)
#define unit_find_path ORIG(int32_t(__cdecl *)(uint32_t, int32_t, int32_t, int, int32_t, int32_t, uint32_t), 0x100b72e6)
#define unit_path_set ORIG(void(__cdecl *)(uint32_t, int32_t, uint32_t, int), 0x100d559a)
#define unit_hold ORIG(void(__cdecl *)(uint32_t, int), 0x100db98e)
#define unit_idle ORIG(void(__cdecl *)(uint8_t *), 0x100d2984)
#define player_is_ai ORIG(int(__cdecl *)(int), 0x100e2df0)
#define ai_path_failed ORIG(void(__cdecl *)(uint32_t, uint32_t, int16_t, int16_t), 0x100daa82)
#define ai_event ORIG(void(__cdecl *)(uint32_t, int, uint32_t, int), 0x100e42a6)
#define unit_set_target ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100dcabd)
#define unit_set_state ORIG(void(__cdecl *)(uint32_t, int), 0x1009ce9f)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)
#define unit_stop_attack ORIG(void(__cdecl *)(uint32_t), 0x100dbcbb)
#define patrol_check ORIG(int32_t(__cdecl *)(uint8_t *), 0x100b31e0)
#define fn_100b95dd ORIG(void(__cdecl *)(uint32_t), 0x100b95dd)
#define fn_100be084 ORIG(void(__cdecl *)(uint32_t, uint32_t, uint32_t, int), 0x100be084)
#define net_send_patrol ORIG(void(__cdecl *)(uint32_t, int), 0x1003254c)
#define occ_unit_raise ORIG(void(__cdecl *)(uint32_t, int), 0x10067316)

/* gives back the map cell's mark of waypoint `k` (unless it has none: flag bit 9) */
static void waypoint_unmark(int32_t k)
{
    if (!I(int32_t, 0xe113) || (P(uint16_t, VEC(k), 0xa) >> 9 & 1))
        return;
    int vx = P(int16_t, VEC(k), 4), vy = P(int16_t, VEC(k), 6), sx, sy;
    cell_lower(IsoVTilePos(g_iso, &vx, &vy, &sx, &sy), 1);
}

/* the original passes a word with the record pointer's upper half above it */
static uint32_t hi(const void *p, uint16_t v)
{
    return ((uint32_t)(uintptr_t)p & 0xffff0000) | v;
}

/* the AI's and player's messages when a recon or guard unit finds no route */
static int tells_player(uint8_t *u)
{
    return I(int32_t, 0x175) || !player_is_ai(P(uint8_t, u, 0x14));
}

static int either_is(uint8_t *u, int a)
{
    return P(uint8_t, u, 0x196) == a || P(uint8_t, u, 0x197) == a;
}

/* 0x100b5897: moves unit `unit` on to its next waypoint and finds the way there; `keep`
 * skips a waypoint's wait. Without a path (or still waiting, or retrying within 50 ms) 0;
 * 1 for another player's unit in a network game (unless `forced`). Otherwise what the path
 * finder (0x100b72e6) returned: above 0 when the unit is on its way, -0x46 when it has to
 * wait; when no way is found, the unit tries again (4 times) before it gives up the path */
int32_t __cdecl path_start(uint32_t unit, int32_t keep, int32_t a3, int32_t forced)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    if (!u)
        return 0;
    if (I(int32_t, 0x175) && !unit_is_local(unit) && !forced)
        return 1;
    if ((F >> 2 & 1) && CUR > 0)
        return 0;
    if (F >> 20 & 1)
        return 0;
    if (P(int16_t, u, 0x113) > 0 || P(int32_t, u, 0x149))
        return 0;
    F &= 0xffffbfff;
    if (FIRST <= 0)
        return 0;
    F &= 0xfbffffff;
    if (P(int8_t, u, 0x177) >= 0) {
        uint32_t now = timeGetTime(), elapsed;
        if (P(uint32_t, u, 0x16f) < now)
            elapsed = now - P(uint32_t, u, 0x16f);
        else
            elapsed = 0xffffffff - P(uint32_t, u, 0x16f) + now;
        if (elapsed < 0x32)
            return 0;
    }
    if (CUR <= 0)
        F &= 0xfdffffff;
    int16_t cur = CUR;

    /* step on from the waypoint reached (unless it is still being approached: bit 25) */
    if (!(F >> 25 & 1)) {
        if (CUR <= 0) {
            CUR = FIRST;
        } else if (P(int16_t, VEC(CUR), 0xe) <= 0) {
            if (FIRST == CUR) {
                /* the only waypoint: the path ends */
                if (!(P(uint16_t, VEC(CUR), 0xa) >> 1 & 1)) {
                    waypoint_unmark(CUR);
                    vector_free(hi(u, (uint16_t)CUR));
                    FIRST = -1;
                    CUR = FIRST;
                    P(int16_t, u, 0x74) = P(int16_t, u, 0x52);
                    P(int16_t, u, 0x76) = P(int16_t, u, 0x54);
                } else {
                    CUR = -1;
                }
                P(uint8_t, u, 0x195) &= 0xfd;
                return 0;
            }
            /* the last waypoint: drop it (unless kept, bit 1) and start over */
            if (!(P(uint16_t, VEC(CUR), 0xa) >> 1 & 1)) {
                int16_t prev = -1, k = FIRST;
                while (k > 0 && P(int16_t, VEC(k), 0xe) > 0) {
                    prev = k;
                    k = P(int16_t, VEC(k), 0xe);
                }
                if (prev > 0)
                    vector_link(prev, -1, 0);
                waypoint_unmark(cur);
                vector_free((uint16_t)cur);
            }
            CUR = FIRST;
        } else if (P(uint16_t, VEC(CUR), 0xa) >> 1 & 1) {
            CUR = P(int16_t, VEC(CUR), 0xe);
        } else {
            /* drop the waypoint reached */
            int16_t prev = -1, k = FIRST;
            while (k > 0 && CUR != k) {
                prev = k;
                k = P(int16_t, VEC(k), 0xe);
            }
            if (prev > 0)
                vector_link(prev, P(int16_t, VEC(CUR), 0xe), 1);
            k = P(int16_t, VEC(CUR), 0xe);
            if (FIRST == CUR) {
                FIRST = k;
                CUR = FIRST;
            } else {
                CUR = k;
            }
            waypoint_unmark(cur);
            vector_free((uint16_t)cur);
        }
    }

    F &= 0xfff7ffff;
    uint8_t *w = VEC(CUR);
    int dx = P(int16_t, u, 0x52) - P(int16_t, w, 4), dy = P(int16_t, u, 0x54) - P(int16_t, w, 6);
    if ((dx < 0 ? -dx : dx) < 0x10 && (dy < 0 ? -dy : dy) < 8) {
        F |= 0x4000;
        F &= 0xfdffffff;
        return 0;
    }
    if (!keep && P(uint16_t, VEC(CUR), 0xc) && !P(uint32_t, VEC(CUR), 0)) {
        /* wait there first */
        P(uint32_t, VEC(CUR), 0) = P(uint16_t, VEC(CUR), 0xc) + I(uint32_t, 0x17dd);
        F |= 0x4000;
        F |= 0x2000000;
        return 0;
    }
    P(uint32_t, VEC(CUR), 0) = 0;
    int32_t tx = P(int16_t, VEC(CUR), 4), ty = P(int16_t, VEC(CUR), 6);
    int16_t k = CUR;
    while (k > 0 && (P(uint16_t, VEC(k), 0xa) & 1))
        k = P(int16_t, VEC(k), 0xe);
    if (k > 0) {
        P(int16_t, u, 0x70) = P(int16_t, VEC(k), 4);
        P(int16_t, u, 0x72) = P(int16_t, VEC(k), 6);
    } else {
        P(int16_t, u, 0x70) = P(int16_t, VEC(CUR), 4);
        P(int16_t, u, 0x72) = P(int16_t, VEC(CUR), 6);
    }
    int32_t ai_unit = P(uint16_t, u, 0x1cd) >> 5 & 1;
    int16_t ai_x = -1, ai_y = -1;
    if (ai_unit == 1 && player_is_ai(P(uint8_t, u, 0x14))) {
        ai_x = P(int16_t, u, 0x386);
        ai_y = P(int16_t, u, 0x388);
    }

    int32_t result;
    if (P(uint16_t, VEC(CUR), 0xa) >> 7 & 1) {
        /* a turn on the spot: a one-point vector with the direction to face */
        int16_t nv = (int16_t)vector_new(hi(u, P(uint8_t, u, 0x14)), P(uint32_t, u, 0xc));
        if (nv > 0) {
            P(int16_t, VEC(nv), 4) = P(int16_t, VEC(CUR), 4);
            P(int16_t, VEC(nv), 6) = P(int16_t, VEC(CUR), 6);
            P(uint8_t, VEC(nv), 9) = (uint8_t)direction_between(P(int16_t, u, 0x52), P(int16_t, u, 0x54),
                                                                P(int16_t, VEC(nv), 4), P(int16_t, VEC(nv), 6));
            if (I(int32_t, 0x175))
                P(uint32_t, VEC(nv), 0) = net_delay() + I(uint32_t, 0x17dd);
            F |= 0x10;
            unit_path_follow(unit, nv, P(uint16_t, VEC(nv), 0xa) >> 8 & 1);
            result = 1;
        } else {
            result = 0;
        }
    } else {
        result = unit_find_path(unit, tx, ty, 0, a3, forced, P(uint16_t, VEC(CUR), 0xa) >> 8 & 1);
    }

    if (result == -0x46) {
        F |= 0x2000000;
        F |= 0x4000;
        return result;
    }
    if (result <= 0 && ai_unit) {
        /* an AI unit gives up at once and tells the AI */
        unit_path_set(unit, -1, 1, 0);
        P(int16_t, u, 0x74) = P(int16_t, u, 0x52);
        P(int16_t, u, 0x76) = P(int16_t, u, 0x54);
        if (F & 1)
            unit_hold(unit, 1);
        unit_idle(u);
        F &= 0xfdffffff;
        P(int8_t, u, 0x177) = -1;
        P(uint32_t, u, 0x16f) = 0;
        if (player_is_ai(P(uint8_t, u, 0x14)))
            ai_path_failed(unit, P(uint32_t, u, 0x382), ai_x, ai_y);
        return result;
    }
    if (result <= 0) {
        if (CUR > 0) {
            if (P(int8_t, u, 0x177) == 0 || (result == -0x1e && P(uint8_t, u, 0x196) != 0x10)) {
                /* "Freeing Path %d" (the debug output is compiled out) */
                if (CUR > 0 && (P(int16_t, VEC(CUR), 0xe) > 0 || FIRST != CUR) && P(int8_t, u, 0x178) < 1) {
                    /* skip this waypoint (once) */
                    F |= 0x4000;
                    waypoint_unmark(CUR);
                    P(int8_t, u, 0x178)++;
                } else {
                    unit_path_set(unit, -1, 1, 0);
                    if (F & 1) {
                        unit_hold(unit, 1);
                        if (!I(int32_t, 0x175) && player_is_ai(P(uint8_t, u, 0x14)))
                            ai_event(P(uint8_t, u, 0x14), 0xf, unit & 0xffff, 0);
                    } else if (P(uint16_t, u, 0x17d) > 0) {
                        unit_set_target(unit, 0);
                    }
                    P(int16_t, u, 0x74) = P(int16_t, u, 0x52);
                    P(int16_t, u, 0x76) = P(int16_t, u, 0x54);
                    P(int8_t, u, 0x178) = 0;
                    if ((P(uint8_t, u, 0x196) == 0x29 || P(uint8_t, u, 0x196) == 0x28) && tells_player(u)) {
                        status_text((const char *)ORIG_PTR(0x10158328), 0); /* "No Available Route.  Terminating Auto Recon." */
                        unit_set_action(unit, 0x1e, 1, 0);
                    }
                    if (P(uint8_t, u, 0x196) == 0x1f) {
                        unit_stop_attack(unit);
                        if (tells_player(u))
                            status_text((const char *)ORIG_PTR(0x10158358), 0); /* "... Terminating Guard Mode." */
                    }
                }
                unit_idle(u);
                F &= 0xfdffffff;
                P(int8_t, u, 0x177) = -1;
                P(uint32_t, u, 0x16f) = 0;
            } else {
                /* try again in 50 ticks ("Decrementing Crash Count", compiled out) */
                P(uint32_t, u, 0x16f) = timeGetTime();
                if (!P(uint32_t, u, 0x16f))
                    P(uint32_t, u, 0x16f) = 1;
                if (P(int8_t, u, 0x177) < 0 || (P(int8_t, u, 0x177) == 0 && P(uint8_t, u, 0x196) == 0x10))
                    P(int8_t, u, 0x177) = 4;
                else if (result < 0)
                    P(int8_t, u, 0x177)--;
                if (CUR > 0)
                    P(uint32_t, VEC(CUR), 0) = I(uint32_t, 0x17dd) + 0x32;
                else
                    P(uint32_t, VEC(FIRST), 0) = I(uint32_t, 0x17dd) + 0x32;
                F |= 0x4000;
                F |= 0x2000000;
            }
        }
    } else {
        P(int8_t, u, 0x177) = -1;
        P(int8_t, u, 0x178) = 0;
        P(uint32_t, u, 0x16f) = 0;
        int both_1e = P(uint8_t, u, 0x196) == 0x1e && P(uint8_t, u, 0x197) == 0x1e;
        if (CUR > 0 && (P(uint16_t, VEC(CUR), 0xa) >> 1 & 1)) {
            if (both_1e || either_is(u, 0xa)) {
                unit_set_state(unit, 0x17);
                unit_set_action(unit, 0x17, 1, 0);
            }
        } else if (both_1e || either_is(u, 0x17)) {
            unit_set_state(unit, 0xa);
            unit_set_action(unit, 0xa, 1, 0);
        }
        F &= 0xfdffffff;
        if (!I(int32_t, 0xe113) && P(int16_t, u, 0x370) >= 0 && P(int16_t, u, 0x113) > 0 && patrol_check(u) != 1)
            return 0;
    }

    if (result > 0 && P(int32_t, u, 0xc) >= 0x10 && P(int32_t, u, 0xc) <= 0x13 &&
        (P(uint8_t, u, 0x196) == 0x42 || P(uint8_t, u, 0x196) == 0x41) && P(int16_t, VEC(CUR), 0xe) <= 0)
        fn_100b95dd(unit);
    if (result > 0 && CUR > 0 && !(P(uint16_t, VEC(CUR), 0xa) >> 7 & 1))
        fn_100be084(unit, P(uint16_t, VEC(CUR), 4), P(uint16_t, VEC(CUR), 6), 0);
    if (result > 0 && I(int32_t, 0x175) && P(int16_t, u, 0x113) > 0)
        net_send_patrol(unit & 0xffff, P(uint16_t, u, 0x17d) == 0);
    if (result > 0 && P(int16_t, u, 0x113) > 0 && I(int32_t, 0xe113))
        occ_unit_raise(unit, 2);
    return result;
}
