/* The tile map (0x1005c010-0x1005f6ac): building the tile blitters' tables, choosing the
 * blitter for a clipped tile, and drawing the map's visible tiles and the layers above
 * them (overlays, the shroud) row by row. Rewritten from the original; the blitters
 * themselves are in tileblit.c.
 *
 * Isometric maps (type 1 and 2) are drawn in half-tile rows: every second row is shifted
 * by half a tile. Other maps are a plain grid of bitmaps. */
#include "orig.h"
#include "blit.h"
#include "tileblit.h"
#include "tiles.h"
#include "draw.h"
#include "view.h"

#define g_hsl G(uint8_t *, 0x10089e24)
#define H(type, off) (*(type *)(g_hsl + (off)))
#define S(type, p, off) (*(type *)((uint8_t *)(p) + (off)))

/* HSL fields */
#define HSL_TILE_MAP 0x480
#define HSL_WIDTH 0x8a4
#define HSL_HEIGHT 0x8a8
#define HSL_ORIGIN_Y 0x8d0
#define HSL_SCROLL_X 0x8d4
#define HSL_SCROLL_Y 0x8d8
#define HSL_VIEW2 0x914
#define HSL_VIEW2_BOTTOM 0x920
#define HSL_TOP_DOWN 0x950
#define HSL_DIRECTDRAW 0x954
#define HSL_TABLES_BUILT 0xb44
#define HSL_DD_PITCH 0xb48
#define HSL_QUALITY 0xb74
#define HSL_OVERLAY_QUALITY 0xb7c

/* tile map fields */
#define MAP_GRID 0x00        /* plain maps: a bitmap per cell */
#define MAP_TILES 0x04       /* pixels of each tile number */
#define MAP_CELLS 0x08       /* u16 tile number per cell */
#define MAP_OVERLAYS 0x0c    /* overlay bitmaps by number */
#define MAP_OVERLAY_IDX 0x10 /* u8 overlay number per cell */
#define MAP_MASK_A 0x14      /* u32 per cell */
#define MAP_MASK_B 0x18
#define MAP_TILE_W 0x1c
#define MAP_TILE_H 0x20
#define MAP_W 0x24
#define MAP_H 0x28
#define MAP_TYPE 0x2c /* short: 1 or 2 isometric */
#define MAP_FLAGS 0x30
#define MAP_MASK_A_BITS 0x34
#define MAP_MASK_B_BITS 0x38
#define MAP_ORG_X 0x44
#define MAP_ORG_Y 0x48
#define MAP_CENTER_X 0x4c
#define MAP_CENTER_Y 0x50
#define MAP_PIX_W 0x54
#define MAP_PIX_H 0x58
#define MAP_RECT 0x5c
#define MAP_CLIP 0x6c

#define OVERLAY_A 0x108 /* drawn where MAP_MASK_B matches (see tiles_row_layers) */
#define OVERLAY_B 0x10c

static TileTables g_tables; /* 0x1008a6d0, 0x1008d838, 0x1008d738, 0x1008d6f8 */

/* state of the current pass (0x1008d6e0...) */
static RECT g_rect;      /* area being drawn */
static int g_org_x;      /* 0x1008d6f0 */
static int g_org_y;      /* 0x1008f838 */
static int g_map_w;      /* 0x1008f83c */
static int g_tile_w;     /* 0x1008d734 */
static int g_half_w;     /* 0x1008c6d0 */
static uint32_t g_dst;   /* 0x10089ec8 */

/* ---- tables (0x1005c010-0x1005c5d6) ---- */

/* 0x1005c010 (unused): a tile's outline as rows of colors 10/125 and 10/115 */
void __cdecl tile_outline_build(void)
{
    uint8_t *o = (uint8_t *)ORIG_PTR(0x10089ed0);
    const uint32_t *c = (const uint32_t *)ORIG_PTR(0x1007b514);
    int i = 4, n = 2;
    o[0] = o[1] = o[2] = o[3] = 0;
    for (int r = 0; r < 0xf; r++, n += 4) {
        for (int k = 0; k < n; k++)
            o[i++] = (uint8_t)(k == 0 ? c[0] : k == n - 1 ? c[1] : c[2]);
        o[i] = o[i + 1] = 0;
        i += 2;
    }
    for (int k = 0; k < 0x40; k++)
        o[i++] = 0xe1;
    for (int k = 0; k < 0x40; k++)
        o[i++] = 0xe6;
    n = 0x3a;
    for (int r = 0x11; r < 0x20; r++, n -= 4) {
        for (int k = 0; k < n; k++)
            o[i++] = (uint8_t)(k == 0 ? c[3] : k == n - 1 ? c[4] : c[5]);
        o[i] = o[i + 1] = 0;
        i += 2;
    }
}

