/* The game logic, 0x100c07c0-0x100c0eab: a computer player taking note of a new target of
 * its own, and linking a new squad into its lists (squads: 0x130-byte records at
 * G(0x1018b608)). */
#include "iso_core.h"
#include "squadlist.h"
#include "squadnote.h"

#define SQUAD(k) (G(uint8_t *, 0x1018b608) + (k) * 0x130)

/* ---- 0x100c07c0-0x100c0d5a ---- */

#define TARGET(t) (G(uint8_t *, 0x1018b610) + (t) * 0x120)
#define TASK(k) (G(uint8_t *, 0x1018b600) + (k) * 0x74)
#define task_wake ORIG(void(__cdecl *)(int32_t), 0x1005df77)
#define ai_squad_new ORIG(int32_t(__cdecl *)(int32_t, uint32_t, int), 0x10092102)
#define squad_add_target ORIG(void(__cdecl *)(int32_t, int32_t), 0x100c1344)
#define ai_squads_changed ORIG(int32_t(__cdecl *)(int32_t, int), 0x100e48de)
#define ai_group_done ORIG(void(__cdecl *)(uint32_t), 0x100e4969)
#define power_check ORIG(uint32_t(__cdecl *)(int16_t), 0x10001f74)
#define unit_kill_quiet ORIG(void(__cdecl *)(uint32_t), 0x1007d323)

/* 0x100c07c0: computer player `player` takes note of target `t` (one of its own units: +0x20
 * the unit, +0x24 its kind in the low 30 bits). Unless `quiet` (or the player's +0x106 is
 * set), a task waiting for that kind of unit takes it (by type: soldiers +0x13b, vehicles
 * +0x133, structures +0x137 or +0x12f). Otherwise structures go to the base squad (made
 * when there is none), and the others to the squad for their type (+0x121, +0x125,
 * +0x11d), else to a new squad (or, before the first tick, the first one, +0x119). Targets
 * of kinds 0x10-0x13 are then taken off the map. The squad, or -1. The debug lines are
 * compiled out. */
int32_t __cdecl target_note_new(int32_t player, int32_t t, int32_t quiet)
{
    if (t == -1 || !P(int32_t, TARGET(t), 0x20))
        return -1;
    uint8_t *pb = G(uint8_t *, 0x1018b604) + player * 0x143 + 0xc;
    uint8_t *u = I(uint8_t **, ISO_UNITS)[P(int32_t, TARGET(t), 0x20)];
    int32_t type = P(int32_t, u, 0xc);
    int32_t squad = -1, task = -1;
    if (!quiet && !P(int32_t, pb, 0x106)) {
        if (type < 0x18)
            task = P(int32_t, pb, 0x13b);
        else if (type < 0x40)
            task = P(int32_t, pb, 0x133);
        else if ((type >= 0x40 && type <= 0x4f) || (type >= 0x84 && type <= 0x87) || (type >= 0x40 && type <= 0x43) ||
                 (type >= 0x88 && type <= 0x9f) || (type >= 0x64 && type < 0x68))
            task = P(int32_t, pb, 0x137);
        else if (type >= 0x40 && type <= 0x9f)
            task = P(int32_t, pb, 0x12f);
    }
    if (task != -1) {
        P(int32_t, TASK(task), 0x48) = t;
        task_wake(task);
    } else if ((type >= 0x40 && type <= 0x87) || (type >= 0x88 && type <= 0x9f)) {
        int32_t base = P(int16_t, pb, 0x115);
        if (base == -1)
            base = P(int16_t, pb, 0x117);
        if (base == -1) {
            base = ai_squad_new(player, 1, 1);
            P(int16_t, pb, 0x117) = (int16_t)base;
            P(int16_t, pb, 0x115) = (int16_t)base;
        }
        squad_add_target(base, t);
    } else if (type >= 0x10 && type <= 0x13) {
        squad = P(int32_t, pb, 0x121);
    } else if (type / 4 == 8) {
        squad = P(int32_t, pb, 0x125);
    } else if (type >= 0x3c && type <= 0x3f) {
        squad = P(int32_t, pb, 0x11d);
    } else if (I(uint32_t, 0x17dd) > 0) {
        squad = ai_squads_changed(player, 1);
        ai_group_done(player);
    } else {
        squad = P(int16_t, pb, 0x119);
    }
    if (squad != -1) {
        squad_add_target(squad, t);
        task = P(int32_t, SQUAD(squad), 0x1c);
        if (task != -1)
            P(int32_t, TASK(task), 0x48) = t;
    }
    uint32_t kind = P(uint32_t, TARGET(t), 0x24);
    if (((kind & 0x3fffffff) >= 0x10 && (kind << 2) <= 0x4f) || ((kind & 0x3fffffff) >= 0x10 && (kind << 2) <= 0x43)) {
        power_check((int16_t)P(int32_t, TARGET(t), 0x20));
        unit_kill_quiet(P(uint32_t, TARGET(t), 0x20));
    }
    return squad;
}

/* ---- 0x100c0d5a-0x100c0eab ---- */

#define ai_list_last ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100e2e62)
#define ai_list_set_first ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x100e2f15)
#define ai_list_set_last ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x100e2e92)

/* 0x100c0d5a: takes squad `k` off the free list and appends it to computer player
 * `player`'s list of squads of its kind (+0xc). When that list is empty, the original
 * resets the links of the free list's first squad (which is `k` as squad_new calls it). `k`,
 * or -1 */
int32_t __cdecl squad_link(int32_t player, int32_t k)
{
    if (k == -1)
        return k;
    int32_t free = squad_first();
    int32_t kind = P(int32_t, SQUAD(k), 0xc);
    int32_t last = ai_list_last(player, kind);
    if (last != -1) {
        squad_set_first(P(int32_t, SQUAD(free), 4));
        squad_set_next(last, k);
        squad_set_next(k, -1);
        squad_set_prev(k, last);
        ai_list_set_last(player, kind, k);
        return k;
    }
    if (free == -1)
        return -1;
    squad_set_first(P(int32_t, SQUAD(free), 4));
    ai_list_set_last(player, kind, k);
    ai_list_set_first(player, kind, k);
    squad_set_next(free, -1);
    squad_set_prev(free, -1);
    return k;
}
