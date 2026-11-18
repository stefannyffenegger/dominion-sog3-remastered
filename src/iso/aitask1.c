/* The game logic, 0x10060d40-0x100616d4: the first of the AI's task handlers (run by task_run
 * by the task's type) and the functions that create those tasks: recon (type 1), scanning for
 * threats (0), building (2), the base's buildings (0xb) and type 0x16. */
#include "iso_core.h"
#include "aitasks.h"
#include "aisquads.h"
#include "aibuild.h"
#include "aitask1.h"

#define TASKS G(uint8_t *, 0x1018b600)
#define AI G(uint8_t *, 0x1018b604)
#define SQUADS G(uint8_t *, 0x1018b608)
#define TARGETS G(uint8_t *, 0x1018b610)
#define TASK(i, off) P(int32_t, TASKS, (i) * 0x74 + (off))
#define SQUAD(s, off) P(int32_t, SQUADS, (s) * 0x130 + (off))
#define TARGET(t, off) P(int32_t, TARGETS, (t) * 0x120 + (off))
#define PLAYER(p, type, off) P(type, AI, (p) * 0x143 + (off))

#define debug_print ORIG(void(__cdecl *)(int, const char *), 0x10095e98)
#define ai_timing_start ORIG(void(__cdecl *)(void), 0x10096250)
#define ai_timing_log ORIG(void(__cdecl *)(int, const char *, uint32_t), 0x1009626b)
#define ai_request_unit ORIG(void(__cdecl *)(int32_t, int, int, int, int32_t, int, int), 0x1005d140)
#define target_next ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x1007223b)
#define unit_kill_quiet ORIG(void(__cdecl *)(uint32_t), 0x1007d323)
#define base_check_buildings ORIG(void(__cdecl *)(int32_t, int32_t), 0x10092ade)
#define base_check_repairs ORIG(void(__cdecl *)(int32_t, int32_t), 0x10092c44)
#define unit_set_order ORIG(void(__cdecl *)(uint32_t, int), 0x1009ce9f)
#define unit_set_flag ORIG(void(__cdecl *)(uint32_t, int), 0x1009fc30)
#define player_men ORIG(uint32_t(__cdecl *)(int32_t), 0x100a04d3)
#define squad_new ORIG(int32_t(__cdecl *)(int32_t, int, int, int), 0x100c0462)
#define squad_add_target_ ORIG(void(__cdecl *)(int32_t, int32_t), 0x100c1344)
#define ai_type16_run ORIG(void(__cdecl *)(int32_t), 0x100e3f0d)

/* 0x10060d40: a recon task for `player` wanting `wanted` men, with its squad */
int32_t __cdecl task_new_recon(int32_t player, uint32_t wanted, int32_t steps)
{
    int32_t t = task_new(player, 1, 7, steps);
    int32_t squad = PLAYER(player, int32_t, 0x129);
    if (t != -1) {
        TASK(t, 0x4c) = (int32_t)wanted;
        TASK(t, 0x50) = 1;
        TASK(t, 0x58) = 0;
        TASK(t, 0x38) = squad;
        if (squad != -1)
            SQUAD(squad, 0x1c) = t;
        TASK(t, 0x20) = 1;
    }
    return t;
}

/* 0x10060e41: the recon task: gets its squad (the player's recon squad, or a new one), adds
 * the target it was given, asks for recon units while the squad has fewer than it wants
 * (0x50) and the player more men than 0x4c, and sends the squad's units of types 0x1e and
 * 0x23 out. 1 if it sent any */
