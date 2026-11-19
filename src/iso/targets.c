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
#define ai_player_active ORIG(int(__cdecl *)(uint32_t), 0x100e2db2)
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

/* ---- 0x10072aa5-0x10072eea (part 77): linking and freeing targets ---- */

#define ai_list_last ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100e2e62)
#define ai_list_set_last ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x100e2e92)
#define ai_list_set_first ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x100e2f15)
#define target_note_new ORIG(void(__cdecl *)(int32_t, int32_t, int), 0x100c07c0)
#define squads_drop_target ORIG(void(__cdecl *)(int32_t), 0x100c154a)
#define tasks_drop_target ORIG(void(__cdecl *)(int32_t), 0x1004cd09)
#define ai_event ORIG(void(__cdecl *)(uint32_t, int, uint32_t, int), 0x100e42a6)

/* 0x10072d31: frees target record `t`: it leaves the squads, tasks and lists, the owner's
 * player counts are lowered, and with `tell` the owner hears of the loss (event 5); the
 * record goes back to the free list. Returns 1 */
int32_t __cdecl target_free(int32_t t, int tell)
{
    char name[0x40];
    uint8_t *r = TARGETS + t * 0x120;
    uint32_t owner = P(uint32_t, r, 0x1c);
    debug_print(0x10, (const char *)ORIG_PTR(0x101465cc), t, owner, P(int32_t, r, 0x20),
                target_kind_name(P(int32_t, r, 0x24), name));
    squads_drop_target(t);
    tasks_drop_target(t);
    target_unlink_all(t, 0);
    target_unlink_owner(owner, t);
    player_targets_add(owner, P(int32_t, TARGETS + t * 0x120, 0x20), 0);
    if (tell)
        ai_event(owner, 5, P(uint32_t, TARGETS + t * 0x120, 0x20), 0);
    r = TARGETS + t * 0x120;
    P(int32_t, r, 4) = ai_head();
    P(int32_t, r, 8) = -1;
    P(int32_t, r, 0) = 0;
    P(int32_t, r, 0x18) = -1;
    P(int32_t, r, 0x110) = -1;
    P(int32_t, r, 0x114) = -1;
    P(int32_t, r, 0x20) = 0;
    ai_set_head(t);
    return 1;
}

/* 0x10072aa5: frees unit `unit`'s target record (+0x1b1), if it has one. Returns it, or -1 */
int32_t __cdecl unit_target_free(uint16_t unit, int tell)
{
    uint8_t *u = unit ? I(uint8_t **, ISO_UNITS)[unit] : 0;
    int32_t t = u ? P(int16_t, u, 0x1b1) : -1;
    if (t == -1)
        return -1;
    target_free(t, tell);
    return t;
}

/* 0x10072b35: appends target `t` to AI player `player`'s own list (0): the per-player links
 * +0x30 (next) and +0xb0 (previous), and notes it */
int32_t __cdecl target_link_own(int32_t player, int32_t t)
{
    if (t == -1)
        return t;
    ai_head();
    int32_t last = ai_list_last(player, 0);
    if (last == -1) {
        ai_list_set_last(player, 0, t);
        ai_list_set_first(player, 0, t);
        target_set_next_by_player(player, t, -1);
        target_set_fieldb0(player, t, -1);
    } else {
        target_set_next_by_player(player, last, t);
        target_set_next_by_player(player, t, -1);
        target_set_fieldb0(player, t, last);
        ai_list_set_last(player, 0, t);
    }
    target_note_new(player, t, 0);
    return t;
}

/* 0x10072c45: appends target `t` to AI player `player`'s list of the others' (1): links
 * +0x50 and +0xd0 */
int32_t __cdecl target_link_other(int32_t player, int32_t t)
{
    if (t == -1)
        return t;
    ai_head();
    int32_t last = ai_list_last(player, 1);
    if (last == -1) {
        ai_list_set_last(player, 1, t);
        ai_list_set_first(player, 1, t);
        target_set_prev_by_player(player, t, -1);
        target_set_fieldd0(player, t, -1);
    } else {
        target_set_prev_by_player(player, last, t);
        target_set_prev_by_player(player, t, -1);
        target_set_fieldd0(player, t, last);
        ai_list_set_last(player, 1, t);
    }
    return t;
}

#define TASKS G(uint8_t *, 0x1018b600)
#define SQUADS G(uint8_t *, 0x1018b608)
#define task_next_ ORIG(int32_t(__cdecl *)(int32_t), 0x1004bb2c)
#define squad_next ORIG(int32_t(__cdecl *)(int32_t), 0x100c038c)
#define ai_list_first ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100e2ee5)
#define unit_target_value ORIG(int32_t(__cdecl *)(int32_t), 0x1009da08)

/* 0x10072eea: removes target `t` from the AI players' tasks (+0x44, and +0x48 unless
 * `others_only`) and squads (+0x2c); with `others_only`, not from its owner's. Returns 1 */
