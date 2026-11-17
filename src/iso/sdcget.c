/* The game logic, 0x1002c74e-0x1002cbfa: what the scripts ask of the scenario
 * description (briefing lines, the briefing's wave, the cinematics, the next
 * scenario), and linking two energy wall towers. */
#include "iso_core.h"
#include "power.h"
#include "display.h"
#include "sdcget.h"

#define link_possible ORIG(int(__cdecl *)(uint32_t, uint32_t), 0x10077c54)
#define link_add ORIG(int(__cdecl *)(uint32_t, uint32_t, int, int), 0x10078dd5)
#define net_link ORIG(void(__cdecl *)(uint32_t, uint32_t, int), 0x1003527c)
#define g_strcpy ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c70)
#define g_strcat ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c80)

static char *var(uint32_t v) { return RtGetVariableString(I(void *, ISO_APP), v); }

/* 0x1002c74e: briefing line n (0-29) into variable `v` */
int __cdecl IsoGetBriefTextLine(uint8_t *iso, uint32_t n, uint32_t v)
{
    g_iso = iso;
    if (n >= 0x1e)
        return 0;
    char *s = var(v);
    if (!s || !I(char *, 0x1860 + n * 4))
        return 0;
    g_strcpy(s, I(char *, 0x1860 + n * 4));
    return 1;
}

/* 0x1002c7eb: the briefing's wave into variable `v` */
int __cdecl IsoGetBriefWav(uint8_t *iso, uint32_t v)
{
    g_iso = iso;
    char *s = var(v);
    if (!s || !I(char *, 0x18d8))
        return 0;
    g_strcpy(s, I(char *, 0x18d8));
    return 1;
}

/* 0x1002c86d: cinematic n (0-2) appended to variable `v` */
int __cdecl IsoGetMissionCine(uint8_t *iso, int n, uint32_t v)
{
    g_iso = iso;
    if (!I(void *, 0xc2e5))
        return 0;
    char *s = var(v);
    if (!s || !I(char *, 0x1908 + n * 4))
        return 0;
    g_strcat(s, I(char *, 0x1908 + n * 4));
    return 1;
}

/* 0x1002c95c: the next scenario's name into variable `v` */
int __cdecl IsoGetNextSDLFilename(uint8_t *iso, uint32_t v)
{
    g_iso = iso;
    char *s = var(v);
    if (!s || !I(char *, 0x1928))
        return 0;
    g_strcpy(s, I(char *, 0x1928));
    return 1;
}

/* 0x1002ca08: links the two energy wall towers (types 0x40-0x4f) standing at (x1, y1)
 * and (x2, y2) */
void __cdecl energywall_link(int x1, int y1, int x2, int y2)
{
    if (!I(int32_t, ISO_HAVE_SPRITES))
        return;
    uint32_t first = 0;
    for (int i = 0; i < I(int32_t, ISO_UNIT_COUNT); i++) {
        uint32_t v = I(uint32_t *, ISO_UNIT_LIST)[i];
        uint8_t *u;
        if (!v || !(u = I(uint8_t **, ISO_UNITS)[v]))
            continue;
        int32_t t = P(int32_t, u, U_ID_TYPE);
        if (t < 0x40 || t >= 0x50)
            continue;
        if (!((P(int16_t, u, U_X) == x1 && P(int16_t, u, U_Y) == y1) || (P(int16_t, u, U_X) == x2 && P(int16_t, u, U_Y) == y2)))
            continue;
        if (!first) {
            first = v;
            continue;
        }
        if (unit_direction(first, v) == -1) {
            DebugStr((const char *)ORIG_PTR(0x1013619c));
            return;
        }
        if (!link_possible(first, v)) {
            DebugStr((const char *)ORIG_PTR(0x10136168));
            return;
        }
        power_check((int16_t)first);
        power_check((int16_t)v);
        if (!link_add(first, v, 0, 1)) {
            DebugStr((const char *)ORIG_PTR(0x10136144), first, v);
            return;
        }
        if (I(int, 0x175))
            net_link(first, v, 1);
        return;
    }
}
