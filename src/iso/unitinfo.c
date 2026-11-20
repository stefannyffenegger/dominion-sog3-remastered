/* The game logic, 0x1009da08-0x1009de5b: small lookups of a unit's type values, and the
 * rank names (0x1013bee8, 0x20 bytes each). */
#include "iso_core.h"
#include "orders2.h"
#include "unitinfo.h"

#define TYPE_TABLE I(uint8_t *, 0x18ec) /* +0 the values, +0x2c values per type */

static uint8_t *unit_ptr(uint32_t id) { return I(uint8_t **, ISO_UNITS)[id & 0xffff]; }

/* 0x1009da08: the unit's type's first table value (its sight), or 0 */
int32_t __cdecl unit_type_value0(uint32_t unit)
{
    if (!(unit & 0xffff) || !unit_ptr(unit))
        return 0;
    return P(int32_t, P(uint8_t *, TYPE_TABLE, 0), 4 + P(int32_t, unit_ptr(unit), 0xc) * P(int32_t, TYPE_TABLE, 0x2c) * 4);
}

/* 0x1009dbc2: one of the unit's type record's values (+0x32): 0 at +0xec, 1 +0xe8, 2 +0xf4,
 * 3 +0xf0; else 0 */
int32_t __cdecl unit_type_field(uint32_t unit, uint8_t which)
{
    if (!(unit & 0xffff) || !unit_ptr(unit))
        return 0;
    uint8_t *t = P(uint8_t *, unit_ptr(unit), 0x32);
    switch (which) {
    case 0: return P(int32_t, t, 0xec);
    case 1: return P(int32_t, t, 0xe8);
    case 2: return P(int32_t, t, 0xf4);
    case 3: return P(int32_t, t, 0xf0);
    default: return 0;
    }
}

/* 0x1009dd57: the unit's kind divided by 4 (its race-free kind), or 0 */
int32_t __cdecl unit_kind_quarter(uint32_t unit)
{
    if (!(unit & 0xffff) || !unit_ptr(unit))
        return 0;
    return P(int32_t, unit_ptr(unit), 0x10) / 4;
}

/* 0x1009ddd0: copies the name of unit kind k (by kind / 4); returns `out` */
char *__cdecl unit_type_name(int32_t k, char *out) { return lstrcpyA(out, (const char *)ORIG_PTR(0x1013bee8) + k * 0x20); }

/* The AI's debug names, copied into `out` (the address of each original string by value) */
static char *name_of(uint32_t v, char *out, const uint32_t *names, uint32_t n, uint32_t unknown)
{
    uint32_t a = v < n && names[v] ? names[v] : unknown;
    return lstrcpyA(out, (const char *)ORIG_PTR(a));
}

/* 0x1009de5b: an AI task type's name ("Scan Threats", "Recon", ...) */
char *__cdecl ai_task_type_name(uint32_t v, char *out)
{
    static const uint32_t n[] = {0x101537a8, 0x101537b8, 0x101537c0, 0x10153868, 0x10153890, 0x101538b0,
                                 0x101538d0, 0x101537d0, 0x101537e0, 0x101537e8, 0,          0x101537f0,
                                 0x1015385c, 0x101537fc, 0x1015381c, 0x1015380c, 0x1015382c, 0x10153834,
                                 0x10153840, 0x1015384c, 0x1015390c, 0x101538f8, 0x101538e8};
    return name_of(v, out, n, 0x17, 0x10153920);
}

/* 0x1009e0e3: an AI task state's name ("INITIALIZING", "WAITING", ...) */
char *__cdecl ai_task_state_name(uint32_t v, char *out)
{
    static const uint32_t n[] = {0,          0x10153928, 0x10153938, 0x10153948, 0x10153950, 0x1015395c, 0x10153940,
                                 0x10153968, 0,          0x10153974, 0x10153980, 0x10153988, 0x10153990, 0x10153998};
    return name_of(v, out, n, 0xe, 0x101539a0);
}

/* 0x1009e26a: an AI attitude's name ("Passive", "Defensive", "Offensive", "Aggressive") */
char *__cdecl ai_attitude_name(uint32_t v, char *out)
{
    static const uint32_t n[] = {0x101539a8, 0x101539b0, 0x101539bc, 0x101539c8};
    return name_of(v, out, n, 4, 0x101539d4);
}

/* 0x1009e31a: an AI squad kind's name ("Reserves", "Defense", ...) */
char *__cdecl ai_squad_kind_name(uint32_t v, char *out)
{
    static const uint32_t n[] = {0x101539dc, 0x101539e8, 0x101539f0, 0x101539f8, 0x10153a00, 0x10153a10,
                                 0x10153a20, 0x10153a30, 0x10153a40, 0x10153a48, 0x10153a5c};
    return name_of(v, out, n, 0xb, 0x10153a68);
}

#define can_attack ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t), 0x100d9855)

/* 0x1009e5d8: the unit's attack target (+0x18b) if it can still attack it, else 0 (only the
 * low 16 bits count) */
uint32_t __cdecl unit_valid_target(uint32_t unit)
{
    if (!(unit & 0xffff) || !unit_ptr(unit))
        return 0;
    uint8_t *u = unit_ptr(unit);
    if (!P(uint16_t, u, 0x18b))
        return 0;
    uint32_t r = can_attack(unit, ((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint16_t, u, 0x18b));
    if (!r)
        return 0;
    return (r & 0xffff0000) | P(uint16_t, unit_ptr(unit), 0x18b);
}

/* 0x1009ebb9: whether the unit is being built (state '#') */
int32_t __cdecl unit_being_built(uint32_t unit)
{
    if (!(unit & 0xffff) || !unit_ptr(unit))
        return 0;
    return P(uint8_t, unit_ptr(unit), 0x196) == '#';
}

/* 0x1009efda: whether the unit can be given to player `p`'s orders: its mask (+0x2f) has p,
 * it is not busy (+0x3e bit 16) and +0x84 is not 1 */
int32_t __cdecl unit_takes_orders_from(uint32_t unit, uint8_t p)
{
    if (!(unit & 0xffff) || !unit_ptr(unit))
        return 0;
    uint8_t *u = unit_ptr(unit);
    return (1u << (p & 0x1f) & P(uint8_t, u, 0x2f)) && !(P(uint32_t, u, 0x3e) >> 16 & 1) && P(int32_t, u, 0x84) != 1;
}

/* 0x1009f0a3: sets the unit's +0x1b1 */
void __cdecl unit_set_1b1(uint32_t unit, uint16_t v)
{
    if ((unit & 0xffff) && unit_ptr(unit))
        P(uint16_t, unit_ptr(unit), 0x1b1) = v;
}

/* 0x1009f861: sends the unit to where it stands (+0x52, +0x54): stops it */
void __cdecl unit_stop(uint32_t unit)
{
    if ((unit & 0xffff) && unit_ptr(unit))
        unit_move_to(unit & 0xffff, P(int16_t, unit_ptr(unit), 0x52), (uint32_t)(int32_t)P(int16_t, unit_ptr(unit), 0x54), 0);
}
