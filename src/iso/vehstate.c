/* The game logic, 0x100d7b3d-0x100d979f: the second unit state machine (+0x196 the state
 * being run, +0x197 the next one), with the states of carriers and the recon double click. */
#include "iso_core.h"
#include "combat.h"
#include "netflags.h"
#include "orders2.h"
#include "patrol.h"
#include "recon.h"
#include "sector.h"
#include "unitface.h"
#include "unitorder.h"
#include "unitstate.h"
#include "vecscore.h"
#include "vehstate.h"
#include "weapons.h"

#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])
#define NET I(int32_t, 0x175)
#define F(u) P(uint32_t, u, 0x10e)
#define S193(u) P(uint16_t, u, 0x193)
#define B195(u) P(uint8_t, u, 0x195)
#define CLASS(u) P(int32_t, u, 0x10)
#define TYPE(u) P(int32_t, u, 0xc)
#define INFO(u) P(uint8_t *, u, 0x32)
#define NEXT(k) P(int16_t, I(uint8_t *, 0x1984) + (k) * 0x12, 0xe)
#define HW(u, off) (((uint32_t)(uintptr_t)(u) & 0xffff0000) | P(uint16_t, u, off))
#define WEAPON_REC(u)                                                                        \
    (P(uint8_t *, I(uint8_t *, 0x1990), 0) +                                                 \
     P(int32_t, I(uint8_t *, 0x1990), 0x2c) * P(int16_t, u, 0xe6) * 4)
#define RELOAD_FULL(u)                                                                       \
    P(int32_t, P(uint8_t *, I(uint8_t *, 0x197c), 0),                                        \
      P(int32_t, I(uint8_t *, 0x197c), 0x2c) * TYPE(u) * 4 + 0x28)
#define ACT(a) unit_set_action(id, (a), 1, 0)
#define IDLE_ACT 0x1e

#define fn_100dcabd ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100dcabd)
#define net_40c27 ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t), 0x10040c27)
#define net_40ec1 ORIG(void(__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t), 0x10040ec1)
#define net_3fe66 ORIG(void(__cdecl *)(uint32_t), 0x1003fe66)
#define fn_1005c64d ORIG(uint16_t(__cdecl *)(int32_t, int32_t, uint32_t, int32_t), 0x1005c64d)
#define fn_1006b343 ORIG(void(__cdecl *)(uint32_t), 0x1006b343)
#define fn_10071bc0 ORIG(int32_t(__cdecl *)(uint32_t), 0x10071bc0)
#define fn_100720d1 ORIG(void(__cdecl *)(uint32_t), 0x100720d1)
#define debug_print ORIG(int32_t(__cdecl *)(int32_t, const char *, ...), 0x10095e98)
#define debug_log ORIG(int32_t(__cdecl *)(int32_t, const char *), 0x10095f64)
#define fn_100a10e5 ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t, int32_t), 0x100a10e5)
#define fn_100bb729 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100bb729)
#define fn_100bbaa3 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100bbaa3)
#define fn_100c6bd6 ORIG(void(__cdecl *)(uint32_t, int32_t), 0x100c6bd6)
#define fn_100d96f6 ORIG(int32_t(__cdecl *)(uint8_t *, uint8_t *), 0x100d96f6)
#define fn_100d9855 ORIG(int32_t(__cdecl *)(uint32_t, uint32_t), 0x100d9855)
#define fn_100d9c57 ORIG(int32_t(__cdecl *)(uint32_t, int32_t *), 0x100d9c57)
#define fn_100d9ead ORIG(int32_t(__cdecl *)(uint32_t, int32_t *, int32_t), 0x100d9ead)
#define fn_100dafc3 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100dafc3)
#define fn_100db772 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100db772)
#define fn_100db98e ORIG(void(__cdecl *)(uint32_t, int32_t), 0x100db98e)
#define fn_100dc1e7 ORIG(int32_t(__cdecl *)(uint32_t), 0x100dc1e7)
#define debug_str ORIG(void(__cdecl *)(const char *, ...), 0x100f492e)

static int finished(const uint8_t *u)
{
    return P(int16_t, u, 0x113) <= 0 && P(int16_t, u, 0x11b) <= 0 && !(P(uint8_t, u, 0x195) & 4) &&
           !(P(uint32_t, u, 0x10e) >> 14 & 1);
}

/* no path left and not bit 14 of the flags */
static int stopped(const uint8_t *u)
{
    return P(int16_t, u, 0x113) <= 0 && P(int16_t, u, 0x11b) <= 0 && !(P(uint32_t, u, 0x10e) >> 14 & 1);
}

/* no pose change running and at most one path node left */
static int settled(const uint8_t *u)
{
    if (P(uint8_t, u, 0x147) || P(int16_t, u, 0x113) > 0)
        return 0;
    return P(int16_t, u, 0x11b) <= 0 || NEXT(P(int16_t, u, 0x11b)) <= 0;
}

/* whether the weapon is aimed; types 0x44-0x47 at a hovercraft type (0x30-0x37) use the
 * other arc */
