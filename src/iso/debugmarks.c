/* The game logic: the debug view's crosses on the map (up to 128, 12 bytes each at
 * 0x101806e8: x, y, white), 0x100d5e5c, 0x100d6003 and 0x100d601d. */
#include "iso_core.h"
#include "debugmarks.h"

#define COUNT G(int32_t, 0x10159240)
#define MARKS ((int32_t *)ORIG_PTR(0x101806e8))

/* 0x100d5e5c: adds a cross at (x, y) unless one is already there; the number of crosses */
int32_t __cdecl debug_mark(int32_t x, int32_t y, int32_t white)
{
    if (COUNT < 0x80) {
        int32_t found = -1;
        for (int32_t i = 0; i < COUNT; i++) {
            if (MARKS[i * 3] == x && MARKS[i * 3 + 1] == y) {
                found = i;
                break;
            }
        }
        if (found == -1) {
            MARKS[COUNT * 3] = x;
            MARKS[COUNT * 3 + 1] = y;
            MARKS[COUNT * 3 + 2] = white;
            COUNT++;
        }
    }
    return COUNT;
}

/* 0x100d6003: removes all crosses */
void __cdecl marks_clear(void)
{
    COUNT = 0;
}

/* 0x100d601d: with debug flag 0xeeb bit 2 set, draws the crosses on the debug DC, 12 pixels
 * wide, black or white */
void __cdecl marks_draw(void)
{
    if (!(I(uint8_t, 0xeeb) & 4) || !COUNT)
        return;
    int32_t black = 1, vx = I(int32_t, 0xcd82), vy = I(int32_t, 0xcd86);
    HGDIOBJ black_pen = GetStockObject(BLACK_PEN), white_pen = GetStockObject(WHITE_PEN);
    HGDIOBJ brush = GetStockObject(NULL_BRUSH);
    HGDIOBJ old_pen = SelectObject(I(HDC, 0xd7b3), black_pen);
    HGDIOBJ old_brush = SelectObject(I(HDC, 0xd7b3), brush);
    for (int32_t i = 0; i < COUNT; i++) {
        int32_t x = MARKS[i * 3] - vx, y = MARKS[i * 3 + 1] - vy;
        if (MARKS[i * 3 + 2] && black) {
            SelectObject(I(HDC, 0xd7b3), white_pen);
            black = 0;
        } else if (!MARKS[i * 3 + 2] && !black) {
            SelectObject(I(HDC, 0xd7b3), black_pen);
            black = 1;
        }
        MoveToEx(I(HDC, 0xd7b3), x - 6, y - 6, 0);
        LineTo(I(HDC, 0xd7b3), x + 6, y + 6);
        MoveToEx(I(HDC, 0xd7b3), x + 6, y - 6, 0);
        LineTo(I(HDC, 0xd7b3), x - 6, y + 6);
    }
    SelectObject(I(HDC, 0xd7b3), old_pen);
    SelectObject(I(HDC, 0xd7b3), old_brush);
}
