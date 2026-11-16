/* The game logic, 0x10018edb-0x10019b52: what a selected unit does when the player
 * clicks another unit: board a vehicle, enter a structure, attack it, an engineer's
 * capture or repair. The first unit to take the order speaks (*voice: its voice handle). */
#include "iso_core.h"
#include "sound.h"
#include "sector.h"
#include "orders.h"

/* not rewritten yet (names ours, provisional) */
#define net_unit_order ORIG(void(__cdecl *)(uint16_t, uint16_t, uint8_t), 0x10040706)
#define unit_set_target ORIG(void(__cdecl *)(uint16_t, uint16_t), 0x100dcabd)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)
#define unit_attack_target ORIG(void(__cdecl *)(uint32_t, uint16_t, int, uint16_t), 0x100dc75c)
#define unit_stop_attack ORIG(void(__cdecl *)(uint32_t), 0x100dbcbb)
#define unit_max_health ORIG(int(__cdecl *)(uint8_t *, int), 0x100a23e0)
#define unit_set_d6f9 ORIG(void(__cdecl *)(uint32_t, int), 0x100e8f48)
#define action_delay ORIG(int(__cdecl *)(void), 0x1003f390)
#define unit_in_range ORIG(int(__cdecl *)(uint32_t, uint32_t), 0x100d9855)
#define unit_wants_follow ORIG(int(__cdecl *)(uint32_t), 0x1009530a)
#define unit_follow_target ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100954b4)
#define vehicle_take ORIG(int(__cdecl *)(uint32_t, uint32_t), 0x100715b7)
#define vehicle_can_leave ORIG(int(__cdecl *)(uint16_t, uint32_t), 0x10071766)
#define unit_order_go ORIG(void(__cdecl *)(uint32_t, int, int, int, int, int, int, int), 0x100b7a66)
#define unit_leave_vehicle ORIG(void(__cdecl *)(uint32_t, int), 0x100db98e)
#define unit_can_reach ORIG(int(__cdecl *)(uint32_t, uint32_t, int, int), 0x1006b6d3)
#define g_rand ORIG(int(__cdecl *)(void), 0x100f4d80)

#define TICK I(int32_t, 0x17dd)
#define U_VEHICLE 0x17d /* short: the unit it is in or is going to */
#define U_INSIDE 0x10e  /* bit 0: inside a vehicle or structure */
#define U_WAKE 0x199    /* tick of its next action */

static void voice_once(int *voice, uint8_t *u, int what)
{
    if (!*voice)
        *voice = unit_voice(u, what, 0, 1);
}

