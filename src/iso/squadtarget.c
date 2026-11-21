/* The game logic, 0x100c0eab-0x100c16c7: a squad's own units (targets of its player,
 * 0x120-byte records at G(0x1018b610): +0x1c the owner, +0x20 the unit, +0x24 its kind) and
 * the totals a squad keeps of them (squads: 0x130-byte records at G(0x1018b608): +0x20 the
 * player, +0x34 the number of units, +0x38 per kind, +0x114-+0x120 their values). */
#include "iso_core.h"
#include "squadtarget.h"

#define SQUAD(k) (G(uint8_t *, 0x1018b608) + (k) * 0x130)
#define TARGET(t) (G(uint8_t *, 0x1018b610) + (t) * 0x120)
#define PLAYER(p) (G(uint8_t *, 0x1018b604) + (p) * 0x143)
#define unit_tile_value ORIG(int32_t(__cdecl *)(uint32_t), 0x1004d14f)
#define unit_type_value0 ORIG(int32_t(__cdecl *)(uint32_t), 0x1009da08)
#define unit_type_field ORIG(int32_t(__cdecl *)(uint32_t, uint32_t), 0x1009dbc2)

/* 0x100c0eab: adds target `t`'s unit to squad `s`'s totals (`add`), or takes it off: the
 * count, its health (+0xd6), its value (0x1004d14f, also kept for its own player, +0x76),
 * the type values 0x1009da08 and 0x1009dbc2 (by the player's difficulty, +0xd), and the
 * count of its kind */
void __cdecl squad_target_changed(int32_t s, int32_t t, int32_t add)
{
    int32_t unit = P(int32_t, TARGET(t), 0x20);
    uint32_t kind = P(uint32_t, TARGET(t), 0x24);
    int32_t owner = P(int32_t, TARGET(t), 0x1c);
    int32_t player = P(int32_t, SQUAD(s), 0x20);
    uint32_t level = P(uint8_t, PLAYER(player), 0xd);
    if (s == -1 || t == -1 || !unit)
        return;
    int32_t value = unit_tile_value(unit);
    int32_t sign = add ? 1 : -1;
    /* the kind is used whole as an index, as the original does */
    int16_t *of_kind = (int16_t *)(uintptr_t)((uintptr_t)SQUAD(s) + 0x38 + kind * 2);
    P(int16_t, SQUAD(s), 0x34) += (int16_t)sign;
    P(int32_t, SQUAD(s), 0x118) += sign * P(int16_t, I(uint8_t **, ISO_UNITS)[unit], 0xd6);
    P(int32_t, SQUAD(s), 0x114) += sign * value;
    P(int32_t, SQUAD(s), 0x11c) += sign * unit_type_value0(unit);
    *of_kind += (int16_t)sign;
    P(int32_t, SQUAD(s), 0x120) += sign * unit_type_field(unit, level);
    if (player == owner)
        P(uint16_t, PLAYER(player), 0x76) = (uint16_t)(P(uint16_t, PLAYER(player), 0x76) + sign * value);
}

#define target_next ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x1007223b)
#define sq_next_target ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x1007225e)

/* 0x100c11d6: adds up the health (+0xd6) of the units squad `s` holds (+0x24 its first
 * target; squads of kinds 2-3 hold their own units, 4-5 enemy targets) into +0x118, and
 * returns it. 0 for no squad. For other kinds the original does not step on and loops
 * forever (it is only called for kinds 2-5). */
int32_t __cdecl squad_health_total(int32_t s)
{
    if (s == -1)
        return 0;
    int32_t player = P(int32_t, SQUAD(s), 0x20);
    int32_t t = P(int32_t, SQUAD(s), 0x24);
    int32_t total = 0;
    int32_t kind = P(int32_t, SQUAD(s), 0xc);
    while (t != -1) {
        int32_t unit = P(int32_t, TARGET(t), 0x20);
        if (unit && I(uint8_t **, ISO_UNITS)[unit])
            total += P(int16_t, I(uint8_t **, ISO_UNITS)[unit], 0xd6);
        switch (kind) {
        case 2:
        case 3:
            t = target_next(player, t);
            break;
        case 4:
        case 5:
            t = sq_next_target(player, t);
            break;
        default:
            break;
        }
    }
    P(int32_t, SQUAD(s), 0x118) = total;
    return total;
}

#define target_set_next ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x10072311)
#define target_set_prev ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x100724cc)

/* 0x100c1344: appends target `t` (one of the player's own units) to squad `s` (its list
 * +0x24/+0x28; the target's squad +0x110, for kind 3 also +0x114) and adds it to the
 * squad's totals. Nothing when it is in the squad already. `t`. The debug lines are
 * compiled out. */
int32_t __cdecl squad_add_target(int32_t s, int32_t t)
{
    if (t == -1 || s == -1)
        return t;
    if (s != -1 && P(int32_t, TARGET(t), 0x110) == s)
        return t;
    int32_t last = P(int32_t, SQUAD(s), 0x28);
    int32_t player = P(int32_t, SQUAD(s), 0x20);
    if (last == -1) {
        P(int32_t, SQUAD(s), 0x24) = t;
        target_set_next(player, t, -1);
        target_set_prev(player, t, -1);
    } else {
        target_set_next(player, last, t);
        target_set_prev(player, t, last);
        target_set_next(player, t, -1);
    }
    P(int32_t, SQUAD(s), 0x28) = t;
    P(int32_t, TARGET(t), 0x110) = s;
    if (P(int32_t, SQUAD(s), 0xc) == 3)
        P(int32_t, TARGET(t), 0x114) = s;
    squad_target_changed(s, t, 1);
    return t;
}

#define target_prev ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100723e7)

/* 0x100c154a: takes target `t` out of the squad that holds it (+0x110) and off its totals */
void __cdecl squad_remove_target(int32_t t)
{
    int32_t player = P(int32_t, TARGET(t), 0x1c);
    int32_t prev = target_prev(player, t);
    int32_t next = target_next(player, t);
    int32_t s = P(int32_t, TARGET(t), 0x110);
    if (s == -1)
        return;
    if (P(int32_t, SQUAD(s), 0x28) == t)
        P(int32_t, SQUAD(s), 0x28) = prev;
    if (prev != -1) {
        target_set_next(player, prev, next);
        if (next != -1)
            target_set_prev(player, next, prev);
    } else {
        if (next != -1)
            target_set_prev(player, next, prev);
        P(int32_t, SQUAD(s), 0x24) = next;
    }
    squad_target_changed(s, t, 0);
    target_set_prev(player, t, -1);
    target_set_next(player, t, -1);
    P(int32_t, TARGET(t), 0x110) = -1;
}
