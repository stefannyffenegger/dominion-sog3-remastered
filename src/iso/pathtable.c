/* The game logic, 0x100983ee-0x100989da: the path table (0xe113: 0x20 bytes; +0x14 the
 * number of cells, +0x18 the rows, +0x1a the columns, +0x1c a byte per cell: 0xff blocked,
 * 0xfe open), the sector under a point, and whether a point is on the map. */
#include "iso_core.h"
#include "pathtable.h"

#define TABLE I(uint8_t *, 0xe113)
#define path_table_free ORIG(void(__cdecl *)(void), 0x100986d3)
#define path_buffer_alloc ORIG(void *(__cdecl *)(void), 0x100986ad)

/* 0x100983ee: the open (0xfe) and blocked (0xff) cells: blocked cells (bit 0) count as
 * open again when what stands there is a transport (kind 0x88-0x9f) */
void __cdecl path_table_fill(uint8_t *cells)
{
    uint8_t *t = TABLE;
    for (int32_t r = 0; r < P(int16_t, t, 0x18); r++)
        for (int32_t c = 0; c < P(int16_t, t, 0x1a); c++) {
            int32_t k = P(int16_t, t, 0x1a) * r + c;
            uint32_t m = I(uint32_t *, 0x1934)[k];
            if (!(m & 1)) {
                cells[k] = 0xfe;
                continue;
            }
            cells[k] = 0xff;
            uint32_t id = m >> 16 & 0xfff;
            if (id) {
                uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
                cells[k] = (!u || P(int32_t, u, 0x10) < 0x88 || P(int32_t, u, 0x10) > 0x9f) ? 0xfe : 0xff;
            }
        }
}

/* 0x10098542: (re)makes the path table at the map's size and fills it; in the map editor
 * also the editor's path buffer. Nonzero if all went well */
int32_t __cdecl path_table_init(void)
{
    int ok;
    if (TABLE)
        path_table_free();
    TABLE = winmalloc(0x20, 0);
    if (!TABLE) {
        ok = 0;
    } else {
        uint32_t *t = (uint32_t *)TABLE;
        t[0] = t[1] = t[2] = t[3] = t[4] = 0;
        P(int16_t, t, 0x18) = (int16_t)I(int32_t, ISO_MAP_H);
        P(int16_t, t, 0x1a) = (int16_t)I(int32_t, ISO_MAP_W);
        t[5] = (uint32_t)(P(int16_t, t, 0x18) * P(int16_t, t, 0x1a));
        t[7] = (uint32_t)(uintptr_t)winmalloc(t[5], 0);
        ok = t[7] != 0;
        if (ok)
            path_table_fill(P(uint8_t *, TABLE, 0x1c));
        else
            OutputDebugStringA((const char *)ORIG_PTR(0x10153268));
    }
    if (!I(int32_t, ISO_HAVE_SPRITES) && !path_buffer_alloc())
        ok = 0;
    if (!ok)
        OutputDebugStringA((const char *)ORIG_PTR(0x1015329c));
    return ok;
}

/* 0x10098865: whether a point (screen-relative) lies inside the map */
int32_t __cdecl point_on_map(int32_t x, int32_t y)
{
    int32_t mx = x + I(int32_t, 0xd709), my = y + I(int32_t, 0xd71e);
    return !(mx < 1 || I(int32_t, 0xd6f1) <= mx || my < 1 || I(int32_t, 0xd6f5) <= my);
}

/* 0x100987c8: the path table's value for the cell under a point, or 0xff */
uint32_t __cdecl path_cell_at(int32_t x, uint32_t y)
{
    if (point_on_map(x, (int32_t)y) == 1) {
        int tx = x, ty = (int)y, a, b;
        int32_t c = IsoVTilePos(g_iso, &tx, &ty, &a, &b);
        if (c >= 0 && c < P(int32_t, TABLE, 0x14))
            return P(uint8_t *, TABLE, 0x1c)[c];
    }
    return 0xff;
}

/* 0x10098970: walks a path's 20 sector slots without effect (a leftover) */
void __cdecl path_slots_walk(void *path) { (void)path; }

/* 0x1009a526: the debug overlay of the path sectors (iso+0xeeb bit 2): for each sector its
 * box's diagonal, lines from its centre to its linked sectors' centres, and its number */
void __cdecl path_sectors_draw(void)
{
    uint8_t *t = TABLE;
    if (!(I(uint8_t, 0xeeb) & 4) || !t)
        return;
    HDC dc = I(HDC, 0xd7b3);
    int16_t sx = (int16_t)I(int32_t, 0xcd82), sy = (int16_t)I(int32_t, 0xcd86);
    HGDIOBJ pen = GetStockObject(6), brush = GetStockObject(5);
    HGDIOBJ old_pen = SelectObject(dc, pen), old_brush = SelectObject(dc, brush);
    for (int32_t k = 0; k < P(int32_t, t, 4); k++) {
        uint8_t *s = P(uint8_t *, t, 8) + k * 0x46c;
        if (!P(int32_t, s, 0))
            continue;
        MoveToEx(dc, (int16_t)(P(int16_t, s, 4) - sx), (int16_t)(P(int16_t, s, 6) - sy), NULL);
        LineTo(dc, (int16_t)(P(int16_t, s, 8) - sx), (int16_t)(P(int16_t, s, 10) - sy));
        for (int l = 0; l < 0x14; l++) {
            int16_t o = P(int16_t, s, 0x1a + l * 4);
            if (o < 0)
                continue;
            uint8_t *os = P(uint8_t *, t, 8) + o * 0x46c;
            MoveToEx(dc, (int16_t)(P(int16_t, s, 0x14) - sx), (int16_t)(P(int16_t, s, 0x16) - sy), NULL);
            LineTo(dc, (int16_t)(P(int16_t, os, 0x14) - sx), (int16_t)(P(int16_t, os, 0x16) - sy));
        }
        char num[8];
        ORIG(int(__cdecl *)(char *, const char *, ...), 0x100f6f40)(num, (const char *)ORIG_PTR(0x101532f8), k);
        TextOutA(dc, (int16_t)(P(int16_t, s, 0x14) - sx), (int16_t)(P(int16_t, s, 0x16) - sy), num, lstrlenA(num));
    }
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
}
