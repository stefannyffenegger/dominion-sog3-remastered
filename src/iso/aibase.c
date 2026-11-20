/* The game logic, 0x10092060-0x1009350e: the AI's squads by kind, a new squad, the base's
 * building list, rebuilding lost buildings, and the squad review. */
#include "iso_core.h"
#include "aibuild.h"
#include "aibase.h"

#define AI G(uint8_t *, 0x1018b604)
#define TASKS G(uint8_t *, 0x1018b600)
#define SQUADS G(uint8_t *, 0x1018b608)
#define TARGETS G(uint8_t *, 0x1018b610)
#define BUILD_LIST ((const uint8_t *)ORIG_PTR(0x10152418)) /* per race 0x90 bytes: 18 x {kind, count} */

#define ai_list_first ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100e2ee5)
#define squad_next ORIG(int32_t(__cdecl *)(int32_t), 0x100c038c)
#define squad_new ORIG(int32_t(__cdecl *)(int32_t, int, int, int), 0x100c0462)
#define ai_squads_changed ORIG(void(__cdecl *)(int32_t, int), 0x100e48de)
#define target_next ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x1007223b)
#define can_rebuild ORIG(int(__cdecl *)(int32_t, int32_t, uint32_t), 0x100a0339)
#define unit_building ORIG(int(__cdecl *)(uint32_t), 0x1009eeb4)
#define task_new_rebuild ORIG(void(__cdecl *)(int32_t, int32_t, int32_t, int), 0x100616d4)
#define kind_cost_men ORIG(int32_t(__cdecl *)(uint32_t), 0x1009da9a)
#define kind_cost_materials ORIG(int32_t(__cdecl *)(uint32_t), 0x1009d9cd)
#define ai_reserve ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x1005e1c0)
#define ai_request_unit ORIG(void(__cdecl *)(int32_t, int, int, int, int32_t, int, int), 0x1005d140)
#define ai_timing_start ORIG(void(__cdecl *)(void), 0x10096250)

/* 0x10092060: the player's squad of `kind`: -0x1e its base squad, -0x14 the first squad,
 * otherwise the first with that kind (+0x18). -1 if none */
int32_t __cdecl ai_squad_of_kind(int32_t player, int32_t kind)
{
    if (kind == -0x1e)
        return P(int16_t, AI, player * 0x143 + 0x123);
    int32_t s = ai_list_first(player, 3);
    while (s != -1 && kind != -0x14 && P(int32_t, SQUADS, s * 0x130 + 0x18) != kind)
        s = squad_next(s);
    return s;
}

/* 0x10092102: a new squad of `kind` for the player (made the base squad with `base`), with
 * a second squad (+0xdc) of its own. The squad, or -1 */
int32_t __cdecl ai_squad_new(int32_t player, uint32_t kind, int base)
{
    int32_t s = squad_new(player, 3, 8, 0);
    uint8_t *ai = AI + player * 0x143;
    if (s != -1) {
        P(uint8_t, ai, 0x116)++;
        P(int16_t, ai, 0x121) = (int16_t)s;
        if (base)
            P(int16_t, ai, 0x123) = (int16_t)s;
        P(uint32_t, SQUADS, s * 0x130 + 0x18) = kind;
        P(int32_t, SQUADS, s * 0x130 + 0xdc) = squad_new(player, 2, 7, 0);
        P(int32_t, SQUADS, s * 0x130 + 300) = 1;
        ai_squads_changed(player, 0);
    }
    return s;
}

/* 0x10092ade: while the player has lost buildings (+0x6a), its task's squad's targets that
 * can be rebuilt by its race, are not being built and not flagged (+0xf1), get a rebuild
 * task */
void __cdecl ai_rebuild_lost(int32_t player, int32_t task)
{
    int32_t t = P(int32_t, SQUADS, P(int32_t, TASKS, task * 0x74 + 0x38) * 0x130 + 0x24);
    if (P(int32_t, AI, player * 0x143 + 0x6a) <= 0)
        return;
    for (; t != -1; t = target_next(player, t)) {
        int32_t id = P(int32_t, TARGETS, t * 0x120 + 0x20);
        if (!id)
            continue;
        uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
        if (can_rebuild(player, P(int32_t, TARGETS, t * 0x120 + 0x24), P(uint8_t, AI, player * 0x143 + 0xd)) &&
            !unit_building((uint16_t)id) && u && !P(uint8_t, u, 0xf1))
            task_new_rebuild(player, t, task, 10);
    }
}

/* 0x10092c44: the base's building list (per race, 18 steps of {kind, count}, at the step
 * +0x102): if the player can afford it and has fewer of the step's kind than its count (and
 * the task is not the one already building, +0x13b), it reserves the costs (when it has
 * them) and asks for one; a power plant (0x17) when it has none sets +0x14b. The step moves
 * on either way */
void __cdecl ai_build_list_step(int32_t player, int32_t task)
{
    uint8_t *ai = AI + player * 0x143;
    int32_t race = P(int32_t, ai, 0xfe);
    int16_t step = (int16_t)P(int32_t, ai, 0x102);
    int32_t men = P(int32_t, ai, 0x10a), materials = P(int32_t, ai, 0x10e);
    if (race < 0 || !ai_can_afford(player))
        return;
    int16_t kind = (int16_t)P(int32_t, BUILD_LIST, race * 0x90 + step * 8);
    int16_t count = (int16_t)P(int32_t, BUILD_LIST, race * 0x90 + step * 8 + 4);
    if ((int32_t)P(uint16_t, ai, 0xac + kind * 2) < count && P(int32_t, ai, 0x13b) != task) {
        uint32_t k = P(uint8_t, ai, 0xd) + kind * 4;
        int32_t cm = kind_cost_men(k), cmat = kind_cost_materials(k);
        if (men <= cm && materials <= cmat)
            ai_reserve(player, cmat + 10, cm + 10);
        ai_request_unit(player, 4, kind, 5, task, 0, 4);
        if (kind == 0x17 && !P(uint16_t, ai, 0xac + kind * 2))
            P(int32_t, ai, 0x14b) = 1;
    }
    P(int32_t, ai, 0x102) = (int16_t)((step + 1) % 0x12);
}

/* 0x100933da: the squad review: only its timing remains */
int32_t __cdecl ai_squad_review(int32_t squad)
{
    ai_timing_start();
    return squad;
}
