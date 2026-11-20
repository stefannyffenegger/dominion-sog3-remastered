/* The game logic, 0x10094d90-0x10095d80: the players' numbered groups (a unit's group at
 * +0x15, the members of group k of player p at 0x1017c8a0 + p*0x28 + k*4) and a group's
 * attack order. */
#include "iso_core.h"
#include "groups.h"

#define GROUPS(p, k) G(int32_t, 0x1017c8a0 + (p) * 0x28 + (k) * 4)
#define can_attack_with ORIG(int(__cdecl *)(uint32_t, uint32_t), 0x100d9855)
#define unit_order ORIG(void(__cdecl *)(uint32_t, int, int, int), 0x1009cc91)

static uint8_t *unit_ptr(int32_t id) { return I(uint8_t **, ISO_UNITS)[id]; }

/* 0x10094d90: allocates `n` records of 0x26 bytes (0x1017c888). Nonzero if it could */
int32_t __cdecl records_1017c888_alloc(int32_t n)
{
    G(int32_t, 0x1017c898) = n;
    G(int32_t, 0x1017c894) = n * 0x26;
    G(void *, 0x1017c888) = winmalloc((SIZE_T)G(int32_t, 0x1017c894), 1);
    return G(void *, 0x1017c888) != 0;
}

/* 0x100950c0: the entry of the unit's group (1-9) in its player's table at 0x1017c89c */
uint32_t __cdecl unit_group_entry(int32_t unit)
{
    uint8_t *u = unit_ptr(unit);
    if (u) {
        uint32_t g = P(uint8_t, u, 0x15);
        if (g && g < 10)
            return G(uint32_t, 0x1017c89c + g * 4 + P(uint8_t, u, 0x14) * 0x28);
    }
    return 0;
}

/* 0x1009528d: takes the unit out of its player's group slots */
void __cdecl unit_groups_drop(int32_t unit)
{
    uint32_t p = P(uint8_t, unit_ptr(unit), 0x14);
    for (int k = 0; k < 10; k++)
        if (GROUPS(p, k) == unit)
            GROUPS(p, k) = 0;
}

/* 0x1009530a: for a unit of kind 0xc, its slot (1-10) in its player's groups, or 0 */
int32_t __cdecl unit_group_slot(int32_t unit)
{
    uint8_t *u = unit_ptr(unit);
    if (!u || P(int32_t, u, 0x10) != 0xc)
        return 0;
    for (int k = 0; k < 10; k++)
        if (GROUPS(P(uint8_t, unit_ptr(unit), 0x14), k) == unit)
            return k + 1;
    return 0;
}

/* 0x10095c3b: the members of `u`'s group (same player, kind 0-7, not flagged +0x10e bits
 * 23, 17 or 3) that can attack `target` are ordered to (order 7) */
void __cdecl group_attack(uint8_t *u, uint32_t target)
{
    uint32_t group = P(uint8_t, u, 0x15);
    if ((int32_t)group <= 0)
        return;
    uint32_t player = P(uint8_t, u, 0x14);
    for (int32_t k = 0; k < I(int32_t, ISO_UNIT_COUNT); k++) {
        uint32_t id = I(uint32_t *, ISO_UNIT_LIST)[k];
        uint8_t *m = unit_ptr((int32_t)id);
        if (!m || P(uint8_t, m, 0x15) != group || P(uint8_t, m, 0x14) != player || P(int32_t, m, 0x10) < 0 ||
            P(int32_t, m, 0x10) > 7 || !target || (P(uint32_t, m, 0x10e) >> 23 & 1) || (P(uint32_t, m, 0x10e) >> 17 & 1) ||
            (P(uint32_t, m, 0x10e) >> 3 & 1))
            continue;
        if (can_attack_with(target, id))
            unit_order(id, 7, 1, 0);
    }
}
