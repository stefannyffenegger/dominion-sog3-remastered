/* The sprite engine's view: background (a bitmap, a tile map or a fill color), the
 * screen rectangle and clip boxes, scrolling, repainting, composing sprites into the
 * background, bitmap (DIB) helpers and screenshots. Rewritten from the original
 * (0x100510fe-0x10053c50, without the palette functions in display.c and the blitter
 * wrappers).
 *
 * Sprite coordinates are relative to the center of the background (HSL_ORIGIN). */
#include "orig.h"
#include "blit.h"
#define intersect_rect(o, a, b) rect_intersect((int32_t *)(o), (const int32_t *)(a), (const int32_t *)(b))
#define union_rect(o, a, b) rect_union((int32_t *)(o), (const int32_t *)(a), (const int32_t *)(b))
#include "draw.h"
#include "spritelib.h"
#include "view.h"
#include "tiles.h"
#include "display.h"
#include "sprite.h"
#include "queue.h"
#include "utils.h"

#define g_hsl G(uint8_t *, 0x10089e24)
#define H(type, off) (*(type *)(g_hsl + (off)))
#define S(type, s, off) (*(type *)((uint8_t *)(s) + (off)))

/* HSL fields */
#define HSL_HEAD 0x000
#define HSL_TAIL 0x004
#define HSL_DC 0x468
#define HSL_BACKGROUND 0x474    /* background bitmap */
#define HSL_SCREEN_DIB 0x478
#define HSL_TILES 0x47c         /* bitmap drawn from the tile map, owned */
#define HSL_TILE_MAP 0x480      /* tile map object; its scroll limits are a RECT at +0x5c */
#define HSL_ZOOM 0x88c
#define HSL_ZOOM_W 0x890
#define HSL_ZOOM_H 0x894
#define HSL_FILL 0x898          /* background fill color, -1 = none */
#define HSL_WIDTH 0x8a4
#define HSL_HEIGHT 0x8a8
#define HSL_COUNT 0x8c4
#define HSL_ORIGIN_X 0x8cc
#define HSL_ORIGIN_Y 0x8d0
#define HSL_SCROLL_X 0x8d4
#define HSL_SCROLL_Y 0x8d8
#define HSL_SCROLL_LIMITS 0x8e4 /* RECT */
#define HSL_SCREEN_RECT 0x8f4   /* RECT, in sprite coordinates */
#define HSL_VIEW 0x904          /* RECT of the play area */
#define HSL_VIEW2 0x914         /* RECT: the play area within the scroll box */
#define HSL_CLIP_BOX 0x924      /* RECT */
#define HSL_SCROLL_BOX 0x934    /* RECT */
#define HSL_BKG_TRANSPARENT 0x948
#define HSL_DIRECTDRAW 0x954
#define HSL_CAPTURE 0x96c
#define HSL_CAPTURE_NAME 0x970
#define HSL_STRETCH 0xa00
#define HSL_ZOOMED 0xa08
#define HSL_PALETTE 0x48c

#define SP_NEXT 0x04
#define SP_BITMAP 0x18
#define SP_PARTS 0x20
#define SP_RECT 0x44
#define SP_FLIP 0x7c
#define SP_NPARTS 0x90
#define SP_SCROLLS 0xe4
#define SP_VISIBLE 0xec

#define REDRAW_ALL 0x80000000u

/* original functions not rewritten yet */



#define FOR_SPRITES(s)                                                                             \
    for (uint8_t *s = S(uint8_t *, H(uint8_t *, HSL_HEAD), SP_NEXT), *end_ = H(uint8_t *, HSL_TAIL); \
         s != end_; s = S(uint8_t *, s, SP_NEXT))

/* ---- scrolling ---- */

void __stdcall SpriteSetScrollable(uint8_t *hsl, uint8_t *s, int on)
{
    g_hsl = hsl;
    uint32_t b = on != 0;
    if (b == S(uint32_t, s, SP_SCROLLS))
        return;
    if (b)
        sprite_offset_rects(s, H(int32_t, HSL_SCROLL_X), H(int32_t, HSL_SCROLL_Y));
    else
        sprite_offset_rects(s, -H(int32_t, HSL_SCROLL_X), -H(int32_t, HSL_SCROLL_Y));
    S(uint32_t, s, SP_SCROLLS) = b;
}

void __stdcall SpriteSetScrollBox(uint8_t *hsl, const RECT *r)
{
    RECT old;
    g_hsl = hsl;
    old = H(RECT, HSL_VIEW2);
    if (r)
        H(RECT, HSL_SCROLL_BOX) = *r;
    else
        memset(&H(RECT, HSL_SCROLL_BOX), 0, 16);
    view2_update(&H(RECT, HSL_SCROLL_BOX));
    RECT *v = &H(RECT, HSL_VIEW2);
    if (v->left != old.left || v->right != old.right || v->top != old.top || v->bottom != old.bottom) {
        union_rect(&old, &old, v);
        SpritePaintRect(hsl, &old, 0);
    }
}

