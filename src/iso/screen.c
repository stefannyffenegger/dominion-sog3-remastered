/* The game logic, 0x10008772-0x100090e0: refreshing the screen, clicks on the radar,
 * the display mode, the unit name popup, the radar on and off, a unit's screen bounds,
 * and the countdown clock. */
#include "iso_core.h"
#include "view.h"
#include "loop.h"
#include "screen.h"

/* not rewritten yet (names ours, provisional) */
#define order_move ORIG(void(__cdecl *)(int, int, uint32_t, int, uint32_t, int, int), 0x100b39aa)
#define time_split ORIG(void(__cdecl *)(uint32_t, uint32_t *, uint32_t *, uint32_t *), 0x1005cc4a)
#define BF_BlastText                                                                         \
    ORIG(int(__cdecl *)(void *, void *, uint32_t, void *, int, int, const char *, int), 0x101120b6)

#define HSL I(void *, ISO_HSL)
#define APP I(void *, ISO_APP)
#define ISO_RADAR_ON 300 /* 0x12c */
#define ISO_RADAR_SPRITE 0xbf1e

static int32_t ftol(long double v) { return (int32_t)(int64_t)v; }

/* 0x10008772 (the second argument is unused) */
uint32_t __stdcall IsoGetProfilerValue(uint8_t *iso, uint32_t unused)
{
    (void)unused;
    g_iso = iso;
    return P(uint32_t, iso, 0xbeda);
}

/* 0x10008797: redraws the screen (inside the interface's frame when there is one) */
void __stdcall IsoRefreshScreen(uint8_t *iso)
{
    g_iso = iso;
    uint8_t *frame = P(uint8_t *, iso, 0x1960);
    SpriteRefreshRect(P(void *, iso, ISO_HSL), frame ? (const RECT *)(frame + 0x5c) : NULL);
}

/* 0x100087fb */
void __stdcall IsoRefreshRect(uint8_t *iso, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    g_iso = iso;
    RECT r = {left, top, right, bottom};
    SpritePaintRect(P(void *, iso, ISO_HSL), &r, 0);
}

/* 0x10008847: a click on the radar at (x, y): moves the view there (`jump`), else sends
 * the local player's selected units there */
void __stdcall IsoRadarClickPos(uint8_t *iso, int x, int y, int jump)
{
    g_iso = iso;
    if (!P(int, iso, ISO_RADAR_ON))
        return;
    if (P(uint32_t, iso, 0x107b) < P(uint32_t, iso, 0x1077))
        P(uint32_t, iso, 0x1077) -= P(uint32_t, iso, 0x107b);
    else
        P(uint32_t, iso, 0x1077) = 0;
    I(int32_t, 0x144) = 0;
    uint8_t *radar = I(uint8_t *, 0xbefe), *sprite = I(uint8_t *, ISO_RADAR_SPRITE);
    int32_t mx = I(int32_t, 0xd6f1) * (x - P(int32_t, sprite, 0x3c)) / P(int32_t, radar, 8);
    int32_t my = I(int32_t, 0xd6f5) * (y - P(int32_t, sprite, 0x40)) / P(int32_t, radar, 0xc);
    float scale = I(float, 0xd733);
    if (scale > G(float, 0x10113054))
        my = ftol((long double)my * scale);
    else
        mx = -ftol((long double)mx * scale);
    int32_t w = I(int32_t, 0xd709), h = I(int32_t, 0xd71e);
    if (mx > w || (-mx != w && -w >= mx) || my > h || (-my != h && -h >= my))
        return;
    uint32_t local = I(uint32_t, ISO_LOCAL_PLAYER);
    if (!jump) {
        if (SELECTED_N(local) > 0) {
            DebugStr((const char *)ORIG_PTR(0x10118af8), SELECTED_N(local));
            order_move((int)local, mx, (uint32_t)my, 2, 10, 0, I(int, 0xd797) == 0);
        }
        return;
    }
    int32_t vw = I(int32_t, 0xd71a), vh = I(int32_t, 0xd72e);
    mx += vw < 0 ? -(vw >> 1) : vw >> 1;
    my += vh < 0 ? -(vh >> 1) : vh >> 1;
    SpriteSetScrollPos(HSL, -mx, -my);
    int32_t pos[2];
    SpriteGetScrollPos(HSL, pos);
    I(int32_t, 0xcd82) = -pos[0];
    I(int32_t, 0xcd86) = -pos[1];
    I(int32_t, 0xbfc1) = pos[0];
    I(int32_t, 0xbfc5) = pos[1];
    I(int32_t, 0xd6bb) = I(int32_t, 0xd78b) + I(int32_t, 0xcd86) - 0x80;
    I(int32_t, 0xd6c3) = I(int32_t, 0xd793) + I(int32_t, 0xcd86) + 0x80;
    I(int32_t, 0xd6bf) = I(int32_t, 0xd78f) + I(int32_t, 0xcd82) + 0x80;
    I(int32_t, 0xd6b7) = I(int32_t, 0xd787) + I(int32_t, 0xcd82) - 0x80;
    IsoCheckRadarScreenAreaUpdate(iso);
    IsoDisplayLoop(iso);
}