static int aimed(uint32_t id, uint8_t *u, uint32_t tid)
{
    int32_t tmp;
    if (!P(int32_t, INFO(u), 0xb4))
        return fn_100d9c57(id, &tmp);
    int flag = TYPE(u) >= 0x44 && TYPE(u) <= 0x47 && TYPE(UNIT(tid)) >= 0x30 && TYPE(UNIT(tid)) <= 0x37;
    return fn_100d9ead(id, &tmp, flag);
}

/* state 0x11: walking to a repair spot and turning to it */
static void state_to_spot(uint32_t id, uint8_t *u)
{
    if (B195(u) & 1)
        return;
    if (P(int16_t, u, 0x113) > 0) {
        if (B195(u) & 2)
            B195(u) &= 0xfd;
        if (!NET || unit_is_local_u(u))
            patrol_order(id, (uint32_t)-1, 1, 1, 0, 0, 0, 1);
        debug_str((const char *)ORIG_PTR(0x10159278));
        ((void(__cdecl *)(uint32_t, int32_t))(uintptr_t)G(uint32_t, 0x101545d4 + TYPE(u) * 0x44))(id, 1);
        return;
    }
    debug_log(4, (const char *)ORIG_PTR(0x10159288));
    if (P(uint8_t, u, 0x79) != 3) {
        unit_face(id, 3, 1, 0);
    } else {
        ACT(IDLE_ACT);
        unit_order(id, 0x41, 1, 0);
        B195(u) &= 0xfe;
        F(u) |= 0x1000;
    }
}

/* state 0x14: ready to fire at the target */
static void state_aim(uint32_t id, uint8_t *u, uint8_t *target, uint32_t tid, int32_t can)
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
    if (!target || !can) {
        fn_100db98e(id, 1);
        return;
    }
    if (NET && (!unit_is_local(id) || P(int32_t, u, 0x149) || P(int32_t, u, 0x199)))
        return;
    if (P(int32_t, WEAPON_REC(u), 0x1c) > (int32_t)P(uint16_t, u, 0xdc))
        P(uint8_t, u, 0xed) = 0;
    int32_t on = aimed(id, u, tid);
    int32_t in_range = fn_100d9855(id, HW(u, 0x17d));
    if (P(uint8_t, u, 0xed) && on && in_range) {
        unit_set_action(id, 0x18, net_delay(), 0);
        if (NET && unit_is_local(id))
            net_40ec1(id, HW(u, 0x17d), HW(u, 0x183), HW(u, 0x185), 0);
    } else if (!in_range || !on) {
        ACT(0xb);
    } else if (RELOAD_FULL(u) <= (int32_t)P(uint16_t, u, 0xdc)) {
        P(uint8_t, u, 0xed) = 1;
        P(uint8_t, u, 0xee) = 0;
    }
}

static void fire(uint32_t id, uint8_t *u)
{
    weapon_fire_at_unit(g_iso, id & 0xffff, P(uint16_t, u, 0x17d), P(int16_t, u, 0x183), P(int16_t, u, 0x185));
}

/* state 0x18: firing, with the burst (weapon +0x58) and the ammunition of type 0x74 == 5 */
static void state_fire(uint32_t id, uint8_t *u, uint8_t *target, uint32_t tid, int32_t can)
{
    if (P(int32_t, u, 0x149) || P(int32_t, u, 0x199))
        return;
    int shoot = target && ((NET && !unit_is_local_u(u)) || (can && fn_100d9855(id, tid)));
    if (shoot) {
        if (P(uint8_t, u, 0xee)) {
            S193(u) &= 0xffef;
            P(uint8_t, u, 0xee)--;
            return;
        }
        if ((!NET || unit_is_local_u(u)) && !aimed(id, u, tid)) {
            ACT(0x14);
            return;
        }
        unit_random_point(id, HW(u, 0x17d), &P(int16_t, u, 0x183), &P(int16_t, u, 0x185));
        uint8_t *w = WEAPON_REC(u);
        if (P(int32_t, INFO(u), 0x74) == 5) {
            if (P(int16_t, u, 0x42) && (!P(int32_t, w, 0x58) || fn_100d96f6(u, w)))
                fire(id, u);
        } else if (!P(int32_t, w, 0x58) || fn_100d96f6(u, w)) {
            if (TYPE(u) >= 0x2c && TYPE(u) <= 0x2f)
                fn_100c6bd6(id, 4);
            int still = !P(uint8_t, u, 0x147);
            fire(id, u);
            if (still)
                unit_order(id, 0x1b, 1, 0);
        }
        uint8_t *o = UNIT(tid);
        if (!UNIT(P(uint16_t, o, 0x18b)))
            P(uint16_t, o, 0x18b) = (uint16_t)id;
        P(uint8_t, u, 0xee) = (uint8_t)P(int32_t, WEAPON_REC(u), 0x20);
    } else {
        P(uint8_t, u, 0xed) = 0;
        ACT(0x14);
    }
    if (P(int32_t, WEAPON_REC(u), 0x1c) > (int32_t)P(uint16_t, u, 0xdc))
        ACT(0x14);
}