/* 0x10018edb: unit `id` boards the vehicle or enters the structure `into` */
int __cdecl order_board(uint32_t id, uint32_t into, int *voice)
{
    uint8_t *v = iso_unit((int)(into & 0xffff));
    uint8_t *u = iso_unit((int)(id & 0xffff));
    int result = 0;
    if (P(int32_t, P(uint8_t *, v, 0x32), 0xe0) <= (int32_t)P(uint8_t, v, 0xf3))
        return 0; /* full */
    if (P(uint8_t, v, 0x196) == ';')
        return 0;
    if (P(int32_t, u, U_CLASS) > 0x18 || P(uint8_t, u, 0x147) == 'E' || !P(int32_t, P(uint8_t *, u, 0x32), 0xe4) ||
        (P(uint32_t, u, U_INSIDE) & 0x30))
        return 0;
    if (voice)
        voice_once(voice, u, 3);
    if (I(int, 0x175) && unit_is_local(into))
        net_unit_order((uint16_t)id, (uint16_t)into, 0);
    if (!(P(uint8_t, u, U_INSIDE) & 1) && P(uint16_t, u, U_VEHICLE) && P(uint16_t, u, U_VEHICLE) != (into & 0xffff) &&
        vehicle_can_leave(P(uint16_t, u, U_VEHICLE), id)) {
        unit_order_go(id, -1, 1, 1, 0, 0, 0, 1);
        unit_set_action(id, 0xf, 1, 0);
    }
    if (P(uint8_t, u, U_INSIDE) & 1)
        unit_leave_vehicle(id, 1);
    unit_set_target((uint16_t)id, (uint16_t)into);
    int first = P(uint16_t, v, 0xfa) == 0;
    result = vehicle_take(into, id);
    if (!result)
        return 0;
    if (first) {
        if ((P(uint8_t, v, U_INSIDE) & 1) || P(uint16_t, v, U_VEHICLE))
            unit_leave_vehicle(into, 1);
        P(int16_t, v, 0x183) = P(int16_t, u, U_X);
        P(int16_t, v, 0x185) = P(int16_t, u, U_Y);
    } else {
        /* where it waits: between the boarders */
        P(int16_t, v, 0x183) = (int16_t)(((int32_t)P(int16_t, v, 0x183) + P(int16_t, u, U_X)) / 2);
        P(int16_t, v, 0x185) = (int16_t)(((int32_t)P(int16_t, v, 0x185) + P(int16_t, u, U_Y)) / 2);
    }
    uint8_t k = P(uint8_t, v, 0x196);
    if (k != '9' && k != '8' && k != ':') {
        unit_set_action(into, 0x39, 1, 0);
    } else if (k == '8' || k == ':') {
        if (!I(int, 0x175) || unit_is_local(into)) {
            unit_set_action(id, 0x32, 1, 0);
            P(int32_t, iso_unit((int)(id & 0xffff)), U_WAKE) = TICK + action_delay() + 0;
        }
        unit_set_target((uint16_t)id, (uint16_t)into);
        P(uint16_t, iso_unit((int)(id & 0xffff)), 0xf4) = (uint16_t)into;
    }
    return result;
}

/* 0x100192ef: unit `id` and the structure `building` of class 100: if its owner has
 * none chosen yet (0xd6f9) and its flags allow, the structure becomes the choice;
 * otherwise an infantry unit goes in (action 0x12) */
int __cdecl order_enter(uint32_t id, uint32_t building, int *voice)
{
    uint8_t *b = iso_unit((int)(building & 0xffff));
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (!I(uint16_t, 0xd6f9 + P(uint8_t, b, U_PLAYER) * 2) && !(P(uint32_t, b, U_FLAGS) & (1u << 0xb)) &&
        !(P(uint32_t, b, U_FLAGS) & (1u << 0x1a))) {
        unit_set_d6f9(building, 1);
        return 0;
    }
    if (P(int32_t, u, U_ID_TYPE) >= 0x40)
        return 0;
    uint8_t c = P(uint8_t, u, 0x147);
    if (c == 'E' || c == 'F' || !(c != 'G' && P(uint8_t, u, 0x196) != ';'))
        return 0;
    voice_once(voice, u, 3);
    unit_set_target((uint16_t)id, (uint16_t)building);
    unit_set_action(id, 0x12, 1, 0);
    return 1;
}

/* 0x10019443: unit `id` of type 0x21 is ordered at `target` (it attacks it, unless it
 * is already in it) */
int __cdecl order_type21(uint32_t id, uint16_t target, int *voice)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    uint8_t *t = iso_unit(target);
    if (P(int32_t, u, U_ID_TYPE) != 0x21 || P(uint16_t, u, U_VEHICLE) == target)
        return 0;
    if (!(P(uint8_t, u, U_INSIDE) & 1) || P(uint16_t, u, U_VEHICLE) != target) {
        unit_stop_attack(id);
        unit_attack_target(id, target, 0, 1);
    }
    voice_once(voice, t, 0x10);
    return 1;
}

/* 0x10019535: an engineer (types 0x10-0x13) and a friendly unit: board it, enter a
 * building of class 100, or repair a damaged structure */
