/* The sprite engine's drawing pass: redraw a rectangle by drawing the background into the
 * screen bitmap, then every visible sprite over it in level order, then copying the result
 * to the window (GDI) or unlocking the back buffer (DirectDraw). Rewritten from the original
 * (0x100562d9-0x10057662 and 0x10057dc3); the blitters it calls are still original.
 *
 * Sprites above level 24,500 are drawn in a second pass, after the tile overlay. With the
 * interface panel on, sprites that belong to it (field 0x9c) are drawn into its own
 * bitmap. */
#include "orig.h"
#include "blit.h"
#define intersect_rect(o, a, b) rect_intersect((int32_t *)(o), (const int32_t *)(a), (const int32_t *)(b))
#define union_rect(o, a, b) rect_union((int32_t *)(o), (const int32_t *)(a), (const int32_t *)(b))
#include "draw.h"
#include "display.h"
#include "spritelib.h"
#include "view.h"
#include "tiles.h"
#include "utils.h"

#define g_hsl G(uint8_t *, 0x10089e24)
#define H(type, off) (*(type *)(g_hsl + (off)))
#define S(type, s, off) (*(type *)((uint8_t *)(s) + (off)))
#define INFO(type, s, off) S(type, S(uint8_t *, s, 0x34), off)

#define g_draw_rect G(RECT, 0x10089e58) /* area of the current drawing pass */
#define g_last_drawn G(uint8_t *, 0x1007b3b4)

/* HSL fields */
#define HSL_HEAD 0x000
#define HSL_TAIL 0x004
#define HSL_PAINT_HOOK 0x028    /* proc(arg, dc, rect, stage 0-2) */
#define HSL_BMI 0x02c           /* BITMAPINFO of the screen bitmap */
#define HSL_PAINT_HOOK_ARG 0x45c
#define HSL_DC 0x468
#define HSL_BACKGROUND 0x474
#define HSL_SCREEN_DIB 0x478
#define HSL_TILES 0x47c
#define HSL_TILE_MAP 0x480
#define HSL_ZOOM 0x88c
#define HSL_ZOOM_W 0x890
#define HSL_ZOOM_H 0x894
#define HSL_FILL 0x898
#define HSL_DIB_INDICES 0x8a0
#define HSL_WIDTH 0x8a4
#define HSL_HEIGHT 0x8a8
#define HSL_DEPTH 0x8bc
#define HSL_COUNT 0x8c4
#define HSL_ORIGIN_X 0x8cc
#define HSL_ORIGIN_Y 0x8d0
#define HSL_SCROLL_X 0x8d4
#define HSL_SCROLL_Y 0x8d8
#define HSL_SCREEN_RECT 0x8f4
#define HSL_VIEW 0x904
#define HSL_VIEW_BOTTOM_ALL 0x910 /* VIEW.bottom */
#define HSL_VIEW2 0x914
#define HSL_VIEW2_BOTTOM 0x920
#define HSL_RESIZED 0x94c
#define HSL_TOP_DOWN 0x950
#define HSL_DIRECTDRAW 0x954
#define HSL_CAPTURE 0x96c
#define HSL_CAPTURE_NAME 0x970
#define HSL_STRETCH 0xa00
#define HSL_REDRAW_ALL 0xa04    /* drawing suspended (SpriteSetSync) */
#define HSL_ZOOMED 0xa08
#define HSL_IFACE_DIB 0xb58
#define HSL_B5C 0xb5c
#define HSL_IFACE_ON 0xb68

/* sprite fields */
#define SP_NEXT 0x04
#define SP_BITMAP 0x18
#define SP_PARTS 0x20
#define SP_DRAW_PROC 0x38       /* proc(hsl, s, arg, rect) draws the sprite itself */
#define SP_RECT 0x44
#define SP_FLIP 0x7c
#define SP_NPARTS 0x90
#define SP_LEVEL 0x98
#define SP_PANEL 0x9c           /* belongs to the interface panel */
#define SP_CLIPPED 0xb0
#define SP_SCROLLS 0xe4
#define SP_VISIBLE 0xec
#define INFO_CLIP_BOX 0x08
#define INFO_DRAW_ARG 0x1c
#define INFO_COLOR_MAP 0x28
#define INFO_FX 0x2c
#define INFO_FX_ARG 0x30