/* 0x1005c471, 0x1005c213, 0x1005c51e: the parts of the tables; tile_tables_build does all three */
void __cdecl tile_tables_start(void)
{
    tile_tables_start_build(&g_tables);
}

void __cdecl tile_tables_adv(void)
{
    tile_tables_adv_build(&g_tables);
}

void __cdecl tile_tables_mirror(int pitch)
{
    for (int i = 0; i < 0xf; i++)
        g_tables.mirror[i] = (i * -2 + 0x1f) * pitch;
}

/* 0x1005c569 */
void __cdecl tile_tables_make(int pitch)
{
    char msg[256];
    wsprintfA(msg, "make_tile_blitter_tables() : pitch = %d  HorzRes = %d\n", pitch, H(int, HSL_WIDTH));
    OutputDebugStringA(msg);
    tile_tables_build(&g_tables, pitch);
    H(uint32_t, HSL_TABLES_BUILT) = 1;
}

/* ---- blitters, with the original's arguments ---- */

#define BM(p) ((void *)(uintptr_t)(p))

/* 0x1005c5d6 */
void __cdecl tile_full_c(uint32_t dst, uint32_t tile, int x, int y)
{
    tile_full(&g_tables, BM(dst), BM(tile), x, y);
}

/* 0x1005c99a */
void __cdecl tile_rect_c(uint32_t dst, uint32_t tile, int x, int y, uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1)
{
    tile_rect(BM(dst), BM(tile), x, y, x0, x1, y0, y1);
}

/* 0x1005cb5c */
void __cdecl tile_clip_bottom_c(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y)
{
    tile_clip_bottom(&g_tables, BM(dst), BM(tile), w, h, x, y);
}

#define CLIP_WRAPPER(name)                                                                                        \
    void __cdecl name##_c(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y, uint32_t cx,         \
                          uint32_t cy)                                                                            \
    {                                                                                                             \
        name(&g_tables, BM(dst), BM(tile), w, h, x, y, cx, cy);                                                   \
    }
CLIP_WRAPPER(tile_clip_top)          /* 0x1005ccf1 */
CLIP_WRAPPER(tile_clip_right)        /* 0x1005ce78 */
CLIP_WRAPPER(tile_clip_left)         /* 0x1005d1b7 */
CLIP_WRAPPER(tile_clip_left_top)     /* 0x1005d550 */
CLIP_WRAPPER(tile_clip_right_top)    /* 0x1005d7dc */
CLIP_WRAPPER(tile_clip_left_bottom)  /* 0x1005da50 */
CLIP_WRAPPER(tile_clip_right_bottom) /* 0x1005dcec */

/* 0x1005df6e */
void __cdecl tile_frame_c(uint32_t dst, int x, uint32_t y, uint32_t w, int h)
{
    tile_frame(BM(dst), x, y, w, h);
}

/* 0x1005e4be */
void __cdecl tile_fill_c(uint32_t dst, int x, int y)
{
    tile_fill(BM(dst), x, y);
}

/* ---- one tile ---- */

/* 0x1005e056: picks the blitter for a clipped tile */
void __cdecl tile_draw_clipped(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y, uint32_t cx,
                               uint32_t cy)
{
    if (w == 0x40 || h == 0x20) {
        if (w == 0x40) {
            if (cy == 0)
                tile_clip_bottom_c(dst, tile, 0x40, h, x, y);
            else
                tile_clip_top_c(dst, tile, 0x40, h, x, y, cx, cy);
        } else if (cx == 0)
            tile_clip_right_c(dst, tile, w, h, x, y, 0, cy);
        else {
            if (H(uint32_t, HSL_DIRECTDRAW)) {
                if (cx & 1)
                    cx--;
                w = 0x40 - cx;
            }
            tile_clip_left_c(dst, tile, w, h, x, y, cx, cy);
        }
    } else if (cx == 0) {
        if (cy == 0)
            tile_clip_right_bottom_c(dst, tile, w, h, x, y, 0, 0);
        else
            tile_clip_right_top_c(dst, tile, w, h, x, y, 0, cy);
    } else if (cy == 0)
        tile_clip_left_bottom_c(dst, tile, w, h, x, y, cx, 0);
    else
        tile_clip_left_top_c(dst, tile, w, h, x, y, cx, cy);
}

