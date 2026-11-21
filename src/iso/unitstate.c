/* The game logic, 0x100d62a0-0x100d7b3d: one step of a unit's action state machine
 * (+0x196 the state being run, +0x197 the next one). */
#include "iso_core.h"
#include "combat.h"
#include "groups.h"
#include "netflags.h"
#include "patrol.h"
#include "pathrelease.h"
#include "playerinfo.h"
#include "sector.h"
#include "spot.h"
#include "unitstate.h"
#include "unitmove.h"
#include "unitorder.h"
#include "vecscore.h"
#include "weapons.h"

#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])
#define NET I(int32_t, 0x175)
#define F(u) P(uint32_t, u, 0x10e)
#define S193(u) P(uint16_t, u, 0x193)
#define B195(u) P(uint8_t, u, 0x195)
#define CLASS(u) P(int32_t, u, 0x10)
/* a word pushed with the high half of the unit's address, as the original does */
#define HW(u, off) (((uint32_t)(uintptr_t)(u) & 0xffff0000) | P(uint16_t, u, off))
/* a field of the unit's weapon record */
#define WEAPON(u, off)                                                                       \
    P(int32_t, P(uint8_t *, I(uint8_t *, 0x1990), 0),                                        \
      P(int32_t, I(uint8_t *, 0x1990), 0x2c) * P(int16_t, u, 0xe6) * 4 + (off))
#define ACT(a) unit_set_action(id, (a), 1, 0)
#define IDLE_ACT 0x1e

#define fn_100dcabd ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100dcabd)
#define net_40c27 ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t), 0x10040c27)
#define net_40706 ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t), 0x10040706)
#define net_40ec1 ORIG(void(__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t), 0x10040ec1)
#define net_3fe66 ORIG(void(__cdecl *)(uint32_t), 0x1003fe66)
#define net_3e0eb ORIG(int32_t(__cdecl *)(uint32_t, uint32_t, int32_t, int32_t), 0x1003e0eb)
#define fn_100b9c83 ORIG(int32_t(__cdecl *)(uint32_t, uint32_t, int32_t, int32_t, int32_t), 0x100b9c83)
#define fn_1005c64d ORIG(uint16_t(__cdecl *)(int32_t, int32_t, uint32_t, int32_t), 0x1005c64d)
#define fn_1006b343 ORIG(void(__cdecl *)(uint32_t), 0x1006b343)
#define fn_10071766 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x10071766)
#define fn_10071d66 ORIG(int32_t(__cdecl *)(uint32_t, uint32_t), 0x10071d66)
#define fn_10071f48 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x10071f48)
#define fn_100a10e5 ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t, int32_t), 0x100a10e5)
#define fn_100bb729 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100bb729)
#define fn_100bbaa3 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100bbaa3)
#define fn_100bc3cd ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100bc3cd)
#define fn_100bc7df ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100bc7df)
#define fn_100d979f ORIG(int32_t(__cdecl *)(uint32_t, int32_t, int32_t, int32_t, int32_t), 0x100d979f)
#define fn_100d9855 ORIG(int32_t(__cdecl *)(uint32_t, uint32_t), 0x100d9855)
#define fn_100d99ca ORIG(int32_t(__cdecl *)(uint32_t, uint32_t, int32_t), 0x100d99ca)
#define fn_100d9c57 ORIG(int32_t(__cdecl *)(uint32_t, int32_t *), 0x100d9c57)
#define fn_100d9cb5 ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t *), 0x100d9cb5)
#define fn_100dafc3 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100dafc3)
#define fn_100db772 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100db772)
#define fn_100db98e ORIG(void(__cdecl *)(uint32_t, int32_t), 0x100db98e)
#define fn_100dbdae ORIG(void(__cdecl *)(uint8_t *), 0x100dbdae)
#define fn_100dc1e7 ORIG(int32_t(__cdecl *)(uint32_t), 0x100dc1e7)
#define debug_str ORIG(void(__cdecl *)(const char *, int32_t), 0x100f492e)

