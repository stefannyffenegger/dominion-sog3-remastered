/* The game logic, 0x1000d362-0x1000d96c: damage from an effect (full at its centre, less
 * further out, minus armour), taking damage, and whether a weapon gets through armour. */
#include "iso_core.h"
#include "damage.h"

/* not rewritten yet (names ours, provisional) */
#define players_allied ORIG(int(__cdecl *)(uint16_t, uint16_t), 0x100d25a3)
#define unit_max_health ORIG(int(__cdecl *)(uint8_t *, int), 0x100a23e0)
#define score_unit_lost ORIG(void(__cdecl *)(uint32_t, uint8_t, uint8_t), 0x1007e870)
#define reveal_around ORIG(void(__cdecl *)(int, int, uint32_t, int, uint32_t, uint16_t, int), 0x10090223)
#define unit_remove ORIG(void(__cdecl *)(uint32_t, uint32_t, int, uint32_t), 0x1000f5e5)
#define net_unit_hit ORIG(int(__cdecl *)(uint32_t, char), 0x10034aef)
#define unit_hit ORIG(void(__cdecl *)(uint32_t), 0x1000e7e2)

#define EFFECT_TYPE(e) P(uint8_t *, I(uint8_t *, 0xd6db) + (e) * 0x44, 0x16)
/* a weapon's armour piercing */
static int32_t weapon_value(int16_t w, int off)
{
    int32_t *t = I(int32_t *, 0x1990);
    return P(int32_t, (uintptr_t)t[0], t[0xb] * w * 4 + off);
}

static int32_t ftol(long double v) { return (int32_t)(int64_t)v; }

/* 0x1000d362: the damage effect `e` does to the unit at `distance` from its centre (0: a
 * direct hit) */
int __cdecl effect_damage(uint32_t id, uint32_t e, int16_t distance)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (!u)
        return 0;
    uint8_t *t = EFFECT_TYPE(e & 0xffff);
    if (!distance) {
        DebugStr((const char *)ORIG_PTR(0x10118be4));
        return P(int32_t, t, 0x38);
    }
    DebugStr((const char *)ORIG_PTR(0x10118bd4));
    int32_t radius = P(int32_t, t, 0x40), full = P(int32_t, t, 0x38), least = P(int32_t, t, 0x4c);
    int32_t part = ftol((long double)(full - least) / (long double)radius *
                        (long double)(radius - (int32_t)(uint16_t)distance));
    return part + (least - P(uint16_t, u, 0xd8));
}

/* 0x1000d4c2: the unit loses `percent` of its health (+1) to `attacker`; at 0 it is
 * destroyed and scored */
void __cdecl unit_take_damage(uint8_t *u, uint32_t attacker, int16_t percent)
{
    if (players_allied(P(uint8_t, u, U_PLAYER), (uint16_t)attacker) == 1)
        return;
    int loss = ((int16_t)unit_max_health(u, 0) + 1) * percent;
    P(int16_t, u, 0xd6) = (int16_t)(P(int16_t, u, 0xd6) - (int16_t)(loss / 100));
    if (P(int16_t, u, 0xd6) >= 1) {
        if (I(int, 0x175) && I(int, 0x148))
            net_unit_hit(P(uint16_t, u, 0x1cb), 0);
        unit_hit(P(uint16_t, u, 0x1cb));
        return;
    }
    P(int16_t, u, 0xd6) = 0;
    if (P(uint8_t, u, 0x196) != 0x1a && !(P(uint32_t, u, U_FLAGS) >> 0x1e & 1)) {
        score_unit_lost(P(uint32_t, u, U_CLASS), P(uint8_t, u, U_PLAYER), (uint8_t)attacker);
        P(uint32_t, u, U_FLAGS) |= 0x40000000;
    }
    reveal_around(P(int16_t, u, U_X), P(int16_t, u, U_Y), attacker,
                  P(int32_t, P(uint8_t *, u, 0x32), 0x74) < 4 ? 2 : 5, 0xffffffff, 0, 0);
    if (P(uint8_t, u, 0x148) != 'L' && P(uint8_t, u, 0x148) != ' ') {
        unit_remove(P(uint16_t, u, 0x1cb), 0x20, 1, 0);
        if (!(P(uint32_t, u, U_FLAGS) >> 0x1e & 1)) {
            score_unit_lost(P(uint32_t, u, U_CLASS), P(uint8_t, u, U_PLAYER), P(uint8_t, u, U_PLAYER));
            P(uint32_t, u, U_FLAGS) |= 0x40000000;
        }
    }
}

/* 0x1000d6ec: whether unit b's weapon gets through unit a's armour */
int __cdecl weapon_pierces(uint32_t a, uint32_t b)
{
    uint8_t *ua = iso_unit((int)(a & 0xffff)), *ub = iso_unit((int)(b & 0xffff));
    if (!ua || !ub || P(int16_t, ub, 0xe6) < 0)
        return 0;
    return (int)P(uint16_t, ua, 0xd8) < weapon_value(P(int16_t, ub, 0xe6), 0x38);
}

/* 0x1000d7cb: the same for a unit inside a vehicle or building: b's weapon must also get
 * through what the carrier's own weapon would */
int __cdecl weapon_pierces_carried(uint32_t a, uint32_t b)
{
    uint8_t *ua = iso_unit((int)(a & 0xffff)), *ub = iso_unit((int)(b & 0xffff));
    if (!ua || !ub)
        return 0;
    int32_t carrier = 0;
    uint16_t c = P(uint16_t, ua, 0x17d);
    if (c && iso_unit(c) && P(int16_t, iso_unit(c), 0xe6) >= 0)
        carrier = weapon_value(P(int16_t, iso_unit(c), 0xe6), 0x38);
    if (P(int16_t, ub, 0xe6) < 0)
        return 0;
    int32_t w = weapon_value(P(int16_t, ub, 0xe6), 0x38);
    return carrier < w && (int)P(uint16_t, ua, 0xd8) < w;
}
