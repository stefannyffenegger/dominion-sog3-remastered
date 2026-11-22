/* The game logic, 0x100ef91a-0x100f00bf: a structure's footprint on the map cells. */
#include "iso_core.h"
#include "footprint.h"
#include "maptiles.h"
#include "screen.h"
#include "tilepos.h"
#include "trig.h"

#define FP_ROWS 9
#define FP_COLS 5
#define g_srand ORIG(void(__cdecl *)(uint32_t), 0x100f4d60)
#define g_rand ORIG(int(__cdecl *)(void), 0x100f4d80)
#define fn_100f34a5 ORIG(int32_t(__cdecl *)(uint32_t), 0x100f34a5)
#define fn_100f40e2 ORIG(void(__cdecl *)(int32_t, uint32_t), 0x100f40e2)

/* the footprint table (+0x18f8): rows of 5 values for each footprint */
static int32_t fp_value(int32_t shape, int32_t row, int32_t col)
{
    uint8_t *t = I(uint8_t *, 0x18f8);
    int32_t width = P(int32_t, t, 0x30);
    return P(int32_t, P(uint8_t *, t, 0), P(int32_t, t, 0x2c) * width * shape * 4 + width * row * 4 + col * 4);
}

static int is_anchor_value(int32_t v)
{
    return v == 2 || v == 4 || v == 6 || v == 8;
}

/* a cell of the footprint is taken */
static void cell_place(uint32_t *cell, int32_t v, uint32_t id, int vx, uint32_t vy)
{
    if (v == 1 || v == 2) {
        *cell |= 1;
        *cell |= 2;
        if (v == 2)
            *cell |= 8;
    } else if (v == 5 || v == 6) {
        *cell = 0x1000;
    } else if (v == 3 || v == 4) {
        *cell = 0x2000;
    } else if (v >= 8 && v <= 0x1e && I(int32_t, 0x154)) {
        if ((*cell >> 14 & 1) || (*cell >> 15 & 1))
            fn_100f40e2(vx, vy);
        *cell = v > 0x14 ? 1 : 0;
    }
    if (v != 3 && v != 5)
        flags_set_field16(cell, (int16_t)id);
    if (I(uint8_t, 0xeeb) & 8)
        IsoSetTileSet(g_iso, 2, vx, (int)vy);
}

/* a cell of the footprint is given up, and may keep a burnt mark */
static void cell_remove(uint32_t *cell, uint8_t *u, uint32_t id, int32_t idx, int burn, uint8_t *marks, int vx,
                        uint32_t vy, int at_anchor, int next_to_anchor)
{
    if (burn && marks && (uint8_t)(g_rand() & 0x1f) < 0x14 && !marks[idx] &&
        !fn_100f34a5(I(uint32_t *, 0x1904)[idx] & 0x7fff))
        marks[idx] = (uint8_t)(g_rand() % 5 + 0x37);
    if (P(int32_t, P(uint8_t *, u, 0x32), 0x74) == 3) {
        *cell &= 0xffffefff;
        *cell &= 0xffffdfff;
        if ((*cell >> 16 & 0xfff) == (id & 0xffff) && !(*cell >> 14 & 1))
            flags_clear_field16(cell);
        *cell = 0;
        return;
    }
    *cell = 0;
    if (P(int32_t, u, 0x10) == 0x5c) {
        if (at_anchor)
            *cell = 0x20000001;
        else if (next_to_anchor)
            *cell = 0x40000001;
        return;
    }
    uint32_t at = (vy << (I(uint8_t, 0x1aa0) & 31)) + (uint32_t)vx;
    uint32_t kind = I(uint32_t *, 0x1904)[at] & 0x7fff;
    if (*I(int32_t **, 0xd755)[kind])
        I(uint32_t *, 0x1934)[at] |= 1;
}

/* 0x100ef91a: marks the footprint of structure `id` on the map cells (`place` 1) or frees it
 * (0), `burn` leaving burnt ground at random. The cells are found from its first anchor
 * value; the random numbers are seeded from the type, so the marks repeat */
int32_t __stdcall IsoUpdateFootprintC(uint8_t *iso, uint32_t id, int32_t place, int32_t burn)
{
    g_iso = iso;
    uint32_t *cells = I(uint32_t *, 0x1934);
    uint8_t *marks = P(uint8_t *, I(uint8_t *, 0x1960), 0x10);
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
    int32_t x = P(int16_t, u, 0x52), y = P(int16_t, u, 0x54);
    int32_t shape = P(int32_t, P(uint8_t *, u, 0x32), 0x88);
    int32_t off_y = 0, off_x = 0, anchor_row = 0, anchor_col = 0;
    g_srand((uint32_t)(P(int32_t, u, 0xc) + 0x64));
    for (int32_t r = 0; r < FP_ROWS; r++)
        for (int32_t c = 0; c < FP_COLS; c++)
            if (is_anchor_value(fp_value(shape, r, c))) {
                off_y = r << 4;
                off_x = (c << 6) + (r % 2 ? 0x20 : 0);
                anchor_row = r;
                anchor_col = c;
            }
    for (int32_t r = 0; r < FP_ROWS; r++)
        for (int32_t c = 0; c < FP_COLS; c++) {
            int32_t v = fp_value(shape, r, c);
            if (v <= 0)
                continue;
            int sx = (c << 6) + x + (r % 2 ? 0x20 : 0) - off_x;
            int sy = (r << 4) + y - off_y;
            int vx = sx;
            uint32_t vy = (uint32_t)sy;
            int32_t idx = IsoVTilePosC(iso, &vx, &vy, &sx, &sy);
            if (place == 1) {
                cell_place(&cells[idx], v, id, vx, vy);
            } else if (place == 0) {
                int at = r == anchor_row && c == anchor_col;
                int beside = (r == anchor_row - 1 || r == anchor_row + 1) && (c == anchor_col || c == anchor_col + 1);
                cell_remove(&cells[idx], u, id, idx, burn, marks, vx, vy, at, beside);
            }
        }
    if (I(uint8_t, 0xeeb) & 8)
        IsoRefreshScreen(iso);
    return 0;
}
