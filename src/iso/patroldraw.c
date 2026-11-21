/* The game logic, 0x100d1afa-0x100d1dbc: the debug view of the units' patrol legs (drawn
 * with GDI on the debug DC iso+0xd7b3 when 0xeeb bit 2 is set). */
#include "iso_core.h"
#include "patroldraw.h"

#define LEG(u, i) ((u) + 0x1e9 + (i) * 25)
#define PTS ((POINT *)ORIG_PTR(0x10180448))

/* 0x100d1bec: draws unit `u`'s legs as a line, white while it patrols (`on` 1), else black */
void __cdecl patrol_draw(uint8_t *u, uint32_t on)
{
    if (!(I(uint8_t, 0xeeb) & 4))
        return;
    int32_t vx = I(int32_t, 0xcd82), vy = I(int32_t, 0xcd86);
    HDC dc = I(HDC, 0xd7b3);
    HGDIOBJ pen = GetStockObject(on == 1 ? WHITE_PEN : BLACK_PEN);
    HGDIOBJ brush = GetStockObject(NULL_BRUSH);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    int32_t n = P(int16_t, u, 0x1e7) + 1;
    PTS[0].x = (int32_t)(long double)P(float, u, 0x1e9);
    PTS[0].y = (int32_t)(long double)P(float, u, 0x1ed);
    for (int32_t i = 0; i < P(int16_t, u, 0x1e7); i++) {
        PTS[i + 1].x = (int32_t)(long double)P(float, LEG(u, i), 8);
        PTS[i + 1].y = (int32_t)(long double)P(float, LEG(u, i), 0xc);
    }
    for (int32_t i = 0; i < n; i++) {
        PTS[i].x -= vx;
        PTS[i].y -= vy;
    }
    Polyline(I(HDC, 0xd7b3), PTS, n);
    SelectObject(I(HDC, 0xd7b3), old_pen);
    SelectObject(I(HDC, 0xd7b3), old_brush);
}

/* 0x100d1afa: draws the legs of every soldier and vehicle not inside anything */
void __cdecl patrols_draw(void)
{
    int32_t n = I(int32_t, 0x1b43);
    for (int32_t i = 0; i < n; i++) {
        uint8_t *u = I(uint8_t **, ISO_UNITS)[I(int32_t *, 0xd67a)[i]];
        if (!u || (P(uint32_t, u, 0x10e) >> 15 & 1))
            continue;
        if (P(uint16_t, u, 0x28) != 1 && P(uint16_t, u, 0x28) != 3)
            continue;
        if ((P(uint32_t, u, 0x10e) >> 4 & 1) || (P(uint32_t, u, 0x10e) >> 6 & 1))
            continue;
        patrol_draw(u, P(uint16_t, u, 0x1cd) & 1);
    }
}
