/* The sprite engine's display layer: DirectDraw object, screen mode, flip chain, palette,
 * the "interface" overlay and its dirty rectangles. Rewritten from the original
 * (0x10048580-0x1004d270), and the palette functions (0x100517c0-0x100524c0).
 *
 * The sprite engine's state ("HSL", over 4 KB) is shared with code not rewritten yet, so
 * its fields are reached at the original's offsets (H()). The DirectDraw objects live in a
 * 0xb4-byte block it points to (DisplayDD). */
#define COBJMACROS
#include "orig.h"
#include "display.h"
#include "view.h"
#include "utils.h"
#include <ddraw.h>

#define g_hsl G(uint8_t *, 0x10089e24)
#define H(type, off) (*(type *)(g_hsl + (off)))
#define HS(type, hsl, off) (*(type *)((uint8_t *)(hsl) + (off)))

/* HSL fields */
#define HSL_DD 0x010           /* DisplayDD * */
#define HSL_DIB_COLORS 0x054   /* colors of the screen DIB: 256 RGBQUADs, or palette indices */
#define HSL_DC 0x468
#define HSL_HPALETTE 0x46c
#define HSL_WND 0x470
#define HSL_BACKGROUND 0x474   /* background bitmap (ResDib *) */
#define HSL_SCREEN_DIB 0x478   /* the screen as a bitmap: points at the locked back buffer */
#define HSL_INTERFACE 0x47c
#define HSL_LOGPALETTE 0x488   /* LOGPALETTE header; the entries follow */
#define HSL_PALETTE 0x48c      /* 256 PALETTEENTRY */
#define HSL_PALETTE_HIGH 0x864 /* entries 246-255 */
#define HSL_FILL 0x898         /* background fill color, -1 = none */
#define HSL_DIB_INDICES 0x8a0  /* HSL_DIB_COLORS holds palette indices */
#define HSL_WIDTH 0x8a4
#define HSL_HEIGHT 0x8a8
#define HSL_DEPTH 0x8ac
#define HSL_DESKTOP_W 0x8b0
#define HSL_DESKTOP_H 0x8b4
#define HSL_ORIGIN_X 0x8cc     /* sprite coordinates are relative to the screen center */
#define HSL_ORIGIN_Y 0x8d0
#define HSL_SCREEN_RECT 0x8f4  /* RECT, centered */
#define HSL_VIEW 0x904         /* RECT of the play area */
#define HSL_VIEW2 0x914
#define HSL_VIEW_BOTTOM 0x920
#define HSL_RESIZED 0x94c
#define HSL_DIRECTDRAW 0x954   /* full-screen DirectDraw mode */
#define HSL_FLIP 0x958         /* flip pages (else blit the back buffer) */
#define HSL_HAS_640 0x95c      /* display mode lists */
#define HSL_HAS_800 0x960
#define HSL_HAS_1024 0x964
#define HSL_HAS_1280 0x968
#define HSL_PALETTE_DEVICE 0x9fc
#define HSL_REDRAW_ALL 0xa04
#define HSL_A0C 0xa0c
#define HSL_BG_X 0xa18
#define HSL_BG_Y 0xa1c
#define HSL_BORDER 0xb20       /* draw a frame around the view; RECT at 0xb24 */
#define HSL_BORDER_RECT 0xb24
#define HSL_INACTIVE 0xb40
#define HSL_PITCH 0xb48
#define HSL_DIRTY_N 0xb4c      /* interface dirty rectangles */
#define HSL_DIRTY_MAX 0xb50
#define HSL_DIRTY 0xb54
#define HSL_IFACE_DIB 0xb58
#define HSL_B5C 0xb5c
#define HSL_IFACE_ON 0xb68
#define HSL_B80 0xb80
#define HSL_SYSPAL_LOW 0xbec
#define HSL_SYSPAL_HIGH 0xfc4
#define HSL_PAINTING 0xff0

typedef struct DisplayDD {
    RECT bg_rect;                    /* 0x00 */
    DDBLTFX fx;                      /* 0x10 (0x64 bytes) */
    IDirectDraw2 *dd2;               /* 0x74 */
    IDirectDraw *dd;                 /* 0x78 */
    IDirectDrawSurface *front;       /* 0x7c flip chain */
    IDirectDrawSurface *third;       /* 0x80 triple buffering */
    IDirectDrawSurface *back;        /* 0x84 */
    IDirectDrawSurface *background;  /* 0x88 */
    IDirectDrawSurface *surf8c;      /* 0x8c */
    IDirectDrawSurface *iface;       /* 0x90 interface overlay */
    IDirectDrawSurface *surf94[3];   /* 0x94 */
    IDirectDrawPalette *palette;     /* 0xa0 */
    int32_t iface_w, iface_h;        /* 0xa4 */
    uint32_t triple;                 /* 0xac */
    uint32_t b0;                     /* 0xb0 */
} DisplayDD;
_Static_assert(sizeof(DisplayDD) == 0xb4, "DisplayDD");

#define DD() H(DisplayDD *, HSL_DD)

/* globals of the original */
#define g_use_ddraw1 G(uint32_t, 0x1007a95c)  /* Dominion.ini UseDDraw1; -1 = not read yet */
#define g_dd_borrowed G(uint32_t, 0x1007a960) /* SpriteGetDDraw handed the object out */
#define g_attach_count G(uint32_t, 0x1007a964)
#define g_blit_surface G(uint32_t, 0x1007a96c)
#define g_flips G(uint32_t, 0x1007a970)
#define g_border_color G(uint8_t, 0x1007a988)
#define g_allow_triple G(uint32_t, 0x1007b29c)
#define g_no_blt_wait G(uint32_t, 0x1007b2a0)
#define g_flip_late G(uint32_t, 0x1007b2a4)
#define g_iface_desc G(DDSURFACEDESC, 0x10088820)
#define g_modes ((uint32_t *)ORIG_PTR(0x10088890)) /* w, h, bpp per mode */
#define g_n_modes G(uint32_t, 0x1008f850)

/* original functions not rewritten yet */
#define draw_view_sprites ORIG(void(__stdcall *)(void *, uint32_t), 0x10056c51)
#define draw_iface_rect ORIG(void(__stdcall *)(void *), 0x100562d9)
#define g_static_low ((PALETTEENTRY *)ORIG_PTR(0x1007b148))  /* Windows' static colors 0-9 */
#define g_static_high ((PALETTEENTRY *)ORIG_PTR(0x1007b170)) /* 246-255 */
#define set_background ORIG(void(__cdecl *)(void *), 0x10049a02)

