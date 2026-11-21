/* The game logic, 0x100d2a70-0x100d2ca0: whether a point is inside the map's margin, and
 * loading the popup menu's graphics. */
#include "iso_core.h"
#include "popupinit.h"

/* 0x100d2a70: whether pixel (x, y) lies inside the map, which is centred on 0 with half
 * sizes +0xd709 / +0xd71e, less a margin of 32 / 16 */
int32_t __cdecl on_map_px(int32_t x, int32_t y)
{
    int32_t w = I(int32_t, 0xd709), h = I(int32_t, 0xd71e);
    return 0x20 - w < x && w - 0x20 > x && 0x10 - h < y && h - 0x10 > y;
}

/* 0x100d2b10: loads the menu's resource `res` (+0xdaba, its second word at +0xdab6), takes
 * the first word of the seven resources `r` (+0xdad0-+0xdae8), keeps `value` (+0xdaec) and
 * clears +0xdaf0 */
void __stdcall IsoInitPopupMenuC(uint8_t *iso, uint32_t res, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3,
                                 uint32_t r4, uint32_t r5, uint32_t r6, uint32_t value)
{
    g_iso = iso;
    RtLoad(I(void *, ISO_APP), res);
    I(uint32_t, 0xdaec) = value;
    I(uint32_t, 0xdaba) = res;
    I(uint32_t, 0xdab6) = ((uint32_t *)RtGetResource(I(void *, ISO_APP), res))[1];
    const uint32_t r[7] = {r0, r1, r2, r3, r4, r5, r6};
    for (int i = 0; i < 7; i++)
        I(uint32_t, 0xdad0 + i * 4) = *(uint32_t *)RtGetResource(I(void *, ISO_APP), r[i]);
    I(uint32_t, 0xdaf0) = 0;
}