int32_t __cdecl target_unlink_all(int32_t t, int others_only)
{
    uint32_t owner = P(uint32_t, TARGETS, t * 0x120 + 0x1c);
    for (uint32_t p = 0; p < 8; p++) {
        if (!ai_player_active(p) || (others_only && p == owner))
            continue;
        for (int32_t k = ai_list_first((int32_t)p, 7); k != -1; k = task_next_(k)) {
            if (P(int32_t, TASKS, k * 0x74 + 0x44) == t)
                P(int32_t, TASKS, k * 0x74 + 0x44) = -1;
            if (!others_only && P(int32_t, TASKS, k * 0x74 + 0x48) == t)
                P(int32_t, TASKS, k * 0x74 + 0x48) = -1;
        }
        for (int32_t s = ai_list_first((int32_t)p, 2); s != -1; s = squad_next(s))
            if (P(int32_t, SQUADS, s * 0x130 + 0x2c) == t)
                P(int32_t, SQUADS, s * 0x130 + 0x2c) = -1;
    }
    return 1;
}

/* 0x1007307b: takes target `t` out of player `player`'s own list */
void __cdecl target_unlink_owner(int32_t player, int32_t t)
{
    int32_t prev = target_fieldb0(player, t), next = target_next_by_player(player, t);
    if (prev == -1) {
        ai_list_set_first(player, 0, next);
        if (next == -1)
            ai_list_set_last(player, 0, -1);
        else
            target_set_fieldb0(player, next, -1);
    } else {
        target_set_next_by_player(player, prev, next);
        if (next == -1)
            ai_list_set_last(player, 0, prev);
        else
            target_set_fieldb0(player, next, prev);
    }
    target_set_fieldb0(player, t, -1);
    target_set_next_by_player(player, t, -1);
}

/* 0x10073175: takes target `t` out of player `player`'s list of the others' */
void __cdecl target_unlink_other(int32_t player, int32_t t)
{
    int32_t prev = target_fieldd0(player, t), next = target_prev_by_player(player, t);
    if (prev == -1) {
        ai_list_set_first(player, 1, next);
        if (next == -1)
            ai_list_set_last(player, 1, -1);
        else
            target_set_fieldd0(player, next, -1);
    } else {
        target_set_prev_by_player(player, prev, next);
        if (next == -1)
            ai_list_set_last(player, 1, prev);
        else
            target_set_fieldd0(player, next, prev);
    }
    target_set_fieldd0(player, t, -1);
    target_set_prev_by_player(player, t, -1);
}

/* 0x1007326f: counts unit `unit` into (`add`) or out of AI player `player`'s strength: its
 * value (+0x106 of the AI block, +0xc), the count per kind (+0xac), and per class: men
 * (types 0x18-0x3f: +0xa8 value, +0xa0 count), types below 0x14 (+0xa6, +0xa0), transports
 * 0x88-0x9f (+0xa4 count) and the rest (+0xaa, +0xa2) */
void __cdecl player_targets_add(int32_t player, int32_t unit, int add)
{
    uint8_t *ai = G(uint8_t *, 0x1018b604) + player * 0x143 + 0xc;
    uint32_t type = P(uint32_t, I(uint8_t **, ISO_UNITS)[unit], 0xc);
    int32_t value = unit_target_value(unit);
    int16_t v = (int16_t)value, d = add ? 1 : -1;
    if (!add)
        v = (int16_t)-v;
    P(int32_t, ai, 0xfa) += add ? value : -value;
    int32_t kind = unit_target_kind((uint32_t)unit);
    P(int16_t, ai, 0xa0 + kind * 2) += d;
    if (type >= 0x18 && type <= 0x3f) {
        P(int16_t, ai, 0x9c) += v;
        P(int16_t, ai, 0x94) += d;
    } else if (type < 0x10 || (type > 0xf && type < 0x14)) {
        P(int16_t, ai, 0x9a) += v;
        P(int16_t, ai, 0x94) += d;
    } else if (type < 0x88 || type > 0x9f) {
        P(int16_t, ai, 0x9e) += v;
        P(int16_t, ai, 0x96) += d;
    } else {
        P(int16_t, ai, 0x98) += d;
    }
}

/* 0x10073598: counts one more for the squad of unit `unit`'s target (+0x128) */
void __cdecl unit_squad_count(int32_t unit)
{
    uint8_t *u = unit ? I(uint8_t **, ISO_UNITS)[unit] : 0;
    int32_t t = u ? P(int16_t, u, 0x1b1) : -1;
    int32_t s = t == -1 ? -1 : P(int32_t, TARGETS, t * 0x120 + 0x110);
    if (s != -1)
        P(int32_t, SQUADS, s * 0x130 + 0x128)++;
}

/* 0x1007364c: whether two units are in the same or neighbouring sectors (+0x1a1) */
int32_t __cdecl units_near_sectors(uint8_t *a, uint8_t *b)
{
    if (!a || !b)
        return 0;
    int16_t sb = P(int16_t, b, 0x1a1);
    uint8_t *sa = I(uint8_t *, 0xc2eb) + P(int16_t, a, 0x1a1) * 0x1e;
    if (sb == P(int16_t, a, 0x1a1))
        return 1;
    for (int k = 0; k < 8; k++)
        if (P(int16_t, sa, 6 + k * 2) == sb)
            return 1;
    return 0;
}