int32_t __cdecl task_recon(int32_t t)
{
    int sent = 0, prio = 5;
    ai_timing_start();
    int32_t player = TASK(t, 8);
    int32_t squad = TASK(t, 0x38);
    int32_t target = TASK(t, 0x48);
    int32_t state = TASK(t, 0x2c);
    if (state == 1) {
        if (squad == -1) {
            TASK(t, 0x38) = PLAYER(player, int32_t, 0x129);
            squad = TASK(t, 0x38);
        }
        if (squad == -1) {
            TASK(t, 0x38) = squad_new(player, 2, 3, 0);
            squad = TASK(t, 0x38);
            PLAYER(player, int32_t, 0x129) = squad;
        }
        if (squad == -1)
            return 0;
        TASK(t, 0x2c) = 4;
        SQUAD(squad, 0x14) = 4;
    }
    if (target != -1 && squad != -1) {
        squad_add_target_(squad, target);
        TASK(t, 0x48) = -1;
        TASK(t, 0x58)--;
    }
    uint32_t men = player_men(player);
    uint32_t have = (uint32_t)TASK(t, 0x58) + P(uint16_t, SQUADS, squad * 0x130 + 0x34);
    uint32_t want = (uint32_t)TASK(t, 0x50);
    if (state == 4 && have <= want) {
        if (have == 0) {
            prio = 10;
        } else {
            TASK(t, 0x2c) = 3;
            SQUAD(squad, 0x14) = 3;
            state = 3;
        }
        if ((uint32_t)TASK(t, 0x4c) < men && have < want) {
            ai_request_unit(player, 5, 0xf, prio, t, 1, 10);
            TASK(t, 0x58)++;
        }
    }
    if (state == 3) {
        for (int32_t k = SQUAD(squad, 0x24); k != -1 && TARGET(k, 0x20); k = target_next(player, k)) {
            uint8_t *u = I(uint8_t **, ISO_UNITS)[TARGET(k, 0x20)];
            if (P(uint8_t, u, 0x196) == 0x1e || P(uint8_t, u, 0x196) == 0x23) {
                unit_set_order((uint32_t)TARGET(k, 0x20), '(');
                unit_set_flag((uint32_t)TARGET(k, 0x20), 1);
                sent = 1;
            }
        }
        TASK(t, 0x2c) = 4;
    }
    ai_timing_log(0x10, (const char *)ORIG_PTR(0x1013f990), 0);
    return sent;
}

static int32_t new_task_state3(int32_t player, int type, int32_t steps)
{
    int32_t t = task_new(player, type, 7, steps);
    if (t != -1) {
        TASK(t, 0x20) = 1;
        TASK(t, 0x2c) = 3;
    }
    return t;
}

/* 0x10061285: a task scanning for threats */
int32_t __cdecl task_new_scan(int32_t player, int32_t steps)
{
    int32_t t = task_new(player, 0, 7, steps);
    if (t != -1) {
        TASK(t, 0x2c) = 3;
        TASK(t, 0x20) = 1;
    }
    return t;
}

/* 0x100612f2: scanning for threats: assigns the AI's targets */
int32_t __cdecl task_scan(int32_t t)
{
    uint32_t start = timeGetTime();
    ai_assign_targets(TASK(t, 8));
    ai_timing_log(0x10, (const char *)ORIG_PTR(0x1013f9a0), start);
    return 1;
}

/* 0x1006134c: a task of type 0x16 */
int32_t __cdecl task_new_16(int32_t player, int32_t steps) { return new_task_state3(player, 0x16, steps); }

/* 0x100613b9: the task of type 0x16 */
int32_t __cdecl task_16(int32_t t)
{
    int32_t player = TASK(t, 8);
    debug_print(0x10, (const char *)ORIG_PTR(0x1013f9b0));
    ai_type16_run(player);
    debug_print(0x10, (const char *)ORIG_PTR(0x1013f9e8));
    return 1;
}

/* 0x10061415: a building task */
int32_t __cdecl task_new_build(int32_t player, int32_t steps) { return new_task_state3(player, 2, steps); }

/* 0x10061482: the building task: the AI's building turn */
int32_t __cdecl task_build(int32_t t)
{
    ai_timing_start();
    int32_t r = ai_build_turn(TASK(t, 8));
    ai_timing_log(0x10, (const char *)ORIG_PTR(0x1013fa20), 0);
    return r;
}

/* 0x100614d7: a task keeping up the base's buildings, with the player's base squad */
int32_t __cdecl task_new_base(int32_t player, int32_t steps)
{
    int32_t t = task_new(player, 0xb, 7, steps);
    if (t != -1) {
        TASK(t, 0x20) = 1;
        TASK(t, 0x38) = PLAYER(player, int16_t, 0x123);
        TASK(t, 0x2c) = 3;
    }
    return t;
}

/* 0x10061579: keeping up the base: a target it was given goes to the base squad (a unit of
 * kind 0x10-0x13 there is removed), then the buildings and repairs are checked */
int32_t __cdecl task_base(int32_t t)
{
    uint32_t start = timeGetTime();
    int32_t player = TASK(t, 8);
    int32_t target = TASK(t, 0x48);
    if (target != -1) {
        squad_add_target_(PLAYER(player, int16_t, 0x121), target);
        TASK(t, 0x48) = -1;
        uint32_t kind = (uint32_t)TARGET(target, 0x24);
        if ((kind & 0x3fffffff) > 0xf && kind << 2 < 0x50)
            unit_kill_quiet((uint32_t)TARGET(target, 0x20));
    }
    base_check_repairs(player, t);
    base_check_buildings(player, t);
    ai_timing_log(0x10, (const char *)ORIG_PTR(0x1013fa3c), start);
    return 0;
}

