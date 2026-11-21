/* The game logic, 0x100ae68b-0x100aed6a: scrolling, scenario and display exports. */
#include "iso_core.h"
#include "debugtime.h"
#include "dispexp.h"

/* IsoSetScrollRate: the scroll steps (iso+0xd712, 0xd726) and the scroll delay (iso+0x130,
 * the previous one returned) */
uint32_t __stdcall IsoSetScrollRateC(uint8_t *iso, uint32_t dx, uint32_t dy, uint32_t delay)
{
    g_iso = iso;
    uint32_t was = I(uint32_t, 0x130);
    I(uint32_t, 0xd712) = dx;
    I(uint32_t, 0xd726) = dy;
    I(uint32_t, 0x130) = delay;
    return was;
}

/* IsoGetScenarioInfo: the map's size in cells and pixels (and half that), its variant, and
 * the numbers of units and objects */
int32_t __stdcall IsoGetScenarioInfoC(uint8_t *iso, uint32_t *out)
{
    g_iso = iso;
    out[0] = I(uint32_t, ISO_MAP_W);
    out[1] = I(uint32_t, ISO_MAP_H);
    out[2] = (uint32_t)(I(int32_t, 0xcd7e) * I(int32_t, ISO_MAP_W));
    out[3] = (I(uint32_t, 0xc34e) >> 1) * I(uint32_t, ISO_MAP_H);
    out[4] = (uint32_t)(I(int32_t, 0xcd7e) * I(int32_t, ISO_MAP_W)) >> 1;
    out[5] = (I(uint32_t, 0xc34e) >> 1) * I(uint32_t, ISO_MAP_H) >> 1;
    out[6] = I(uint16_t, 0xc340);
    out[7] = I(uint32_t, ISO_UNIT_COUNT);
    out[8] = I(uint32_t, 0x1b4d);
    return 1;
}

/* IsoGetDisplayMode */
void __stdcall IsoGetDisplayModeC(uint8_t *iso, uint32_t *w, uint32_t *h, uint32_t *bpp, uint32_t *refresh)
{
    g_iso = iso;
    uint32_t r;
    RtGetDisplayMode(I(void *, ISO_APP), w, h, bpp, &r);
    *refresh = r;
}

static void view_size(int32_t w, int32_t h, int32_t ox, int32_t oy)
{
    I(int32_t, 0xd716) = w - (ox < 0 ? -ox : ox);
    I(int32_t, 0xd72a) = h - (oy < 0 ? -oy : oy);
    I(int32_t, 0xd71a) = ox;
    I(int32_t, 0xd72e) = oy;
}

/* IsoSetScreenRes: the view's size (the screen less its offsets) and, with `set`, the
 * display mode. The original then only builds a debug description of the buffers */
int32_t __stdcall IsoSetScreenResC(uint8_t *iso, int32_t w, int32_t h, uint32_t bpp, int32_t ox, int32_t oy,
                                   int32_t set, uint32_t v128)
{
    g_iso = iso;
    view_size(w, h, ox, oy);
    I(uint32_t, 0x128) = v128;
    I(int32_t, 0xd70e) = w;
    I(int32_t, 0xd722) = h;
    iso_debug_print();
    if (!set)
        return 1;
    if (!RtSetDisplayMode(I(void *, ISO_APP), (uint32_t)w, (uint32_t)h, bpp, I(uint32_t, 0xd79f), 0, 0, I(int32_t, 0xd79f)))
        return 0;
    RtGetDirectDrawStats(I(void *, ISO_APP), g_iso + 0x1087, g_iso + 0x108b, g_iso + 0x108f);
    return 1;
}

/* IsoSetDisplayMode: as IsoSetScreenRes, with the display flags (iso+0xd79f); the mode is
 * set when asked or with flags */
int32_t __stdcall IsoSetDisplayModeC(uint8_t *iso, int32_t w, int32_t h, uint32_t bpp, int32_t ox, int32_t oy,
                                     int32_t set, int32_t flags, uint32_t unused)
{
    (void)unused;
    g_iso = iso;
    view_size(w, h, ox, oy);
    I(int32_t, 0xd79f) = flags;
    I(uint32_t, 0x128) = I(uint32_t, 0xd79f);
    I(int32_t, 0xd70e) = w;
    I(int32_t, 0xd722) = h;
    iso_debug_print();
    if (!set && !flags)
        return 1;
    return RtSetDisplayMode(I(void *, ISO_APP), (uint32_t)w, (uint32_t)h, bpp, (uint32_t)flags, 0, 0, I(int32_t, 0xd79f));
}

/* IsoGetScreenPos: the view's position on the map */
void __stdcall IsoGetScreenPosC(uint8_t *iso, int32_t *x, int32_t *y)
{
    g_iso = iso;
    *x = I(int32_t, 0xcd82);
    *y = I(int32_t, 0xcd86);
}
