/* The game logic, 0x100524e9-0x10052df4 (without the radar's view box, radarmap.c):
 * small exports for the scripts: unit names, sliders, DIB sizes, races, campaign data. */
#include "iso_core.h"
#include "exports.h"

#define orig_strcpy ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c70)

static int32_t ftol(long double v) { return (int32_t)(int64_t)v; }

/* 0x100524e9: a unit type's name (32 characters each, by the player's race) into a
 * script variable */
int32_t __stdcall IsoGetUnitString(uint8_t *iso, int32_t type, int32_t player, uint32_t var)
{
    g_iso = iso;
    orig_strcpy((char *)RtGetVariableString(I(void *, 0x180e), var),
                (const char *)ORIG_PTR(0x1013bee8 + I(uint8_t, player * 0x1461 + 0x1b9f) * 0x520 + type * 0x20));
    return 1;
}

/* 0x1005256e: a slider's position: pos / count * size, on the x87, truncated */
int32_t __stdcall IsoCalcSliderPosition(uint8_t *iso, uint32_t pos, uint32_t count, uint32_t size)
{
    g_iso = iso;
    if (!count)
        return 0;
    return ftol((long double)(uint64_t)pos / (long double)(uint64_t)count * (long double)(uint64_t)size);
}

/* 0x100525da: the first item shown for a slider at `a` (from `b`): |a - b| / (count /
 * size), on the x87, truncated */
int32_t __stdcall IsoCalcSliderStartItem(uint8_t *iso, uint32_t size, int32_t a, int32_t b, uint32_t count)
{
    g_iso = iso;
    if (!size)
        return 0;
    int32_t d = a - b;
    if (d < 0)
        d = -d;
    return ftol((long double)d / ((long double)(uint64_t)count / (long double)(uint64_t)size));
}

/* 0x10052bc2: a DIB resource's width + 1 and height */
int32_t __stdcall IsoGetDibDimensions(uint8_t *iso, uint32_t res, int32_t *w, int32_t *h)
{
    g_iso = iso;
    if (!RtLoad(I(void *, 0x180e), res))
        return 0;
    uint8_t *d = RtGetResource(I(void *, 0x180e), res)[0];
    if (!d)
        return 0;
    *w = P(int32_t, d, 4) + 1;
    *h = P(int32_t, d, 0xc);
    return 1;
}

/* 0x10052c50: a player's race and colour; its allies: itself only */
void __stdcall IsoSetRace(uint8_t *iso, int32_t player, int32_t race, int32_t colour)
{
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x1013d7e8), player, race, colour);
    I(uint8_t, player * 0x1461 + 0x1b9f) = (uint8_t)race;
    I(int32_t, player * 0x1461 + 0x1ba2) = colour;
    I(int32_t, player * 0x1461 + 0x1ba6) = 0;
    I(uint8_t, player * 0x1461 + 0x1ba0) = 0;
    I(uint8_t, player * 0x1461 + 0x1ba0) |= (uint8_t)(1 << (player & 0x1f));
}

/* 0x10052d62 */
uint8_t __stdcall IsoGetRace(uint8_t *iso, int32_t player)
{
    g_iso = iso;
    return I(uint8_t, player * 0x1461 + 0x1b9f);
}

/* 0x10052da1: the campaign's values (I(0xa0a), ints): read, or with `set` written */
int32_t __stdcall IsoCampaignInfo(uint8_t *iso, int32_t set, int32_t i, int32_t value)
{
    g_iso = iso;
    if (!set)
        return I(int32_t, 0xa0a + i * 4);
    I(int32_t, 0xa0a + i * 4) = value;
    return 0;
}
