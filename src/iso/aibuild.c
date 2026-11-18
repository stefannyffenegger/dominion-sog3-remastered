/* The game logic, 0x1005d140-0x1005e5e0 (the parts every game runs): the AI's building
 * tasks (list 8) and whether a player can afford something. */
#include "iso_core.h"
#include "aitasks.h"
#include "aibuild.h"

/* not rewritten yet (names ours, provisional) */
#define task_finish ORIG(void(__cdecl *)(int32_t), 0x1004c1bf)
#define build_task_try ORIG(int32_t(__cdecl *)(int32_t), 0x1005db67)
#define IsoBuildingOnTabset ORIG(int32_t(__cdecl *)(uint8_t *, int32_t, int32_t), 0x100a5bf4)
#define ai_list_first ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100e2ee5)
#define player_men ORIG(int32_t(__cdecl *)(int32_t), 0x100a0285)
#define player_materials ORIG(int32_t(__cdecl *)(int32_t), 0x100a02b6)

#define AI G(uint8_t *, 0x1018b604)
#define TASK_PRIO(t) P(uint32_t, G(uint8_t *, 0x1018b600), (t) * 0x74 + 0x28)

/* tries the player's building tasks whose priority fits, in list order; 1 when one
 * went ahead. *found: one fitted */
static int try_tasks(int32_t player, uint32_t lo, uint32_t hi, int *found)
{
    int32_t t = ai_list_first(player, 8);
    while (t != -1) {
        int32_t next = task_next(t);
        if (TASK_PRIO(t) >= lo && TASK_PRIO(t) <= hi) {
            if (found)
                *found = 1;
            if (build_task_try(t))
                return 1;
        }
        t = next;
    }
    return 0;
}

/* 0x1005d7c1: the AI player's building turn: the building tasks of the four tab sets
 * whose building is gone are finished; then the waiting tasks are tried by priority:
 * 10, then 7-9, then 3-6, and only when none of those exists, 0-3. 1 when one went
 * ahead */
int32_t __cdecl ai_build_turn(int32_t player)
{
    uint8_t *p = AI + player * 0x143;
    int found = 0;
    ai_list_first(player, 8);
    static const int32_t slot[4] = {0x13f, 0x13b, 0x143, 0x147}, tabset[4] = {3, 0, 1, 2};
    for (int i = 0; i < 4; i++)
        if (P(int32_t, p, slot[i]) != -1 && IsoBuildingOnTabset(g_iso, player, tabset[i]) == -1) {
            task_finish(P(int32_t, p, slot[i]));
            P(int32_t, p, slot[i]) = -1;
        }
    if (try_tasks(player, 10, 10, &found) || try_tasks(player, 7, 9, &found) || try_tasks(player, 3, 6, &found))
        return 1;
    if (found)
        return 0;
    return try_tasks(player, 0, 3, NULL);
}

/* 0x1005e4b6: whether the player has the men and materials its AI wants (+0x10a,
 * +0x10e) */
int32_t __cdecl ai_can_afford(int32_t player)
{
    uint8_t *p = AI + player * 0x143;
    if (P(int32_t, p, 0x10a) <= player_men(player) && P(int32_t, AI + player * 0x143, 0x10e) <= player_materials(player))
        return 1;
    return 0;
}