int __cdecl order_engineer(uint32_t id, uint32_t target, int *voice, int *stagger)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    uint8_t *t = iso_unit((int)(target & 0xffff));
    unit_max_health(t, 0);
    if (P(int32_t, P(uint8_t *, t, 0x32), 0xe0))
        return order_board(id, target, voice);
    uint32_t f = P(uint32_t, t, U_FLAGS);
    if (P(int32_t, t, U_CLASS) == 100 && !(f & 0x1000) && !(f & 4))
        return order_enter(id, target, voice);
    int32_t tt = P(int32_t, t, U_ID_TYPE);
    if (!((tt >= 0x40 && tt <= 0x4f) || (tt >= 0x50 && tt <= 0x87)) || (!(f & 0x1000) && !(f & 4)))
        return 0;
    unit_set_target((uint16_t)id, (uint16_t)target);
    P(uint32_t, u, U_FLAGS) &= ~0x1000u;
    unit_set_action(id, 0x41, 1, 0);
    P(int32_t, u, U_WAKE) = TICK + action_delay() + *stagger;
    *stagger += 2;
    P(int32_t, u, 0xc2) = 0;
    if (P(uint8_t, u, 0x195) & 2)
        P(uint8_t, u, 0x195) ^= 2;
    voice_once(voice, u, 0);
    return 1;
}

/* 0x10019715: unit `id` and an enemy (or forced) `target`: attack it; an engineer
 * captures a structure */
int __cdecl order_attack(uint32_t id, uint32_t target, int *voice, int *stagger)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    uint8_t ttype = P(uint8_t, iso_unit((int)(target & 0xffff)), U_ID_TYPE);
    uint8_t utype = P(uint8_t, u, U_ID_TYPE);
    if (P(uint32_t, u, U_FLAGS) & (1u << 0x19))
        return 0;
    if ((target & 0xffff) != (id & 0xffff)) {
        int engineer = utype >= 0x10 && utype <= 0x13;
        if (!engineer && !unit_can_reach(id, target, 1, 1) && !I(int32_t, 0xd79b))
            return 0;
        if (engineer || P(int32_t, P(uint8_t *, u, 0x32), 0x20) >= 0 || I(int32_t, 0xd79b)) {
            if (P(uint8_t, u, 0x147) == 'E')
                return 0;
            if (engineer) {
                if (!((ttype >= 0x40 && ttype <= 0x4f) || (ttype >= 0x50 && ttype <= 0x87)) ||
                    (P(uint32_t, u, U_INSIDE) & 0x10))
                    return 0;
                P(uint32_t, u, U_FLAGS) &= ~0x1000u;
                P(uint16_t, u, U_VEHICLE) = (uint16_t)target;
                P(uint16_t, u, 0x193) &= 0xfeff;
                unit_set_action(id, 0x41, 1, 0);
            } else {
                int at = P(uint16_t, u, U_VEHICLE) == (target & 0xffff);
                int32_t t = P(int32_t, u, U_ID_TYPE);
                if (!(at && ((P(uint8_t, u, U_INSIDE) & 1) || (t >= 0x40 && t <= 0x4f)))) {
                    unit_stop_attack(id);
                    unit_attack_target(id, (uint16_t)target, *stagger, I(int32_t, 0xd797) == 0);
                    *stagger += 2;
                }
            }
            if (P(uint16_t, u, U_VEHICLE) != (target & 0xffff) ||
                (P(int32_t, u, U_CLASS) >= 0xc && P(int32_t, u, U_CLASS) <= 0xf)) {
                int32_t t = P(int32_t, u, U_ID_TYPE);
                int local = !I(int, 0x175) || (t >= 0x30 && t <= 0x37);
                unit_stop_attack(id);
                if (local)
                    unit_set_target((uint16_t)id, (uint16_t)target);
                P(uint16_t, u, 0x193) &= 0xfeff;
                if (local && unit_in_range(id, target)) {
                    int r = g_rand() & 4;
                    r += action_delay();
                    P(int32_t, u, U_WAKE) = r + TICK;
                } else {
                    int d = action_delay();
                    P(int32_t, u, U_WAKE) = TICK + d + *stagger;
                    *stagger += 2;
                }
                P(int32_t, u, 0xc2) = 0;
                if (P(uint8_t, u, 0x195) & 2)
                    P(uint8_t, u, 0x195) ^= 2;
                if (unit_wants_follow(id & 0xffff) > 0)
                    unit_follow_target(id & 0xffff, target & 0xffff);
            }
        }
    }
    voice_once(voice, u, 0);
    return 1;
}