static void union_rect(RECT *out, const RECT *a, const RECT *b)
{
    /* original: assembly that changes EBX */
    uint32_t args[3] = {(uint32_t)(uintptr_t)out, (uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b};
    orig_stdcall_unsafe(ORIG_PTR(0x1005f70a), 3, args);
}

/* Restores a lost surface (the window lost the screen) */
static int restored(IDirectDrawSurface *s)
{
    return IDirectDrawSurface_Restore(s) == DD_OK;
}

static void fill_fx(DisplayDD *d, uint32_t color)
{
    d->fx.dwFillColor = color;
    d->fx.dwSize = sizeof d->fx;
}

/* ---- video memory, display modes ---- */

void __cdecl dd_video_memory(uint32_t *total, uint32_t *free)
{
    DDCAPS hw, hel;
    memset(&hw, 0, sizeof hw);
    hw.dwSize = 0x16c;
    memset(&hel, 0, sizeof hel);
    hel.dwSize = 0x16c;
    IDirectDraw2_GetCaps(DD()->dd2, &hw, &hel);
    *total = hw.dwVidMemTotal;
    *free = *total; /* original: reports the total as free */
}

HRESULT __stdcall dd_enum_mode(DDSURFACEDESC *m, void *unused)
{
    (void)unused;
    if (m->dwWidth == 640 && m->dwHeight == 480 && m->ddpfPixelFormat.dwRGBBitCount == 8)
        H(uint32_t, HSL_HAS_640) = 1;
    if (m->dwWidth == 800 && m->dwHeight == 600 && m->ddpfPixelFormat.dwRGBBitCount == 8)
        H(uint32_t, HSL_HAS_800) = 1;
    if (m->dwWidth == 1024 && m->dwHeight == 768 && m->ddpfPixelFormat.dwRGBBitCount == 8)
        H(uint32_t, HSL_HAS_1024) = 1;
    if (m->dwWidth == 1280 && m->dwHeight == 1024 && m->ddpfPixelFormat.dwRGBBitCount == 8)
        H(uint32_t, HSL_HAS_1280) = 1;
    /* original: no bound on the table */
    g_modes[g_n_modes * 3] = m->dwWidth;
    g_modes[g_n_modes * 3 + 1] = m->dwHeight;
    g_modes[g_n_modes * 3 + 2] = m->ddpfPixelFormat.dwRGBBitCount;
    g_n_modes++;
    return DDENUMRET_OK;
}

HRESULT __stdcall dd_enum_attached(IDirectDrawSurface *s, DDSURFACEDESC *desc, IDirectDrawSurface **out)
{
    (void)desc;
    if (g_attach_count++ == 0)
        *out = s;
    return DDENUMRET_OK;
}

/* ---- the DirectDraw object ---- */

int __cdecl dd_create(void)
{
    DisplayDD *d;
    IDirectDraw *dd = NULL;
    HRESULT r;
    if (DD())
        return 1;
    H(uint32_t, HSL_REDRAW_ALL) = 1;
    d = winmalloc(sizeof *d, 1);
    DD() = d;
    if (!d)
        return 0;
    if (DirectDrawCreate(NULL, &dd, NULL) == DD_OK &&
        IDirectDraw_QueryInterface(dd, &IID_IDirectDraw2, (void **)&d->dd2) == DD_OK) {
        d->dd = dd;
        if (g_use_ddraw1 == 0xffffffff)
            g_use_ddraw1 = GetPrivateProfileIntA("Dominion", "UseDDraw1", 0, "Dominion.ini");
        g_use_ddraw1 = g_use_ddraw1 != 0;
        if (!g_use_ddraw1)
            r = IDirectDraw2_SetCooperativeLevel(d->dd2, H(HWND, HSL_WND),
                                                 DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT);
        else
            r = IDirectDraw_SetCooperativeLevel(d->dd, H(HWND, HSL_WND),
                                                DDSCL_NORMAL | DDSCL_ALLOWREBOOT | DDSCL_FULLSCREEN); /* original: 0x0b */
        if (r == DD_OK)
            return 1;
    }
    if (dd)
        IDirectDraw_Release(dd);
    winfree(DD());
    DD() = NULL;
    return 0;
}

int __cdecl dd_destroy(void)
{
    DisplayDD *d = DD();
    if (!d)
        return 0;
    if (d->dd2)
        IDirectDraw2_Release(d->dd2);
    winfree(d);
    DD() = NULL;
    return 1;
}

IDirectDraw2 *__stdcall SpriteGetDDraw(uint8_t *hsl)
{
    g_hsl = hsl;
    g_dd_borrowed = 1;
    return DD() ? DD()->dd2 : NULL;
}

/* ---- display mode and surfaces ---- */

int __cdecl dd_set_mode(uint32_t w, uint32_t h)
{
    DisplayDD *d = DD();
    char msg[1024];
    HRESULT r;
    int unknown_mode;
    if (!d)
        return 0;
    H(uint32_t, 0xb44) = 0;
    wsprintfA(msg, "setres: %d x %d\r\n", w, h);
    OutputDebugStringA(msg);
    r = IDirectDraw2_SetCooperativeLevel(d->dd2, H(HWND, HSL_WND),
                                         DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT);
    if (r != DD_OK && r != DDERR_EXCLUSIVEMODEALREADYSET)
        return 0;
    g_n_modes = 0;
    if (IDirectDraw2_EnumDisplayModes(d->dd2, 0, NULL, NULL, (LPDDENUMMODESCALLBACK)dd_enum_mode) != DD_OK)
        return 0;
    switch (w) {
    case 640: unknown_mode = !H(uint32_t, HSL_HAS_640); break;
    case 800: unknown_mode = !H(uint32_t, HSL_HAS_800); break;
    case 1024: unknown_mode = !H(uint32_t, HSL_HAS_1024); break;
    case 1280: unknown_mode = !H(uint32_t, HSL_HAS_1280); break;
    default: unknown_mode = 1;
    }
    /* original: refused only if neither the width nor the height is the desktop's */
    if (unknown_mode && H(uint32_t, HSL_DESKTOP_W) != w && H(uint32_t, HSL_DESKTOP_H) != h) {
        wsprintfA(msg, "WARNING: %d x %d is not supported by your video card and/or monitor. "
                       "Please choose another resolution or contact Eidos Tech Support", w, h);
        MessageBoxA(H(HWND, HSL_WND), msg, "Dominion: Storm Over Gift3", MB_ICONHAND | MB_TASKMODAL);
        return 0;
    }
    /* first the size the engine had, then the new one if it differs */
    wsprintfA(msg, "setres: -> %d x %d x 8\r\n", H(uint32_t, HSL_WIDTH), H(uint32_t, HSL_HEIGHT));
    OutputDebugStringA(msg);
    if (!g_use_ddraw1) {
        if (IDirectDraw2_SetDisplayMode(d->dd2, H(uint32_t, HSL_WIDTH), H(uint32_t, HSL_HEIGHT), 8, 0, 0) != DD_OK)
            return 0;
    } else if (IDirectDraw_SetDisplayMode(d->dd, H(uint32_t, HSL_WIDTH), H(uint32_t, HSL_HEIGHT), 8) != DD_OK) {
        return 0;
    }
    if (H(uint32_t, HSL_WIDTH) != w || H(uint32_t, HSL_HEIGHT) != h) {
        wsprintfA(msg, "setres: -> %d x %d x 8\r\n", w, h);
        OutputDebugStringA(msg);
        if (!g_use_ddraw1) {
            if (IDirectDraw2_SetDisplayMode(d->dd2, w, h, 8, 0, 0) != DD_OK)
                return 0;
        } else if (IDirectDraw_SetDisplayMode(d->dd, w, h, 8) != DD_OK) {
            return 0;
        }
    }
    if (g_use_ddraw1)
        IDirectDraw_SetCooperativeLevel(d->dd, H(HWND, HSL_WND), /* original: 0x4b */
                                        DDSCL_ALLOWMODEX | DDSCL_NORMAL | DDSCL_ALLOWREBOOT | DDSCL_FULLSCREEN);
    return 1;
}

/* Fills the flip chain (and the third buffer) with black */
int __cdecl SpritePaintDirectDrawSurfacesBlack(uint8_t *hsl, uint32_t w, uint32_t h)
{
    DisplayDD *d;
    RECT r;
    HRESULT e;
    g_hsl = hsl;
    if (!w)
        w = H(uint32_t, HSL_WIDTH);
    if (!h)
        h = H(uint32_t, HSL_HEIGHT);
    d = DD();
    if (!d || !d->dd2)
        return 0;
    fill_fx(d, 0);
    SetRect(&r, 0, 0, (int)w, (int)h);
    if (!d->front)
        return 0;
    while ((e = IDirectDrawSurface_Blt(d->front, &r, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &d->fx)) != DD_OK)
        if (e != DDERR_SURFACEBUSY)
            return 0;
    if (d->triple && d->third) {
        while ((e = IDirectDrawSurface_Blt(d->third, &r, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &d->fx)) != DD_OK) {
            if (e == DDERR_SURFACELOST) {
                if (!restored(d->front))
                    return 0;
            } else if (e != DDERR_SURFACEBUSY) {
                return 0;
            }
        }
    }
    if (d->back) {
        for (;;) {
            e = IDirectDrawSurface_Blt(d->back, &r, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &d->fx);
            if (e == DD_OK)
                break;
            if (e == DDERR_SURFACELOST) {
                if (!restored(d->front))
                    return 0;
            } else if (e != DDERR_SURFACEBUSY) {
                return 0;
            }
        }
    }
    g_flips = 0;
    return 1;
}

/* original: an older version that only fills the front surface */
int __cdecl SpritePaintDirectDrawSurfacesBlack1(uint8_t *hsl, uint32_t w, uint32_t h)
{
    DisplayDD *d;
    RECT r;
    HRESULT e;
    g_hsl = hsl;
    if (!w)
        w = H(uint32_t, HSL_WIDTH);
    if (!h)
        h = H(uint32_t, HSL_HEIGHT);
    d = DD();
    if (!d || !d->dd2)
        return 0;
    fill_fx(d, 0);
    SetRect(&r, 0, 0, (int)w, (int)h);
    if (d->front) {
        while ((e = IDirectDrawSurface_Blt(d->front, &r, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &d->fx)) != DD_OK)
            if (e != DDERR_SURFACEBUSY)
                return 0;
    }
    g_flips = 0;
    return 1;
}

static int paint_three_black(uint32_t w, uint32_t h)
{
    DisplayDD *d = DD();
    for (int i = 0; i < 3; i++) {
        if (i)
            IDirectDrawSurface_Flip(d->front, NULL, DDFLIP_WAIT);
        if (!SpritePaintDirectDrawSurfacesBlack(g_hsl, w, h))
            return 0;
    }
    return 1;
}

static void flip_chain_desc(DDSURFACEDESC *desc, int w, int h, uint32_t triple)
{
    memset(desc, 0, sizeof *desc);
    desc->dwSize = sizeof *desc;
    desc->dwWidth = (DWORD)w;
    desc->dwHeight = (DWORD)h;
    desc->dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    desc->ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    desc->dwBackBufferCount = triple ? 2 : 1;
}

/* The flip chain: front + back (+ a third buffer when there is video memory for it) */
int __cdecl dd_create_surfaces(int w, int h)
{
    DisplayDD *d;
    char msg[80];
    uint32_t total, free;
    DDSURFACEDESC desc;
    DDSCAPS caps;
    HRESULT r;
    wsprintfA(msg, "cdds: %d x %d\r\n", w, h);
    OutputDebugStringA(msg);
    d = DD();
    if (!d)
        return 0;
    dd_video_memory(&total, &free);
    if (!total || !free) {
        OutputDebugStringA("***** NO FREE VIDMEM: assuming  4 MB *****\r\n");
        total = free = 0x400000;
    }
    wsprintfA(msg, "vidmem before create: total=%u avail=%u\r\n", total, free);
    OutputDebugStringA(msg);
    if (!g_allow_triple || free < (uint32_t)(h * w * 2)) {
        if (free < (uint32_t)(h * w)) {
            OutputDebugStringA("*** insufficient vram for double buffering!! *** \r\n");
            return 0;
        }
        d->triple = 0;
        d->b0 = 0;
        OutputDebugStringA("double buffer\r\n");
    } else {
        d->triple = 1;
        d->b0 = 0;
        OutputDebugStringA("triple buffer\r\n");
    }
    flip_chain_desc(&desc, w, h, d->triple);
    r = IDirectDraw2_CreateSurface(d->dd2, &desc, &d->front, NULL);
    wsprintfA(msg, "CREATE FLIP CHAIN: %d x %d, pitch %d,  %d bufs\r\n", desc.dwWidth, desc.dwHeight,
              desc.lPitch, desc.dwBackBufferCount + 1);
    OutputDebugStringA(msg);
    if (r == DDERR_NOEXCLUSIVEMODE) {
        if (IDirectDraw2_SetCooperativeLevel(d->dd2, H(HWND, HSL_WND),
                                             DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT) != DD_OK)
            return 0;
        flip_chain_desc(&desc, w, h, d->triple);
        r = IDirectDraw2_CreateSurface(d->dd2, &desc, &d->front, NULL);
        if (!d->front)
            return 0;
    }
    if (r != DD_OK) {
        OutputDebugStringA("*** couldn't create FlipSurface: CreateSurface() failed ***\n");
        dd_video_memory(&total, &free);
        wsprintfA(msg, "total=%d free=%d\r\n", total, free);
        OutputDebugStringA(msg);
        return 0;
    }
    H(int32_t, HSL_PITCH) = desc.lPitch;
    memset(&caps, 0, sizeof caps);
    caps.dwCaps = DDSCAPS_BACKBUFFER;
    if (IDirectDrawSurface_GetAttachedSurface(d->front, &caps, &d->back) != DD_OK) {
        OutputDebugStringA("*** GetAttachedSurface() failed ***\n");
        return 0;
    }
    if (d->triple) {
        g_attach_count = 0;
        if (IDirectDrawSurface_EnumAttachedSurfaces(d->front, &d->third,
                                                    (LPDDENUMSURFACESCALLBACK)dd_enum_attached) != DD_OK) {
            OutputDebugStringA("*** EnumAttachedSurfaces() failed ***\n");
            return 0;
        }
    }
    d->b0 = 0;
    H(uint32_t, HSL_B5C) = d->b0;
    if (!paint_three_black((uint32_t)w, (uint32_t)h))
        return 0;
    dd_video_memory(&total, &free);
    wsprintfA(msg, "after creating surfaces: vidmem = %lu\r\n", free);
    OutputDebugStringA(msg);
    if (d->palette)
        palette_attach();
    return 1;
}

int __cdecl dd_release_surfaces(void)
{
    DisplayDD *d = DD();
    if (!d)
        return 0;
    if (d->front)
        paint_three_black(H(uint32_t, HSL_WIDTH), H(uint32_t, HSL_HEIGHT));
    if (d->background) {
        RECT r;
        HRESULT e;
        fill_fx(d, 0);
        SetRect(&r, 0, 0, H(int32_t, HSL_WIDTH), H(int32_t, HSL_HEIGHT));
        while ((e = IDirectDrawSurface_Blt(d->background, &r, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &d->fx)) != DD_OK)
            if (e != DDERR_SURFACEBUSY)
                return 0;
        IDirectDrawSurface_Release(d->background);
        d->background = NULL;
    }
    if (d->front) {
        IDirectDrawSurface_Release(d->front);
        d->front = NULL;
        d->back = NULL;
        d->third = NULL;
    }
    if (d->iface)
        iface_surface_release();
    return 1;
}

/* The engine's idea of the screen size; the screen as a bitmap */
int __cdecl display_set_size(uint32_t w, uint32_t h, uint32_t depth)
{
    uint8_t *dib;
    H(uint32_t, HSL_RESIZED) = 1;
    if (H(void *, HSL_SCREEN_DIB))
        H(void *, HSL_SCREEN_DIB) = winfree(H(void *, HSL_SCREEN_DIB));
    if (H(void *, HSL_IFACE_DIB))
        H(void *, HSL_IFACE_DIB) = winfree(H(void *, HSL_IFACE_DIB));
    dib = SpriteDibCreate(w, 0, 0, 8);
    H(uint8_t *, HSL_SCREEN_DIB) = dib;
    HS(uint32_t, dib, 0xc) = h;
    HS(uint32_t, dib, 0x10) |= 0x40;
    SetViewportOrgEx(H(HDC, HSL_DC), (int)(w >> 1), (int)(h >> 1), NULL);
    H(int32_t, HSL_SCREEN_RECT + 4) = 0;
    H(int32_t, HSL_SCREEN_RECT) = 0;
    H(uint32_t, HSL_WIDTH) = w;
    H(uint32_t, HSL_SCREEN_RECT + 8) = w;
    H(uint32_t, HSL_HEIGHT) = h;
    H(uint32_t, HSL_SCREEN_RECT + 12) = h;
    H(uint32_t, HSL_DEPTH) = depth;
    H(int32_t, HSL_ORIGIN_X) = (H(int32_t, HSL_SCREEN_RECT + 8) - 1) / 2;
    H(int32_t, HSL_ORIGIN_Y) = (H(int32_t, HSL_SCREEN_RECT + 12) - 1) / 2;
    OffsetRect((RECT *)(g_hsl + HSL_SCREEN_RECT), -H(int32_t, HSL_ORIGIN_X), -H(int32_t, HSL_ORIGIN_Y));
    /* original: GetDeviceCaps(RASTERCAPS) & RC_PALETTE. A modern true-color desktop says
     * no, and the engine then takes an untested path that crashes (Phase 1a, problem 2);
     * patch_game.py "palette" forced this. */
    H(uint32_t, HSL_PALETTE_DEVICE) = RC_PALETTE;
    return 1;
}

/* full_screen: switch to DirectDraw; flip: page flipping (else blits) */
int __cdecl SpriteSetDisplayMode(uint8_t *hsl, uint32_t w, uint32_t h, uint32_t depth, int was_full,
                                 int full_screen, int change, uint32_t flip)
{
    g_hsl = hsl;
    H(uint32_t, HSL_REDRAW_ALL) = 1;
    if (!dd_create())
        return 0;
    if (was_full) {
        if (full_screen) {
            if (change && (!dd_release_surfaces() || !dd_set_mode(w, h) || !dd_create_surfaces((int)w, (int)h) ||
                           !display_set_size(w, h, depth)))
                return 0;
        } else {
            if (!dd_release_surfaces())
                return 0;
            H(uint32_t, HSL_DIRECTDRAW) = 0;
            if (change && (!dd_set_mode(w, h) || !display_set_size(w, h, depth)))
                return 0;
        }
    } else if (change) {
        if (full_screen ? !dd_set_mode(w, h) || !dd_create_surfaces((int)w, (int)h) || !display_set_size(w, h, depth)
                        : !dd_set_mode(w, h) || !display_set_size(w, h, depth))
            return 0;
    }
    H(uint32_t, HSL_DIRECTDRAW) = (uint32_t)full_screen;
    H(uint32_t, HSL_FLIP) = flip;
    if (!H(uint32_t, HSL_DIRECTDRAW))
        H(uint32_t, HSL_REDRAW_ALL) = 0;
    else
        H(uint32_t, HSL_REDRAW_ALL) = DD()->b0 == 0;
    return 1;
}

int __stdcall SpriteCreateDirectDrawSurfaces(uint8_t *hsl)
{
    g_hsl = hsl;
    if (!H(uint32_t, HSL_DIRECTDRAW))
        return 1;
    g_dd_borrowed = 0;
    return dd_create_surfaces(H(int32_t, HSL_WIDTH), H(int32_t, HSL_HEIGHT));
}

int __stdcall SpriteReleaseDirectDrawSurfaces(uint8_t *hsl)
{
    g_hsl = hsl;
    return H(uint32_t, HSL_DIRECTDRAW) ? dd_release_surfaces() : 1;
}

int __stdcall SpriteCreateDirectDrawObject(uint8_t *hsl)
{
    g_hsl = hsl;
    if (H(uint32_t, HSL_DIRECTDRAW))
        dd_create();
    return 1;
}

int __stdcall SpriteDestroyDirectDrawObject(uint8_t *hsl)
{
    g_hsl = hsl;
    if (H(uint32_t, HSL_DIRECTDRAW))
        dd_destroy();
    return 1;
}

int __cdecl surface_restore(IDirectDrawSurface *s)
{
    if (s && IDirectDrawSurface_IsLost(s) != DD_OK && IDirectDrawSurface_Restore(s) < 0)
        return 0;
    return 1;
}

int __cdecl surfaces_restore_all(uint8_t *hsl)
{
    DisplayDD *d;
    g_hsl = hsl;
    if (H(uint32_t, HSL_DIRECTDRAW) && (d = DD()) != NULL) {
        surface_restore(d->front);
        surface_restore(d->third);
        surface_restore(d->back);
        surface_restore(d->background);
        surface_restore(d->surf8c);
        surface_restore(d->iface);
        for (int i = 0; i < 3; i++)
            surface_restore(d->surf94[i]);
    }
    return 1;
}

/* Clears the video memory by allocating it all as one surface */
void __cdecl ClearVideoMem(IDirectDraw2 *dd)
{
    IDirectDrawSurface *s = NULL;
    DDSURFACEDESC desc;
    DDSCAPS caps;
    DWORD total, free;
    if (!dd)
        return; /* original: a debug assertion */
    IDirectDraw2_FlipToGDISurface(dd);
    caps.dwCaps = DDSCAPS_VIDEOMEMORY;
    if (IDirectDraw2_GetAvailableVidMem(dd, &caps, &total, &free) != DD_OK)
        total = free = 0;
    memset(&desc, 0, sizeof desc);
    desc.dwSize = sizeof desc;
    desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    desc.dwWidth = 0x400;
    desc.dwHeight = 10;
    desc.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;
    if (IDirectDraw2_CreateSurface(dd, &desc, &s, NULL) != DD_OK)
        return;
    memset(&desc, 0, sizeof desc);
    desc.dwSize = sizeof desc;
    if (IDirectDrawSurface_Lock(s, NULL, &desc, DDLOCK_WAIT, NULL) == DD_OK)
        return; /* original: a 0x400-wide test surface that locks is left as it is */
    {
        DWORD pitch = (DWORD)desc.lPitch;
        RECT r;
        DDBLTFX fx;
        HRESULT e;
        IDirectDrawSurface_Unlock(s, NULL);
        IDirectDrawSurface_Release(s);
        desc.dwSize = sizeof desc;
        desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        desc.dwWidth = pitch;
        desc.dwHeight = free / pitch;
        desc.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;
        if (IDirectDraw2_CreateSurface(dd, &desc, &s, NULL) != DD_OK)
            return;
        SetRect(&r, 0, 0, (int)desc.dwWidth, (int)desc.dwHeight);
        memset(&fx, 0, sizeof fx);
        fx.dwSize = 100;
        while ((e = IDirectDrawSurface_Blt(s, &r, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx)) != DD_OK)
            if (e != DDERR_SURFACEBUSY)
                return;
        IDirectDrawSurface_Release(s);
    }
}

/* ---- palette ---- */

void __stdcall SpriteCreateDirectDrawPalette(uint8_t *hsl)
{
    g_hsl = hsl; /* original: does nothing else */
}

void __stdcall SpriteReleaseDirectDrawPalette(uint8_t *hsl)
{
    g_hsl = hsl;
}

void __cdecl palette_attach(void)
{
    DisplayDD *d = DD();
    if (!d || !d->front || !d->palette)
        return;
    if (IDirectDrawSurface_SetPalette(d->front, d->palette) == DDERR_SURFACELOST && restored(d->front))
        IDirectDrawSurface_SetPalette(d->front, d->palette);
}

void __cdecl palette_release(void)
{
    DisplayDD *d = DD();
    if (d && d->palette) {
        IDirectDrawPalette_Release(d->palette);
        d->palette = NULL;
    }
}

/* Creates the DirectDraw palette or loads the engine's colors into it */
void __cdecl palette_apply(void)
{
    DisplayDD *d = DD();
    HRESULT e;
    if (!d || !d->front)
        return;
    e = IDirectDrawSurface_IsLost(d->front);
    if (e == DDERR_SURFACELOST && !restored(d->front))
        return;
    if (!d->palette) {
        if (IDirectDraw2_CreatePalette(d->dd2, DDPCAPS_8BIT | DDPCAPS_ALLOW256,
                                       (PALETTEENTRY *)(g_hsl + HSL_PALETTE), &d->palette, NULL) != DD_OK)
            return;
    } else {
        IDirectDrawPalette_SetEntries(d->palette, 0, 0, 256, (PALETTEENTRY *)(g_hsl + HSL_PALETTE));
    }
    if (!H(uint32_t, HSL_PAINTING))
        palette_changed(g_hsl);
    palette_attach();
}

/* The game window lost the focus: give Windows its 20 system colors back */
void __cdecl display_lost_focus(uint8_t *hsl)
{
    DisplayDD *d;
    g_hsl = hsl;
    if (!H(uint32_t, HSL_DIRECTDRAW)) {
        SpriteRealizePalette2(hsl, 1);
    } else {
        d = DD();
        if (!d)
            return;
        if (d->palette) {
            IDirectDrawPalette_SetEntries(d->palette, 0, 0, 10, (PALETTEENTRY *)(hsl + HSL_SYSPAL_LOW));
            IDirectDrawPalette_SetEntries(d->palette, 0, 0xf6, 10, (PALETTEENTRY *)(g_hsl + HSL_SYSPAL_HIGH));
        }
        if (d->dd2)
            IDirectDraw2_FlipToGDISurface(d->dd2);
    }
    if (H(HWND, HSL_WND) && IsWindow(H(HWND, HSL_WND))) {
        DrawMenuBar(H(HWND, HSL_WND));
        RedrawWindow(H(HWND, HSL_WND), NULL, NULL, RDW_FRAME);
    }
}

void __cdecl display_got_focus(uint8_t *hsl)
{
    DisplayDD *d;
    g_hsl = hsl;
    if (!H(uint32_t, HSL_DIRECTDRAW)) {
        SpriteRealizePalette2(hsl, 0);
    } else {
        d = DD();
        if (!d)
            return;
        if (d->palette)
            IDirectDrawPalette_SetEntries(d->palette, 0, 0, 256, (PALETTEENTRY *)(hsl + HSL_PALETTE));
    }
    if (H(HWND, HSL_WND) && IsWindow(H(HWND, HSL_WND))) {
        DrawMenuBar(H(HWND, HSL_WND));
        RedrawWindow(H(HWND, HSL_WND), NULL, NULL, RDW_FRAME);
    }
}

/* Out: video memory and the number of buffers (2 or 3); 0 when not full screen */
int __stdcall display_get_memory(uint8_t *hsl, uint32_t *total, uint32_t *free, uint32_t *buffers)
{
    g_hsl = hsl;
    *buffers = *free = *total = 0;
    if (!H(uint32_t, HSL_DIRECTDRAW) || !DD())
        return 0;
    *buffers = DD()->triple ? 3 : 2;
    dd_video_memory(total, free);
    return 1;
}

/* ---- the back buffer as the screen bitmap ---- */

static void lock_back(DDSURFACEDESC *desc)
{
    DisplayDD *d = DD();
    memset(desc, 0, sizeof *desc);
    desc->dwSize = sizeof *desc;
    for (;;) {
        HRESULT e = IDirectDrawSurface_Lock(d->back, NULL, desc, DDLOCK_WAIT, NULL);
        if (e == DD_OK || e != DDERR_SURFACELOST || !restored(d->front))
            break;
    }
}

static void unlock_back(void)
{
    DisplayDD *d = DD();
    for (;;) {
        HRESULT e = IDirectDrawSurface_Unlock(d->back, NULL);
        if (e == DD_OK || e != DDERR_SURFACELOST || !restored(d->front))
            break;
    }
}

static void screen_dib_points_at(DDSURFACEDESC *desc)
{
    uint8_t *dib = H(uint8_t *, HSL_SCREEN_DIB);
    H(int32_t, HSL_PITCH) = desc->lPitch;
    HS(int32_t, dib, 8) = H(int32_t, HSL_PITCH);
    HS(int32_t, dib, 4) = desc->lPitch;
    HS(void *, dib, 0x20) = desc->lpSurface;
}

static void screen_dib_unlocked(void)
{
    uint8_t *dib = H(uint8_t *, HSL_SCREEN_DIB);
    HS(void *, dib, 0x20) = NULL;
    HS(int32_t, dib, 4) = 0;
    HS(int32_t, dib, 8) = 0;
}

void __cdecl back_lock(void)
{
    DDSURFACEDESC desc;
    if (!DD())
        return;
    lock_back(&desc);
    screen_dib_points_at(&desc);
}

void __cdecl back_unlock(void)
{
    if (!DD())
        return;
    unlock_back();
    screen_dib_unlocked();
}

/* In blit mode (no flipping) the original then also copies rect to the front; it never
 * runs in the game, which flips (and the original asserts there). */
void __cdecl back_unlock_rect(int32_t *rect)
{
    DisplayDD *d = DD();
    if (!d)
        return;
    unlock_back();
    if (!H(uint32_t, HSL_FLIP)) {
        HRESULT e;
        rect[0] += H(int32_t, HSL_ORIGIN_X);
        rect[1] += H(int32_t, HSL_ORIGIN_Y);
        rect[2] += H(int32_t, HSL_ORIGIN_X);
        rect[3] += H(int32_t, HSL_ORIGIN_Y);
        do {
            e = IDirectDrawSurface_BltFast(d->front, (DWORD)rect[0], (DWORD)rect[1], d->back, (RECT *)rect,
                                           DDBLTFAST_WAIT);
        } while (e == DDERR_SURFACELOST && restored(d->front));
    }
    screen_dib_unlocked();
}

/* Moves rect (screen coordinates) of a surface by (dx, dy) within itself */
static void scroll_surface(IDirectDrawSurface *s, IDirectDrawSurface *restore, int dx, int dy, const int32_t *rect)
{
    int x0 = (int)(H(uint32_t, HSL_WIDTH) >> 1) + rect[0] - 1;
    int y0 = (int)(H(uint32_t, HSL_HEIGHT) >> 1) + rect[1];
    int w, h, sx, sy;
    RECT src;
    HRESULT e;
    if (dy > 0)
        y0--;
    w = rect[2] - rect[0] + (dx < 0 ? dx : -dx);
    h = rect[3] - rect[1] + (dy < 0 ? dy : -dy) - 1;
    sx = dx < 0 ? x0 - dx : x0;
    sy = dy < 0 ? y0 - dy : y0;
    SetRect(&src, sx, sy, sx + w, sy + h);
    for (;;) {
        e = IDirectDrawSurface_BltFast(s, (DWORD)(dx + sx), (DWORD)(sy + dy), s, &src, DDBLTFAST_WAIT);
        if (e == DD_OK)
            return;
        if (e == DDERR_SURFACELOST && !restored(restore))
            return;
        if (e != DDERR_WASSTILLDRAWING)
            return;
    }
}

void __cdecl back_scroll(int dx, int dy, int32_t *rect)
{
    scroll_surface(DD()->back, DD()->front, dx, dy, rect);
}

void __cdecl surf8c_scroll(int dx, int dy, int32_t *rect)
{
    scroll_surface(DD()->surf8c, DD()->surf8c, dx, dy, rect);
}

/* Fills the back buffer at pos (w x h) from the background: a color, or the background
 * surface; then locks the back buffer as the screen bitmap */
void __cdecl back_fill(int32_t *pos, int w, int h)
{
    DisplayDD *d = DD();
    DDSURFACEDESC desc;
    int x, y, sx, sy;
    if (!d)
        return;
    x = H(int32_t, HSL_ORIGIN_X) + pos[0];
    y = H(int32_t, HSL_ORIGIN_Y) + pos[1];
    sx = x;
    sy = y;
    if (!H(void *, HSL_BACKGROUND)) {
        if (H(int32_t, HSL_FILL) != -1) {
            RECT r;
            HRESULT e;
            if (!H(uint32_t, HSL_RESIZED)) {
                sy = sx = 0; /* original: fills at 0,0 until the first resize */
            } else {
                sx = pos[0] - H(int32_t, HSL_VIEW);
                sy = pos[1] - H(int32_t, HSL_VIEW + 4);
            }
            SetRect(&r, sx, sy, w + sx, h + sy);
            do {
                fill_fx(d, H(int32_t, HSL_FILL) == -1 ? 0 : H(uint32_t, HSL_FILL));
                e = IDirectDrawSurface_Blt(d->back, &r, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &d->fx);
            } while (e != DD_OK && (e != DDERR_SURFACELOST || restored(d->front)));
        }
    } else if (d->background) {
        RECT src;
        HRESULT e;
        SetRect(&src, x, y, x + w, y + h);
        while ((e = IDirectDrawSurface_BltFast(d->back, (DWORD)sx, (DWORD)sy, d->background, &src, 0)) != DD_OK) {
            if (e == DDERR_SURFACELOST &&
                (IDirectDrawSurface_Restore(d->background) < 0 || IDirectDrawSurface_Restore(d->front) < 0))
                break;
        }
    }
    lock_back(&desc);
    screen_dib_points_at(&desc);
}

/* ---- view border ---- */

/* A one-pixel frame around rect in the border color */
void __cdecl border_draw(int32_t *rect, DDSURFACEDESC *desc)
{
    int x0 = H(int32_t, HSL_ORIGIN_X) + rect[0], x1 = H(int32_t, HSL_ORIGIN_X) + rect[2];
    int y0 = H(int32_t, HSL_ORIGIN_Y) + rect[1], y1 = H(int32_t, HSL_ORIGIN_Y) + rect[3];
    int pitch = desc->lPitch, y;
    uint16_t n;
    uint8_t *p;
    if (x0 < 0)
        x0 = 0;
    if (x1 > H(int32_t, HSL_WIDTH) - 1)
        x1 = H(int32_t, HSL_WIDTH) - 1;
    if (y0 < 0)
        y0 = 0;
    if (y1 > H(int32_t, HSL_HEIGHT) - 1)
        y1 = H(int32_t, HSL_HEIGHT) - 1;
    p = (uint8_t *)desc->lpSurface + y0 * pitch + x0;
    n = (uint16_t)(x1 - x0 + 1);
    memset(p, g_border_color, n);
    p += n + pitch - 1;
    for (y = y0 + 1; y <= y1; y++, p += pitch)
        *p = g_border_color;
    memset(p - (n - 1), g_border_color, n);
    p = p - (n - 1) - pitch;
    for (y = y1 - 1; y >= y0; y--, p -= pitch)
        *p = g_border_color;
}

void __cdecl border_paint(int32_t *rect)
{
    DisplayDD *d = DD();
    DDSURFACEDESC desc;
    memset(&desc, 0, sizeof desc); /* original: only dwSize was set */
    desc.dwSize = sizeof desc;
    for (;;) {
        HRESULT e = IDirectDrawSurface_Lock(d->back, NULL, &desc, DDLOCK_WAIT, NULL);
        if (e == DD_OK || e != DDERR_SURFACELOST || !restored(d->front))
            break;
    }
    border_draw(rect, &desc);
    for (;;) {
        HRESULT e = IDirectDrawSurface_Unlock(d->back, NULL);
        if (e == DD_OK || e != DDERR_SURFACELOST || !restored(d->front))
            return;
    }
}

/* A border appears when the view (vw x vh) is smaller than the screen (w x h) */
int __stdcall display_set_border(uint8_t *hsl, int w, int h, int vw, int vh)
{
    g_hsl = hsl;
    if (vw == w && vh == h) {
        H(uint32_t, HSL_BORDER) = 0;
        return 0;
    }
    H(uint32_t, HSL_BORDER) = 1;
    H(int32_t, HSL_BORDER_RECT) = w < vw ? w : vw;
    H(int32_t, HSL_BORDER_RECT + 4) = h < vh ? h : vh;
    H(int32_t, HSL_BORDER_RECT + 8) = w < vw ? vw : w;
    H(int32_t, HSL_BORDER_RECT + 12) = h < vh ? vh : h;
    return 1;
}

uint32_t __stdcall display_get_b5c(uint8_t *hsl)
{
    g_hsl = hsl;
    return H(uint32_t, HSL_B5C);
}

/* ---- the interface overlay: a separate surface for the bottom panel, with dirty rects ---- */

int __cdecl iface_surface_create(int32_t *rect)
{
    DisplayDD *d = DD();
    DDSURFACEDESC desc;
    RECT r;
    HRESULT e;
    if (!d || !H(void *, HSL_INTERFACE))
        return 0;
    if (d->iface)
        return 1;
    memset(&desc, 0, sizeof desc);
    desc.dwSize = sizeof desc;
    desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    desc.dwWidth = H(uint32_t, HSL_WIDTH);
    desc.dwHeight = (DWORD)(rect[3] - rect[1] + 1);
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
    if (IDirectDraw2_CreateSurface(d->dd2, &desc, &d->iface, NULL) != DD_OK || !d->iface) {
        desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        if (IDirectDraw2_CreateSurface(d->dd2, &desc, &d->iface, NULL) != DD_OK || !d->iface)
            return 0;
    }
    d->iface_h = (int32_t)desc.dwHeight;
    d->iface_w = (int32_t)desc.dwWidth;
    fill_fx(d, 0);
    SetRect(&r, 0, 0, d->iface_w - 1, d->iface_h - 1);
    while ((e = IDirectDrawSurface_Blt(d->iface, &r, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &d->fx)) != DD_OK)
        if (e != DDERR_SURFACEBUSY)
            return 0;
    return 1;
}

void __cdecl iface_surface_release(void)
{
    DisplayDD *d = DD();
    if (!d)
        return;
    while (d->iface && IDirectDrawSurface_Release(d->iface))
        ;
    d->iface = NULL;
}

void __cdecl dirty_reset(void)
{
    H(uint32_t, HSL_DIRTY_N) = 0;
    H(uint32_t, HSL_DIRTY_MAX) = 100;
    if (H(void *, HSL_DIRTY))
        winfree(H(void *, HSL_DIRTY));
    H(void *, HSL_DIRTY) = winmalloc(H(uint32_t, HSL_DIRTY_MAX) << 4, 0);
}

void __cdecl dirty_clear(void)
{
    H(uint32_t, HSL_DIRTY_N) = 0;
}

static void dirty_push(const int32_t *r)
{
    int32_t *rects = H(int32_t *, HSL_DIRTY);
    uint32_t n = H(uint32_t, HSL_DIRTY_N);
    if (n && !memcmp(rects + (n - 1) * 4, r, 16))
        return; /* the same as the last one */
    memcpy(rects + n * 4, r, 16);
    H(uint32_t, HSL_DIRTY_N) = n + 1;
}

/* The interface rectangle of the view, below the play area */
static void view_interface_rect(int32_t *r)
{
    r[0] = H(int32_t, HSL_VIEW);
    r[2] = H(int32_t, HSL_VIEW + 8);
    r[1] = H(int32_t, HSL_VIEW_BOTTOM) + 1;
    r[3] = H(int32_t, HSL_VIEW + 12) - 1;
}

static void iface_create_and_mark(void)
{
    int32_t r[4];
    view_interface_rect(r);
    iface_surface_create(r);
    dirty_reset();
    memcpy(H(int32_t *, HSL_DIRTY) + H(uint32_t, HSL_DIRTY_N) * 4, r, 16);
    H(uint32_t, HSL_DIRTY_N)++;
}

/* A sprite on the interface changed: its old and new rectangles (or their union) */
void __cdecl dirty_add_sprite(uint8_t *s)
{
    HS(uint32_t, s, 0x9c) = 1;
    if (!H(uint32_t, HSL_DIRTY_MAX))
        dirty_reset();
    if (H(uint32_t, HSL_DIRTY_MAX) - 1 <= H(uint32_t, HSL_DIRTY_N)) {
        /* original: a debug assertion at 6000 */
        if (H(uint32_t, HSL_DIRTY_N) > 5999)
            return;
        H(uint32_t, HSL_DIRTY_MAX) += 0x32;
        H(void *, HSL_DIRTY) = winrealloc(H(void *, HSL_DIRTY), H(uint32_t, HSL_DIRTY_MAX) << 4, 0);
    }
    if (!HS(uint32_t, s, 0xe0) || !HS(uint32_t, s, 0xc4)) {
        dirty_push((int32_t *)(s + 0x44));
        dirty_push((int32_t *)(s + 0x54));
    } else {
        RECT u;
        union_rect(&u, (RECT *)(s + 0x44), (RECT *)(s + 0x54));
        dirty_push((int32_t *)&u);
    }
}

void __cdecl dirty_add_if_shown(uint8_t *s)
{
    if (!IsIconic(H(HWND, HSL_WND)))
        dirty_add_sprite(s);
}

/* The sprite list: a ring with its head at hsl[0] and end marker hsl[1] */
#define SPRITES_ON 0x8c4 /* hsl[0x231] */
#define SPRITE_FIRST() HS(uint8_t *, H(uint8_t *, 0), 4)
#define SPRITE_END() H(uint8_t *, 4)

void __cdecl dirty_add_interface_sprites(void)
{
    if (!H(uint32_t, SPRITES_ON))
        return;
    for (uint8_t *s = SPRITE_FIRST(); s != SPRITE_END(); s = HS(uint8_t *, s, 4)) {
        if (!HS(uint32_t, s, 0xe4) && H(int32_t, 0x920) - 2 <= HS(int32_t, s, 0x48)) {
            memcpy(s + 0x54, s + 0x44, 16);
            dirty_add_sprite(s);
        }
    }
}

void __cdecl sprites_clear_dirty_flag(void)
{
    if (!H(uint32_t, SPRITES_ON))
        return;
    for (uint8_t *s = SPRITE_FIRST(); s != SPRITE_END(); s = HS(uint8_t *, s, 4))
        HS(uint32_t, s, 0x9c) = 0;
}

void __cdecl iface_lock(void)
{
    DisplayDD *d = DD();
    HRESULT e;
    if (!d)
        return;
    if (!d->iface)
        iface_create_and_mark();
    memset(&g_iface_desc, 0, sizeof g_iface_desc);
    g_iface_desc.dwSize = sizeof g_iface_desc;
    while ((e = IDirectDrawSurface_Lock(d->iface, NULL, &g_iface_desc, DDLOCK_WAIT, NULL)) != DD_OK) {
        if (e == DDERR_SURFACELOST) {
            if (!restored(d->iface))
                break;
            dirty_add_interface_sprites();
        }
    }
    if (g_iface_desc.lpSurface) {
        uint8_t *dib;
        if (!H(void *, HSL_IFACE_DIB))
            H(uint8_t *, HSL_IFACE_DIB) = SpriteDibCreate(H(uint32_t, HSL_WIDTH), 0, 0, 8);
        dib = H(uint8_t *, HSL_IFACE_DIB);
        H(int32_t, HSL_PITCH) = g_iface_desc.lPitch;
        HS(int32_t, dib, 8) = H(int32_t, HSL_PITCH);
        HS(int32_t, dib, 4) = g_iface_desc.lPitch;
        HS(int32_t, dib, 0xc) = d->iface_h;
        HS(void *, dib, 0x20) = g_iface_desc.lpSurface;
        HS(uint32_t, dib, 0x10) |= 0x40;
    }
}

void __cdecl iface_unlock(void)
{
    DisplayDD *d = DD();
    if (!d)
        return;
    for (;;) {
        HRESULT e = IDirectDrawSurface_Unlock(d->iface, NULL);
        if (e == DD_OK || e != DDERR_SURFACELOST || !restored(d->iface))
            break;
        dirty_add_interface_sprites();
    }
    HS(void *, H(uint8_t *, HSL_IFACE_DIB), 0x20) = NULL;
}

/* Copies the interface surface below the play area of the back buffer */
void __cdecl iface_blit(void)
{
    DisplayDD *d = DD();
    RECT src;
    if (!d || !d->iface)
        return;
    SetRect(&src, 0, 0, d->iface_w, d->iface_h);
    for (;;) {
        HRESULT e = IDirectDrawSurface_BltFast(d->back, 0,
                                               (DWORD)(H(int32_t, HSL_ORIGIN_Y) + H(int32_t, HSL_VIEW_BOTTOM) + 1),
                                               d->iface, &src, d->triple ? 0 : DDBLTFAST_WAIT);
        if (e == DD_OK && !g_no_blt_wait && !d->triple) {
            while (IDirectDrawSurface_GetBltStatus(d->back, DDGBS_ISBLTDONE) != DD_OK)
                ;
            return;
        }
        if (e != DDERR_SURFACELOST)
            return;
        if (IDirectDrawSurface_IsLost(d->back) == DDERR_SURFACELOST && !restored(d->back))
            return;
        if (IDirectDrawSurface_IsLost(d->iface) == DDERR_SURFACELOST) {
            if (!restored(d->iface))
                return;
            dirty_add_interface_sprites();
        }
    }
}

void __cdecl iface_update(void)
{
    iface_blit();
    sprites_clear_dirty_flag();
    dirty_clear();
}

int __cdecl display_true(void) { return 1; }
void __cdecl display_nop1(void) {}
int __cdecl display_false(void) { return 0; }
void __cdecl display_nop2(void) {}
void __stdcall display_nop4(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    (void)a, (void)b, (void)c, (void)d;
}

void __stdcall SpriteRefreshInterfaceArea(uint8_t *hsl, uint8_t *sprite)
{
    g_hsl = hsl;
    if (!sprite)
        dirty_add_interface_sprites();
    else
        dirty_add_sprite(sprite);
}

/* ---- presenting a frame ---- */

/* Copies a sprite's rectangle from the back buffer to one of the surfaces at 0x94 */
void __cdecl blit_sprite_rect(uint8_t *s)
{
    DisplayDD *d = DD();
    RECT r;
    SetRect(&r, HS(int32_t, s, 0x44) + H(int32_t, HSL_ORIGIN_X), HS(int32_t, s, 0x48) + H(int32_t, HSL_ORIGIN_Y),
            HS(int32_t, s, 0x4c) + H(int32_t, HSL_ORIGIN_X), HS(int32_t, s, 0x50) + H(int32_t, HSL_ORIGIN_Y));
    IDirectDrawSurface_BltFast(d->surf94[g_blit_surface], 0, 0, d->back, &r, 0);
}

/* Draws the view's sprites, then the interface sprites into the interface surface */
void __cdecl display_draw_sprites(void)
{
    H(uint32_t, HSL_REDRAW_ALL) = 0;
    draw_view_sprites(g_hsl + (H(uint32_t, HSL_IFACE_ON) ? HSL_VIEW2 : HSL_VIEW), 0x80000000);
    H(uint32_t, HSL_REDRAW_ALL) = 1;
    if (H(uint32_t, HSL_IFACE_ON) && H(uint32_t, HSL_DIRTY_N)) {
        iface_lock();
        if (H(void *, HSL_IFACE_DIB) && HS(void *, H(uint8_t *, HSL_IFACE_DIB), 0x20))
            for (uint32_t i = 0; i < H(uint32_t, HSL_DIRTY_N); i++)
                draw_iface_rect(H(int32_t *, HSL_DIRTY) + i * 4);
        iface_unlock();
    }
}

/* Shows the frame: flip (or blit) the back buffer. Returns the milliseconds it took. */
int __stdcall SpriteFlipPages(uint8_t *hsl)
{
    DisplayDD *d;
    DWORD start = 0;
    int took;
    HRESULT e;
    g_hsl = hsl;
    if (!H(uint32_t, HSL_PAINTING) || H(uint32_t, HSL_INACTIVE))
        return 0;
    d = DD();
    if (!d->front)
        return 0;
    if (!H(uint32_t, HSL_FLIP)) {
        RECT r;
        SetRect(&r, 0, 0, H(int32_t, HSL_WIDTH), H(int32_t, HSL_HEIGHT));
        if (H(uint32_t, HSL_BORDER))
            border_paint((int32_t *)(g_hsl + HSL_BORDER_RECT));
        while ((e = IDirectDrawSurface_BltFast(d->front, 0, 0, d->back, &r, DDBLTFAST_WAIT)) != DD_OK) {
            if ((e == DDERR_SURFACELOST && !restored(d->front)) || e != DDERR_WASSTILLDRAWING)
                break;
        }
        took = 0;
    } else {
        int32_t was_dirty;
        if (H(uint32_t, HSL_IFACE_ON) && !H(uint32_t, HSL_B5C) && !d->iface)
            iface_create_and_mark();
        if (!g_flip_late) {
            e = IDirectDrawSurface_Flip(d->front, NULL, 0);
            if (e == DDERR_SURFACELOST) {
                if (!restored(d->front))
                    return 0;
                e = DD_OK;
            } else if (e == DDERR_NOEXCLUSIVEMODE) {
                IDirectDraw2_SetCooperativeLevel(d->dd2, H(HWND, HSL_WND),
                                                 DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT);
                return 0;
            }
            if (e != DD_OK)
                return 0;
            g_flips++;
        }
        start = timeGetTime();
        display_draw_sprites();
        /* the interface goes into every buffer once after a change */
        was_dirty = (int32_t)H(uint32_t, HSL_DIRTY_N);
        if (H(uint32_t, HSL_IFACE_ON) && (was_dirty || g_flips < 3u - (d->triple == 0))) {
            iface_update();
            if (was_dirty)
                g_flips = 0;
        }
        if (H(uint32_t, HSL_BORDER))
            border_paint((int32_t *)(g_hsl + HSL_BORDER_RECT));
        if (g_flip_late) {
            e = IDirectDrawSurface_Flip(d->front, NULL, DDFLIP_WAIT);
            if (e == DDERR_SURFACELOST && !restored(d->front))
                return 0;
            if (e != DD_OK && e != DDERR_SURFACELOST)
                return 0;
            g_flips++;
        }
        took = (int)(timeGetTime() - start);
    }
    return took ? took : 1;
}

/* Waits until a flip is possible and shows the frame (twice with triple buffering) */
void __stdcall display_flip_now(uint8_t *hsl)
{
    DisplayDD *d;
    g_hsl = hsl;
    d = DD();
    if (!d || !d->front)
        return;
    while (IDirectDrawSurface_GetFlipStatus(d->front, DDGFS_CANFLIP) == DDERR_WASSTILLDRAWING)
        ;
    SpriteFlipPages(hsl);
    if (d->triple) {
        while (IDirectDrawSurface_GetFlipStatus(d->front, DDGFS_CANFLIP) == DDERR_WASSTILLDRAWING)
            ;
        SpriteFlipPages(hsl);
    }
}

/* Restores the surfaces after they were lost, recreating them if needed */
int __cdecl surfaces_restore(void)
{
    DisplayDD *d = DD();
    if (!d)
        return 0;
    if (IDirectDrawSurface_IsLost(d->front) == DDERR_SURFACELOST && !restored(d->front)) {
        dd_release_surfaces();
        dd_create_surfaces(H(int32_t, HSL_WIDTH), H(int32_t, HSL_HEIGHT));
    }
    return 1;
}

/* The game window got (active) or lost the focus in full-screen mode */
int __stdcall display_activate(uint8_t *hsl, int active)
{
    DisplayDD *d;
    char msg[80];
    g_hsl = hsl;
    d = DD();
    if (g_dd_borrowed)
        return 1;
    wsprintfA(msg, "sdda:cur=%d new=%d\r\n", H(uint32_t, HSL_INACTIVE), active);
    OutputDebugStringA(msg);
    if (active) {
        OutputDebugStringA("sdda: setres\r\n");
        dd_set_mode(H(uint32_t, HSL_WIDTH), H(uint32_t, HSL_HEIGHT));
        surfaces_restore();
        if (H(uint32_t, HSL_IFACE_ON) && !H(uint32_t, HSL_B5C)) {
            dirty_reset();
            dirty_add_interface_sprites();
        }
        if (!paint_three_black(H(uint32_t, HSL_WIDTH), H(uint32_t, HSL_HEIGHT)))
            return 0;
        if (H(void *, HSL_BACKGROUND) != H(void *, HSL_INTERFACE))
            set_background(H(void *, HSL_BACKGROUND));
        SpriteRealizePalette2(g_hsl, 0);
        IDirectDrawPalette_SetEntries(d->palette, 0, 0, 256, (PALETTEENTRY *)(g_hsl + HSL_PALETTE));
    }
    H(uint32_t, HSL_INACTIVE) = !active;
    return 1;
}

/* ---- palette (0x100517c0-0x100524c0) ---- */

#define PAL_FLAG(i) H(uint8_t, HSL_PALETTE + (i) * 4 + 3)

static void dib_colors_are_indices(void)
{
    H(uint32_t, HSL_DIB_INDICES) = 1;
    for (uint32_t i = 0; i < 0x100; i++)
        H(uint16_t, HSL_DIB_COLORS + i * 2) = (uint16_t)i;
}

/* Deletes the GDI palette */
void __cdecl palette_delete(void)
{
    if (!H(HPALETTE, HSL_HPALETTE))
        return;
    SelectPalette(H(HDC, HSL_DC), GetStockObject(DEFAULT_PALETTE), FALSE);
    RealizePalette(H(HDC, HSL_DC));
    DeleteObject(H(HPALETTE, HSL_HPALETTE));
    H(HPALETTE, HSL_HPALETTE) = NULL;
}

/* GDI palette with Windows' 20 static colors at both ends */
int __cdecl palette_create(void)
{
    if (g_use_ddraw1)
        return palette_create_reserved();
    memcpy(&H(uint8_t, HSL_PALETTE), g_static_low, 0x28);
    memcpy(&H(uint8_t, HSL_PALETTE_HIGH), g_static_high, 0x28);
    for (uint32_t i = 0; i < 0x100; i++)
        PAL_FLAG(i) = 0;
    dib_colors_are_indices();
    palette_delete();
    H(HPALETTE, HSL_HPALETTE) = CreatePalette((LOGPALETTE *)(g_hsl + HSL_LOGPALETTE));
    if (!H(HPALETTE, HSL_HPALETTE))
        return 0;
    SpriteRealizePalette(g_hsl);
    if (H(uint32_t, HSL_DIRECTDRAW))
        palette_apply();
    return 1;
}

/* GDI palette whose colors 10-245 can be animated */
int __cdecl palette_create_reserved(void)
{
    for (uint32_t i = 0; i < 0x100; i++)
        PAL_FLAG(i) = (i < 10 || i > 0xf5) ? 0 : PC_RESERVED;
    dib_colors_are_indices();
    palette_delete();
    H(HPALETTE, HSL_HPALETTE) = CreatePalette((LOGPALETTE *)(g_hsl + HSL_LOGPALETTE));
    if (!H(HPALETTE, HSL_HPALETTE))
        return 0;
    if (H(uint32_t, HSL_DIRECTDRAW))
        palette_apply();
    return 1;
}

int __stdcall SpriteChangePalette(uint8_t *hsl, uint32_t first, uint32_t n, const uint32_t *colors, int animate,
                                  int same)
{
    g_hsl = hsl;
    if (first + n - 1 > 0x100)
        return 0;
    for (uint32_t i = first; i < first + n; i++) {
        H(uint32_t, HSL_PALETTE + i * 4) = *colors;
        PAL_FLAG(i) = (!g_use_ddraw1 && H(uint32_t, HSL_DIRECTDRAW)) ? 0 : PC_RESERVED;
        if (!same)
            colors++;
    }
    if (!H(uint32_t, HSL_PALETTE_DEVICE)) {
        palette_to_dib(first, first + n, animate);
    } else if (!H(HPALETTE, HSL_HPALETTE) || !animate) {
        DisplayDD *d = DD();
        if (d && d->palette)
            IDirectDrawPalette_SetEntries(d->palette, 0, 0, 256, (PALETTEENTRY *)(g_hsl + HSL_PALETTE));
    } else {
        AnimatePalette(H(HPALETTE, HSL_HPALETTE), first, n, (PALETTEENTRY *)(g_hsl + HSL_PALETTE + first * 4));
    }
    return 1;
}

/* Copies palette entries first..end-1 into the screen DIB's colors */
void __stdcall palette_to_dib(uint32_t first, uint32_t end, uint32_t unused)
{
    (void)unused;
    uint8_t *q = g_hsl + HSL_DIB_COLORS + first * 4;
    H(uint32_t, HSL_DIB_INDICES) = 0;
    for (uint32_t i = first; i < end; i++, q += 4) {
        q[2] = H(uint8_t, HSL_PALETTE + i * 4);
        q[1] = H(uint8_t, HSL_PALETTE + i * 4 + 1);
        q[0] = H(uint8_t, HSL_PALETTE + i * 4 + 2);
    }
}

void __stdcall SpriteGetPalette(uint8_t *hsl, void *out, uint32_t first, uint32_t n)
{
    g_hsl = hsl;
    memcpy(out, hsl + HSL_PALETTE + first * 4, n * 4);
}

/* returns 1 when there is no GDI palette (the colors are the real ones) */
int __stdcall SpriteGetRgbPalette(uint8_t *hsl, void *out, uint32_t first, uint32_t n)
{
    g_hsl = hsl;
    memcpy(out, hsl + HSL_DIB_COLORS + first * 4, n * 4);
    return H(HPALETTE, HSL_HPALETTE) == NULL;
}

/* Shows the palette faded to step/steps (assembly helper of the original) */
int __stdcall SpriteFadePalette(uint8_t *hsl, uint32_t step, uint32_t steps, uint32_t unused1, uint32_t unused2)
{
    PALETTEENTRY faded[256];
    (void)unused1, (void)unused2;
    g_hsl = hsl;
    if (!steps)
        steps = 1;
    if (steps < step || steps > 0xff || !H(HPALETTE, HSL_HPALETTE))
        return 0;
    uint32_t first = H(uint32_t, HSL_DIRECTDRAW) == 0; /* GDI: leave the static colors alone */
    uint32_t n = first ? 0xfe : 0x100;
    uint32_t args[5] = {(uint32_t)(uintptr_t)faded, (uint32_t)(uintptr_t)(hsl + HSL_PALETTE + first * 4), step,
                        steps, n};
    orig_stdcall_unsafe(ORIG_PTR(0x10062ed8), 5, args);
    if (!H(uint32_t, HSL_DIRECTDRAW)) {
        AnimatePalette(H(HPALETTE, HSL_HPALETTE), first, n, faded);
    } else {
        DisplayDD *d = DD();
        if (d && d->palette)
            IDirectDrawPalette_SetEntries(d->palette, 0, first, n, faded);
    }
    return 1;
}

UINT __stdcall SpriteRealizePalette(uint8_t *hsl)
{
    g_hsl = hsl;
    if (!hsl || !H(HPALETTE, HSL_HPALETTE))
        return 0;
    return palette_realize(H(HDC, HSL_DC), H(HPALETTE, HSL_HPALETTE), 0);
}

UINT __stdcall SpriteRealizePalette2(uint8_t *hsl, int background)
{
    g_hsl = hsl;
    if (!hsl || !H(HPALETTE, HSL_HPALETTE))
        return 0;
    return palette_realize(H(HDC, HSL_DC), H(HPALETTE, HSL_HPALETTE), background);
}

HPALETTE __stdcall SpriteSelectPalette(uint8_t *hsl, HDC dc)
{
    g_hsl = hsl;
    if (!H(HPALETTE, HSL_HPALETTE))
        return NULL;
    return SelectPalette(dc, H(HPALETTE, HSL_HPALETTE), FALSE);
}

int __stdcall SpriteSetPalette(uint8_t *hsl, const void *colors)
{
    g_hsl = hsl;
    palette_delete();
    if (!colors)
        return 0;
    memcpy(&H(uint8_t, HSL_PALETTE), colors, 0x400);
    if (H(uint32_t, HSL_A0C) && !dib_section_colors())
        return 0;
    H(uint32_t, HSL_LOGPALETTE) = 0x1000300; /* version 0x300, 256 entries */
    if (H(uint32_t, HSL_PALETTE_DEVICE))
        palette_create();
    if (!H(HPALETTE, HSL_HPALETTE))
        palette_to_dib(0, 0x100, 1);
    return H(HPALETTE, HSL_HPALETTE) != NULL;
}

uint32_t __stdcall palette_get_b80(uint8_t *hsl)
{
    g_hsl = hsl;
    return H(uint32_t, HSL_B80);
}

uint32_t __stdcall palette_get_b80_2(uint32_t unused1, uint32_t unused2)
{
    (void)unused1, (void)unused2;
    return H(uint32_t, HSL_B80);
}

int __cdecl palette_false(void) { return 0; }
int __cdecl palette_true(void) { return 1; }

UINT __cdecl palette_realize(HDC dc, HPALETTE pal, int background)
{
    /* (a debug assertion on dc left out; the 0xb80 mode only adds calls that do nothing) */
    if (H(uint32_t, HSL_B80) && !background && !palette_true())
        return (UINT)-1;
    if (!SelectPalette(dc, pal, background))
        return (UINT)-1;
    return RealizePalette(dc);
}

/* Loads all-black colors into the DirectDraw palette, keeping the flags */
void __stdcall palette_changed(uint8_t *hsl)
{
    uint8_t black[0x400];
    g_hsl = hsl;
    DisplayDD *d = DD();
    if (!d || !d->palette)
        return;
    for (int i = 0; i < 0x100; i++) {
        black[i * 4] = black[i * 4 + 1] = black[i * 4 + 2] = 0;
        black[i * 4 + 3] = PAL_FLAG(i);
    }
    IDirectDrawPalette_SetEntries(d->palette, 0, 0, 256, (PALETTEENTRY *)black);
}

void __stdcall palette_set_painting(uint8_t *hsl, int painting, int apply)
{
    g_hsl = hsl;
    H(uint32_t, HSL_PAINTING) = painting;
    DisplayDD *d = DD();
    if (painting && apply && d && d->palette)
        IDirectDrawPalette_SetEntries(d->palette, 0, 0, 256, (PALETTEENTRY *)(g_hsl + HSL_PALETTE));
}
