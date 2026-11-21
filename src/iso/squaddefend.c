/* The game logic, 0x100c25d2-0x100c2765: a computer player's defence squad taking up its
 * guard (squads: 0x130-byte records at G(0x1018b608): +0x14 the state, +0x20 the player,
 * +0x24 its first unit, +0x2c the target to guard). */
#include "iso_core.h"
#include "squaddefend.h"

#define SQUAD(k) (G(uint8_t *, 0x1018b608) + (k) * 0x130)
#define TARGET(t) (G(uint8_t *, 0x1018b610) + (t) * 0x120)
#define ai_timing_start ORIG(void(__cdecl *)(void), 0x10096250)
#define ai_timing_log ORIG(void(__cdecl *)(int, const char *, uint32_t), 0x1009626b)
#define unit_guard ORIG(void(__cdecl *)(int32_t, int32_t), 0x1009f71f)
#define unit_stop ORIG(void(__cdecl *)(int32_t), 0x1009f861)
#define target_next ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x1007223b)

/* 0x100c25d2: the units of squad `s` that are guarding (+0x196 0x1e) guard the squad's
 * target, or stop when it has none; the squad's state becomes 6 (with a target) or 7. The
 * time it took goes to the AI's timing log ("    Setup Defense - "). `s` */
int32_t __cdecl squad_setup_defense(int32_t s)
{
    ai_timing_start();
    int32_t player = P(int32_t, SQUAD(s), 0x20);
    int32_t t = P(int32_t, SQUAD(s), 0x24);
    int32_t guarded = P(int32_t, SQUAD(s), 0x2c);
    while (t != -1 && P(int32_t, TARGET(t), 0x20)) {
        int32_t unit = P(int32_t, TARGET(t), 0x20);
        if (P(uint8_t, I(uint8_t **, ISO_UNITS)[unit], 0x196) == 0x1e) {
            if (guarded != -1)
                unit_guard(unit, P(int32_t, TARGET(guarded), 0x20));
            else
                unit_stop(unit);
        }
        t = target_next(player, t);
    }
    P(int32_t, SQUAD(s), 0x14) = guarded != -1 ? 6 : 7;
    ai_timing_log(0x10, (const char *)ORIG_PTR(0x101588ec), 0);
    return s;
}

/* ---- 0x100c2d66-0x100c2f86 ---- */

#define ai_list_first ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100e2ee5)
#define squad_next ORIG(int32_t(__cdecl *)(int32_t), 0x100c038c)
#define squad_idle ORIG(void(__cdecl *)(int32_t), 0x100c197b)
#define target_find ORIG(int32_t(__cdecl *)(int32_t, int32_t, int, int, int, int, int), 0x1004e212)
#define target_pick ORIG(int32_t(__cdecl *)(int32_t, int, int32_t), 0x1004db61)
#define squad_attack_target ORIG(void(__cdecl *)(int32_t, int32_t), 0x100c21e9)
#define squad_attack_on ORIG(void(__cdecl *)(int32_t, int32_t), 0x100c23c8)

/* 0x100c2d66: computer player `player`'s detached attack squads (its list 2, task 2, no
 * task record +0x1c): an empty one is let go (0x100c197b); one without a squad to attack
 * (+0x30) looks for a target near its first unit (0x1004e212) and takes the target's
 * squad, or else sets up its defence; one with a squad but no target picks one of that
 * squad's units (0x1004db61) and attacks it; otherwise it keeps attacking its target. The
 * time goes to the AI's timing log ("Handle Detatched Attack Groups - ") */
void __cdecl ai_player_review(int32_t player)
{
    uint32_t start = timeGetTime();
    int32_t k = ai_list_first(player, 2);
    while (k != -1) {
        int32_t next = squad_next(k);
        if (P(int32_t, SQUAD(k), 0x1c) == -1 && P(int32_t, SQUAD(k), 0x10) == 2) {
            if (!P(uint16_t, SQUAD(k), 0x34)) {
                squad_idle(k);
                k = next;
                continue;
            }
            int32_t enemy = P(int32_t, SQUAD(k), 0x30);
            if (enemy == -1) {
                int32_t t = target_find(P(int32_t, SQUAD(k), 0x24), player, 0x14, 0, -1, 0, 0);
                if (t != -1) {
                    P(int32_t, SQUAD(k), 0x30) = P(int32_t, TARGET(t), 0x90 + player * 4);
                    P(int32_t, SQUAD(k), 0x2c) = t;
                } else {
                    squad_setup_defense(k);
                }
            } else if (P(int32_t, SQUAD(k), 0x2c) == -1) {
                squad_attack_target(k, target_pick(enemy, 0, k));
            } else {
                squad_attack_on(k, P(int32_t, SQUAD(k), 0x2c));
            }
        }
        k = squad_next(k);
    }
    ai_timing_log(0x10, (const char *)ORIG_PTR(0x10158934), start);
}

/* ---- 0x100c2f86-0x100c3171 ---- */

/* 0x100c2f86: computer player `player`'s attack squads of task 1 (list 2, no task record):
 * an empty one is let go; one waiting (state 9) is left alone; one without a squad to
 * attack (+0x30) goes to state 2 and sets up its defence; one with a squad but no target
 * picks one of its units (0x1004db61), goes to state 6 and attacks it; otherwise it keeps
 * attacking its target. The time goes to the AI's timing log ("Handle Detatched Defense
 * Groups - ") */
void __cdecl ai_player_plan(int32_t player)
{
    uint32_t start = timeGetTime();
    int32_t k = ai_list_first(player, 2);
    while (k != -1) {
        int32_t next = squad_next(k);
        if (P(int32_t, SQUAD(k), 0x1c) == -1 && P(int32_t, SQUAD(k), 0x10) == 1) {
            if (!P(uint16_t, SQUAD(k), 0x34)) {
                squad_idle(k);
                k = next;
                continue;
            }
            if (P(int32_t, SQUAD(k), 0x14) == 9) {
                k = next;
                continue;
            }
            int32_t enemy = P(int32_t, SQUAD(k), 0x30);
            if (enemy == -1) {
                P(int32_t, SQUAD(k), 0x14) = 2;
                squad_setup_defense(k);
            } else if (P(int32_t, SQUAD(k), 0x2c) == -1) {
                int32_t t = target_pick(enemy, -1, k);
                P(int32_t, SQUAD(k), 0x14) = 6;
                squad_attack_target(k, t);
            } else {
                squad_attack_on(k, P(int32_t, SQUAD(k), 0x2c));
            }
        }
        k = squad_next(k);
    }
    ai_timing_log(0x10, (const char *)ORIG_PTR(0x10158958), start);
}
