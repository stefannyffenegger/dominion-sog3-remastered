/* The game logic, 0x100c0350-0x100c0462: the AI's squad list (squads are 0x130-byte records
 * at G(0x1018b608), +4 the next, +8 the previous; the first is at +8 of the block at
 * G(0x1018b604)). */
#include "iso_core.h"
#include "squadlist.h"

#define SQUAD(k) (G(uint8_t *, 0x1018b608) + (k) * 0x130)

/* 0x100c0350 */
int32_t __cdecl squad_first(void)
{
    return P(int32_t, G(uint8_t *, 0x1018b604), 8);
}

/* 0x100c0368 */
int32_t __cdecl squad_set_first(int32_t k)
{
    P(int32_t, G(uint8_t *, 0x1018b604), 8) = k;
    return P(int32_t, G(uint8_t *, 0x1018b604), 8);
}

/* 0x100c038c */
int32_t __cdecl squad_next(int32_t k)
{
    return P(int32_t, SQUAD(k), 4);
}

/* 0x100c03b4 */
int32_t __cdecl squad_set_next(int32_t k, int32_t next)
{
    P(int32_t, SQUAD(k), 4) = next;
    return P(int32_t, SQUAD(k), 4);
}

/* 0x100c03f7 */
int32_t __cdecl squad_prev(int32_t k)
{
    return P(int32_t, SQUAD(k), 8);
}

/* 0x100c041f */
int32_t __cdecl squad_set_prev(int32_t k, int32_t prev)
{
    P(int32_t, SQUAD(k), 8) = prev;
    return P(int32_t, SQUAD(k), 8);
}

#define squad_link ORIG(void(__cdecl *)(int32_t, int32_t), 0x100c0d5a)
#define ai_group_take ORIG(void(__cdecl *)(uint32_t, int), 0x100e487c)

/* 0x100c0462: makes a new squad of kind `kind` for computer player `player` (its block:
 * 0x143 bytes each at G(0x1018b604), +0x72 the number of squads made) with task `task` and
 * the unit types `types` (up to 16; may be 0), takes it from the free list (0x100c0d5a)
 * and hands it to the player (0x100e487c). The squad, or -1 when none is free. The debug
 * line "CREATING GROUP: ..." (and the name of the task it needs) is compiled out. */
int32_t __cdecl squad_new(int32_t player, int32_t kind, int32_t task, const int32_t *types)
{
    int32_t k = squad_first();
    if (k == -1)
        return k;
    P(uint16_t, G(uint8_t *, 0x1018b604) + player * 0x143, 0x72)++;
    uint8_t *s = SQUAD(k);
    P(int32_t, s, 0xc) = kind;
    P(int32_t, s, 0x20) = player;
    P(int32_t, s, 0x10) = task;
    P(int32_t, s, 0) = 1;
    P(int32_t, s, 0x24) = -1;
    P(int32_t, s, 0x2c) = -1;
    P(int32_t, s, 0x30) = -1;
    P(int32_t, s, 0xdc) = -1;
    P(int32_t, s, 0x1c) = -1;
    P(int32_t, s, 0x114) = 0;
    P(int32_t, s, 0x118) = 0;
    P(int32_t, s, 0x120) = 0;
    P(int32_t, s, 0x11c) = 0;
    P(int32_t, s, 0x124) = -1;
    P(int16_t, s, 0x34) = 0;
    P(int32_t, s, 0x128) = 0;
    P(int32_t, s, 0x14) = 1;
    for (int i = 0; i < 0x29; i++) {
        P(int16_t, SQUAD(k), 0x38 + i * 2) = 0;
        P(int16_t, SQUAD(k), 0x8a + i * 2) = 0;
        if (types && i < 0x10)
            P(int16_t, SQUAD(k), 0x8a + i * 2) = (int16_t)types[i];
    }
    squad_link(player, k);
    if (P(int32_t, G(uint8_t *, 0x1018b604), 0xa48) < k)
        P(int32_t, G(uint8_t *, 0x1018b604), 0xa48) = k;
    ai_group_take(player, k);
    return k;
}
