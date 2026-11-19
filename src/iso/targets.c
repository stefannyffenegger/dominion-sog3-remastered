/* The game logic, 0x100721b0-0x10072550: the fields of the AI's target records (0x120 bytes
 * each, G(0x1018b610)): per-target links (+0x28 next, +0x2c previous) and per-player arrays
 * of 8 (+0x30, +0x50, +0x70, +0xb0, +0xd0, +0xf0). Each setter returns the value stored. */
#include "iso_core.h"
#include "targets.h"

#define TARGETS G(uint8_t *, 0x1018b610)
#define FIELD(t, p, off) P(int32_t, TARGETS, (t) * 0x120 + (p) * 4 + (off))

/* 0x100721b0, 0x100721c7: the first int of the AI player block (the free target list) */
int32_t __cdecl ai_head(void) { return *G(int32_t *, 0x1018b604); }
int32_t __cdecl ai_set_head(int32_t v)
{
    *G(int32_t *, 0x1018b604) = v;
    return *G(int32_t *, 0x1018b604);
}

/* 0x100721e9 */
int32_t __cdecl target_next_by_player(int32_t player, int32_t t) { return FIELD(t, player, 0x30); }
/* 0x10072212 */
int32_t __cdecl target_prev_by_player(int32_t player, int32_t t) { return FIELD(t, player, 0x50); }
/* 0x1007223b */
int32_t __cdecl target_next(int32_t unused, int32_t t) { (void)unused; return FIELD(t, 0, 0x28); }
/* 0x1007225e */
int32_t __cdecl target_field70(int32_t player, int32_t t) { return FIELD(t, player, 0x70); }
/* 0x1007238f */
int32_t __cdecl target_fieldb0(int32_t player, int32_t t) { return FIELD(t, player, 0xb0); }
/* 0x100723bb */
int32_t __cdecl target_fieldd0(int32_t player, int32_t t) { return FIELD(t, player, 0xd0); }
/* 0x100723e7 */
int32_t __cdecl target_prev(int32_t unused, int32_t t) { (void)unused; return FIELD(t, 0, 0x2c); }
/* 0x1007240a */
int32_t __cdecl target_fieldf0(int32_t player, int32_t t) { return FIELD(t, player, 0xf0); }
/* 0x10072287 */
int32_t __cdecl target_set_next_by_player(int32_t player, int32_t t, int32_t v)
{
    FIELD(t, player, 0x30) = v;
    return FIELD(t, player, 0x30);
}
/* 0x100722cc */
int32_t __cdecl target_set_prev_by_player(int32_t player, int32_t t, int32_t v)
{
    FIELD(t, player, 0x50) = v;
    return FIELD(t, player, 0x50);
}
/* 0x10072311 */
int32_t __cdecl target_set_next(int32_t unused, int32_t t, int32_t v)
{
    (void)unused;
    FIELD(t, 0, 0x28) = v;
    return FIELD(t, 0, 0x28);
}
/* 0x1007234a */
int32_t __cdecl target_set_field70(int32_t player, int32_t t, int32_t v)
{
    FIELD(t, player, 0x70) = v;
    return FIELD(t, player, 0x70);
}
/* 0x10072436 */
int32_t __cdecl target_set_fieldb0(int32_t player, int32_t t, int32_t v)
{
    FIELD(t, player, 0xb0) = v;
    return FIELD(t, player, 0xb0);
}
/* 0x10072481 */
int32_t __cdecl target_set_fieldd0(int32_t player, int32_t t, int32_t v)
{
    FIELD(t, player, 0xd0) = v;
    return FIELD(t, player, 0xd0);
}
/* 0x100724cc */
int32_t __cdecl target_set_prev(int32_t unused, int32_t t, int32_t v)
{
    (void)unused;
    FIELD(t, 0, 0x2c) = v;
    return FIELD(t, 0, 0x2c);
}
/* 0x10072505 */
int32_t __cdecl target_set_fieldf0(int32_t player, int32_t t, int32_t v)
{
    FIELD(t, player, 0xf0) = v;
    return FIELD(t, player, 0xf0);
}

/* ---- 0x10072550-0x10072aa5 (part 76): making targets ---- */