/* no path left, not turning (+0x195 bit 2), not bit 14 of the flags */
static int finished(const uint8_t *u)
{
    return P(int16_t, u, 0x113) <= 0 && P(int16_t, u, 0x11b) <= 0 && !(P(uint8_t, u, 0x195) & 4) &&
           !(P(uint32_t, u, 0x10e) >> 14 & 1);
}

static int is_group_leader(const uint8_t *u)
{
    return P(int32_t, u, 0x10) >= 0xc && P(int32_t, u, 0x10) <= 0xf && P(uint8_t, u, 0x15) > 0;
}

/* state 0x33: boarding the carrier +0xf4 */
static void state_board(uint32_t id, uint8_t *u, uint8_t *target)
{
    if (NET && !unit_is_local(id))
        return;
    if (target || (P(int32_t, u, 0xc) >= 0x10 && P(int32_t, u, 0xc) <= 0x13)) {
        uint32_t w = P(uint16_t, u, 0xf4);
        uint8_t *c = UNIT(w);
        if (c && P(int32_t, P(uint8_t *, u, 0x32), 0xe4) + P(uint8_t, c, 0xf3) <=
                     P(int32_t, P(uint8_t *, c, 0x32), 0xe0)) {
            RECT r = {P(int16_t, c, 0x52) - 0x20, P(int16_t, c, 0x54) - 0x20, P(int16_t, c, 0x52) + 0x20,
                      P(int16_t, c, 0x54) + 0x20};
            POINT pt = {P(int16_t, u, 0x52), P(int16_t, u, 0x54)};
            if (PtInRect(&r, pt) || (!P(uint8_t, u, 0x147) && P(int16_t, u, 0x113) <= 0 &&
                                     P(int16_t, u, 0x11b) <= 0 && !(F(u) >> 14 & 1))) {
                if (NET)
                    net_3e0eb(id, w, 0, 1);
                else
                    fn_100b9c83(id, w, 0, 0, 1);
            }
        } else {
            if (NET)
                net_40706(id, HW(u, 0xf4), 4);
            fn_10071766(HW(u, 0xf4), id);
            int32_t x = P(int16_t, u, 0x52), y = P(int16_t, u, 0x54), cell = 0;
            formation_spot(id, 3, 1, 0, &x, &y, &cell, 1, 1);
            unit_move_order(id, (int16_t)x, (int16_t)y, 2, 0, 0, 1, 0, 0, 0);
            ACT(IDLE_ACT);
        }
    } else {
        if (P(uint16_t, u, 0xf4) && UNIT(P(uint16_t, u, 0xf4))) {
            if (NET && unit_is_local_u(u))
                net_40706(id, HW(u, 0xf4), 4);
            fn_10071766(HW(u, 0xf4), id);
        }
        ACT(IDLE_ACT);
    }
}

/* state 0x14: ready to fire at the target */
static void state_aim(uint32_t id, uint8_t *u, uint8_t *target, uint32_t tid)
{
    if (P(uint32_t, u, 0x3e) >> 23 & 1) {
        if (NET) {
            net_3fe66(id);
        } else {
            fn_1006b343(id);
            unit_kill(id & 0xffff, 0x20, 1, 0);
            if (!(P(uint32_t, u, 0x3e) >> 30 & 1)) {
                score_unit_killed(CLASS(u), P(uint8_t, u, 0x14), P(uint8_t, u, 0x14));
                P(uint32_t, u, 0x3e) |= 0x40000000;
            }
        }
        return;
    }
    S193(u) &= 0xffef;
    B195(u) &= 0xf9;
    P(uint8_t, u, 0x177) = 0xff;
    P(int16_t, u, 0x70) = P(int16_t, u, 0x52);
    P(int16_t, u, 0x72) = P(int16_t, u, 0x54);
    if (target) {
        if (NET && (!unit_is_local(id) || P(int32_t, u, 0x149) || P(int32_t, u, 0x199)))
            return;
        if (P(int16_t, u, 0x113) > 0)
            return;
        if (!(F(u) >> 23 & 1) && !(F(u) >> 17 & 1) && !(F(u) >> 3 & 1) && P(uint8_t, u, 0x148) != 7)
            unit_order(id, 0x49, 1, 0);
        if (WEAPON(u, 0x1c) > (int32_t)P(uint16_t, u, 0xdc))
            P(uint8_t, u, 0xed) = 0;
        int32_t tmp;
        int32_t in_range = fn_100d9855(id, HW(u, 0x17d));
        int32_t facing = fn_100d9c57(id, &tmp);
        if (P(uint8_t, u, 0xed) && facing && in_range && P(uint8_t, u, 0x148) != 7) {
            unit_set_action(id, 0x18, net_delay(), 0);
            if (NET && unit_is_local(id))
                net_40ec1(id, HW(u, 0x17d), HW(u, 0x183), HW(u, 0x185), 0);
        } else if (!in_range || !facing) {
            if (F(u) >> 23 & 1)
                unit_order(id, 0x4a, 1, 0);
            ACT(0xb);
        } else if (P(int32_t, P(uint8_t *, u, 0x32), 0x28) <= (int32_t)P(uint16_t, u, 0xdc)) {
            P(uint8_t, u, 0xed) = 1;
            P(uint8_t, u, 0xee) = 0;
        }
    } else {
        fn_100db98e(id, 1);
    }
    if (is_group_leader(u))
        group_attack(u, tid);
}

