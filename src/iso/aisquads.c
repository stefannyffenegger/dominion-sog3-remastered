/* The game logic, 0x1004c950-0x1004d392: the AI's targets and the squads that attack
 * them. Squads are 0x130-byte records (G(0x1018b608)), targets 0x120-byte records
 * (G(0x1018b610): +0x1c the owner, +0x20 the unit, +0x90 + player * 4 the squad of
 * each player that attacks it, -1 for none). A squad's targets are a list (+0x24
 * first, +0x28 last) through the original's per-player list helpers. */
#include "iso_core.h"
#include "aisquads.h"

/* not rewritten yet (names ours, provisional) */
#define unit_name ORIG(const char *(__cdecl *)(int32_t, void *), 0x1009ddd0)
#define debug_printf ORIG(void(__cdecl *)(int, const char *, ...), 0x10095e98)
#define ai_timing_start ORIG(void(__cdecl *)(void), 0x10096250)
#define ai_timing_log ORIG(void(__cdecl *)(int, const char *, uint32_t), 0x1009626b)
#define tl_next ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x10072212)  /* the player's target list */
#define sq_next_target ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x1007225e)
#define sq_set_next_target ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x1007234a)
#define sq_prev_target ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x1007240a)
#define sq_set_prev_target ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x10072505)
#define target_known ORIG(void(__cdecl *)(int32_t, int32_t), 0x10073175)
#define unit_can_hit ORIG(int32_t(__cdecl *)(uint8_t *, uint8_t *), 0x1007364c)
#define unit_spotted_by ORIG(int32_t(__cdecl *)(uint16_t, uint8_t), 0x1009efda)
#define unit_is_dead ORIG(int32_t(__cdecl *)(uint16_t), 0x1009ebb9)
#define squad_next ORIG(int32_t(__cdecl *)(int32_t), 0x100c038c)
#define squad_new ORIG(int32_t(__cdecl *)(int32_t, int32_t, int32_t, int32_t), 0x100c0462)
#define squad_target_changed ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x100c0eab)
#define squad_idle ORIG(void(__cdecl *)(int32_t), 0x100c197b)
#define ai_player_active ORIG(int32_t(__cdecl *)(uint32_t), 0x100e2db2)
#define ai_list_first ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100e2ee5)

#define SQ(i, type, off) P(type, G(uint8_t *, 0x1018b608), (i) * 0x130 + (off))
#define TG(i, type, off) P(type, G(uint8_t *, 0x1018b610), (i) * 0x120 + (off))
#define AI G(uint8_t *, 0x1018b604)

static int32_t ftol(long double v) { return (int32_t)(int64_t)v; }

/* 0x1004c950: whether another player's squad attacks the unit's target */
int32_t __cdecl unit_attacked_by(uint8_t *u, uint32_t player)
{
    if (P(int16_t, u, 0x1b1) < 1 || P(uint8_t, u, 0x14) == player ||
        TG(P(int16_t, u, 0x1b1), int32_t, 0x90 + player * 4) < 0)
        return 0;
    return 1;
}

/* 0x1004c9b9: the target added at the end of the squad's list */
int32_t __cdecl squad_add_target(int32_t squad, int32_t target)
{
    if (target == -1 || squad == -1)
        return target;
    uint8_t name[0x1c];
    debug_printf(0x10, (const char *)ORIG_PTR(0x1013a550), unit_name(TG(target, int32_t, 0x24), name), target,
                 squad);
    int32_t last = SQ(squad, int32_t, 0x28);
    int32_t player = SQ(squad, int32_t, 0x20);
    if (last == -1) {
        SQ(squad, int32_t, 0x24) = target;
        sq_set_next_target(player, target, -1);
        sq_set_prev_target(player, target, -1);
    } else {
        sq_set_next_target(player, last, target);
        sq_set_prev_target(player, target, last);
        sq_set_next_target(player, target, -1);
    }
    TG(target, int32_t, 0x90 + player * 4) = squad;
    squad_target_changed(squad, target, 1);
    SQ(squad, int32_t, 0x28) = target;
    debug_printf(0x10, (const char *)ORIG_PTR(0x1013a57c), squad, SQ(squad, uint16_t, 0x34));
    return target;
}

/* 0x1004cb5c: the target taken out of the list of the player's squad that attacks it */
void __cdecl squad_drop_target(int32_t player, int32_t target)
{
    int32_t prev = sq_prev_target(player, target);
    int32_t next = sq_next_target(player, target);
    int32_t squad = TG(target, int32_t, 0x90 + player * 4);
    if (squad == -1)
        return;
    if (prev == -1) {
        SQ(squad, int32_t, 0x24) = next;
        if (next == -1)
            SQ(squad, int32_t, 0x28) = -1;
        else
            sq_set_prev_target(player, next, -1);
    } else {
        sq_set_next_target(player, prev, next);
        if (next == -1)
            SQ(squad, int32_t, 0x28) = prev;
        else
            sq_set_prev_target(player, next, prev);
    }
    squad_target_changed(squad, target, 0);
    sq_set_prev_target(player, target, -1);
    sq_set_next_target(player, target, -1);
    TG(target, int32_t, 0x90 + player * 4) = -1;
    if (!SQ(squad, int16_t, 0x34))
        squad_idle(squad);
}