/* 0x10066470 */
void __stdcall tile_full_dd_c(uint32_t dst, uint32_t tile, int x, int y)
{
    tile_full_dd(&g_tables, BM(dst), BM(tile), x, y);
}

static void full_tile(uint32_t dst, uint32_t tile, int x, int y)
{
    if (!H(uint32_t, HSL_DIRECTDRAW))
        tile_full_c(dst, tile, x, y);
    else
        tile_full_dd_c(dst, tile, x, y);
}

/* 0x1005e243: draws a tile w x h from cx, cy at x, y. With clip (GDI only) the visible
 * part is worked out again from the whole tile and the screen. */
void __cdecl tile_draw(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y, uint32_t cx, uint32_t cy,
                       uint32_t unused, uint32_t clip)
{
    (void)unused;
    if (H(uint32_t, HSL_DIRECTDRAW))
        x++;
    if (cx == 0 && cy == 0 && w == 0x40 && h == 0x20) {
        full_tile(dst, tile, x, y);
        return;
    }
    if (clip && !H(uint32_t, HSL_DIRECTDRAW)) {
        uint32_t ocx = cx, ocy = cy;
        cx = 0;
        w = 0x40;
        x -= ocx;
        if (x < 0) {
            cx = -x;
            w = x + 0x40;
            x = 0;
        } else {
            int over = (x + 0x40) - H(int, HSL_WIDTH);
            if (over > 0)
                w = 0x40 - over;
        }
        uint32_t end = cy + h;
        cy = 0;
        h = h + (0x20 - end) + ocy;
        y -= ocy;
        if (y < 0) {
            cy = -y;
            h += y;
            y = 0;
        } else {
            int over = (y + h - 1) - (H(int, HSL_ORIGIN_Y) + H(int, HSL_VIEW2_BOTTOM));
            if (over > 0)
                h -= over;
        }
    }
    if (cx == 0 && cy == 0 && w == 0x40 && h == 0x20)
        full_tile(dst, tile, x, y);
    else
        tile_draw_clipped(dst, tile, w, h, x, y, cx, cy);
}

/* SpriteTileMerge (0x1005f655) */
void __cdecl SpriteTileMerge(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y, uint32_t cx,
                             uint32_t cy)
{
    if (dst && tile)
        tile_draw(dst, tile, w, h, x, y, cx, cy, 0, 0);
}

/* ---- the map ---- */

/* 0x1005e5bf: works out the map's size and visible area (+0x4c-+0x7b) and creates the
 * screen-sized bitmap the tiles are drawn into */
uint8_t *__stdcall tiles_create(uint8_t *map)
{
    int iso = S(int16_t, map, MAP_TYPE) == 1 || S(int16_t, map, MAP_TYPE) == 2;
    int mw = S(int, map, MAP_W);
    uint32_t mh = S(uint32_t, map, MAP_H);
    uint32_t tw = S(uint32_t, map, MAP_TILE_W), th = S(uint32_t, map, MAP_TILE_H);
    int32_t r[4];
    if (iso) {
        int hw = (int)(tw >> 1), hh = (int)(th >> 1);
        int off = (mh & 1) ? 0 : hh / 2;
        S(int, map, MAP_PIX_W) = tw * mw + hw;
        S(int, map, MAP_CENTER_X) = S(int, map, MAP_PIX_W) / 2;
        S(int, map, MAP_PIX_H) = (mh + 1) * hh;
        S(int, map, MAP_CENTER_Y) = S(int, map, MAP_PIX_H) / 2;
        r[0] = (hw + hw / 2) - S(int, map, MAP_CENTER_X);
        r[1] = (off + hh) - S(int, map, MAP_CENTER_Y);
        r[2] = ((S(int, map, MAP_PIX_W) + r[0]) - (int)tw) - hw;
        r[3] = ((S(int, map, MAP_PIX_H) + r[1]) - (int)th) - off * 2;
        r[0] += hw;
        r[1] += hh;
        r[3] += hh;
    } else {
        S(int, map, MAP_PIX_W) = tw * mw;
        S(int, map, MAP_CENTER_X) = S(int, map, MAP_PIX_W) / 2;
        S(int, map, MAP_PIX_H) = th * mh;
        S(int, map, MAP_CENTER_Y) = S(int, map, MAP_PIX_H) / 2;
        r[0] = -S(int, map, MAP_CENTER_X);
        r[1] = -S(int, map, MAP_CENTER_Y);
        r[2] = S(int, map, MAP_PIX_W) + r[0];
        r[3] = S(int, map, MAP_PIX_H) + r[1];
    }
    memcpy(map + MAP_RECT, r, sizeof r);
    uint8_t *bm = SpriteDibCreate(H(int, HSL_WIDTH), H(int, HSL_HEIGHT), 1, 8);
    if (H(uint32_t, HSL_TOP_DOWN))
        S(uint32_t, bm, 0x10) |= 0x40;
    RECT box = {0, 0, S(int, bm, 4), S(int, bm, 0xc)};
    S(int, map, MAP_ORG_X) = (box.right - 1) / 2;
    S(int, map, MAP_ORG_Y) = (box.bottom - 1) / 2;
    OffsetRect(&box, -S(int, map, MAP_ORG_X), -S(int, map, MAP_ORG_Y));
    rect_intersect((int32_t *)(map + MAP_CLIP), r, (const int32_t *)&box);
    return bm;
}