/* state 0x18: firing */
static void state_fire(uint32_t id, uint8_t *u, uint8_t *target, uint32_t tid, int32_t can)
{
    if (P(int32_t, u, 0x149) || P(int32_t, u, 0x199))
        return;
    int32_t tmp, shoot = 0;
    if (target) {
        if (NET && !unit_is_local_u(u))
            shoot = 1;
        else
            shoot = can && fn_100d9855(id, tid) && fn_100d9c57(id, &tmp);
    }
    if (shoot) {
        if (P(uint8_t, u, 0xee)) {
            S193(u) &= 0xffef;
            P(uint8_t, u, 0xee)--;
            return;
        }
        if (F(u) >> 23 & 1)
            unit_order(id, 0x1b, 1, 0);
        else if (F(u) >> 17 & 1)
            unit_order(id, 0x1c, 1, 0);
        else if (F(u) >> 3 & 1)
            unit_order(id, 0x1d, 1, 0);
        else
            ACT(0x14);
        unit_random_point(id, HW(u, 0x17d), &P(int16_t, u, 0x183), &P(int16_t, u, 0x185));
        weapon_fire_at_unit(g_iso, id & 0xffff, P(uint16_t, u, 0x17d), P(int16_t, u, 0x183), P(int16_t, u, 0x185));
        uint8_t *o = UNIT(tid);
        if (!UNIT(P(uint16_t, o, 0x18b)))
            P(uint16_t, o, 0x18b) = (uint16_t)id;
        P(uint8_t, u, 0xee) = (uint8_t)WEAPON(u, 0x20);
    } else {
        P(uint8_t, u, 0xed) = 0;
        ACT(0x14);
    }
    if (WEAPON(u, 0x1c) > (int32_t)P(uint16_t, u, 0xdc))
        ACT(0x14);
    if (is_group_leader(u) && (!NET || unit_is_local_u(u)))
        group_attack(u, tid);
}