#define TOP_LEVEL 0x5fb4        /* sprites above it are drawn in the second pass */
#define REDRAW_ALL 0x80000000u

typedef void(__stdcall *DrawProc)(uint8_t *, uint8_t *, uint32_t, RECT *);
typedef void(__stdcall *PaintHook)(uint32_t, HDC, RECT *, uint32_t);

/* blitters, still original: dst, src, w, h, dst x, y, src x, y, flip, ... */
#define dib_fill16 ORIG(void(__stdcall *)(uint32_t, int, int, int, int, uint32_t), 0x100638d8)
#define dib_copy16 ORIG(void(__stdcall *)(uint32_t, uint32_t, int, int, int, int, int, int, uint32_t), 0x10063a78)




/* 0x10059e74: draws bitmap src (w x h from sx, sy) into dst at dx, dy, with transparency,
 * shadows, flips and an optional color map. Run-length coded sprites are drawn by our C
 * (blit.c); plain bitmaps still by the original assembly (0x10060c14). */
uint32_t __cdecl dib_merge(uint32_t dst, uint32_t src, int w, int h, int dx, int dy, int sx, int sy, uint32_t flip,
                           uint32_t cmap)
{
    uint8_t *s = (uint8_t *)(uintptr_t)src;
    if (sx < 0 || sy < 0 || S(uint32_t, s, 4) - 1 < (uint32_t)sx || S(uint32_t, s, 0xc) - 1 < (uint32_t)sy || w < 1 ||
        h < 1 || dx < 0 || dy < 0) {
        DebugBreak(); /* original */
        return 0;
    }
    if (!(S(uint8_t, s, 0x10) & 0x80))
        return dib_merge_plain_cm(dst, src, w, h, dx, dy, sx, sy, flip, cmap);
    MergeEnv e = {(const uint32_t *)ORIG_PTR(0x1007b7e8), H(uint32_t, 0xb74) && H(uint32_t, HSL_WIDTH) > 0x280,
                  H(uint32_t, 0xb74) && H(uint32_t, HSL_IFACE_ON)};
    dib_merge_rle(&e, (void *)(uintptr_t)dst, s, w, h, dx, dy, sx, sy, flip, (const uint32_t *)(uintptr_t)cmap);
    return 0;
}

#define HSL_FX_TABLE 0xb6c
#define HSL_FX_BLEND 0xb70
#define HSL_FX_DITHER 0xb78

static FxEnv fx_env(void)
{
    FxEnv e = {{(const uint32_t *)ORIG_PTR(0x1007b7e8), H(uint32_t, 0xb74) && H(uint32_t, HSL_WIDTH) > 0x280, 0},
               H(const uint8_t *, HSL_FX_BLEND), H(const uint32_t *, HSL_FX_TABLE), H(uint32_t, HSL_FX_DITHER) != 0,
               0, 0};
    return e;
}

/* 0x1005adb3: dib_merge with a blit effect (SpriteSetBlitFX). Plain bitmaps get no effect
 * and lose their color map. */
void __cdecl dib_fx_merge(uint32_t dst, uint32_t src, int w, int h, int dx, int dy, int sx, int sy, uint32_t flip,
                          uint32_t fx, uint32_t arg, uint32_t cmap)
{
    uint8_t *s = (uint8_t *)(uintptr_t)src;
    if (sx < 0 || sy < 0 || S(uint32_t, s, 4) - 1 < (uint32_t)sx || S(uint32_t, s, 0xc) - 1 < (uint32_t)sy || w < 1 ||
        h < 1 || dx < 0 || dy < 0) {
        DebugBreak(); /* original */
        return;
    }
    if (!(S(uint8_t, s, 0x10) & 0x80)) {
        dib_merge_plain_cm(dst, src, w, h, dx, dy, sx, sy, flip, 0);
        return;
    }
    FxEnv e = fx_env();
    dib_merge_fx_rle(&e, (void *)(uintptr_t)dst, s, w, h, dx, dy, sx, sy, flip, fx, (int32_t)arg,
                     (const uint32_t *)(uintptr_t)cmap);
}

