/* The game logic, 0x1000cdd4-0x1000d362: reading and setting single values of units and
 * map objects for the engine's scripts (IsoSetUnitItem and friends). */
#include "iso_core.h"
#include "items.h"

#define action_set ORIG(void(__cdecl *)(uint32_t, uint8_t), 0x1009cc60)
#define ISO_OBJECTS 0x192c /* map objects, 0x12 bytes each */

/* 0x1000cdd4: 0x3ea player, 0x3eb x, 0x3ec y, 0x3ed sprite cell, 0x3ee selection state,
 * 0x3ef 0x2a (original: then also the action, and reports failure), 0x3f0 the action
 * (reports failure). 1 when set. */
int __stdcall IsoSetUnitItem(uint8_t *iso, uint32_t value, uint32_t id, int item)
{
    g_iso = iso;
    uint8_t *u = P(uint8_t **, iso, ISO_UNITS)[id];
    if (!u)
        return 0;
    switch (item) {
    case 0x3ea: P(uint8_t, u, U_PLAYER) = (uint8_t)value; break;
    case 0x3eb: P(uint16_t, u, U_X) = (uint16_t)value; break;
    case 0x3ec: P(uint16_t, u, U_Y) = (uint16_t)value; break;
    case 0x3ed: P(uint16_t, u, 0x145) = (uint16_t)value; break;
    case 0x3ee: P(uint8_t, u, U_SEL_STATE) = (uint8_t)value; break;
    case 0x3ef:
        P(uint8_t, u, 0x2a) = (uint8_t)value;
        /* fall through */
    case 0x3f0:
        action_set(id, (uint8_t)value);
        /* fall through */
    default:
        return 0;
    }
    return 1;
}

/* 0x1000cf22: 1000 type, 1001 its resource, then as IsoSetUnitItem, 0x3f0 the action,
 * 0x3f1-0x3f2 its facings; -2 no unit, -3 unknown item */
uint32_t __stdcall IsoGetUnitItem(uint8_t *iso, int id, int item)
{
    g_iso = iso;
    uint8_t *u = P(uint8_t **, iso, ISO_UNITS)[id];
    if (!u)
        return 0xfffffffe;
    switch (item) {
    case 1000: return P(uint32_t, u, U_ID_TYPE);
    case 0x3e9: return P(uint32_t, u, 0);
    case 0x3ea: return P(uint8_t, u, U_PLAYER);
    case 0x3eb: return (uint32_t)(int32_t)P(int16_t, u, U_X);
    case 0x3ec: return (uint32_t)(int32_t)P(int16_t, u, U_Y);
    case 0x3ed: return P(uint16_t, u, 0x145);
    case 0x3ee: return P(uint8_t, u, U_SEL_STATE);
    case 0x3ef: return P(uint8_t, u, 0x2a);
    case 0x3f0: return P(uint8_t, u, 0x147);
    case 0x3f1: return P(uint8_t, u, 0x79);
    case 0x3f2: return P(uint8_t, u, 0x7a);
    default: return 0xfffffffd;
    }
}

/* 0x1000d116: 3000 the object's value, 0xbb9-0xbbc its four 16-bit fields */
int __stdcall IsoSetObjectItem(uint8_t *iso, uint32_t value, int i, int item)
{
    g_iso = iso;
    uint8_t *o = P(uint8_t *, iso, ISO_OBJECTS);
    if (!o)
        return 0;
    o += i * 0x12;
    switch (item) {
    case 3000: P(uint32_t, o, 0) = value; break;
    case 0xbb9: P(uint16_t, o, 4) = (uint16_t)value; break;
    case 0xbba: P(uint16_t, o, 6) = (uint16_t)value; break;
    case 0xbbb: P(uint16_t, o, 8) = (uint16_t)value; break;
    case 0xbbc: P(uint16_t, o, 10) = (uint16_t)value; break;
    default: return 0;
    }
    return 1;
}

/* 0x1000d23a */
uint32_t __stdcall IsoGetObjectItem(uint8_t *iso, int i, int item)
{
    g_iso = iso;
    uint8_t *o = P(uint8_t *, iso, ISO_OBJECTS);
    if (!o)
        return 0;
    o += i * 0x12;
    switch (item) {
    case 3000: return P(uint32_t, o, 0);
    case 0xbb9: return (uint32_t)(int32_t)P(int16_t, o, 4);
    case 0xbba: return (uint32_t)(int32_t)P(int16_t, o, 6);
    case 0xbbb: return P(uint16_t, o, 8);
    case 0xbbc: return P(uint16_t, o, 10);
    default: return 0;
    }
}