/* Adds d to the scroll position; the x position is kept odd */
static void scroll_by(int32_t *d)
{
    H(int32_t, HSL_SCROLL_X) += d[0];
    int32_t x = H(int32_t, HSL_SCROLL_X);
    if (((x < 0 ? -x : x) & 1) == 0) {
        if (x < 1)
            H(int32_t, HSL_SCROLL_X)++, d[0]++;
        else
            H(int32_t, HSL_SCROLL_X)--, d[0]--;
    }
    H(int32_t, HSL_SCROLL_Y) += d[1];
    if (H(uint32_t, HSL_COUNT) && H(uint8_t *, HSL_HEAD)) {
        FOR_SPRITES(s) {
            if (S(uint32_t, s, SP_SCROLLS))
                sprite_offset_rects(s, d[0], d[1]);
        }
    }
}

void __stdcall SpriteSetScrollPos(uint8_t *hsl, int x, int y)
{
    RECT r;
    g_hsl = hsl;
    int32_t d[2] = {x - H(int32_t, HSL_SCROLL_X), y - H(int32_t, HSL_SCROLL_Y)};
    if (!sprite_scroll_box((int32_t *)&r, d))
        return;
    scroll_by(d);
    if (H(uint32_t, HSL_TILES) && !H(uint32_t, HSL_DIRECTDRAW))
        tiles_draw(&H(RECT, HSL_VIEW2), H(uint32_t, HSL_TILES), 4);
    SpritePaintRect(hsl, &H(RECT, HSL_VIEW2), 0);
}

/* Scrolls by dx, dy; with blit, moves the window contents and repaints only the edges */
void __stdcall SpriteScroll(uint8_t *hsl, int dx, int dy, int blit)
{
    RECT r, uncovered;
    g_hsl = hsl;
    int32_t d[2] = {dx, dy};
    if (!sprite_scroll_box((int32_t *)&r, d))
        return;
    scroll_by(d);
    /* (the original moves the sprites after ScrollDC; nothing in between uses them) */
    if (H(uint32_t, HSL_DIRECTDRAW))
        return;
    if (blit) {
        r.bottom++;
        ScrollDC(H(HDC, HSL_DC), d[0], d[1], &r, &r, NULL, &uncovered);
    }
    if (H(uint32_t, HSL_TILES))
        tiles_scroll(H(uint8_t *, HSL_TILES), d[0], d[1], (int32_t *)&r);
    if (!blit) {
        SpritePaintRect(hsl, &H(RECT, HSL_VIEW2), 0);
    } else if (!H(uint32_t, HSL_TILES)) {
        SpritePaintRect(hsl, &uncovered, 0);
    } else {
        if (d[0])
            scroll_paint_x(r.left, r.top, r.right, r.bottom, d[0]);
        if (d[1])
            scroll_paint_y(r.left, r.top, r.right, r.bottom, d[1]);
    }
}

/* Repaints the column uncovered by scrolling dx */
void __stdcall scroll_paint_x(int l, int t, int r, int b, int dx)
{
    if (dx < 0)
        l = r + dx;
    else
        r = l + dx;
    RECT rc = {l, t, r, b};
    if (H(uint32_t, HSL_TILE_MAP) && !H(uint32_t, HSL_DIRECTDRAW))
        tiles_draw(&rc, H(uint32_t, HSL_TILES), 1);
    SpritePaintRect(g_hsl, &rc, 0);
}

/* Repaints the row uncovered by scrolling dy */
void __stdcall scroll_paint_y(int l, int t, int r, int b, int dy)
{
    if (dy < 0)
        t = b + dy;
    else
        b = t + dy;
    RECT rc = {l, t, r, b};
    if (H(uint32_t, HSL_TILE_MAP) && !H(uint32_t, HSL_DIRECTDRAW))
        tiles_draw(&rc, H(uint32_t, HSL_TILES), 2);
    SpritePaintRect(g_hsl, &rc, 0);
}

/* Moves the pixels of bitmap bm by dx or dy within rectangle r */
void __stdcall tiles_scroll(uint8_t *bm, int dx, int dy, const int32_t *r)
{
    uint8_t *bits = S(uint8_t *, bm, 0x20);
    uint32_t pitch = S(uint32_t, bm, 8);
    if (dx == 0) {
        int rows = (r[3] - r[1]) + 1 - (dy < 0 ? -dy : dy);
        if (dy < 1)
            rows_copy_huge(bits, bits - pitch * dy, pitch, pitch, rows);
        else
            rows_copy_huge(bits + (r[3] - r[1]) * pitch, bits + (rows - 1) * pitch, -pitch, pitch, rows);
    } else {
        uint32_t len = (r[2] - r[0]) - (dx < 0 ? -dx : dx);
        if (dx < 1)
            rows_copy_huge(bits, bits - dx, pitch, len, r[3] - r[1]);
        else
            rows_move(bits + dx, bits, pitch, len, r[3] - r[1]);
    }
}

void __stdcall rows_copy_huge(uint8_t *dst, const uint8_t *src, int stride, uint32_t n, int rows)
{
    for (; rows; rows--, dst += stride, src += stride)
        CopyHugeBytes(dst, src, n);
}