/* the common part of state 0x1f: following a unit, or going back to the guard point */
void unit_guard_step(uint32_t id, uint8_t *u)
{
    if (S193(u) >> 3 & 1) {
        int32_t r = WEAPON(u, 4) / 2;
        if (r <= 0xe1)
            r = 0xe1;
        if (!fn_100d979f(id, r, P(int16_t, u, 0x187), P(int16_t, u, 0x189), 1) && !(B195(u) & 2)) {
            int32_t x = P(int16_t, u, 0x187), y = P(int16_t, u, 0x189), cell = 0;
            formation_spot(id, 3, 1, 0, &x, &y, &cell, 0, 0);
            if (!patrol_order(id, (uint32_t)-1, 1, 1, 0, 0, 0, 1)) {
                int32_t on = P(int16_t, u, 0x113) > 0;
                path_release(id, on ? P(int16_t, u, 0x11b) : -1, 1, on);
            }
            if (unit_move_order(id, (int16_t)x, (int16_t)y, 3, 0, 0, 1, 0, 0, 0) > 0)
                B195(u) |= 2;
        } else if (P(int16_t, u, 0x113) <= 0 && P(int16_t, u, 0x11b) <= 0 && !(F(u) >> 14 & 1)) {
            B195(u) &= 0xf9;
        }
    } else {
        uint8_t *o = UNIT(P(uint16_t, u, 0x1ab));
        if (o) {
            S193(o) |= 2;
            if (!fn_100d99ca(id, HW(u, 0x1ab), 1) && !(B195(u) & 2)) {
                int32_t x = P(int16_t, o, 0x52), y = P(int16_t, o, 0x54), cell = 0, dir = 0;
                if (!(S193(u) >> 6 & 1)) {
                    int moving = P(uint8_t, o, 0x196) == 0x17 || P(uint8_t, o, 0x196) == 0xf;
                    x = moving ? P(int16_t, o, 0x52) : P(int16_t, o, 0x70);
                    y = moving ? P(int16_t, o, 0x54) : P(int16_t, o, 0x72);
                }
                fn_100d9cb5(id, HW(u, 0x1ab), &dir);
                dir = G(int32_t, 0x10157350 + dir * 4);
                formation_spot(id, 3, 1, dir, &x, &y, &cell, 1, 0);
                if (unit_move_order(id, (int16_t)x, (int16_t)y, 1, 0, 0, 1, 0, 0, 0) > 0)
                    B195(u) |= 2;
            } else if (P(int16_t, u, 0x113) <= 0 && P(int16_t, u, 0x11b) <= 0 && !(F(u) >> 14 & 1)) {
                B195(u) &= 0xf9;
            }
        } else {
            P(uint16_t, u, 0x1ab) = 0;
            S193(u) &= 0xfffb;
            ACT(IDLE_ACT);
        }
    }
}

/* state 0x1f of this state machine */
static void state_guard(uint32_t id, uint8_t *u)
{
    if (P(uint8_t, u, 0x10e) & 1)
        fn_100db98e(id, 1);
    unit_guard_step(id, u);
    fn_100dbdae(u);
}

/* 0x100d62a0: one step of unit `id`'s state machine. A delay (+0x199) holds it until that
 * tick. Leaving the idle state 0x1e calls the handler of its type. */