/* 0x10059b6e and 0x10059cda: an effect run, forward or mirrored */
void __cdecl fx_run(uint8_t *d, const uint8_t *s, uint32_t n, uint32_t fx, int32_t arg, uint32_t row,
                    const uint32_t *cmap)
{
    FxEnv e = fx_env();
    dib_fx_run(&e, d, s, n, fx, arg, row, cmap);
}

void __cdecl fx_run_mirrored(uint8_t *d, const uint8_t *s, uint32_t n, uint32_t fx, int32_t arg, const uint32_t *cmap)
{
    FxEnv e = fx_env();
    dib_fx_run_mirrored(&e, d, s, n, fx, arg, cmap);
}

/* 0x100653dd, 0x100656a0, 0x10065a19: the effect's assembly routines */
void __stdcall fx_blend_asm(uint8_t *d, const uint8_t *s, uint32_t n, const uint8_t *blend)
{
    fx_blend(d, s, n, blend);
}

void __stdcall fx_blend_cmap_asm(uint8_t *d, const uint8_t *s, uint32_t n, const uint8_t *blend, const uint32_t *cmap)
{
    G(const uint32_t *, 0x1007bd38) = cmap; /* the original keeps it there */
    fx_blend_cmap(d, s, n, blend, cmap);
}

void __stdcall fx_dither_asm(uint8_t *d, const uint8_t *s, uint32_t n, uint32_t row)
{
    fx_dither(d, s, n, row);
}

/* 0x1005bc90: copies n bytes, writing leftwards */
void __cdecl copy_mirrored(uint8_t *d, const uint8_t *s, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        *d-- = *s++;
}

/* 0x1005bce0: the slow paths' shadow (type 1); type 2 does nothing */
void __cdecl shadow_run(int type, uint8_t *p, uint32_t n, uint32_t row)
{
    if (type != 0)
        return;
    if (!H(uint32_t, 0xb74) || H(uint32_t, HSL_WIDTH) < 0x281)
        dib_shadow(p, n, (const uint32_t *)ORIG_PTR(0x1007b7e8));
    else
        dib_shadow_dither(p, n, row);
}

static PlainEnv plain_env(void)
{
    PlainEnv e = {(const uint32_t *)ORIG_PTR(0x1007b7e8), (const uint8_t *)ORIG_PTR(0x1007bbe8), G(uint8_t, 0x10089e1c)};
    return e;
}

/* 0x10060c14: the plain merge with a color map (the original jumps to 0x1005fcec without
 * one; that removes 9 arguments instead of 10, which its callers' frames hid) */
uint32_t __stdcall dib_merge_plain_cm(uint32_t dst, uint32_t src, int w, int h, int dx, int dy, int sx, int sy,
                                      uint32_t flip, uint32_t cmap)
{
    PlainEnv e = plain_env();
    dib_merge_plain(&e, (void *)(uintptr_t)dst, (void *)(uintptr_t)src, w, h, dx, dy, sx, sy, flip,
                    (const uint32_t *)(uintptr_t)cmap);
    return 0;
}

/* 0x1005fcec: the plain merge without a color map */
uint32_t __stdcall dib_merge_plain9(uint32_t dst, uint32_t src, int w, int h, int dx, int dy, int sx, int sy,
                                    uint32_t flip)
{
    PlainEnv e = plain_env();
    dib_merge_plain(&e, (void *)(uintptr_t)dst, (void *)(uintptr_t)src, w, h, dx, dy, sx, sy, flip, NULL);
    return 0;
}