/* the original's memcpy handles overlapping buffers */
void __stdcall rows_move(uint8_t *dst, const uint8_t *src, int stride, uint32_t n, int rows)
{
    for (; rows; rows--, dst += stride, src += stride)
        memmove(dst, src, n);
}

/* ---- background and clip boxes ---- */

/* The play area within the scroll box, the screen and the scroll limits */
void __stdcall view2_update(const RECT *box)
{
    if (box->left < box->right && box->top < box->bottom) {
        intersect_rect(&H(RECT, HSL_VIEW2), &H(RECT, HSL_VIEW), box);
        intersect_rect(&H(RECT, HSL_VIEW2), &H(RECT, HSL_VIEW2), &H(RECT, HSL_SCREEN_RECT));
    } else {
        H(RECT, HSL_VIEW2) = H(RECT, HSL_VIEW);
    }
    if (H(uint8_t *, HSL_TILE_MAP))
        intersect_rect(&H(RECT, HSL_VIEW2), &H(RECT, HSL_VIEW2), H(uint8_t *, HSL_TILE_MAP) + 0x5c);
    else if (H(uint32_t, HSL_BACKGROUND))
        intersect_rect(&H(RECT, HSL_VIEW2), &H(RECT, HSL_VIEW2), &H(RECT, HSL_SCROLL_LIMITS));
}

/* Centers the coordinates on the background (or the window) and resets scrolling */
void __cdecl view_reset(void)
{
    H(int32_t, HSL_SCROLL_X) = H(int32_t, HSL_SCROLL_Y) = 0;
    RECT *sr = &H(RECT, HSL_SCREEN_RECT);
    sr->left = sr->top = 0;
    if (!H(uint8_t *, HSL_BACKGROUND)) {
        sr->right = H(int32_t, HSL_WIDTH);
        sr->bottom = H(int32_t, HSL_HEIGHT);
    } else {
        sr->right = S(int32_t, H(uint8_t *, HSL_BACKGROUND), 4);
        sr->bottom = S(int32_t, H(uint8_t *, HSL_BACKGROUND), 0xc);
    }
    H(int32_t, HSL_ORIGIN_X) = (sr->right - 1) / 2;
    H(int32_t, HSL_ORIGIN_Y) = (sr->bottom - 1) / 2;
    OffsetRect(sr, -H(int32_t, HSL_ORIGIN_X), -H(int32_t, HSL_ORIGIN_Y));
    H(RECT, HSL_SCROLL_LIMITS) = *sr;
    if (H(RECT, HSL_CLIP_BOX).left < H(RECT, HSL_CLIP_BOX).right)
        intersect_rect(sr, sr, &H(RECT, HSL_CLIP_BOX));
    intersect_rect(sr, sr, &H(RECT, HSL_VIEW));
    view2_update(&H(RECT, HSL_SCROLL_BOX));
}

/* bg: a bitmap, a fill color (0-255), -1 for none, or 0 */
int __stdcall SpriteSetBackground(uint8_t *hsl, uint8_t *bg, RECT *out)
{
    g_hsl = hsl;
    H(uint32_t, HSL_BACKGROUND) = 0;
    if (!H(uint32_t, HSL_DIRECTDRAW))
        screen_dib_create(H(uint32_t, HSL_WIDTH), H(uint32_t, HSL_HEIGHT));
    H(uint32_t, HSL_TILE_MAP) = 0;
    if (H(uint32_t, HSL_TILES))
        winfree(H(void *, HSL_TILES));
    H(uint32_t, HSL_TILES) = 0;
    H(RECT, HSL_VIEW2) = H(RECT, HSL_VIEW);
    sprites_clear_dirty_flag();
    H(uint32_t, HSL_FILL) = 0;
    if (!H(uint32_t, HSL_DIRECTDRAW)) {
        if (bg == (uint8_t *)-1)
            H(uint32_t, HSL_FILL) = 0xffffffff;
        else if ((uintptr_t)bg >> 16 == 0)
            H(uint32_t, HSL_FILL) = (uintptr_t)bg & 0xff;
        else
            H(uint8_t *, HSL_BACKGROUND) = bg;
    } else {
        set_background(bg);
    }
    view_reset();
    if (out)
        *out = H(RECT, HSL_SCREEN_RECT);
    return 1;
}

int __stdcall SpriteSetTileBackground(uint8_t *hsl, uint32_t map, RECT *out)
{
    uint8_t *tiles;
    g_hsl = hsl;
    if (!map || !(tiles = tiles_create((uint8_t *)(uintptr_t)map)))
        return SpriteSetBackground(hsl, NULL, out);
    H(uint32_t, HSL_FILL) = 0;
    H(uint32_t, HSL_TILE_MAP) = map;
    if (H(uint32_t, HSL_TILES))
        winfree(H(void *, HSL_TILES));
    H(uint8_t *, HSL_TILES) = tiles;
    H(uint8_t *, HSL_BACKGROUND) = tiles;
    view_reset();
    if (out)
        *out = H(RECT, HSL_SCREEN_RECT);
    if (!H(uint32_t, HSL_DIRECTDRAW))
        tiles_draw(&H(RECT, HSL_VIEW2), H(uint32_t, HSL_TILES), 0);
    return 1;
}