static void tables_ready(void)
{
    if (!H(uint32_t, HSL_TABLES_BUILT))
        tile_tables_make(H(uint32_t, HSL_DIRECTDRAW) ? H(int, HSL_DD_PITCH) : H(int, HSL_WIDTH));
}

/* Clips the pass to the view, r and the map; 0 when nothing is left */
static int pass_rect(const RECT *r)
{
    g_rect = H(RECT, HSL_VIEW2);
    if (r && !rect_intersect((int32_t *)&g_rect, (const int32_t *)&g_rect, (const int32_t *)r))
        return 0;
    return rect_intersect((int32_t *)&g_rect, (const int32_t *)&g_rect,
                          (const int32_t *)(H(uint8_t *, HSL_TILE_MAP) + MAP_CLIP));
}

static int is_iso(const uint8_t *map)
{
    return S(int16_t, map, MAP_TYPE) == 2 || S(int16_t, map, MAP_TYPE) == 1;
}

/* 0x1005e85a: draws the tiles within r (or the whole view) into dst */
void __stdcall tiles_draw(const RECT *r, uint32_t dst, uint32_t clip)
{
    if (H(uint32_t, HSL_DIRECTDRAW))
        clip = 0;
    tables_ready();
    if (!pass_rect(r))
        return;
    if (is_iso(H(uint8_t *, HSL_TILE_MAP)))
        tiles_draw_iso(dst, 1, clip);
    else
        tiles_draw_grid(dst);
}

/* 0x1005f4fc: draws the layers above the sprites (shroud, high overlays) within r, with
 * the overlay quality setting. The quality is put back only when something was drawn. */
void __stdcall tiles_overlay(const RECT *r, uint32_t dst)
{
    uint32_t quality = H(uint32_t, HSL_QUALITY);
    H(uint32_t, HSL_QUALITY) = H(uint32_t, HSL_OVERLAY_QUALITY);
    tables_ready();
    if (!pass_rect(r))
        return;
    if (is_iso(H(uint8_t *, HSL_TILE_MAP)))
        tiles_draw_iso(dst, 3, 0);
    else
        tiles_draw_grid(dst);
    H(uint32_t, HSL_QUALITY) = quality;
}

/* 0x1005e99a: walks the visible half-tile rows. mode 1: tiles and low overlays,
 * mode 3: the layers above the sprites. */
