/* The game logic, 0x10020b81-0x10020bea and 0x10021667-0x10021c65: the map's cells and
 * tiles for the scripts: IsoRescanMap, IsoGetTile, IsoSetTile, IsoSetTileSet,
 * IsoSetTileLib; and IsoMessageBox. */
#include "iso_core.h"
#include "maptiles.h"

#define message_box ORIG(void(__cdecl *)(int, const char *, const char *, uint32_t), 0x1005c515)

#define ISO_OBJ_MAP 0x1934
#define ISO_TILE_INFO 0xd755 /* per tile: its info (first int: blocks) */

/* 0x10020b81: a message box with strings `text_id` and `title_id` */
void __stdcall IsoMessageBox(uint8_t *iso, uint32_t title_id, uint32_t text_id, uint32_t flags)
{
    g_iso = iso;
    const char *title = RtGetString(I(void *, ISO_APP), title_id);
    const char *text = RtGetString(I(void *, ISO_APP), text_id);
    message_box(0, text, title, flags);
}

static int on_map(int x, int y)
{
    return I(void *, ISO_MAP) && x >= 0 && x <= I(int32_t, ISO_MAP_W) && y >= 0 && y <= I(int32_t, ISO_MAP_H);
}

/* 0x10021667: clears the cells' top byte and works out which cells block: the edge
 * (one column, two rows) and cells whose tile blocks */
int __stdcall IsoRescanMap(uint8_t *iso)
{
    g_iso = iso;
    if (!I(void *, ISO_MAP) || !I(void *, ISO_OBJ_MAP))
        return 0;
    int w = I(int32_t, ISO_MAP_W), h = I(int32_t, ISO_MAP_H);
    for (int row = 0; row < I(int32_t, ISO_MAP_H); row++) {
        int c = row << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f);
        for (int col = 0; col < I(int32_t, ISO_MAP_W); col++, c++) {
            uint32_t *m = &I(uint32_t *, ISO_MAP)[c], *o = &I(uint32_t *, ISO_OBJ_MAP)[c];
            *m &= 0xffffff;
            if (col < 1 || col > w - 2 || row < 2 || row > h - 2) {
                *o |= 1;
                *o |= 2;
            } else if (!*I(int32_t **, ISO_TILE_INFO)[*m & 0x7fff]) {
                *o &= ~1u;
                *o &= ~2u;
            } else {
                *o |= 1;
            }
        }
    }
    return 1;
}

/* 0x10021863: clears the object map's marks of every cell (bits 2-11, 13, 14 and the
 * group in 16-27) */
void __cdecl map_clear_marks(void)
{
    uint32_t n = (uint32_t)(I(int32_t, ISO_MAP_H) * I(int32_t, ISO_MAP_W));
    for (uint32_t i = 0; i < n; i++)
        I(uint32_t *, ISO_OBJ_MAP)[i] &= ~(0x4u | 0x8 | 0xf0 | 0xf00 | 0x2000 | 0x4000 | 0x0fff0000);
}

/* 0x10021962: tile library slot `slot` (0-9) gets the name of string `name_id` */
int __stdcall IsoSetTileLib(uint8_t *iso, int slot, uint32_t name_id)
{
    g_iso = iso;
    const char *name = RtGetString(I(void *, ISO_APP), name_id);
    if (slot < 0 || slot > 9)
        return 0;
    lstrcpyA((char *)(g_iso + slot * 0x104 + 0xc352), name);
    return 1;
}

/* 0x100219df: the tile of cell (x, y), or -1 */
int32_t __stdcall IsoGetTile(uint8_t *iso, int x, int y)
{
    g_iso = iso;
    if (!on_map(x, y))
        return -1;
    return (int32_t)(I(uint32_t *, ISO_MAP)[(y << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f)) + x] & 0x7fff);
}

/* 0x10021a7e: cell (x, y) gets tile `tile` (also in the tile background). Only in the
 * editor (0x10c) or without sprites; otherwise it does nothing and returns 1. */
int __stdcall IsoSetTile(uint8_t *iso, uint32_t tile, int x, int y)
{
    g_iso = iso;
    if (!I(int32_t, 0x10c) && I(int32_t, ISO_HAVE_SPRITES))
        return 1;
    if (!on_map(x, y))
        return 0;
    int c = (y << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f)) + x;
    I(uint32_t *, ISO_MAP)[c] = (I(uint32_t *, ISO_MAP)[c] & 0xffff8000) | (tile & 0x7fff);
    P(int16_t *, I(uint8_t *, 0x1960), 8)[c] = (int16_t)tile;
    return 1;
}

/* 0x10021b8b: the tile background's tile of cell (x, y), as IsoSetTile */
int __stdcall IsoSetTileSet(uint8_t *iso, int16_t tile, int x, int y)
{
    g_iso = iso;
    if (!I(int32_t, 0x10c) && I(int32_t, ISO_HAVE_SPRITES))
        return 1;
    if (!on_map(x, y))
        return 0;
    P(int16_t *, I(uint8_t *, 0x1960), 8)[(y << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f)) + x] = tile;
    return 1;
}