void __stdcall SpriteSetBkgTransparent(uint8_t *hsl, uint32_t on)
{
    g_hsl = hsl;
    H(uint32_t, HSL_BKG_TRANSPARENT) = on;
}

/* r is inclusive */
void __stdcall SpriteSetClipBox(uint8_t *hsl, const RECT *r)
{
    g_hsl = hsl;
    RECT *c = &H(RECT, HSL_CLIP_BOX);
    if (!r) {
        memset(c, 0, 16);
    } else {
        *c = *r;
        if (c->left < c->right && c->top < c->bottom)
            c->right++, c->bottom++;
    }
    view_reset();
}

void __stdcall SpriteGetClipBox(uint8_t *hsl, RECT *r)
{
    g_hsl = hsl;
    *r = H(RECT, HSL_SCREEN_RECT);
}

void __stdcall SpriteSetStretch(uint8_t *hsl, uint32_t on)
{
    g_hsl = hsl;
    H(uint32_t, HSL_STRETCH) = on;
}

uint32_t __stdcall SpriteGetStretch(uint8_t *hsl)
{
    g_hsl = hsl;
    return H(uint32_t, HSL_STRETCH);
}

/* returns whether zooming was switched on or off */
int __stdcall SpriteSetZoom(uint8_t *hsl, int zoom, int w, int h)
{
    g_hsl = hsl;
    uint32_t was = H(uint32_t, HSL_ZOOMED);
    H(int32_t, HSL_ZOOM_W) = w * zoom - 1;
    H(int32_t, HSL_ZOOM_H) = h * zoom - 1;
    H(int32_t, HSL_ZOOM) = zoom;
    H(uint32_t, HSL_ZOOMED) = !(zoom == 0 || (zoom == 1 && w == 0 && h == 0));
    if (!H(uint32_t, HSL_ZOOMED)) {
        H(int32_t, HSL_ZOOM_W) = H(int32_t, HSL_ZOOM_H) = 0;
        H(int32_t, HSL_ZOOM) = 1;
    }
    return H(uint32_t, HSL_ZOOMED) != was;
}

/* ---- painting ---- */

/* Redraws s, or everything (the background too), optionally into another DC */
void __stdcall SpritePaint(uint8_t *hsl, uint8_t *s, HDC dc)
{
    g_hsl = hsl;
    HDC old = H(HDC, HSL_DC);
    if (dc)
        H(HDC, HSL_DC) = dc;
    if (H(uint32_t, HSL_DIRECTDRAW))
        H(HDC, HSL_DC) = NULL;
    if (s) {
        sprite_reposition(s);
    } else if (!draw_view_sprites(&H(RECT, HSL_SCREEN_RECT), REDRAW_ALL)) {
        if (H(uint32_t, HSL_BACKGROUND))
            background_draw(H(int32_t, HSL_SCROLL_X) - H(int32_t, HSL_ORIGIN_X),
                            H(int32_t, HSL_SCROLL_Y) - H(int32_t, HSL_ORIGIN_Y), H(uint8_t *, HSL_BACKGROUND));
        /* original: reads the next sprite through s (NULL here), which would crash after
         * the first sprite; this path never seems to run */
        if (H(uint32_t, HSL_COUNT) && H(uint8_t *, HSL_HEAD)) {
            FOR_SPRITES(p) { sprite_reposition(p); }
        }
    }
    H(HDC, HSL_DC) = old;
}

/* screen: r is in window coordinates */
void __stdcall SpritePaintRect(uint8_t *hsl, const RECT *r, int screen)
{
    g_hsl = hsl;
    RECT rc = *r;
    if (screen)
        OffsetRect(&rc, -H(int32_t, HSL_ORIGIN_X), -H(int32_t, HSL_ORIGIN_Y));
    draw_view_sprites(&rc, REDRAW_ALL);
}

void __stdcall SpriteRefreshRect(uint8_t *hsl, const RECT *r)
{
    g_hsl = hsl;
    if (H(uint32_t, HSL_DIRECTDRAW))
        return;
    RECT rc = r ? *r : H(RECT, HSL_VIEW2);
    if (H(RECT, HSL_VIEW2).bottom == rc.bottom)
        rc.bottom++;
    if (H(uint32_t, HSL_TILE_MAP))
        tiles_draw(&rc, H(uint32_t, HSL_TILES), 4);
    draw_view_sprites(&rc, REDRAW_ALL);
}

/* ---- composing sprites into the background ---- */

static void merge_parts(uint32_t dst, const int32_t *dst_rect, uint8_t *s, const RECT *r, int to_screen)
{
    RECT i;
    int32_t *p = S(int32_t *, s, SP_PARTS), *end = p + S(uint32_t, s, SP_NPARTS) * 5;
    for (; p < end; p += 5)
        if (intersect_rect(&i, r, p + 1)) {
            int dx = to_screen ? H(int32_t, HSL_ORIGIN_X) + i.left : i.left - dst_rect[0];
            int dy = to_screen ? H(int32_t, HSL_ORIGIN_Y) + i.top : i.top - dst_rect[1];
            dib_merge(dst, p[0], i.right - i.left, i.bottom - i.top, dx, dy, i.left - p[1], i.top - p[2],
                      S(uint32_t, s, SP_FLIP), 0);
        }
}

