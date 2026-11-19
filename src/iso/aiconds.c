/* The game logic, 0x1006f4d0-0x1006fd69: helpers of the AI scripts' conditions: comparing
 * with an operator, cached condition values, and whether a squad's units are under attack. */
#include "iso_core.h"
#include "aiconds.h"

#define SQUADS G(uint8_t *, 0x1018b608)
#define TARGETS G(uint8_t *, 0x1018b610)

#define ai_list_first ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100e2ee5)
#define squad_next ORIG(int32_t(__cdecl *)(int32_t), 0x100c038c)
#define target_next ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x1007223b)
#define squad_of_kind ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x10092060)
#define unit_under_attack ORIG(uint32_t(__cdecl *)(uint32_t), 0x1009e5d8)
#define difficulty ORIG(uint32_t(__cdecl *)(void), 0x100a04f4)

/* 0x1006f4d0: a OP b, for OP 0 <, 1 <=, 2 >, 3 >=, 4 ==, 5 != (others: false) */
int32_t __cdecl ai_compare(int32_t a, int32_t b, uint32_t op)
{
    switch (op) {
    case 0: return a < b;
    case 1: return a <= b;
    case 2: return a > b;
    case 3: return a >= b;
    case 4: return a == b;
    case 5: return a != b;
    }
    return 0;
}

/* a condition: +4 how it is evaluated (0 always, 1 once, 2 never: the stored value), +8
 * evaluated, +0xc the value, +0x10 its number of arguments (0-4), +0x14 the function */
typedef int32_t(__cdecl *cond_fn)();

/* 0x1006f601: a condition's value for `player` */
int32_t __cdecl ai_condition_value(int32_t player, uint8_t *c, uint32_t a, uint32_t b, uint32_t d, uint32_t e)
{
    cond_fn fn = P(cond_fn, c, 0x14);
    switch (P(int32_t, c, 4)) {
    case 0:
        switch (P(uint32_t, c, 0x10)) {
        case 0: P(int32_t, c, 0xc) = fn(player); break;
        case 1: P(int32_t, c, 0xc) = fn(player, a); break;
        case 2: P(int32_t, c, 0xc) = fn(player, a, b); break;
        case 3: P(int32_t, c, 0xc) = fn(player, a, b, d); break;
        case 4: P(int32_t, c, 0xc) = fn(player, a, b, d, e); break;
        }
        break;
    case 1:
        if (!P(int32_t, c, 8)) {
            P(int32_t, c, 0xc) = fn(player);
            P(int32_t, c, 8) = 1;
        }
        break;
    }
    return P(int32_t, c, 0xc);
}

/* 0x1006f781: evaluates a condition (no arguments) and stores its value */
void __cdecl ai_condition_update(int32_t player, uint8_t *c)
{
    P(int32_t, c, 0xc) = P(cond_fn, c, 0x14)(player);
}

/* 0x1006f7be: a condition that is always true */
int32_t __cdecl ai_condition_true(void) { return 1; }

static int squad_attacked(int32_t player, int32_t squad)
{
    for (int32_t t = P(int32_t, SQUADS, squad * 0x130 + 0x24); t != -1 && P(int32_t, TARGETS, t * 0x120 + 0x20);
         t = target_next(player, t))
        if (unit_under_attack(P(uint32_t, TARGETS, t * 0x120 + 0x20)) & 0xffff)
            return 1;
    return 0;
}

/* 0x1006f7e5: whether a unit of the player's squad of `kind` (-10: of any squad) is under
 * attack */
int32_t __cdecl ai_squad_attacked(int32_t player, int32_t kind)
{
    if (kind == -10) {
        for (int32_t s = ai_list_first(player, 3); s != -1; s = squad_next(s))
            if (squad_attacked(player, s))
                return 1;
        return 0;
    }
    int32_t s = squad_of_kind(player, kind);
    if (s == -1)
        return 0;
    return squad_attacked(player, s);
}

/* 0x1006fd43: the condition "difficulty OP value" */
int32_t __cdecl ai_condition_difficulty(int32_t player, uint32_t op, int32_t value)
{
    (void)player;
    return ai_compare((int32_t)difficulty(), value, op);
}