void __stdcall tiles_draw_iso(uint32_t dst, uint32_t mode, uint32_t clip)
{
    if (H(uint32_t, HSL_DIRECTDRAW)) {
        g_rect.left--;
        g_rect.right--;
    }
    uint8_t *map = H(uint8_t *, HSL_TILE_MAP);
    int layers = S(uint32_t, map, MAP_OVERLAY_IDX) && S(uint32_t, map, MAP_OVERLAYS);
    g_dst = dst;
    int mh = S(int, map, MAP_H);
    int th = S(int, map, MAP_TILE_H);
    int half = th >> 1;
    int off = (mh & 1) ? 0 : half / 2;
    int oy = (S(int, map, MAP_CENTER_Y) - H(int, HSL_SCROLL_Y)) - off;
    int row = (g_rect.top + oy) / half - 1;
    int cy = (g_rect.top + oy) - row * half;
    int h = th - cy;
    int odd = row & 1;
    g_org_x = S(int, map, MAP_ORG_X);
    g_org_y = S(int, map, MAP_ORG_Y);
    g_map_w = S(int, map, MAP_W);
    g_tile_w = S(int, map, MAP_TILE_W);
    g_half_w = g_tile_w >> 1;
    int ox = (S(int, map, MAP_CENTER_X) - H(int, HSL_SCROLL_X)) - (g_half_w >> 1);
    int col = (g_rect.left + ox) / g_tile_w;
    int cx = (g_rect.left + ox) - col * g_tile_w;
    int w = g_tile_w - cx;
    int y = g_rect.top;
    int idx = row * g_map_w + col;
    int left = (g_rect.bottom - g_rect.top) * 2 + 0x10;
    while (left > 0) {
        if (layers)
            tiles_row_layers(map, idx, col, y, cx, cy, w, h, odd, mode, clip);
        else
            tiles_row(map, idx, col, y, cx, cy, w, h, odd);
        if (++row >= mh)
            break;
        idx += g_map_w;
        left -= h;
        y += h - half;
        if (y < g_rect.top) {
            cy = g_rect.top - y;
            y = g_rect.top;
        } else
            cy = 0;
        h = th - cy;
        if (layers)
            tiles_row_layers(map, idx, col, y, cx, cy, w, h, !odd, mode, clip);
        else
            tiles_row(map, idx, col, y, cx, cy, w, h, !odd);
        if (++row >= mh)
            break;
        idx += g_map_w;
        left -= h;
        y += h - half;
        cy = 0;
        h = th;
    }
    if (H(uint32_t, HSL_DIRECTDRAW)) {
        g_rect.left++;
        g_rect.right++;
    }
}

/* A bitmap header around a tile's pixels, for the blitters (only +0x20 is read) */
typedef struct {
    uint8_t head[0x20];
    const uint8_t *bits;
} TileHeader;

/* Shifts a row that starts half a tile in */
static void odd_row(int *idx, int col, int *cx, int *w)
{
    if (g_half_w < *w) {
        *w -= g_half_w;
        if (col != 0)
            (*idx)--;
    } else
        *w += g_half_w;
    *cx = g_tile_w - *w;
}

/* 0x1005ed22: one half-tile row of an isometric map with its layers. mode 1 draws the
 * tiles, overlays numbered 0x37 and up, and the two masked overlays; mode 3 draws
 * overlays 2-0x36 and the shroud (1). The masks are crossed: flag 1 of +0x30 (for the
 * +0x14 masks) tests the +0x18 masks and the other way round. */
void __stdcall tiles_row_layers(uint8_t *map, int idx, int col, int y, int cx, int cy, int w, int h, int odd,
                                uint32_t mode, uint32_t clip)
{
    if (H(uint32_t, HSL_DIRECTDRAW))
        w++;
    if (g_rect.bottom < h + y - 1)
        h = (g_rect.bottom - y) + 1;
    if (h <= 0)
        return;
    if (odd)
        odd_row(&idx, col, &cx, &w);
    int ty = y + g_org_y;
    int use_b = S(uint32_t, map, MAP_MASK_A) && (S(uint8_t, map, MAP_FLAGS) & 1);
    int use_a = S(uint32_t, map, MAP_MASK_B) && (S(uint8_t, map, MAP_FLAGS) & 2);
    int dd = H(uint32_t, HSL_DIRECTDRAW) != 0;
    TileHeader hdr = {{0}, NULL};
    for (int x = g_rect.left; x < g_rect.right; idx++) {
        int tx = x + g_org_x;
        if (g_rect.right - x < w)
            w = g_rect.right - x;
        if (w < 1)
            return;
        hdr.bits = S(const uint8_t **, map, MAP_TILES)[S(uint16_t *, map, MAP_CELLS)[idx]];
        uint32_t ov = S(uint8_t *, map, MAP_OVERLAY_IDX)[idx];
        uint32_t ovb = S(uint32_t *, map, MAP_OVERLAYS)[ov];
        uint32_t overlays = S(uint32_t, map, MAP_OVERLAYS);
        if (mode == 1) {
            if (hdr.bits && ov != 1)
                tile_draw(g_dst, (uint32_t)(uintptr_t)&hdr, w, h, tx, ty, cx, cy, 0, clip);
            if (ovb && ov > 0x36)
                dib_merge(g_dst, ovb, w, h, dd + tx, ty, cx, cy, 0, 0);
            if (use_b) {
                if (S(uint32_t, map, MAP_MASK_B_BITS) & S(uint32_t *, map, MAP_MASK_B)[idx])
                    dib_merge(g_dst, S(uint32_t, overlays, OVERLAY_A), w, h, dd + tx, ty, cx, cy, 0, 0);
            } else if (use_a && (S(uint32_t, map, MAP_MASK_A_BITS) & S(uint32_t *, map, MAP_MASK_A)[idx]))
                dib_merge(g_dst, S(uint32_t, overlays, OVERLAY_B), w, h, dd + tx, ty, cx, cy, 0, 0);
        }
        if (mode == 3 && ovb && ov != 0 && ov < 0x37) {
            if (H(uint32_t, HSL_DIRECTDRAW) && (cx & 1))
                cx--;
            if (ov == 1)
                tile_shroud(g_dst, ovb, w, h, dd + tx, ty, cx, cy);
            else
                dib_merge(g_dst, ovb, w, h, dd + tx, ty, cx, cy, 0, 0);
        }
        x += w;
        w = g_tile_w;
        cx = 0;
    }
}