/* 0x100d7b3d: one step of unit `id`'s second state machine. A delay (+0x199) holds it until
 * that tick. Leaving the idle state 0x1e calls the handler of its type. */
void __cdecl unit_ai_state2(uint32_t id)
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
        int32_t k = P(int32_t, INFO(u), 4);
        if (k >= 0 && G(uint32_t, 0x10118d90 + k * 12))
            ((void(__cdecl *)(uint8_t *))(uintptr_t)G(uint32_t, 0x10118d90 + k * 12))(u);
    }
    switch (state) {
    case 0xa:
    case 0x17:
        if (finished(u))
            ACT(IDLE_ACT);
        break;
    case 0xb: {
        int32_t ready = fn_100dc1e7(id);
        if (target && can && ready) {
            if ((S193(u) >> 5 & 1) || (TYPE(u) >= 0x40 && TYPE(u) <= 0x4f))
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
    case 0xf:
        if (finished(u)) {
            debug_print(4, (const char *)ORIG_PTR(0x1015929c), id & 0xffff, (uint32_t)P(uint8_t, u, 0x196),
                        (uint32_t)P(uint8_t, u, 0x197), (uint32_t)P(uint16_t, u, 0x17d),
                        (uint32_t)P(uint16_t, u, 0x18b));
            debug_print(4, (const char *)ORIG_PTR(0x101592e0), id & 0xffff);
            ACT(IDLE_ACT);
        }
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
    case 0x11:
        state_to_spot(id, u);
        break;
    case 0x12:
        if (target && P(int32_t, INFO(u), 0x78))
            fn_100bb729(id, tid);
        else
            ACT(IDLE_ACT);
        break;
    case 0x13:
        if (target && P(int32_t, INFO(u), 0x78))
            fn_100bbaa3(id, tid);
        else
            ACT(IDLE_ACT);
        break;
    case 0x14:
        state_aim(id, u, target, tid, can);
        break;
    case 0x18:
        state_fire(id, u, target, tid, can);
        break;
    case 0x1e:
        if (P(uint8_t, u, 0x10e) & 1)
            fn_100db98e(id, 1);
        break;
    case 0x1f:
        if (TYPE(u) >= 0x40 && TYPE(u) <= 0x9f) {
            ACT(IDLE_ACT);
            break;
        }
        if (P(uint8_t, u, 0x10e) & 1)
            fn_100db98e(id, 1);
        unit_guard_step(id, u);
        break;
    case 0x28:
        if (settled(u))
            recon_double_click(id);
        break;
    case 0x29:
        if (settled(u))
            recon_double_click_x(id);
        break;
    case 0x38:
        if (fn_10071bc0(id))
            ACT(0x3a);
        else
            ACT(IDLE_ACT);
        break;
    case 0x39:
        if (!P(uint16_t, u, 0xfa))
            ACT(IDLE_ACT);
        if (stopped(u)) {
            if (!P(uint8_t, u, 0x147) && !P(uint8_t, u, 0x148)) {
                if (P(uint8_t, u, 0x79) != 7)
                    unit_face(id, 7, 1, 0);
                else
                    unit_order(id, 0x46, 1, 0);
            } else if (P(uint8_t, u, 0x147) == 0x47 && P(uint8_t, u, 0x148) == 0x47) {
                ACT(0x38);
            }
        }
        break;
    case 0x3a:
        if (P(int32_t, INFO(u), 0xe0) <= (int32_t)P(uint8_t, u, 0xf3) || !P(uint16_t, u, 0xfa)) {
            unit_order(id, 0x48, 1, 0);
            P(uint32_t, u, 0x149) = I(uint32_t, 0x17dd) + 0x14;
            ACT(IDLE_ACT);
            P(uint8_t, u, 0x2d) = 4;
        }
        break;
    case 0x3b:
        if (!P(uint16_t, u, 0xf6))
            ACT(IDLE_ACT);
        if (stopped(u)) {
            if (!P(uint8_t, u, 0x147) && !P(uint8_t, u, 0x148)) {
                if (P(uint8_t, u, 0x79) != 7) {
                    if (!NET || unit_is_local(id))
                        unit_face(id, 7, 1, 0);
                } else {
                    unit_order(id, 0x46, 1, 0);
                    if (P(int32_t, u, 0x84))
                        unit_buildup_set(id, 2);
                }
            } else if (P(uint8_t, u, 0x147) == 0x47 && P(uint8_t, u, 0x148) == 0x47) {
                if (!NET || unit_is_local(id))
                    fn_100720d1(id);
                unit_order(id, 0x48, 1, 0);
                P(uint32_t, u, 0x149) = I(uint32_t, 0x17dd) + 0xa;
                ACT(IDLE_ACT);
                P(uint8_t, u, 0x2d) = 4;
            }
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
        break;
    default:
        debug_str((const char *)ORIG_PTR(0x101592f8), (int32_t)state);
        ACT(IDLE_ACT);
        break;
    }
}