#define debug_print ORIG(void(__cdecl *)(int, const char *, ...), 0x10095e98)
#define unit_target_kind ORIG(int32_t(__cdecl *)(uint32_t), 0x1009dd57)
#define target_kind_name ORIG(const char *(__cdecl *)(int32_t, char *), 0x1009ddd0)
#define player_targets_add ORIG(void(__cdecl *)(uint32_t, int32_t, int), 0x1007326f)
#define ai_player_active ORIG(int(__cdecl *)(uint32_t), 0x100e2db2)
#define target_link_own ORIG(void(__cdecl *)(uint32_t, int32_t), 0x10072b35)
#define target_link_other ORIG(void(__cdecl *)(uint32_t, int32_t), 0x10072c45)
#define unit_set_target ORIG(void(__cdecl *)(int32_t, int32_t), 0x1009f0a3)
#define ai_unit_noted ORIG(void(__cdecl *)(int32_t, int), 0x100e4a43)

/* 0x10072550: makes the AI's target record for unit `unit`, taken from the free list: its
 * kind, the owner's list, and for every active AI player its per-player links (the owner's
 * own list or the others'). Returns the record, or -1 */
int32_t __cdecl target_new(int32_t unit)
{
    char name[0x80];
    int32_t t = ai_head();
    uint32_t owner = P(uint8_t, I(uint8_t **, ISO_UNITS)[unit], 0x14);
    if (t != -1) {
        uint8_t *r = TARGETS + t * 0x120;
        P(int32_t, r, 0) = 1;
        P(int32_t, r, 0x20) = unit;
        P(uint32_t, r, 0x1c) = owner;
        P(int32_t, r, 0xc) = 0;
        P(int32_t, r, 0x24) = unit_target_kind((uint32_t)unit);
        player_targets_add(owner, unit, 1);
        debug_print(0x10, (const char *)ORIG_PTR(0x1014655c), owner, target_kind_name(P(int32_t, r, 0x24), name), t,
                    unit);
        ai_set_head(P(int32_t, r, 4));
        P(int32_t, r, 4) = -1;
        for (int p = 0; p < 8; p++) {
            if (!ai_player_active((uint32_t)p))
                continue;
            static const int offs[] = { 0x90, 0x30, 0x50, 0x70, 0xb0, 0xd0, 0xf0 };
            for (int k = 0; k < 7; k++)
                FIELD(t, p, offs[k]) = -1;
        }
        for (uint32_t p = 0; p < 8; p++) {
            if (!ai_player_active(p))
                continue;
            if (owner == p)
                target_link_own(owner, t);
            else
                target_link_other(p, t);
        }
    }
    unit_set_target(unit, t);
    ai_unit_noted(unit, 1);
    if (P(int32_t, G(uint8_t *, 0x1018b604), 0xa40) < t)
        P(int32_t, G(uint8_t *, 0x1018b604), 0xa40) = t;
    return t;
}

/* 0x100728c9: a target for a transport (unit types 0x90-0x93, 0x9c-0x9f, and with 0xdaae set
 * also 0x8c-0x8f, 0x98-0x9b), noting the vehicle it is loaded in (+0xf6 up) or, at the top,
 * the last of its cargo (+0xf8 down). Kind 2. -1 for other units */
int32_t __cdecl target_new_transport(uint32_t unit)
{
    if (!unit)
        return -1;
    uint8_t **units = I(uint8_t **, ISO_UNITS);
    uint32_t type = P(uint32_t, units[unit], 0xc);
    int carrier = (type >= 0x90 && type < 0x94) || (type >= 0x9c && type < 0xa0) ||
                  (I(int32_t, 0xdaae) && ((type >= 0x8c && type < 0x90) || (type >= 0x98 && type < 0x9c)));
    if (!carrier)
        return -1;
    uint32_t top = unit;
    while (P(int16_t, units[top], 0xf6))
        top = P(uint16_t, units[top], 0xf6);
    if (unit == top)
        while (P(int16_t, units[top], 0xf8))
            top = P(uint16_t, units[top], 0xf8);
    int32_t t = target_new((int32_t)unit);
    if (t != -1) {
        debug_print(0x10, (const char *)ORIG_PTR(0x10146594), t, unit);
        P(int32_t, TARGETS, t * 0x120 + 0xc) = 2;
        P(uint32_t, TARGETS, t * 0x120 + 0x118) = unit;
        P(uint32_t, TARGETS, t * 0x120 + 0x11c) = top;
    }
    return t;
}