/* Draws s into the background bitmap; unless keep, hides s and empties its queue */
void __stdcall comp_to_background(uint8_t *s, int keep)
{
    RECT r;
    if (!H(uint32_t, HSL_BACKGROUND) || !S(uint32_t, s, SP_VISIBLE))
        return;
    if (intersect_rect(&r, &H(RECT, HSL_SCREEN_RECT), s + SP_RECT)) {
        if (!S(uint32_t, s, SP_NPARTS))
            dib_merge(H(uint32_t, HSL_BACKGROUND), S(uint32_t, s, SP_BITMAP), r.right - r.left, r.bottom - r.top,
                      H(int32_t, HSL_ORIGIN_X) + r.left, H(int32_t, HSL_ORIGIN_Y) + r.top,
                      r.left - S(int32_t, s, SP_RECT), r.top - S(int32_t, s, SP_RECT + 4), S(uint32_t, s, SP_FLIP), 0);
        else
            comp_parts_to_background(s, &r);
    }
    if (!keep) {
        SpriteHide(g_hsl, s, 0);
        SpriteEmptyQueue(g_hsl, s);
    }
}

void __stdcall comp_parts_to_background(uint8_t *s, const RECT *r)
{
    merge_parts(H(uint32_t, HSL_BACKGROUND), NULL, s, r, 1);
}

/* Draws src into dst's bitmap */
void __stdcall comp_to_sprite(uint8_t *dst, uint8_t *src)
{
    RECT r;
    if (!S(uint32_t, src, SP_VISIBLE) || !intersect_rect(&r, dst + SP_RECT, src + SP_RECT))
        return;
    if (S(uint32_t, src, SP_NPARTS)) {
        comp_parts_to_sprite(dst, src, &r);
    } else if (!S(uint32_t, dst, SP_NPARTS)) {
        dib_merge(S(uint32_t, dst, SP_BITMAP), S(uint32_t, src, SP_BITMAP), r.right - r.left, r.bottom - r.top,
                  r.left - S(int32_t, dst, SP_RECT), r.top - S(int32_t, dst, SP_RECT + 4),
                  r.left - S(int32_t, src, SP_RECT), r.top - S(int32_t, src, SP_RECT + 4), S(uint32_t, src, SP_FLIP), 0);
    }
}

void __stdcall comp_parts_to_sprite(uint8_t *dst, uint8_t *src, const RECT *r)
{
    merge_parts(S(uint32_t, dst, SP_BITMAP), (int32_t *)(dst + SP_RECT), src, r, 0);
}

/* original: with a sprite given, it still goes on to compose every sprite */
void __stdcall SpriteCompToBackground(uint8_t *hsl, uint8_t *s, int keep)
{
    g_hsl = hsl;
    if (s)
        comp_to_background(s, keep);
    else if (!H(uint32_t, HSL_COUNT))
        return;
    FOR_SPRITES(p) { comp_to_background(p, keep); }
}

void __stdcall SpriteCompToSprite(uint8_t *hsl, uint32_t a, uint32_t b)
{
    (void)a, (void)b;
    g_hsl = hsl;
    OutputDebugStringA("*** SpriteCompToSprite UNIMPLEMENTED ***\r\n"); /* original */
}

/* A copy of the screen bitmap, bottom-up */
uint8_t *__stdcall screen_copy(uint8_t *hsl)
{
    g_hsl = hsl;
    uint8_t *src = H(uint8_t *, HSL_SCREEN_DIB);
    if (!src)
        return NULL;
    uint8_t *p = winptrcpy(src);
    if (!p)
        return NULL;
    S(uint8_t *, p, 0x20) = p + 0x24;
    if (S(uint8_t, p, 0x10) & 0x40) { /* top-down: flip the rows */
        uint32_t pitch = S(uint32_t, p, 8), h = S(uint32_t, p, 0xc);
        for (uint32_t y = 0; y < h; y++)
            memcpy(S(uint8_t *, p, 0x20) + (h - y - 1) * pitch, S(uint8_t *, src, 0x20) + pitch * y, pitch);
    }
    S(uint32_t, p, 0x10) = 0;
    return p;
}

/* ---- bitmaps ---- */

/* A 0x24-byte header followed by the pixels, rows padded to 4 bytes */
uint8_t *__stdcall SpriteDibCreate(int w, int h, int zero, uint32_t bpp)
{
    if (!bpp)
        bpp = 8;
    uint32_t pitch = (w + 3) & ~3u;
    uint8_t *p = winmalloc((bpp >> 3) * pitch * h + 0x24, zero);
    if (p) {
        memset(p, 0, 0x24);
        S(uint32_t, p, 0) = 0x24;
        S(int32_t, p, 4) = w;
        S(uint32_t, p, 8) = pitch;
        S(int32_t, p, 0xc) = h;
        S(uint8_t *, p, 0x20) = p + 0x24;
    }
    return p;
}