void __cdecl unit_ai_state(uint32_t id)
{
    uint8_t *u = UNIT(id & 0xffff);
    if (!u)
        return;
    if (P(uint32_t, u, 0x199)) {
        if (I(uint32_t, 0x17dd) < P(uint32_t, u, 0x199)) {
            if (NET && unit_is_local_u(u) && P(uint8_t, u, 0x197) == 0xb && (S193(u) >> 11 & 1) &&
                P(uint32_t, u, 0x199) - (uint32_t)net_delay() <= I(uint32_t, 0x17dd)) {
                fn_100dcabd(id, HW(u, 0x17f));
                net_40c27(id, HW(u, 0x17f), 0);
                S193(u) &= 0xf7ff;
            }
            return;
        }
        P(uint32_t, u, 0x199) = 0;
    }
    uint32_t tid = P(uint16_t, u, 0x17d);
    uint8_t *target = UNIT(tid);
    int32_t can = unit_can_attack(id, tid, 0, 1);
    uint32_t prev = P(uint8_t, u, 0x196);
    P(uint8_t, u, 0x196) = P(uint8_t, u, 0x197);
    uint8_t state = P(uint8_t, u, 0x196);
    if (prev == 0x1e && state != 0x1e && (CLASS(u) < 0x40 || CLASS(u) > 0x9f)) {
        int32_t k = P(int32_t, P(uint8_t *, u, 0x32), 4);
        if (k >= 0 && G(uint32_t, 0x10118d90 + k * 12))
            ((void(__cdecl *)(uint8_t *))(uintptr_t)G(uint32_t, 0x10118d90 + k * 12))(u);
    }
    switch (state) {
    case 0xa:
    case 0xf:
    case 0x17:
        if (finished(u))
            ACT(IDLE_ACT);
        break;
    case 0xb: {
        int32_t ready = fn_100dc1e7(id);
        if (target && can && ready) {
            if (S193(u) >> 5 & 1)
                fn_100db772(id, tid);
            else
                fn_100dafc3(id, tid);
        } else {
            fn_100db98e(id, 1);
        }
        break;
    }
    case 0xc:
        if (finished(u)) {
            if (target) {
                ACT(0xb);
                if (NET && unit_is_local(id))
                    net_40c27(id, HW(u, 0x17d), 0);
            } else {
                ACT(IDLE_ACT);
            }
        }
        break;
    case 0xd:
        fn_100a10e5(id, 0, 0, 1);
        ACT(0xf);
        break;
    case 0x10:
        if (finished(u)) {
            S193(u) &= 0xff7f;
            uint16_t w = fn_1005c64d(P(int16_t, u, 0x52), P(int16_t, u, 0x54), P(uint8_t, u, 0x14), 1);
            if (w) {
                P(uint16_t, u, 0x17d) = w;
                if (!P(uint16_t, UNIT(w), 0x9a)) {
                    P(uint16_t, UNIT(w), 0x9a) = (uint16_t)id;
                    ACT(0x13);
                }
            } else {
                P(uint32_t, u, 0x3e) &= 0xfdffffff;
                ACT(IDLE_ACT);
            }
        }
        break;
    case 0x12:
        if (target)
            fn_100bb729(id, tid);
        else
            ACT(IDLE_ACT);
        break;
    case 0x13:
        if (target)
            fn_100bbaa3(id, tid);
        else
            ACT(IDLE_ACT);
        break;
    case 0x14:
        state_aim(id, u, target, tid);
        break;
    case 0x18:
        state_fire(id, u, target, tid, can);
        break;
    case 0x1e:
        if (P(uint8_t, u, 0x10e) & 1)
            fn_100db98e(id, 1);
        break;
    case 0x1f:
        state_guard(id, u);
        break;
    case 0x32:
        if ((target || (P(int32_t, u, 0xc) >= 0x10 && P(int32_t, u, 0xc) <= 0x13)) &&
            fn_10071d66(HW(u, 0xf4), id)) {
            ACT(0x33);
        } else {
            if (NET && unit_is_local_u(u))
                net_40706(id, HW(u, 0xf4), 4);
            fn_10071766(HW(u, 0xf4), id);
            ACT(IDLE_ACT);
        }
        break;
    case 0x33:
        state_board(id, u, target);
        break;
    case 0x34:
        fn_10071f48(HW(u, 0xf4), id);
        ACT(IDLE_ACT);
        break;
    case 0x41:
        if (target && P(int32_t, P(uint8_t *, u, 0x32), 0x78))
            fn_100bc3cd(id, tid);
        break;
    case 0x42:
        if (target && P(int32_t, P(uint8_t *, u, 0x32), 0x78) && (!NET || unit_is_local(id)))
            fn_100bc7df(id, tid);
        break;
    case 0x43:
        if (target && P(int32_t, P(uint8_t *, u, 0x32), 0x78) && (!NET || unit_is_local(id))) {
            uint8_t *o = UNIT(tid);
            if (o && P(uint8_t, o, 0x14) == P(uint8_t, u, 0x14) && !(P(uint32_t, o, 0x3e) >> 12 & 1) &&
                P(int16_t, o, 0xd6) == (int16_t)unit_max_health(o, 0))
                ACT(IDLE_ACT);
            else if (NET)
                net_3e0eb(id, tid, 1, 0);
            else
                fn_100b9c83(id, tid, 1, 0, 0);
        }
        break;
    case 0xe:
    case 0x15:
    case 0x16:
    case 0x19:
    case 0x1a:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x28:
    case 0x29:
        break;
    default:
        debug_str((const char *)ORIG_PTR(0x1015925c), state);
        ACT(IDLE_ACT);
        break;
    }
}