/* 0x1005f119: overlay 1 is the shroud; a whole tile of it is filled with color 10 */
void __cdecl tile_shroud(uint32_t dst, uint32_t bm, int w, uint32_t h, int x, int y, uint32_t cx, uint32_t cy)
{
    if (w <= 0)
        return;
    if (cx == 0 && cy == 0 && w == 0x40 && h == 0x20)
        tile_fill_c(dst, x, y);
    else
        dib_merge(dst, bm, w, h, x, y, cx, cy, 0, 0);
}

/* 0x1005f1a5: one half-tile row of an isometric map without layers. It stops a row
 * earlier at the bottom than tiles_row_layers. */
void __stdcall tiles_row(uint8_t *map, int idx, int col, int y, int cx, int cy, int w, int h, int odd)
{
    if (g_rect.bottom - y < h)
        h = g_rect.bottom - y;
    if (h <= 0)
        return;
    if (odd)
        odd_row(&idx, col, &cx, &w);
    int ty = y + g_org_y;
    TileHeader hdr = {{0}, NULL};
    for (int x = g_rect.left; x < g_rect.right; idx++) {
        if (g_rect.right - x < w)
            w = g_rect.right - x;
        if (w < 1)
            return;
        hdr.bits = S(const uint8_t **, map, MAP_TILES)[S(uint16_t *, map, MAP_CELLS)[idx]];
        if (hdr.bits)
            tile_draw(g_dst, (uint32_t)(uintptr_t)&hdr, w, h, x + g_org_x, ty, cx, cy, 0, 0);
        x += w;
        w = g_tile_w;
        cx = 0;
    }
}

/* 0x1005f2fb: a plain map, one bitmap per cell, flipped by its flags 0x10 / 0x20 */
void __stdcall tiles_draw_grid(uint32_t dst)
{
    uint8_t *map = H(uint8_t *, HSL_TILE_MAP);
    uint32_t mw = S(uint32_t, map, MAP_W);
    uint32_t tw = S(uint32_t, map, MAP_TILE_W), th = S(uint32_t, map, MAP_TILE_H);
    int sx = S(int, map, MAP_CENTER_X) - H(int, HSL_SCROLL_X);
    int sy = S(int, map, MAP_CENTER_Y) - H(int, HSL_SCROLL_Y);
    uint32_t row = (uint32_t)(g_rect.top + sy) / th;
    for (int y = g_rect.top; y < g_rect.bottom; row++) {
        uint32_t col = (uint32_t)(g_rect.left + sx) / tw;
        if (col >= mw)
            break;
        int oy = (y + sy) - th * row;
        int hh = th - oy;
        if (g_rect.bottom - y < hh)
            hh = g_rect.bottom - y;
        int ww;
        for (int x = g_rect.left; x < g_rect.right; x += ww, col++) {
            int ox = (sx + x) - tw * col;
            ww = tw - ox;
            if (g_rect.right - x < ww)
                ww = g_rect.right - x;
            uint8_t *bm = S(uint8_t **, map, MAP_GRID)[row * mw + col];
            if (bm) {
                uint8_t f = S(uint8_t, bm, 0x10);
                dib_merge_plain9(dst, (uint32_t)(uintptr_t)bm, ww, hh, S(int, map, MAP_ORG_X) + x,
                                 S(int, map, MAP_ORG_Y) + y, ox, oy, ((f & 0x20) ? 0x100 : 0) | ((f & 0x10) ? 1 : 0));
            }
        }
        y += hh;
    }
}