void __stdcall SpriteDibFree(void *p) { winfree(p); }

/* The bounds of a bitmap centered on its hot spot */
void __stdcall SpriteDibGetBox(uint8_t *p, int32_t *r)
{
    r[0] = S(int32_t, p, 0x14) - (int16_t)(S(int32_t, p, 4) - 1) / 2;
    r[2] = S(int32_t, p, 4) + r[0];
    r[1] = S(int32_t, p, 0x18) - (int16_t)(S(int32_t, p, 0xc) - 1) / 2;
    r[3] = S(int32_t, p, 0xc) + r[1];
}

void __stdcall SpriteInitScreenCapture(uint8_t *hsl, const char *name)
{
    g_hsl = hsl;
    H(uint32_t, HSL_CAPTURE) = 1;
    char *d = &H(char, HSL_CAPTURE_NAME);
    while ((*d++ = *name++))
        ;
}

/* Writes an 8-bit bitmap with the current palette as a .bmp file */
void __stdcall SpriteSaveScreenShot(uint8_t *p, char *path)
{
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    uint8_t *colors = winmalloc(0x400, 1);
    if (!colors)
        return;
    for (uint32_t i = 0; i < 0x100; i++) {
        colors[i * 4 + 2] = H(uint8_t, HSL_PALETTE + i * 4);
        colors[i * 4 + 1] = H(uint8_t, HSL_PALETTE + i * 4 + 1);
        colors[i * 4] = H(uint8_t, HSL_PALETTE + i * 4 + 2);
    }
    int size = S(int32_t, p, 8) * S(int32_t, p, 0xc);
    memset(&ih, 0, sizeof ih);
    ih.biSize = sizeof ih;
    ih.biWidth = S(int32_t, p, 4);
    ih.biHeight = S(int32_t, p, 0xc);
    ih.biPlanes = 1;
    ih.biBitCount = 8;
    ih.biSizeImage = size;
    memset(&fh, 0, sizeof fh);
    fh.bfType = 0x4d42;
    fh.bfOffBits = 0x400 + 0x36;
    fh.bfSize = fh.bfOffBits + size;
    HMMIO f = mmioOpenA(path, NULL, MMIO_CREATE | MMIO_WRITE);
    if (!f) {
        winfree(colors);
        return;
    }
    int ok = mmioWrite(f, (char *)&fh, 0xe) && mmioWrite(f, (char *)&ih, 0x28) && mmioWrite(f, (char *)colors, 0x400) &&
             mmioWrite(f, S(char *, p, 0x20), size);
    mmioClose(f, 0);
    if (!ok)
        mmioOpenA(path, NULL, MMIO_DELETE);
    winfree(colors);
}

/* ---- the screen bitmap (0x10054da0-0x10055321) ---- */

#define HSL_BITMAPINFO 0x02c    /* BITMAPINFOHEADER of the screen bitmap */
#define HSL_SCREEN_SIZE 0x454   /* header + pixels */
#define HSL_DEPTH 0x8bc
#define HSL_RESIZED 0x94c       /* the window size wins over the requested one */
#define HSL_TOP_DOWN 0x950
#define HSL_DIB_SECTION 0xa0c   /* draw into a GDI DIB section */
#define HSL_DIB_DC 0xa10
#define HSL_DIB_BITMAP 0xa14    /* after selecting: the DC's previous bitmap */
#define HSL_IFACE_DIB 0xb58

/* A bitmap header without pixels, w x h */
static uint8_t *dib_header(int w, int h)
{
    uint8_t *p = SpriteDibCreate(0, 0, 1, 8);
    H(uint8_t *, HSL_SCREEN_DIB) = p;
    if (p) {
        S(int32_t, p, 4) = w;
        S(int32_t, p, 8) = w;
        S(int32_t, p, 0xc) = h;
    }
    return p;
}

/* (Re)creates the bitmap sprites are drawn into */
int __stdcall screen_dib_create(int w, int h)
{
    if (H(uint32_t, HSL_RESIZED))
        w = H(int32_t, HSL_WIDTH);
    if (H(uint32_t, HSL_RESIZED))
        h = H(int32_t, HSL_HEIGHT);
    if (H(void *, HSL_SCREEN_DIB))
        H(void *, HSL_SCREEN_DIB) = winfree(H(void *, HSL_SCREEN_DIB));
    if (H(void *, HSL_IFACE_DIB))
        H(void *, HSL_IFACE_DIB) = winfree(H(void *, HSL_IFACE_DIB));
    if (H(uint32_t, HSL_DIB_SECTION)) {
        if (!dib_header(w, h) || !dib_section_create())
            return 0;
    } else if (!H(uint32_t, HSL_DIRECTDRAW)) {
        H(uint8_t *, HSL_SCREEN_DIB) = SpriteDibCreate(w, h, 1, H(uint32_t, HSL_DEPTH));
    } else {
        /* the pixels are the locked back buffer */
        uint8_t *p = dib_header(w, h);
        if (!p)
            return 0;
        S(uint32_t, p, 0x10) |= 0x40;
    }
    uint8_t *p = H(uint8_t *, HSL_SCREEN_DIB);
    if (!p)
        return 0;
    if (H(uint32_t, HSL_TOP_DOWN))
        S(uint32_t, p, 0x10) |= 0x40;
    H(uint32_t, HSL_SCREEN_SIZE) = S(int32_t, p, 8) * S(int32_t, p, 0xc) + 0x24;
    return 1;
}