/* Draws one bitmap of s, with its effect if it has one */
static void blit(uint8_t *s, uint32_t dst, uint32_t src, int w, int h, int dx, int dy, int sx, int sy)
{
    if (!INFO(uint32_t, s, INFO_FX))
        dib_merge(dst, src, w, h, dx, dy, sx, sy, S(uint32_t, s, SP_FLIP), INFO(uint32_t, s, INFO_COLOR_MAP));
    else
        dib_fx_merge(dst, src, w, h, dx, dy, sx, sy, S(uint32_t, s, SP_FLIP), INFO(uint32_t, s, INFO_FX),
                     INFO(uint32_t, s, INFO_FX_ARG), INFO(uint32_t, s, INFO_COLOR_MAP));
}

/* The play area, one pixel taller with GDI */
static RECT view2_inclusive(void)
{
    RECT v = H(RECT, HSL_VIEW2);
    if (!H(uint32_t, HSL_DIRECTDRAW))
        v.bottom++;
    return v;
}

/* Redraws the interface panel's sprites within r */
int __stdcall draw_iface_rect(const int32_t *r)
{
    RECT rc = {r[0], r[1], r[2], r[3]};
    int top = H(int32_t, HSL_VIEW2_BOTTOM) + 1;
    int t = top;
    if (top <= r[1]) {
        if (H(int32_t, HSL_VIEW_BOTTOM_ALL) <= r[1] || rc.bottom < top)
            return 1;
        t = r[1];
        if (H(int32_t, HSL_VIEW_BOTTOM_ALL) <= rc.bottom)
            rc.bottom = H(int32_t, HSL_VIEW_BOTTOM_ALL) - 1;
    }
    rc.top = t;
    if (rc.left < H(int32_t, HSL_VIEW))
        rc.left = H(int32_t, HSL_VIEW);
    draw_sprites(&rc, 1, 0, NULL);
    return 1;
}

/* Draws the visible sprites over rect r, starting at `from` (or the first).
 * panel: draw into the interface panel. pass 1 stops at the first sprite above
 * TOP_LEVEL and returns it. */
uint8_t *__stdcall draw_sprites(const RECT *r, int panel, int pass, uint8_t *from)
{
    RECT c;
    if (!H(uint32_t, HSL_COUNT))
        return NULL;
    RECT v2 = view2_inclusive();
    uint8_t *s = from ? from : S(uint8_t *, H(uint8_t *, HSL_HEAD), SP_NEXT);
    for (; s != H(uint8_t *, HSL_TAIL); g_last_drawn = s, s = S(uint8_t *, s, SP_NEXT)) {
        if (!S(uint32_t, s, SP_VISIBLE))
            continue;
        if (pass == 1 && S(int32_t, s, SP_LEVEL) > TOP_LEVEL)
            return s;
        if (!H(uint32_t, HSL_IFACE_ON) && S(uint32_t, s, SP_SCROLLS))
            continue;
        if (!intersect_rect(&c, r, s + SP_RECT))
            continue;
        if (S(DrawProc, s, SP_DRAW_PROC)) {
            S(DrawProc, s, SP_DRAW_PROC)(g_hsl, s, INFO(uint32_t, s, INFO_DRAW_ARG), &c);
            continue;
        }
        if (S(uint32_t, s, SP_NPARTS)) {
            draw_parts(r, s, panel);
            continue;
        }
        if (S(uint32_t, s, SP_SCROLLS) && (panel || !intersect_rect(&c, &c, &v2)))
            continue;
        uint32_t dst;
        if (panel) {
            if (!S(uint32_t, s, SP_PANEL) && S(uint32_t, s, SP_SCROLLS))
                continue;
            dst = H(uint32_t, HSL_IFACE_DIB);
        } else if (!S(uint32_t, s, SP_PANEL)) {
            if (H(uint32_t, HSL_IFACE_ON) && H(uint32_t, HSL_DIRECTDRAW) &&
                H(int32_t, HSL_VIEW2_BOTTOM) <= S(int32_t, s, SP_RECT + 12))
                c.bottom++;
            dst = H(uint32_t, HSL_SCREEN_DIB);
        } else {
            if (H(int32_t, HSL_VIEW2_BOTTOM) < S(int32_t, s, SP_RECT + 4))
                continue;
            dst = H(uint32_t, HSL_SCREEN_DIB);
            if (H(uint32_t, HSL_DIRECTDRAW))
                c.bottom++;
        }
        if (S(uint32_t, s, SP_CLIPPED) && !intersect_rect(&c, &c, &INFO(uint8_t, s, INFO_CLIP_BOX)))
            continue;
        uint32_t w = c.right - c.left, h = c.bottom - c.top;
        int sx = c.left - S(int32_t, s, SP_RECT), sy = c.top - S(int32_t, s, SP_RECT + 4), dx, dy;
        if (panel) {
            dx = c.left - H(int32_t, HSL_VIEW);
            dy = c.top - H(int32_t, HSL_VIEW + 4);
            if (H(uint32_t, HSL_DIRECTDRAW) && H(uint32_t, HSL_WIDTH) <= w + dx) {
                w = H(uint32_t, HSL_WIDTH) - dx;
                if ((int)w < 1)
                    continue;
            }
            dy -= H(int32_t, HSL_VIEW2_BOTTOM) + H(int32_t, HSL_ORIGIN_Y) + 1;
            if (dy == 0 && h > 100)
                h++;
        } else if (H(uint32_t, HSL_RESIZED)) {
            dx = c.left - H(int32_t, HSL_VIEW);
            dy = c.top - H(int32_t, HSL_VIEW + 4);
        } else {
            dx = c.left - r->left;
            dy = c.top - r->top;
        }
        blit(s, dst, S(uint32_t, s, SP_BITMAP), w, h, dx, dy, sx, sy);
    }
    return NULL;
}

