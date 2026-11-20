/* The game logic, 0x10091550-0x10091a00: the shade and highlight levels, writing a default
 * Dominion.ini, and the direction (0-7) between two points. */
#include "iso_core.h"
#include "shade.h"

#define SHADOW_LEVELS ((const uint8_t *)ORIG_PTR(0x1014eb78)) /* 14 levels of 256 */
#define HILITE_LEVELS ((const uint8_t *)ORIG_PTR(0x1014f978))

/* IsoSetShadeLevel: loads shade level `level` (0-13) into the engine's shadow table
 * (`which` 1, as ints) or its highlight table (0, as bytes). 0 if out of range */
int32_t __stdcall IsoSetShadeLevel(uint8_t *iso, uint32_t unused, int32_t which, int32_t level)
{
    (void)unused;
    g_iso = iso;
    I(uint32_t *, 0xc31b) = RtGetShadowTable(P(void *, iso, ISO_APP));
    I(uint8_t *, 0x1802) = RtGetHiliteTable(I(void *, ISO_APP));
    if (which < 0 || which > 1 || level < 0 || level > 0xd)
        return 0;
    for (int k = 0; k < 0x100; k++) {
        if (which == 1)
            I(uint32_t *, 0xc31b)[k] = SHADOW_LEVELS[level * 0x100 + k];
        else
            I(uint8_t *, 0x1802)[k] = HILITE_LEVELS[level * 0x100 + k];
    }
    return 1;
}

/* 0x10091790: writes the game's default Dominion.ini (the template at 0x10150778) into the
 * Windows folder if there is none */
void __cdecl ini_write_default(void)
{
    char path[0x100];
    DWORD written;
    GetWindowsDirectoryA(path, 0xff);
    lstrcatA(path, (const char *)ORIG_PTR(0x10150d54));
    HANDLE f = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        const char *text = (const char *)ORIG_PTR(0x10150778);
        f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        WriteFile(f, text, (DWORD)lstrlenA(text), &written, NULL);
    }
    CloseHandle(f);
}

/* 0x10091860: copies entry `k` (8 bytes) of the table at 0x10150d68 */
void __cdecl table_10150d68_get(uint32_t unused, int16_t k, void *out)
{
    (void)unused;
    memcpy(out, (const uint8_t *)ORIG_PTR(0x10150d68) + k * 8, 8);
}

/* 0x1009188a: the direction (0-7) from (x1, y1) to (x2, y2), by the engine's angle in tenths
 * of a degree: 2 around 0, then counterclockwise in 45-degree sectors centred on the
 * isometric axes; -1 never (the sectors cover the circle) */
int32_t __cdecl direction_between(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    int a = GetAngle((int)x2 - (int)x1, (int)y2 - (int)y1);
    if (!(a < 0xd7a && a > 0x95))
        return 2;
    if (a >= 0x96 && a <= 0x2ed)
        return 1;
    if (a >= 0x2ee && a <= 0x419)
        return 0;
    if (a >= 0x41a && a <= 0x671)
        return 7;
    if (a >= 0x672 && a <= 0x79d)
        return 6;
    if (a >= 0x79e && a <= 0x9f5)
        return 5;
    if (a >= 0x9f6 && a <= 0xb21)
        return 4;
    if (a >= 0xb22 && a <= 0xd79)
        return 3;
    return -1;
}