/* A GDI DIB section for the screen bitmap's pixels */
int __cdecl dib_section_create(void)
{
    uint8_t buf[0x428];
    BITMAPINFO *bi = (BITMAPINFO *)buf;
    dib_section_delete();
    memcpy(buf, &H(uint8_t, HSL_BITMAPINFO), 0x28);
    if (!H(uint8_t *, HSL_SCREEN_DIB)) {
        bi->bmiHeader.biWidth = 640;
        bi->bmiHeader.biHeight = 480;
    } else {
        bi->bmiHeader.biWidth = S(int32_t, H(uint8_t *, HSL_SCREEN_DIB), 8);
        bi->bmiHeader.biHeight = S(int32_t, H(uint8_t *, HSL_SCREEN_DIB), 0xc);
    }
    for (int i = 0; i < 0x100; i++) {
        bi->bmiColors[i].rgbRed = H(uint8_t, HSL_PALETTE + i * 4);
        bi->bmiColors[i].rgbGreen = H(uint8_t, HSL_PALETTE + i * 4 + 1);
        bi->bmiColors[i].rgbBlue = H(uint8_t, HSL_PALETTE + i * 4 + 2);
        bi->bmiColors[i].rgbReserved = 0;
    }
    H(HDC, HSL_DIB_DC) = CreateCompatibleDC(NULL);
    if (!H(HDC, HSL_DIB_DC))
        return 0;
    /* (the screen bitmap must exist here: the original writes its pixel pointer) */
    H(HBITMAP, HSL_DIB_BITMAP) = CreateDIBSection(H(HDC, HSL_DIB_DC), bi, DIB_RGB_COLORS,
                                                  (void **)(H(uint8_t *, HSL_SCREEN_DIB) + 0x20), NULL, 0);
    if (!H(HBITMAP, HSL_DIB_BITMAP))
        return dib_section_delete();
    H(HGDIOBJ, HSL_DIB_BITMAP) = SelectObject(H(HDC, HSL_DIB_DC), H(HGDIOBJ, HSL_DIB_BITMAP));
    SetStretchBltMode(H(HDC, HSL_DIB_DC), HALFTONE);
    return 1;
}

/* original: deletes the bitmap that was selected before (the section itself stays), and
 * the DC with DeleteObject */
int __cdecl dib_section_delete(void)
{
    if (H(HGDIOBJ, HSL_DIB_BITMAP)) {
        SelectObject(H(HDC, HSL_DIB_DC), H(HGDIOBJ, HSL_DIB_BITMAP));
        DeleteObject(H(HGDIOBJ, HSL_DIB_BITMAP));
        H(HGDIOBJ, HSL_DIB_BITMAP) = NULL;
    }
    if (H(HDC, HSL_DIB_DC)) {
        DeleteObject(H(HDC, HSL_DIB_DC));
        H(HDC, HSL_DIB_DC) = NULL;
    }
    return 0;
}

UINT __cdecl dib_section_colors(void)
{
    RGBQUAD q[256];
    if (!H(HDC, HSL_DIB_DC))
        return 0;
    for (int i = 0; i < 0x100; i++) {
        q[i].rgbRed = H(uint8_t, HSL_PALETTE + i * 4);
        q[i].rgbGreen = H(uint8_t, HSL_PALETTE + i * 4 + 1);
        q[i].rgbBlue = H(uint8_t, HSL_PALETTE + i * 4 + 2);
        q[i].rgbReserved = 0;
    }
    return SetDIBColorTable(H(HDC, HSL_DIB_DC), 0, 0x100, q);
}

uint32_t __stdcall view_false4(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    (void)a, (void)b, (void)c, (void)d;
    return 0;
}

/* ---- wipes (0x1005f760-0x1005fcec): the background appears from the middle outwards,
 * or is blanked from the edges inwards, in steps with the sprites animating between ---- */

#define HSL_NO_SEND_MSG 0x944
#define HSL_9FC 0x9fc

static struct {
    int w, h;      /* 0x1008f840, 0x1008f848: size of the scrollable area */
    uint8_t *src;  /* 0x1008f844: the bitmap being revealed */
    int reveal;    /* 0x1008f84c: copy from src; otherwise fill with color 0 */
} g_wipe;

/* 0x1005fb02: the rectangle of a step: step 0 is the whole area, steps - 1 the smallest.
 * Without `sides` it spans the full width. */
