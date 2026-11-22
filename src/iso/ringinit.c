/* The game logic, 0x100dcda0-0x100dcf34: the tables of rings of cells around a cell. */
#include "iso_core.h"
#include "ringinit.h"

#define RINGS ((int32_t *)ORIG_PTR(0x10181328))
#define ring_build ORIG(void(__cdecl *)(int32_t, int32_t, int32_t, int32_t), 0x100dcf34)

/* 0x100dced2: the point (*x, *y) moved to its tile: the tile's corner plus (32, 16); the
 * tile's row in *row; returns its cell */
int32_t __cdecl tile_corner(int32_t *x, int32_t *y, int32_t *row)
{
    int32_t px = *x, py = *y, tx, ty;
    int32_t cell = IsoVTilePos(g_iso, &px, &py, &tx, &ty);
    *row = py;
    *x = tx + 0x20;
    *y = ty + 0x10;
    return cell;
}

/* 0x100dcda0: clears the 13 ring records (16 bytes each) and builds rings 1-12 for both
 * kinds of row, from the tile at the origin and the one half a tile along */
void __cdecl rings_init(void)
{
    for (int32_t i = 0; i < 0xd * 4; i++)
        RINGS[i] = 0;
    int32_t x = 0, y = 0, row;
    tile_corner(&x, &y, &row);
    int32_t ax = 0, ay = 0, bx = 0x20, by = 0x10;
    if (row & 1) {
        ax = 0x20;
        ay = 0x10;
        bx = 0;
        by = 0;
    }
    for (int32_t i = 1; i < 0xd; i++) {
        ring_build(ax, ay, i, 1);
        ring_build(bx, by, i, 0);
    }
}