/* Draws the parts of a composed sprite s over rect r */
void __stdcall draw_parts(const RECT *r, uint8_t *s, int panel)
{
    RECT c;
    RECT v2 = view2_inclusive();
    int32_t *p = S(int32_t *, s, SP_PARTS), *end = p + S(uint32_t, s, SP_NPARTS) * 5;
    for (; p < end; p += 5) {
        if (!intersect_rect(&c, r, p + 1))
            continue;
        if (S(uint32_t, s, SP_SCROLLS) && (panel || !intersect_rect(&c, &c, &v2)))
            continue;
        if (S(uint32_t, s, SP_CLIPPED) && !intersect_rect(&c, &c, &INFO(uint8_t, s, INFO_CLIP_BOX)))
            continue;
        int w = c.right - c.left, h = c.bottom - c.top, sx = c.left - p[1], sy = c.top - p[2];
        if (!H(uint32_t, HSL_RESIZED)) {
            blit(s, H(uint32_t, HSL_SCREEN_DIB), p[0], w, h, c.left - r->left, c.top - r->top, sx, sy);
            continue;
        }
        uint32_t dst;
        if (!panel) {
            if (S(uint32_t, s, SP_PANEL) && H(int32_t, HSL_VIEW2_BOTTOM) < S(int32_t, s, SP_RECT + 4))
                continue;
            dst = H(uint32_t, HSL_SCREEN_DIB);
        } else {
            if (!S(uint32_t, s, SP_PANEL) && S(uint32_t, s, SP_SCROLLS))
                continue;
            dst = H(uint32_t, HSL_IFACE_DIB);
        }
        int dx = c.left - H(int32_t, HSL_VIEW), dy = c.top - H(int32_t, HSL_VIEW + 4);
        if (panel)
            dy -= H(int32_t, HSL_VIEW2_BOTTOM) + H(int32_t, HSL_ORIGIN_Y) + 1;
        blit(s, dst, p[0], w, h, dx, dy, sx, sy);
    }
}

