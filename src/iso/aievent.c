/* The game logic, 0x100e42a6-0x100e487c: events and alerts for the computer players. */
#include "iso_core.h"
#include "aiactivate.h"
#include "aievent.h"
#include "ailists.h"

#define AI G(uint8_t *, 0x1018b604)
#define PLAYER(k) (AI + (k) * 0x143 + 0xc)
#define TARGET_ORDER(t) P(int32_t, G(uint8_t *, 0x1018b610) + (t) * 0x120, 0x18)
#define TASK_STATE(t) P(int32_t, G(uint8_t *, 0x1018b600) + (t) * 0x74, 0x2c)
#define SQUAD(s) (G(uint8_t *, 0x1018b608) + (s) * 0x130)
#define STR(a) ((const char *)ORIG_PTR(a))
#define LOG(...) debug_print(0x10, __VA_ARGS__)
#define debug_print ORIG(int32_t(__cdecl *)(int32_t, const char *, ...), 0x10095e98)
#define target_new ORIG(void(__cdecl *)(uint32_t), 0x10072550)
#define unit_target_free ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x10072aa5)
#define target_tell_players ORIG(void(__cdecl *)(int32_t), 0x1004cd09)
#define target_unlink_all ORIG(void(__cdecl *)(int32_t, int32_t), 0x10072eea)
#define target_link_other ORIG(void(__cdecl *)(int32_t, int32_t), 0x10072c45)
#define unit_squad_count ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x10073598)
#define task_type_done ORIG(void(__cdecl *)(int32_t, uint32_t, int32_t), 0x1005d326)
#define structure_lost ORIG(void(__cdecl *)(uint32_t, int32_t), 0x10093752)
#define squad_new_raw ORIG(int32_t(__cdecl *)(int32_t, int32_t, int32_t, int32_t), 0x100c0462)
#define squad_take ORIG(void(__cdecl *)(int32_t, int32_t), 0x100c179d)
#define refinery_sell ORIG(void(__cdecl *)(uint32_t), 0x1009f4f3)
#define unit_stop ORIG(void(__cdecl *)(uint32_t), 0x1009f861)

/* a unit went cloaked: its target record is taken out of all tasks and squads and linked
 * again as an "other" target for every other computer player */
static void unit_cloaked(int32_t k, uint32_t unit, int32_t target)
{
    LOG(STR(0x1015b300), k, unit, target);
    if (!unit || target == -1)
        return;
    target_tell_players(target);
    target_unlink_all(target, 1);
    for (uint32_t i = 0; i < 8; i++)
        if (ai_player_active((int32_t)i) && (uint32_t)k != i)
            target_link_other((int32_t)i, target);
}

/* a digger unloaded: its order in transit (state 5) goes on (0xc), any other restarts (1) */
static void digger_unloaded(int32_t k, uint32_t unit, int32_t target)
{
    LOG(STR(0x1015b33c), k, unit, target);
    int32_t t = TARGET_ORDER(target);
    if (t == -1)
        return;
    if (TASK_STATE(t) == 5) {
        LOG(STR(0x1015b374));
        TASK_STATE(t) = 0xc;
    } else {
        LOG(STR(0x1015b390), TASK_STATE(t));
        TASK_STATE(t) = 1;
    }
}

/* the counts of virused structures, in all (+0x6e) and per type */
static void virus_count(int32_t k, const uint8_t *u, int delta)
{
    P(int16_t, AI, k * 0x143 + 0x7a) += (int16_t)delta;
    P(int16_t, AI, k * 0x143 + P(int32_t, u, 0x10) / 4 * 2 - 0x20 + 0x7c) += (int16_t)delta;
}

/* 0x100e42a6: event `ev` of player `k`, about unit `unit`. Events 0, 1 and 0x10 are noted for
 * every game; the others only for a computer player once the game runs. */
void __cdecl ai_event(int32_t k, uint32_t ev, uint32_t unit, uint32_t arg)
{
    if (I(int32_t, 0x175) || !AI)
        return;
    uint8_t *u = unit ? I(uint8_t **, ISO_UNITS)[unit] : 0;
    int32_t target = u ? P(int16_t, u, 0x1b1) : -1;
    switch (ev) {
    case 0:
        target_new(unit);
        return;
    case 1:
        unit_target_free(unit, arg);
        return;
    case 0x10:
        unit_cloaked(k, unit, target);
        return;
    }
    if (!player_is_ai(k) || !I(int32_t, 0x154) || !I(int32_t, 0x17dd))
        return;
    switch (ev) {
    case 2:
        task_type_done(k, unit >> 2, 1);
        break;
    case 3:
        unit_squad_count(unit, arg);
        break;
    case 4:
        digger_unloaded(k, unit, target);
        break;
    case 5:
        if (u && P(int32_t, u, 0xc) >= 0x40 && P(int32_t, u, 0xc) <= 0x9f && P(int32_t, PLAYER(k), 0xf2) == -1)
            structure_lost(unit, k);
        break;
    case 6:
    case 7:
    case 8:
        break;
    case 9:
        LOG(STR(0x1015b3a8), k);
        break;
    case 10:
        LOG(STR(0x1015b3c8), k);
        break;
    case 11:
        LOG(STR(0x1015b3f8), k, unit);
        virus_count(k, u, 1);
        break;
    case 12:
        LOG(STR(0x1015b430), k, unit);
        virus_count(k, u, -1);
        break;
    case 13: {
        int32_t s = squad_new_raw(k, 2, 1, 0);
        squad_take(target, s);
        P(int32_t, SQUAD(s), 0x14) = 9;
        LOG(STR(0x1015b470), k, unit);
        break;
    }
    case 14:
        LOG(STR(0x1015b4a8), k, unit);
        if (unit)
            refinery_sell(unit);
        break;
    case 15:
        LOG(STR(0x1015b4e8), k, unit);
        if (unit)
            unit_stop(unit);
        break;
    default:
        LOG(STR(0x1015b518), ev, unit);
        break;
    }
}