/* 0x10008c12: DirectDraw (full screen) on or off */
int __cdecl IsoSetDirectDrawMode(uint8_t *iso, int full)
{
    g_iso = iso;
    I(int32_t, 0xd79f) = RtSetDisplayMode(P(void *, iso, ISO_APP), P(uint32_t, iso, 0xd70e),
                                          P(uint32_t, iso, 0xd722), 8, full != 0, 0, 0, 1);
    return I(int32_t, 0xd79f);
}

/* 0x10008c7d */
void __cdecl IsoHideUnitNamePopup(uint8_t *iso)
{
    g_iso = iso;
    if (P(int, iso, 0xd692)) {
        void *s = RtGetResource(P(void *, iso, ISO_APP), P(uint32_t, iso, 0xd686))[1];
        if (s)
            SpriteHide(HSL, s, 1);
    }
    if (I(int, 0xd68a)) {
        I(uint32_t, 0xd68a) = 0;
        I(uint32_t, 0xd68e) = 0;
        I(uint32_t, 0xd692) = 0;
    }
}

/* 0x10008d30: the radar on or off */
void __cdecl radar_show(int on)
{
    if (!I(int, ISO_HAVE_SPRITES))
        return;
    if (!on) {
        if (I(int, 0xd79f))
            SpriteRefreshInterfaceArea(HSL, I(void *, ISO_RADAR_SPRITE));
        SpriteHide(HSL, I(void *, ISO_RADAR_SPRITE), 1);
    } else {
        if (I(int, 0xbf06)) {
            G(uint32_t, 0x1015728c) = 2;
            G(uint32_t, 0x10157290) = 0;
            G(uint32_t, 0x10157294) = 3;
            G(uint32_t, 0x10157298) = 1;
            G(uint32_t, 0x1015729c) = 0xffffffff;
            SpriteSendMessage(HSL, I(void *, ISO_RADAR_SPRITE), 7, ORIG_PTR(0x10157288));
        }
        I(uint32_t, 0x1077) = 0;
        I(uint32_t, 0x107b) = 0;
    }
    I(int32_t, ISO_RADAR_ON) = on;
}

/* 0x10008e49: the unit's sprite's bounds (one pixel larger) on the map */
void __cdecl unit_update_bounds(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (!u)
        return;
    uint8_t *s = P(uint8_t *, u, 4);
    int32_t hx = P(int32_t, s, 0x3c), hy = P(int32_t, s, 0x40);
    RECT r;
    SpriteGetBounds(HSL, s, &r);
    InflateRect(&r, 1, 1);
    P(int32_t, u, 0xc6) = P(int16_t, u, U_X) - (hx - r.left);
    P(int32_t, u, 0xca) = P(int16_t, u, U_Y) - (hy - r.top);
    P(int32_t, u, 0xce) = (r.right - hx) + P(int16_t, u, U_X);
    P(int32_t, u, 0xd2) = (r.bottom - hy) + P(int16_t, u, U_Y);
}

/* 0x10008f27: the countdown clock (hours:minutes:seconds); removed when it runs out */
void __cdecl countdown_draw(void)
{
    if (I(uint32_t, 0xcd9a) < I(uint32_t, 0x1093)) {
        I(uint32_t, 0xcd8a) = 0;
        I(uint32_t, 0xcd9a) = 0;
        SpriteHide(HSL, I(void *, 0xcd96), 1);
        I(uint32_t, 0xcd96) = 0;
        I(uint32_t, 0xcd92) = 0;
        RtUnload(APP, I(uint32_t, 0xcd8e));
        return;
    }
    uint32_t a, b, c;
    time_split(I(uint32_t, 0xcd9a) - I(uint32_t, 0x1093), &a, &b, &c);
    uint8_t *dib = I(uint8_t *, 0xcd92);
    memset(P(void *, dib, 0x20), 0, P(uint32_t, dib, 8) * P(uint32_t, dib, 0xc));
    char text[32];
    wsprintfA(text, (const char *)ORIG_PTR(0x10118b0c), a, b, c);
    BF_BlastText(I(void *, 0x1812), dib, I(uint32_t, 0xd9a6), ORIG_PTR(0x101418d8), -1, 0, text, 0);
    G(uint32_t, 0x101572c4) = 0;
    G(uint32_t, 0x101572c8) = 0;
    G(uint32_t, 0x101572cc) = 0;
    SpriteSendMessage(HSL, I(void *, 0xcd96), 4, ORIG_PTR(0x101572c0));
}