static void paint_hook(uint32_t stage)
{
    if (H(PaintHook, HSL_PAINT_HOOK) && !H(uint32_t, HSL_DIRECTDRAW))
        H(PaintHook, HSL_PAINT_HOOK)(H(uint32_t, HSL_PAINT_HOOK_ARG), H(HDC, HSL_DC), &g_draw_rect, stage);
}

/* Redraws rectangle r: background (when level is REDRAW_ALL), sprites, then the window.
 * Returns 0 when the screen bitmap could not grow. */
int __stdcall draw_view_sprites(const void *r, uint32_t level)
{
    RECT t;
    if (H(uint32_t, HSL_REDRAW_ALL) || !intersect_rect(&g_draw_rect, r, &H(RECT, HSL_SCREEN_RECT)))
        return 1;
    if (H(uint32_t, HSL_IFACE_ON) && !H(uint32_t, HSL_TILES))
        H(uint32_t, HSL_IFACE_ON) = 0;
    uint32_t w = g_draw_rect.right - g_draw_rect.left, h = g_draw_rect.bottom - g_draw_rect.top;
    if (!H(uint32_t, HSL_DIRECTDRAW) && !H(uint32_t, HSL_RESIZED) && !screen_dib_fit(w, h))
        return 0;
    paint_hook(0);
    if (level == REDRAW_ALL)
        draw_background(w, h);
    if (H(uint32_t, HSL_DIRECTDRAW) && H(uint32_t, HSL_IFACE_ON) && level)
        tiles_draw(&H(RECT, HSL_VIEW2), H(uint32_t, HSL_SCREEN_DIB), 0);
    const RECT *area = (H(uint32_t, HSL_IFACE_ON) && H(uint32_t, HSL_DIRECTDRAW)) ? &H(RECT, HSL_VIEW2) : &g_draw_rect;
    uint8_t *rest = draw_sprites(area, 0, 1, NULL);
    paint_hook(1);
    if (!H(uint32_t, HSL_B5C) && !level)
        return 1;
    if (H(uint32_t, HSL_IFACE_ON) && intersect_rect(&t, &g_draw_rect, &H(RECT, HSL_VIEW2)) &&
        H(uint8_t *, HSL_TILE_MAP) && S(uint32_t, H(uint8_t *, HSL_TILE_MAP), 0xc))
        tiles_overlay(&t, H(uint32_t, HSL_SCREEN_DIB));
    if (rest) {
        area = (H(uint32_t, HSL_IFACE_ON) && H(uint32_t, HSL_DIRECTDRAW)) ? &H(RECT, HSL_VIEW2) : &g_draw_rect;
        draw_sprites(area, 0, 2, rest);
    }
    if (H(uint32_t, HSL_DIRECTDRAW) && H(uint32_t, HSL_CAPTURE)) {
        uint8_t *shot = SpriteDibCreate(H(int32_t, HSL_WIDTH), H(int32_t, HSL_HEIGHT), 1, 8);
        if (shot && H(uint32_t, HSL_SCREEN_DIB)) {
            S(uint32_t, shot, 0x10) |= 0x40;
            SpriteDibCopy((uint32_t)(uintptr_t)shot, H(uint32_t, HSL_SCREEN_DIB), H(uint32_t, HSL_WIDTH), H(uint32_t, HSL_HEIGHT), 0, 0, 0, 0);
            SpriteSaveScreenShot(shot, &H(char, HSL_CAPTURE_NAME));
        }
        H(uint32_t, HSL_CAPTURE) = 0;
        if (shot)
            winfree(shot);
    }
    screen_blit(H(uint8_t *, HSL_SCREEN_DIB));
    paint_hook(2);
    return 1;
}

