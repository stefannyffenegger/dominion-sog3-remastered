/* The game logic, 0x100aed6a-0x100af179: from a screen point to its map cell. The map is
 * isometric: cells 64x32 pixels, odd rows shifted by 32. Each function puts the cell's
 * column and row back into *x and *y, the cell's corner on the screen into *sx and *sy,
 * and returns the cell's index. */
#include "iso_core.h"
#include "tilepos.h"

/* the cell under the map point (a, b) (map pixels, iso+0xd709/0xd71e already added) */
static int32_t cell_at(int32_t a, int32_t b, int *x, uint32_t *y, int *sx, int *sy, int32_t ox, int32_t oy)
{
    int32_t d = a - 2 * b;
    int32_t p = d < -0x20 ? (d - 0x20) / 64 : (d + 0x20) / 64;
    int32_t q = -((a + 2 * b + 0x20) / 64);
    int32_t col = (p - q) / 2;
    int32_t row = -(q + p);
    *sx = (int)((((uint32_t)row & 1) << 5) + (uint32_t)(col << 6)) - I(int32_t, 0xd709) + ox;
    *sy = (int)((uint32_t)row << 4) - I(int32_t, 0xd71e) + oy;
    *x = col;
    *y = (uint32_t)row;
    return (int32_t)(((uint32_t)row << (I(uint8_t, 0x1aa0) & 0x1f)) + (uint32_t)col);
}

/* IsoVTilePos: for a point on the view */
int32_t __stdcall IsoVTilePosC(uint8_t *iso, int *x, uint32_t *y, int *sx, int *sy)
{
    g_iso = iso;
    return cell_at(*x - 0x20 + I(int32_t, 0xd709), (int32_t)(*y - 0x10) + I(int32_t, 0xd71e), x, y, sx, sy, 0, 0);
}

/* IsoSnapTilePos: for a point given as a cell corner */
int32_t __stdcall IsoSnapTilePosC(uint8_t *iso, int *x, uint32_t *y, int *sx, int *sy)
{
    g_iso = iso;
    return cell_at(*x + I(int32_t, 0xd709), (int32_t)*y + I(int32_t, 0xd71e), x, y, sx, sy, 0, 0);
}

/* IsoTilePos: for a point on the screen (the view's position added); the corner is given
 * as the cell's centre */
int32_t __stdcall IsoTilePosC(uint8_t *iso, int *x, uint32_t *y, int *sx, int *sy)
{
    g_iso = iso;
    return cell_at(*x - 0x20 + I(int32_t, 0xcd82) + I(int32_t, 0xd709),
                   (int32_t)(*y - 0x10) + I(int32_t, 0xcd86) + I(int32_t, 0xd71e), x, y, sx, sy, 0x20, 0x10);
}
