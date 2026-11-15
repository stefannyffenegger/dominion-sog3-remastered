/* The game logic, 0x1001065b-0x10010ca9: the map's sectors (256x256 pixels): each keeps a
 * list of its units (linked through +0x393/+0x397 by their +0x395 id), how many, and
 * which players have units and which have structures in it. Also small unit tests and
 * a player's slots. */
#include "iso_core.h"
#include "sector.h"

#define U_SECTOR 0x1a1
#define U_NEXT 0x393
#define U_LIST_ID 0x395
#define U_PREV 0x397
#define S_COUNT 0x00   /* short */
#define S_FIRST 0x02   /* short */
#define S_BUILT 0x18   /* byte: players with structures here */
#define S_UNITS 0x19   /* byte: players with units here */
#define ISO_SECTOR_UNITS 0xc2ff

static void sector_mark(uint8_t *s, uint8_t *u)
{
    uint8_t bit = (uint8_t)(1u << (P(uint8_t, u, U_PLAYER) & 0x1f));
    if (P(int32_t, P(uint8_t *, u, 0x32), 0x74) < 4)
        P(uint8_t, s, S_UNITS) |= bit;
    else
        P(uint8_t, s, S_BUILT) |= bit;
}

/* 0x1001065b: the unit leaves its sector */
void __cdecl sector_leave(uint8_t *u)
{
    uint8_t *s = I(uint8_t *, 0xc2eb) + P(int16_t, u, U_SECTOR) * 0x1e;
    if (P(int16_t, s, S_FIRST) == P(int16_t, u, U_LIST_ID)) {
        P(int16_t, s, S_FIRST) = P(int16_t, u, U_NEXT);
    } else {
        uint16_t next = P(uint16_t, u, U_NEXT), prev = P(uint16_t, u, U_PREV);
        if (next && iso_unit(next))
            P(uint16_t, iso_unit(next), U_PREV) = P(uint16_t, u, U_PREV);
        if (prev && iso_unit(prev))
            P(uint16_t, iso_unit(prev), U_NEXT) = P(uint16_t, u, U_NEXT);
    }
    P(uint16_t, u, U_PREV) = 0;
    P(uint16_t, u, U_NEXT) = P(uint16_t, u, U_PREV);
    P(int16_t, s, S_COUNT)--;
    I(int32_t, ISO_SECTOR_UNITS)--;
    P(uint8_t, s, S_UNITS) = 0;
    P(uint8_t, s, S_BUILT) = P(uint8_t, s, S_UNITS);
    for (uint16_t v = P(uint16_t, s, S_FIRST); v && iso_unit(v); v = P(uint16_t, iso_unit(v), U_NEXT))
        sector_mark(s, iso_unit(v));
}

/* 0x100108af: the unit joins sector `sector` (record s), at the front of its list */
void __cdecl sector_enter(uint8_t *u, int16_t *s, int16_t sector)
{
    uint16_t first = (uint16_t)s[1];
    for (uint16_t v = first; v && iso_unit(v); v = P(uint16_t, iso_unit(v), U_NEXT))
        if (P(int16_t, iso_unit(v), U_LIST_ID) == P(int16_t, u, U_LIST_ID))
            return;
    P(uint16_t, u, U_PREV) = 0;
    P(uint16_t, u, U_NEXT) = 0;
    s[1] = P(int16_t, u, U_LIST_ID);
    if (first && iso_unit(first)) {
        P(int16_t, iso_unit(first), U_PREV) = P(int16_t, u, U_LIST_ID);
        P(uint16_t, u, U_NEXT) = first;
    }
    sector_mark((uint8_t *)s, u);
    s[0]++;
    I(int32_t, ISO_SECTOR_UNITS)++;
    P(int16_t, u, U_SECTOR) = sector;
}

/* 0x10010a60 */
int __cdecl unit_is_local(uint32_t id)
{
    return P(uint8_t, iso_unit((int)(id & 0xffff)), U_PLAYER) == I(uint32_t, ISO_LOCAL_PLAYER);
}

/* 0x10010ab0: actions that use up a path step */
int __cdecl action_counts(uint8_t a)
{
    switch (a) {
    case 3: case 4: case 5: case 6: case 8: case 0x17: case 0x18: case 0x23: case 0x24:
        return 1;
    default:
        return 0;
    }
}

/* 0x10010b40 */
int __cdecl unit_is_local_u(uint8_t *u) { return P(uint8_t, u, U_PLAYER) == I(uint32_t, ISO_LOCAL_PLAYER); }

/* 0x10010b80: a free slot (of 128, 16 bytes each at +0xa42 of the player's block), cleared
 * and taken; -1 when none */
int16_t __cdecl player_slot_alloc(uint8_t player)
{
    uint8_t *block = g_iso + ISO_PLAYERS + player * ISO_PLAYER_SIZE;
    for (int16_t i = 0; i <= 0x7f; i++)
        if (!P(int16_t, block, 0xa42 + i * 0x10)) {
            memset(block + 0xa42 + i * 0x10, 0, 0x10);
            P(int16_t, block, 0xa42 + i * 0x10) = 1;
            return i;
        }
    return -1;
}

/* 0x10010c33: frees the unit's slot (its +0x9e) */
void __cdecl player_slot_free(int16_t id)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
    I(uint16_t, P(uint16_t, u, 0x9e) * 0x10 + P(uint8_t, u, U_PLAYER) * ISO_PLAYER_SIZE + 0x25ab) = 0;
}