/* Draws the background (bitmap, tiles or fill color) of g_draw_rect into the screen bitmap */
void __stdcall draw_background(uint32_t w, uint32_t h)
{
    if (H(uint32_t, HSL_DIRECTDRAW)) {
        if (!H(uint32_t, HSL_IFACE_ON))
            back_fill((int32_t *)&g_draw_rect, w, h);
        else
            back_lock();
        return;
    }
    int bx = H(int32_t, HSL_ORIGIN_X) + g_draw_rect.left, by = H(int32_t, HSL_ORIGIN_Y) + g_draw_rect.top;
    int x = 0, y = 0;
    if (H(uint32_t, HSL_RESIZED)) {
        x = g_draw_rect.left - H(int32_t, HSL_VIEW);
        y = g_draw_rect.top - H(int32_t, HSL_VIEW + 4);
    }
    uint32_t screen = H(uint32_t, HSL_SCREEN_DIB), bg = H(uint32_t, HSL_BACKGROUND);
    int sx = bx - H(int32_t, HSL_SCROLL_X), sy = by - H(int32_t, HSL_SCROLL_Y);
    if (H(uint32_t, HSL_TILES)) {
        if (H(uint32_t, HSL_IFACE_ON) && H(int32_t, HSL_ORIGIN_Y) + H(int32_t, HSL_VIEW2_BOTTOM) == (int)(y + h))
            h++;
        dib_copy((void *)(uintptr_t)screen, H(void *, HSL_TILES), w, h, x, y, bx, by, 0xff00);
    } else if (!bg) {
        if (H(int32_t, HSL_FILL) == -1)
            return;
        if (H(uint32_t, HSL_DEPTH) == 16)
            dib_fill16(screen, x, y, w, h, H(uint16_t, HSL_FILL));
        else
            dib_fill8((void *)(uintptr_t)screen, x, y, w, h, H(uint8_t, HSL_FILL));
    } else if (H(uint32_t, HSL_DEPTH) == 16) {
        dib_copy16(screen, bg, w, h, x, y, sx, sy, 0);
    } else if (!(S(uint8_t, (uintptr_t)bg, 0x10) & 0x80)) {
        dib_copy((void *)(uintptr_t)screen, (void *)(uintptr_t)bg, w, h, x, y, sx, sy, 0);
    } else {
        dib_merge(screen, bg, w, h, x, y, sx, sy, 0, 0); /* run-length coded background */
    }
}

/* Copies the screen bitmap's g_draw_rect to the window, or ends the DirectDraw frame */
void __stdcall screen_blit(uint8_t *bm)
{
    if (H(uint32_t, HSL_DIRECTDRAW)) {
        if (!H(uint32_t, HSL_IFACE_ON))
            back_unlock();
        else
            back_unlock_rect((int32_t *)&g_draw_rect);
        return;
    }
    BITMAPINFO *bmi = (BITMAPINFO *)(g_hsl + HSL_BMI);
    int bw = S(int32_t, bm, 4), bh = S(int32_t, bm, 0xc);
    bmi->bmiHeader.biWidth = S(int32_t, bm, 8);
    bmi->bmiHeader.biHeight = H(uint32_t, HSL_TOP_DOWN) ? -bh : bh;
    RECT *g = &g_draw_rect;
    if (H(uint32_t, HSL_ZOOMED) || H(uint32_t, HSL_STRETCH)) {
        int z = H(int32_t, HSL_ZOOM);
        StretchDIBits(H(HDC, HSL_DC), z * g->left - H(int32_t, HSL_ZOOM_W), z * g->top - H(int32_t, HSL_ZOOM_H), bw * z,
                      z * bh, 0, 0, bw, bh, S(void *, bm, 0x20), bmi, H(uint32_t, HSL_DIB_INDICES), SRCCOPY);
    } else if (!H(uint32_t, HSL_RESIZED)) {
        SetDIBitsToDevice(H(HDC, HSL_DC), g->left, g->top, bw, bh, 0, 0, 0, bh, S(void *, bm, 0x20), bmi,
                          H(uint32_t, HSL_DIB_INDICES));
    } else {
        int gh = g->bottom - g->top;
        SetDIBitsToDevice(H(HDC, HSL_DC), g->left, g->top, g->right - g->left, gh, g->left - H(int32_t, HSL_VIEW),
                          (bh - gh) - (g->top - H(int32_t, HSL_VIEW + 4)), 0, bh, S(void *, bm, 0x20), bmi,
                          H(uint32_t, HSL_DIB_INDICES));
    }
}

