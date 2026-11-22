/* The game logic, 0x100e487c-0x100e4a9b: a computer player's current squad while units are
 * added to it. */
#include "iso_core.h"
#include "aigroup.h"

#define AI G(uint8_t *, 0x1018b604)
#define PLAYER(k) (AI + (k) * 0x143 + 0xc)
#define debug_print ORIG(int32_t(__cdecl *)(int32_t, const char *, ...), 0x10095e98)
#define squad_create ORIG(int32_t(__cdecl *)(int32_t, int32_t, int32_t, int32_t), 0x100c0462)

/* 0x100e487c: squad `s` becomes player `k`'s current one (+0x119, the old one kept in +0x11b)
 * and `k` the AI's current player (+0xa28). Returns `s` */
int32_t __cdecl ai_group_take(int32_t k, int32_t s)
{
    uint8_t *p = PLAYER(k);
    P(uint16_t, p, 0x11b) = P(uint16_t, p, 0x119);
    P(uint16_t, p, 0x119) = (uint16_t)s;
    P(int32_t, AI, 0xa28) = k;
    return s;
}

/* 0x100e4a10: sets player `k`'s "adding units" value (+0x106); returns it */
int32_t __cdecl ai_adding(int32_t k, int32_t v)
{
    P(int32_t, PLAYER(k), 0x106) = v;
    return v;
}

/* 0x100e48de: makes player `k`'s holding squad (+0x129, created when there is none) the
 * current one, with the value `v`; the squad */
int32_t __cdecl ai_player_hold(int32_t k, int32_t v)
{
    uint8_t *p = PLAYER(k);
    int32_t s = P(int16_t, p, 0x129);
    if (s == -1) {
        s = squad_create(k, 2, 0, 0);
        P(uint16_t, p, 0x129) = (uint16_t)s;
    }
    ai_group_take(k, s);
    ai_adding(k, v);
    return s;
}

/* 0x100e4969: goes back to player `k`'s previous squad; the AI's current player becomes that
 * squad's owner (+0x20). Returns the squad, or -1 */
int32_t __cdecl ai_group_done(int32_t k)
{
    uint8_t *p = PLAYER(k);
    int32_t s = P(int16_t, p, 0x11b);
    P(uint16_t, p, 0x119) = (uint16_t)s;
    P(uint16_t, p, 0x11b) = 0xffff;
    if (s != -1) {
        P(int32_t, AI, 0xa28) = P(int32_t, G(uint8_t *, 0x1018b608) + s * 0x130, 0x20);
        debug_print(0x10, (const char *)ORIG_PTR(0x1015b540), s);
    }
    ai_adding(k, 0);
    return s;
}

/* 0x100e4a43: stores `v` in +0x10 of unit `unit`'s target record, if it has one */
void __cdecl ai_unit_noted(uint32_t unit, int32_t v)
{
    if (!unit)
        return;
    int32_t t = P(int16_t, I(uint8_t **, ISO_UNITS)[unit], 0x1b1);
    if (t != -1)
        P(int32_t, G(uint8_t *, 0x1018b610) + t * 0x120, 0x10) = v;
}