void __stdcall wipe_rect(int step, int32_t *r, int steps, int sides)
{
    uint32_t div = (uint32_t)(steps * 2 - 1);
    r[0] = sides ? (int32_t)((uint32_t)(step * g_wipe.w) / div) : 0;
    r[2] = g_wipe.w - r[0];
    r[1] = (int32_t)((uint32_t)(step * g_wipe.h) / div);
    r[3] = g_wipe.h - r[1];
}

/* 0x1005f981: reveals or blanks r in the background and paints it */
void __stdcall wipe_apply(const int32_t *r)
{
    uint8_t *bg = H(uint8_t *, HSL_BACKGROUND);
    if (!g_wipe.reveal)
        dib_fill8(bg, r[0], r[1], r[2] - r[0], r[3] - r[1], 0);
    else
        SpriteCopyRect(bg, g_wipe.src, (int32_t *)r);
    RECT rc = {r[0], r[1], r[2], r[3]};
    OffsetRect(&rc, H(int32_t, HSL_SCROLL_X), H(int32_t, HSL_SCROLL_Y));
    SpritePaintRect(g_hsl, &rc, 1);
}

/* 0x1005fa4b: the strips left and right of b, between a's sides, as tall as b */
void __stdcall wipe_sides(const int32_t *a, const int32_t *b)
{
    int32_t r[4] = {a[0], b[1], b[0], b[3]};
    wipe_apply(r);
    r[0] = b[2];
    r[2] = a[2];
    wipe_apply(r);
}

/* 0x1005faa6: the strips above and below b, as wide as a */
void __stdcall wipe_bands(const int32_t *a, const int32_t *b)
{
    int32_t r[4] = {a[0], a[1], a[2], b[1]};
    wipe_apply(r);
    r[1] = b[3];
    r[3] = a[3];
    wipe_apply(r);
}

/* 0x1005f885: reveal from the smallest rectangle outwards. Every step redraws all the
 * way in to the first rectangle. Sprite messages are held back while animating. */
void __stdcall wipe_reveal_run(int steps, int sides)
{
    int32_t first[4], r[4];
    wipe_rect(steps - 1, first, steps, sides);
    wipe_apply(first);
    for (int i = steps - 2; i >= 0; i--) {
        uint32_t saved = H(uint32_t, HSL_NO_SEND_MSG);
        H(uint32_t, HSL_NO_SEND_MSG) = 1;
        SpriteAnimate(g_hsl);
        H(uint32_t, HSL_NO_SEND_MSG) = saved;
        wipe_rect(i, r, steps, sides);
        if (sides)
            wipe_sides(r, first);
        wipe_bands(r, first);
    }
}

/* 0x1005fc24: blank from the edges inwards; each step redraws from the outer edge */
void __stdcall wipe_blank_run(uint32_t steps, int sides)
{
    int32_t r[4], outer[4];
    wipe_rect((int)steps - 1, r, (int)steps, sides);
    wipe_rect(0, outer, (int)steps, sides);
    for (uint32_t i = 1; i < steps; i++) {
        wipe_rect((int)i, r, (int)steps, sides);
        wipe_bands(outer, r);
        if (sides)
            wipe_sides(outer, r);
        SpriteAnimate(g_hsl);
    }
    wipe_apply(r);
}

static void wipe_size(void)
{
    g_wipe.w = H(int32_t, HSL_SCROLL_LIMITS + 8) - H(int32_t, HSL_SCROLL_LIMITS);
    g_wipe.h = H(int32_t, HSL_SCROLL_LIMITS + 12) - H(int32_t, HSL_SCROLL_LIMITS + 4);
}

/* 0x1005f760: wipes bitmap bm in as the new background. Without a background (or in 16
 * bits) it only repaints the screen. */
void __stdcall wipe_in(uint8_t *hsl, uint8_t *bm, uint32_t steps, int sides)
{
    g_hsl = hsl;
    if ((!H(uint32_t, HSL_9FC) && !H(uint32_t, HSL_BACKGROUND)) || H(uint32_t, 0x8bc) == 0x10) {
        uint32_t saved = H(uint32_t, HSL_BKG_TRANSPARENT);
        H(uint32_t, HSL_BKG_TRANSPARENT) = 0;
        SpritePaintRect(hsl, &H(RECT, HSL_SCREEN_RECT), 0);
        H(uint32_t, HSL_BKG_TRANSPARENT) = saved;
    } else if (H(uint32_t, HSL_BACKGROUND)) {
        g_wipe.src = bm;
        wipe_size();
        g_wipe.reveal = 1;
        wipe_reveal_run(steps < 3 ? 2 : (int)steps, sides);
        H(uint8_t *, HSL_BACKGROUND) = bm;
    }
}

/* 0x1005fb80: makes bm the background and wipes it out to color 0 */
void __stdcall wipe_out(uint8_t *hsl, uint8_t *bm, uint32_t steps, int sides)
{
    g_hsl = hsl;
    if (!bm)
        return;
    H(uint8_t *, HSL_BACKGROUND) = bm;
    wipe_size();
    g_wipe.reveal = 0;
    wipe_blank_run(steps < 3 ? 2 : steps, sides);
    H(uint8_t *, HSL_BACKGROUND) = bm;
}