/* Draws sprite s into bitmap dst, which covers the play area */
void __cdecl draw_sprite_into(uint8_t *s, uint32_t dst)
{
    RECT c;
    if (!intersect_rect(&c, &H(RECT, HSL_VIEW), s + SP_RECT))
        return;
    blit(s, dst, S(uint32_t, s, SP_BITMAP), c.right - c.left, c.bottom - c.top, c.left - H(int32_t, HSL_VIEW),
         c.top - H(int32_t, HSL_VIEW + 4), c.left - S(int32_t, s, SP_RECT), c.top - S(int32_t, s, SP_RECT + 4));
}

/* ---- the exported bitmap operations (0x100537b1-0x100539eb) ---- */

#define BMP(p) ((void *)(uintptr_t)(p))

void __stdcall SpriteDibCopy(uint32_t dst, uint32_t src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy)
{
    dib_copy(BMP(dst), BMP(src), w, h, dx, dy, sx, sy, 0);
}

/* Puts src's pixels back wherever dst is 255 */
void __stdcall SpriteDibDiff(uint32_t dst, uint32_t src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy)
{
    dib_restore(BMP(dst), BMP(src), w, h, dx, dy, sx, sy, 0);
}

/* 0x10053823 (not exported): clears dst wherever src is not 0 */
void __stdcall sprite_dib_mask(uint32_t dst, uint32_t src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy)
{
    dib_mask(BMP(dst), BMP(src), w, h, dx, dy, sx, sy, 0);
}

void __stdcall SpriteDibMerge(uint32_t dst, uint32_t src, int w, int h, int dx, int dy, int sx, int sy)
{
    dib_merge(dst, src, w, h, dx, dy, sx, sy, 0, 0);
}

void __stdcall SpriteDibSet(uint32_t bm, int x, int y, int w, int h, uint32_t color)
{
    dib_fill8(BMP(bm), x, y, w, h, color);
}

void __stdcall SpriteDibColorMapMerge(uint32_t dst, uint32_t src, int w, int h, int dx, int dy, int sx, int sy,
                                      uint32_t flip, uint32_t cmap)
{
    dib_merge(dst, src, w, h, dx, dy, sx, sy, flip, cmap);
}

void __stdcall SpriteDibFXMerge(uint32_t dst, uint32_t src, int w, int h, int dx, int dy, int sx, int sy,
                                uint32_t flip, uint32_t fx, uint32_t arg)
{
    dib_fx_merge(dst, src, w, h, dx, dy, sx, sy, flip, fx, arg, 0);
}

/* 0x10062f2c, 0x10062ffe: the assembly behind SpriteDibDiff and the mask */
void __stdcall dib_restore_asm(uint32_t dst, uint32_t src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy,
                               uint32_t flip)
{
    dib_restore(BMP(dst), BMP(src), w, h, dx, dy, sx, sy, flip);
}

void __stdcall dib_mask_asm(uint32_t dst, uint32_t src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy,
                            uint32_t flip)
{
    dib_mask(BMP(dst), BMP(src), w, h, dx, dy, sx, sy, flip);
}

/* 0x100661b0, 0x1006630c: the engine's own memcpy and memmove */
void __stdcall rt_memcpy(void *d, const void *s, uint32_t n)
{
    memcpy(d, s, n);
}

void __stdcall rt_memmove(void *d, const void *s, uint32_t n)
{
    memmove(d, s, n);
}

/* 0x10059a70 (unused): the name of a drawing order (T tiles, O overlays, S sprites).
 * "T+O" can't be reached: it is tested for 0 again. */
char *__cdecl draw_order_name(int order)
{
    static const char *const names[] = {"T", "O+T", "ARB", "T+S", "S+T", "O"};
    char *buf = (char *)ORIG_PTR(0x10089e78);
    lstrcpyA(buf, order >= 0 && order <= 5 ? names[order] : "???");
    return buf;
}