/* ---- 0x10062630-0x10062b12 (part 66) ---- */

#define task_next_ ORIG(int32_t(__cdecl *)(int32_t), 0x1004bb2c)
#define ai_list_first ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100e2ee5)
#define target_to_base ORIG(void(__cdecl *)(int32_t, int32_t, uint32_t, int32_t, int), 0x10093150)
#define squad_review ORIG(void(__cdecl *)(int32_t, int32_t, int), 0x100933da)
#define ai_build_done ORIG(void(__cdecl *)(int32_t), 0x1005e21d)
#define can_build ORIG(int(__cdecl *)(int32_t, int32_t, uint32_t, int), 0x1009e9a3)
#define player_men_total ORIG(int32_t(__cdecl *)(int32_t), 0x100a0285)

/* 0x10062630: the reserves task (type 0xe), unless the player has one already (-1) */
int32_t __cdecl task_new_reserves(int32_t player, int32_t steps)
{
    for (int32_t t = ai_list_first(player, 7); t != -1; t = task_next_(t))
        if (TASK(t, 0x30) == 0xe)
            return -1;
    return task_new(player, 0xe, 7, steps);
}

/* 0x100626bb: building reserves: a target it was given goes to its squad and the base; at
 * first it notes the player's men (up to 500) and takes the reserve squad; later, when no
 * building is under way and the player can afford it, it asks for the first unit kind from
 * 0xe down (not 8) the player can build. 1 when it asked */
int32_t __cdecl task_reserves(int32_t t)
{
    uint32_t start = timeGetTime();
    int32_t player = TASK(t, 8);
    uint8_t *ai = AI + player * 0x143;
    uint32_t race = P(uint8_t, ai, 0xd);
    int32_t squad = TASK(t, 0x38);
    int32_t men = player_men_total(player);
    int32_t base_squad = P(int16_t, ai, 0x121);
    TASK(t, 0x58)++;
    int32_t state = TASK(t, 0x2c);
    int32_t target = TASK(t, 0x48);
    if (target != -1) {
        squad_add_target_(TASK(t, 0x38), target);
        uint32_t n = (uint32_t)TASK(t, 0x64);
        TASK(t, 0x64)++;
        target_to_base(target, base_squad, n, TASK(t, 0x64) % 6, 1);
        TASK(t, 0x48) = -1;
        TASK(t, 0x2c) = 2;
        state = 2;
    }
    if (state == 1) {
        TASK(t, 0x4c) = men >= 500 ? 500 : men;
        TASK(t, 0x2c) = 2;
        TASK(t, 0x38) = P(int16_t, ai, 0x135);
        TASK(t, 0x20) = 1;
        ai_timing_log(0x10, (const char *)ORIG_PTR(0x1013fc1c), start);
        return 1;
    }
    int32_t slot = P(int32_t, ai, 0x13f);
    if (slot == -1)
        slot = P(int32_t, ai, 0x13b);
    if (slot == -1)
        slot = P(int32_t, ai, 0x143);
    if (slot == -1)
        slot = P(int32_t, ai, 0x147);
    if (slot != -1) {
        ai_timing_log(0x10, (const char *)ORIG_PTR(0x1013fc40), start);
        return 0;
    }
    if (TASK(t, 0x58) % 10 == 0)
        squad_review(squad, base_squad, 0);
    if (!ai_can_afford(player) || P(uint16_t, ai, 0x76) > P(uint16_t, ai, 0x78)) {
        ai_set_lethality(player, 0);
        ai_timing_log(0x10, (const char *)ORIG_PTR(0x1013fc68), start);
        return 0;
    }
    if (state == 2) {
        for (int kind = 0xe; kind >= 0; kind--) {
            if (kind == 8 || !can_build(player, kind, race, 1))
                continue;
            ai_request_unit(player, kind > 5 ? 5 : 6, kind, 3, t, 0, 10);
            ai_build_done(player);
            ai_set_lethality(player, 0);
            TASK(t, 0x2c) = 4;
            ai_timing_log(0x10, (const char *)ORIG_PTR(0x1013fc90), start);
            return 1;
        }
    }
    ai_timing_log(0x10, (const char *)ORIG_PTR(0x1013fcbc), start);
    return 0;
}