/* 0x1004cd09: every other active AI player learns of the target, or stops attacking it */
void __cdecl target_tell_players(int32_t target)
{
    for (uint32_t p = 0; p < 8; p++) {
        if (TG(target, uint32_t, 0x1c) != p && ai_player_active(p)) {
            if (TG(target, int32_t, 0x90 + p * 4) == -1)
                target_known((int32_t)p, target);
            else
                squad_drop_target((int32_t)p, target);
        }
    }
}

/* 0x1004d025: whether a unit of the squad (or for a structure: of any of its targets'
 * units) can hit the unit */
int32_t __cdecl squad_can_attack(int32_t player, uint8_t *u, int32_t squad)
{
    int32_t t = SQ(squad, int32_t, 0x24);
    if (t == -1)
        return 0;
    if (P(int32_t, u, U_ID_TYPE) < 0x40 || P(int32_t, u, U_ID_TYPE) > 0x9f)
        return unit_can_hit(u, iso_unit(TG(t, int32_t, 0x20)));
    while (t != -1 && TG(t, int32_t, 0x20)) {
        if (unit_can_hit(u, iso_unit(TG(t, int32_t, 0x20))))
            return 1;
        t = sq_next_target(player, t);
    }
    return 0;
}

/* 0x1004cfba: the first of the player's squads in the list that can attack the unit */
int32_t __cdecl squad_find_for(uint8_t *u, int32_t player, int32_t list)
{
    int32_t s = ai_list_first(player, list);
    while (s != -1 && !squad_can_attack(player, u, s))
        s = squad_next(s);
    return s;
}

/* 0x1004ce15: a squad of the player for the target (a new one when none fits); -1 when
 * the target is not to be attacked */
int32_t __cdecl target_assign(int32_t player, int32_t target, int32_t force)
{
    if (target == -1 || !TG(target, int32_t, 0x20))
        return -1;
    int32_t id = TG(target, int32_t, 0x20);
    uint8_t *u = iso_unit(id);
    if (TG(target, int32_t, 0x90 + player * 4) != -1)
        return -1;
    if (P(int32_t, AI, 0xa2c) && !force && !unit_spotted_by((uint16_t)id, (uint8_t)player))
        return -1;
    if (unit_is_dead((uint16_t)id))
        return -1;
    if ((1 << (player & 0x1f)) & I(uint8_t, P(uint8_t, u, U_PLAYER) * 0x1461 + 0x1ba0))
        return -1;
    int32_t list = P(int32_t, u, U_ID_TYPE) < 0x40 || P(int32_t, u, U_ID_TYPE) > 0x9f ? 4 : 5;
    int32_t squad = squad_find_for(u, player, list);
    if (squad == -1)
        squad = squad_new(player, list, 9, 0);
    squad_add_target(squad, target);
    target_known(player, target);
    return squad;
}

/* 0x1004cdb7: the player's known targets get squads */
void __cdecl ai_assign_targets(int32_t player)
{
    int32_t t = ai_list_first(player, 1);
    while (t != -1) {
        int32_t next = tl_next(player, t);
        target_assign(player, t, 0);
        t = next;
    }
}

/* 0x1004d14f: the value (+0x38) of the tile record of the unit's tile (+0xe6), 0 when
 * there is none */
int32_t __cdecl unit_tile_value(uint16_t id)
{
    uint8_t *u;
    if (!id || !(u = iso_unit(id)) || P(int16_t, u, 0xe6) < 0)
        return 0;
    uint8_t *lib = I(uint8_t *, 0x1990);
    uint8_t *rec = *(uint8_t **)lib + P(int16_t, u, 0xe6) * P(int32_t, lib, 0x2c) * 4;
    if (!rec)
        return 0;
    return P(int32_t, rec, 0x38);
}

/* 0x1004d20d: the player's lethality threshold: given, or the summed strength of its
 * squads in lists 4 and 5 scaled by the difficulty (+0x6a: 0 none, 1 x1.2, 2 x2, 3 x3) */
int32_t __cdecl ai_set_lethality(int32_t player, int32_t value)
{
    ai_timing_start();
    if (!value) {
        int32_t s;
        for (s = ai_list_first(player, 4); s != -1; s = squad_next(s))
            value += SQ(s, int32_t, 0x114);
        for (s = ai_list_first(player, 5); s != -1; s = squad_next(s))
            value += SQ(s, int32_t, 0x114);
        switch (P(int32_t, AI, player * 0x143 + 0x6a)) {
        case 0:
            value = 0;
            break;
        case 1: /* the x87 at the game's precision, truncated */
            value = ftol((long double)(uint32_t)value * (long double)G(double, 0x10113088));
            break;
        case 2:
            value <<= 1;
            break;
        case 3:
            value *= 3;
            break;
        }
    }
    P(int16_t, AI, player * 0x143 + 0x78) = (int16_t)value;
    ai_timing_log(0x10, (const char *)ORIG_PTR(0x1013a598), 0);
    return value;
}
