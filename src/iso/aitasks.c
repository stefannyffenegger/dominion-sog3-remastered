/* The game logic, 0x1004baf0-0x1004c950: the AI's task pool. Tasks are 0x74-byte
 * records (G(0x1018b600)); each AI player (G(0x1018b604), 0x143 bytes each) keeps a
 * doubly linked list of its tasks, the free ones are linked from G(0x1018b604)+4.
 * Every AI turn each task of a player takes a step; when its steps are used up its
 * handler (by type, +0x30) runs. */
#include "iso_core.h"
#include "aitasks.h"

/* not rewritten yet (names ours, provisional) */
#define task_finish ORIG(void(__cdecl *)(int32_t), 0x1004c1bf)
#define ai_timing_log ORIG(void(__cdecl *)(int, const char *, uint32_t), 0x1009626b)
#define ai_player_review ORIG(void(__cdecl *)(int32_t), 0x100c2d66)
#define ai_player_plan ORIG(void(__cdecl *)(int32_t), 0x100c2f86)
#define ai_list_last ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100e2e62)
#define ai_list_set_last ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x100e2e92)
#define ai_list_first ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100e2ee5)
#define ai_list_set_first ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x100e2f15)

#define TASKS G(uint8_t *, 0x1018b600)
#define AI G(uint8_t *, 0x1018b604)
#define TASK(i, off) P(int32_t, TASKS, (i) * 0x74 + (off))
#define PLAYER(p, type, off) P(type, AI, (p) * 0x143 + (off))

/* 0x1004baf0 */
int32_t __cdecl task_free_head(void)
{
    return P(int32_t, AI, 4);
}

/* 0x1004bb08 */
int32_t __cdecl task_set_free_head(int32_t t)
{
    P(int32_t, AI, 4) = t;
    return P(int32_t, AI, 4);
}

/* 0x1004bb2c */
int32_t __cdecl task_next(int32_t t)
{
    return TASK(t, 0x14);
}

/* 0x1004bb53 */
int32_t __cdecl task_set_next(int32_t t, int32_t next)
{
    TASK(t, 0x14) = next;
    return TASK(t, 0x14);
}

/* 0x1004bb94 */
int32_t __cdecl task_prev(int32_t t)
{
    return TASK(t, 0x18);
}

/* 0x1004bbbb */
int32_t __cdecl task_set_prev(int32_t t, int32_t prev)
{
    TASK(t, 0x18) = prev;
    return TASK(t, 0x18);
}

/* 0x1004c071: a task taken from the free list into the player's list (list 7); -1 when
 * none was free */
int32_t __cdecl task_link(int32_t player, int32_t t)
{
    if (t == -1)
        return t;
    int32_t free = task_free_head();
    int32_t list = TASK(free, 0x34);
    int32_t last = ai_list_last(player, list);
    if (last == -1) {
        if (free == -1)
            return -1;
        task_set_free_head(TASK(free, 0x14));
        ai_list_set_last(player, list, t);
        ai_list_set_first(player, list, t);
        task_set_next(free, -1);
        task_set_prev(free, -1);
    } else {
        task_set_free_head(TASK(free, 0x14));
        task_set_next(last, t);
        task_set_next(t, -1);
        task_set_prev(t, last);
        ai_list_set_last(player, list, t);
    }
    return t;
}

/* 0x1004bbfc: a new task of the player: type, list, steps until its handler runs */
int32_t __cdecl task_new(int32_t player, int32_t type, int32_t list, int32_t steps)
{
    int32_t t = task_free_head();
    if (t != -1) {
        PLAYER(player, int16_t, 0x70)++;
        TASK(t, 0x30) = type;
        TASK(t, 0x34) = list;
        TASK(t, 8) = player;
        TASK(t, 0x10) = steps;
        TASK(t, 0) = 1;
        TASK(t, 4) = 1;
        TASK(t, 0xc) = 0;
        TASK(t, 0x38) = -1;
        TASK(t, 0x48) = -1;
        TASK(t, 0x40) = -1;
        TASK(t, 0x44) = -1;
        TASK(t, 0x2c) = 1;
        for (int i = 0; i < 10; i++)
            TASK(t, 0x4c + i * 4) = -1;
        task_link(player, t);
    }
    if (P(int32_t, AI, 0xa44) < t)
        P(int32_t, AI, 0xa44) = t;
    return t;
}

/* 0x1004bdf3: the steps until the task's handler runs (-1: never) */
int32_t __cdecl task_set_steps(int32_t t, int32_t steps)
{
    if (steps < 1)
        steps = -1;
    if (t != -1)
        TASK(t, 0x10) = steps;
    return steps;
}

/* 0x1004c75a: the task's handler, by type; 0 for types without one */
int32_t __cdecl task_run(int32_t t)
{
    typedef int32_t(__cdecl * Handler)(int32_t);
    uint32_t h;
    switch (TASK(t, 0x30)) {
    case 0: h = 0x100612f2; break;
    case 1: h = 0x10060e41; break;
    case 2: h = 0x10061482; break;
    case 7: h = 0x10061f17; break;
    case 8: h = 0x10063a72; break;
    case 0xb: h = 0x10061579; break;
    case 0xd: h = 0x100617f9; break;
    case 0xe: h = 0x100626bb; break;
    case 0xf: h = 0x10062d10; break;
    case 0x10: h = 0x10063294; break;
    case 0x11: h = 0x100646c8; break;
    case 0x12: h = 0x10064a75; break;
    case 0x13: h = 0x10066837; break;
    case 0x14: h = 0x10064f4b; break;
    case 0x15: h = 0x100656bb; break;
    case 0x16: h = 0x100613b9; break;
    default: return 0;
    }
    return ((Handler)ORIG_PTR(h))(t);
}

/* 0x1004bf7c: one step of a task; when its steps are used up its handler runs, and a
 * task without the repeat flag (+0x20) finishes. The task to go on with */
int32_t __cdecl task_step(int32_t t)
{
    int32_t next = task_next(t);
    TASK(t, 0xc)++;
    if ((uint32_t)TASK(t, 0x10) <= (uint32_t)TASK(t, 0xc)) {
        task_run(t);
        if (!TASK(t, 0))
            return -1;
        if (!TASK(t, 0x20)) {
            task_finish(t);
            return next;
        }
        TASK(t, 0xc) = 0;
    }
    return t;
}

/* 0x1004be3b: the player's tasks take a step (an AI turn) */
void __cdecl ai_run_tasks(int32_t player)
{
    if (!PLAYER(player, uint16_t, 0x70))
        return;
    uint32_t start = timeGetTime();
    int32_t t = ai_list_first(player, 7);
    while (t > -1 && t < 2000) {
        int32_t next = task_next(t);
        if (!TASK(t, 0))
            return;
        if (TASK(t, 4))
            task_step(t);
        t = next;
    }
    ai_timing_log(0x10, (const char *)ORIG_PTR(0x1013a508), start);
    if ((uint32_t)PLAYER(player, int32_t, 0x1e) - 1 <= (uint32_t)PLAYER(player, int32_t, 0x1a)) {
        ai_player_review(player);
        ai_player_plan(player);
    }
}
