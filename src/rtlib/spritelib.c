/* The sprite engine's library state and sprite display: creating the engine state (HSL),
 * exit and reset, the screen modes of the drawing bitmap, and showing, hiding, moving and
 * redrawing sprites. Rewritten from the original (0x10055321-0x100562d9 and
 * 0x10057662-0x10057f40; the drawing pass between them is still original). */
#include "orig.h"
#include "blit.h"
#define intersect_rect(o, a, b) rect_intersect((int32_t *)(o), (const int32_t *)(a), (const int32_t *)(b))
#define union_rect(o, a, b) rect_union((int32_t *)(o), (const int32_t *)(a), (const int32_t *)(b))
#include "draw.h"
#include "spritelib.h"
#include "display.h"
#include "queue.h"
#include "sprite.h"
#include "view.h"
#include "utils.h"

#define g_hsl G(uint8_t *, 0x10089e24)
#define H(type, off) (*(type *)(g_hsl + (off)))
#define S(type, s, off) (*(type *)((uint8_t *)(s) + (off)))

#define HSL_SIZE 0xff4
#define g_static_hsl ((uint8_t *)ORIG_PTR(0x10088e28)) /* the first engine state */
#define g_static_used G(uint32_t, 0x10089e20)
#define g_draw_rect G(RECT, 0x10089e58)                /* area of the current drawing pass */
#define g_allow_triple G(uint32_t, 0x1007b29c)
#define g_no_blt_wait G(uint32_t, 0x1007b2a0)
#define g_flip_late G(uint32_t, 0x1007b2a4)
#define g_wait_drawing G(uint32_t, 0x1007b2a8)

/* HSL fields */
#define HSL_HEAD 0x000
#define HSL_TAIL 0x004
#define HSL_CURRENT 0x008
#define HSL_BITMAPINFO 0x02c    /* BITMAPINFOHEADER of the screen bitmap */
#define HSL_PAINT_HOOK 0x028
#define HSL_SCREEN_MODE 0x458
#define HSL_DC 0x468
#define HSL_HPALETTE 0x46c
#define HSL_HWND 0x470
#define HSL_BACKGROUND 0x474
#define HSL_SCREEN_DIB 0x478
#define HSL_TILES 0x47c
#define HSL_TILE_MAP 0x480
#define HSL_SCREEN_SIZE 0x454
#define HSL_ZOOM 0x88c
#define HSL_ZOOM_W 0x890
#define HSL_ZOOM_H 0x894
#define HSL_FILL 0x898
#define HSL_WIDTH 0x8a4
#define HSL_HEIGHT 0x8a8
#define HSL_BITS 0x8ac
#define HSL_DESKTOP_W 0x8b0
#define HSL_DESKTOP_H 0x8b4
#define HSL_DESKTOP_BITS 0x8b8
#define HSL_DEPTH 0x8bc
#define HSL_8C0 0x8c0
#define HSL_COUNT 0x8c4
#define HSL_ANIMATED 0x8c8
#define HSL_SCROLL_X 0x8d4
#define HSL_SCROLL_Y 0x8d8
#define HSL_AUX_SCROLL 0x8dc
#define HSL_SCROLL_LIMITS 0x8e4
#define HSL_VIEW 0x904
#define HSL_VIEW2 0x914
#define HSL_VIEW_BOTTOM 0x920
#define HSL_CLIP_BOX 0x924
#define HSL_SCROLL_BOX 0x934
#define HSL_RESIZED 0x94c
#define HSL_TOP_DOWN 0x950
#define HSL_DIRECTDRAW 0x954
#define HSL_9F0 0x9f0
#define HSL_PALETTE_DEVICE 0x9fc
#define HSL_REDRAW_ALL 0xa04
#define HSL_ZOOMED 0xa08
#define HSL_DIB_SECTION 0xa0c
#define HSL_BG_X 0xa18
#define HSL_BG_Y 0xa1c
#define HSL_QUALITY_B74 0xb74
#define HSL_B78 0xb78
#define HSL_IFACE_DIB 0xb58
#define HSL_B5C 0xb5c
#define HSL_B60 0xb60
#define HSL_OVERLAY 0xb64
#define HSL_IFACE_ON 0xb68
#define HSL_B80 0xb80
#define HSL_B84 0xb84
#define HSL_SYS_COLORS 0xb88    /* GetSysColor 0-24 */
#define HSL_SYSPAL_LOW 0xbec
#define HSL_SYSPAL_HIGH 0xfc4
#define HSL_SAFE_MODE 0xfec
#define HSL_PAINTING 0xff0

/* sprite fields */
#define SP_BITMAP 0x18
#define SP_PARTS 0x20
#define SP_CELS 0x24
#define SP_QUEUE_CUR 0x2c
#define SP_INFO 0x34
#define SP_X 0x3c
#define SP_RECT 0x44
#define SP_OLD_RECT 0x54
#define SP_NEXT_MOVE 0x68
#define SP_MOTION 0x70
#define SP_SHOWN_CEL 0x74
#define SP_CEL 0x80
#define SP_NCELS 0x8c
#define SP_NPARTS 0x90
#define SP_LEVEL 0x98
#define SP_MOVED 0xb4
#define SP_EXTERNAL_DRAW 0xbc
#define SP_OVERLAPS 0xc4   /* new bounds overlap the old ones */
#define SP_CC 0xcc
#define SP_PRIORITY 0xd4
#define SP_POSITIONED 0xdc
#define SP_DRAWN 0xe0      /* drawn at SP_OLD_RECT */
#define SP_SCROLLS 0xe4
#define SP_UPDATED 0xe8
#define SP_VISIBLE 0xec

#define REDRAW_ALL 0x80000000u




/* ---- the engine state ---- */

void __stdcall SpriteExitLibrary(uint8_t *hsl)
{
    g_hsl = hsl;
    if (H(HDC, HSL_DC)) {
        GetDeviceCaps(H(HDC, HSL_DC), NUMCOLORS);
        if (H(HDC, HSL_DC))
            palette_false();
        SetSystemPaletteUse(H(HDC, HSL_DC), SYSPAL_NOSTATIC);
        SetSystemPaletteUse(H(HDC, HSL_DC), SYSPAL_STATIC);
    }
    if (H(uint32_t, HSL_DIB_SECTION))
        dib_section_delete();
    SpriteDestroyAll(hsl, 0);
    H(void *, HSL_SCREEN_DIB) = winfree(H(void *, HSL_SCREEN_DIB));
    if (H(void *, HSL_IFACE_DIB))
        H(void *, HSL_IFACE_DIB) = winfree(H(void *, HSL_IFACE_DIB));
    palette_delete();
    palette_release();
    if (H(HDC, HSL_DC))
        SetViewportOrgEx(H(HDC, HSL_DC), 0, 0, NULL);
    hsl_free(hsl);
}

uint32_t __stdcall hsl_free(uint8_t *hsl)
{
    if (hsl) {
        if (hsl == g_static_hsl)
            g_static_used = 0;
        else
            LocalFree(hsl);
    }
    return 0;
}

uint8_t *__cdecl hsl_alloc(void)
{
    if (g_static_used)
        return LocalAlloc(LPTR, HSL_SIZE);
    g_static_used = 1;
    memset(g_static_hsl, 0, HSL_SIZE);
    return g_static_hsl;
}

/* Creates the engine state for window hwnd drawing on dc with depth bits per pixel */
uint8_t *__stdcall SpriteInitLibrary(HWND hwnd, HDC dc, int depth)
{
    char buf[128];
    if (!dc)
        return NULL;
    uint8_t *hsl = hsl_alloc();
    if (!hsl)
        return NULL;
    g_hsl = hsl;
    int n = GetDeviceCaps(dc, NUMCOLORS);
    if (n == -1) {
        /* original: both halves get the first 10 colors of the default palette */
        HGDIOBJ pal = GetStockObject(DEFAULT_PALETTE);
        GetPaletteEntries(pal, 0, 10, (PALETTEENTRY *)(g_hsl + HSL_SYSPAL_LOW));
        GetPaletteEntries(pal, 0, 10, (PALETTEENTRY *)(g_hsl + HSL_SYSPAL_HIGH));
    } else if (n <= 0x100) {
        SetSystemPaletteUse(dc, SYSPAL_NOSTATIC);
        GetLastError();
        SetSystemPaletteUse(dc, SYSPAL_STATIC);
        wsprintfA(buf, "SpriteInitLibrary()  iSysColors = %d\n", n);
        OutputDebugStringA(buf);
        GetSystemPaletteEntries(dc, 0, 0x100, (PALETTEENTRY *)(g_hsl + HSL_SYSPAL_LOW));
    }
    H(HPALETTE, HSL_HPALETTE) = NULL;
    H(uint32_t, HSL_DIB_SECTION) = 1;
    H(uint32_t, HSL_RESIZED) = 1;
    H(uint32_t, HSL_TOP_DOWN) = 1;
    H(HWND, HSL_HWND) = hwnd;
    H(HDC, HSL_DC) = dc;
    H(int32_t, HSL_DEPTH) = depth;
    /* Phase 1a palette fix built in: always a palette device. The original took
     * GetDeviceCaps(RASTERCAPS) & RC_PALETTE; on true-color desktops that leaves the 8-bit
     * screen without a palette (black). */
    GetDeviceCaps(H(HDC, HSL_DC), RASTERCAPS);
    H(uint32_t, HSL_PALETTE_DEVICE) = RC_PALETTE;
    if (depth == 16 && H(uint32_t, HSL_PALETTE_DEVICE))
        return NULL; /* (the state stays allocated: original) */
    H(uint32_t, HSL_BITMAPINFO) = 0x28;
    H(uint16_t, HSL_BITMAPINFO + 0xc) = 1;
    H(uint16_t, HSL_BITMAPINFO + 0xe) = (uint16_t)depth;
    H(uint32_t, HSL_BITMAPINFO + 0x10) = 0;
    H(uint32_t, HSL_ZOOM) = 1;
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    H(int32_t, HSL_WIDTH) = w;
    H(int32_t, HSL_HEIGHT) = h;
    H(int32_t, HSL_DESKTOP_W) = w;
    H(int32_t, HSL_DESKTOP_H) = h;
    H(int32_t, HSL_BITS) = GetDeviceCaps(dc, BITSPIXEL);
    H(int32_t, HSL_DESKTOP_BITS) = H(int32_t, HSL_BITS);
    RECT *v = &H(RECT, HSL_VIEW);
    v->left = -((w - 1) / 2);
    v->top = -((h - 1) / 2);
    v->right = v->left + w;
    v->bottom = v->top + h;
    H(RECT, HSL_VIEW2) = *v;
    H(RECT, HSL_SCROLL_LIMITS) = *v;
    SetViewportOrgEx(H(HDC, HSL_DC), (uint32_t)w >> 1, (uint32_t)h >> 1, NULL);
    if (!screen_dib_create(w, h))
        return (uint8_t *)(uintptr_t)hsl_free(hsl);
    SpriteSetBackground(hsl, NULL, NULL);
    queue_init_sizes();
    H(uint32_t, HSL_HEAD) = H(uint32_t, HSL_TAIL) = H(uint32_t, HSL_CURRENT) = 0;
    H(uint32_t, HSL_COUNT) = 0;
    H(uint32_t, HSL_ANIMATED) = 0;
    g_allow_triple = GetPrivateProfileIntA("DirectDraw", "AllowTripleBuffer", 1, "dominion.ini");
    g_no_blt_wait = GetPrivateProfileIntA("DirectDraw", "NoWaitForIFBlit", 0, "dominion.ini");
    g_flip_late = GetPrivateProfileIntA("DirectDraw", "WaitForFlip", 0, "dominion.ini");
    g_wait_drawing = GetPrivateProfileIntA("DirectDraw", "WaitIfStillDrawing", 0, "dominion.ini");
    H(uint32_t, HSL_QUALITY_B74) = 1;
    H(uint32_t, HSL_B78) = 0;
    H(uint32_t, HSL_SAFE_MODE) = GetPrivateProfileIntA("dominion", "SafeModeSwitch", 0, "dominion.ini");
    H(uint32_t, HSL_B80) = 0;
    H(uint32_t, HSL_B84) = 0;
    H(uint32_t, HSL_PAINTING) = 0;
    for (int i = 0; i < 25; i++)
        H(uint32_t, HSL_SYS_COLORS + i * 4) = GetSysColor(i);
    return hsl;
}

void __stdcall SpriteResetLibrary(uint8_t *hsl)
{
    g_hsl = hsl;
    H(uint32_t, HSL_PAINT_HOOK) = 0;
    H(uint32_t, HSL_ZOOMED) = 0;
    H(uint32_t, HSL_REDRAW_ALL) = 0;
    H(uint32_t, HSL_ZOOM_W) = H(uint32_t, HSL_ZOOM_H) = 0;
    H(uint32_t, HSL_SCROLL_X) = H(uint32_t, HSL_SCROLL_Y) = 0;
    H(uint32_t, HSL_AUX_SCROLL) = H(uint32_t, HSL_AUX_SCROLL + 4) = 0;
    H(uint32_t, HSL_ZOOM) = 1;
    H(uint32_t, HSL_BG_X) = H(uint32_t, HSL_BG_Y) = 0;
    memset(&H(uint8_t, HSL_CLIP_BOX), 0, 16);
    memset(&H(uint8_t, HSL_SCROLL_BOX), 0, 16);
    H(RECT, HSL_VIEW2) = H(RECT, HSL_VIEW);
    H(RECT, HSL_SCROLL_LIMITS) = H(RECT, HSL_VIEW);
    H(uint32_t, HSL_FILL) = 0;
    H(uint32_t, HSL_BACKGROUND) = 0;
    H(uint32_t, HSL_TILE_MAP) = 0;
    if (H(void *, HSL_TILES))
        H(void *, HSL_TILES) = winfree(H(void *, HSL_TILES));
    screen_dib_create(H(int32_t, HSL_WIDTH), H(int32_t, HSL_HEIGHT));
}

void __stdcall set_screen_mode(uint8_t *hsl, uint32_t mode, uint32_t top_down, uint32_t v9f0, int a, int b, int c)
{
    g_hsl = hsl;
    H(uint32_t, HSL_SCREEN_MODE) = mode;
    if (!mode) {
        H(uint32_t, HSL_9F0) = H(uint32_t, HSL_TOP_DOWN) = H(uint32_t, HSL_RESIZED) = 0;
    } else {
        if (H(void *, HSL_SCREEN_DIB))
            winfree(H(void *, HSL_SCREEN_DIB)); /* original: the pointer is kept */
        if (H(void *, HSL_IFACE_DIB))
            H(void *, HSL_IFACE_DIB) = winfree(H(void *, HSL_IFACE_DIB));
        H(uint32_t, HSL_RESIZED) = 1;
        H(uint32_t, HSL_TOP_DOWN) = top_down;
        H(uint32_t, HSL_9F0) = v9f0;
    }
    set_8c0(hsl, a, b, c);
}

/* Takes the screen size from the desktop and clears to no background */
void __stdcall screen_size_from_desktop(uint8_t *hsl)
{
    g_hsl = hsl;
    H(int32_t, HSL_WIDTH) = GetSystemMetrics(SM_CXSCREEN);
    H(int32_t, HSL_HEIGHT) = GetSystemMetrics(SM_CYSCREEN);
    RECT *v = &H(RECT, HSL_VIEW);
    v->left = -((H(int32_t, HSL_WIDTH) - 1) / 2);
    v->top = -((H(int32_t, HSL_HEIGHT) - 1) / 2);
    v->right = v->left + H(int32_t, HSL_WIDTH);
    v->bottom = v->top + H(int32_t, HSL_HEIGHT);
    SetViewportOrgEx(H(HDC, HSL_DC), H(uint32_t, HSL_WIDTH) >> 1, H(uint32_t, HSL_HEIGHT) >> 1, NULL);
    GetDeviceCaps(H(HDC, HSL_DC), RASTERCAPS);
    H(uint32_t, HSL_PALETTE_DEVICE) = 1; /* Phase 1a palette fix built in (see SpriteInitLibrary) */
    SpriteSetBackground(hsl, (uint8_t *)-1, NULL);
}

/* returns the new screen bitmap */
uint32_t __stdcall SpriteSetComposeFlags(uint8_t *hsl, uint32_t top_down, uint32_t resized, uint32_t dib_section)
{
    g_hsl = hsl;
    H(uint32_t, HSL_DIB_SECTION) = dib_section;
    if (dib_section)
        resized = 1;
    H(uint32_t, HSL_RESIZED) = resized;
    H(uint32_t, HSL_TOP_DOWN) = top_down;
    if (H(void *, HSL_SCREEN_DIB))
        H(void *, HSL_SCREEN_DIB) = winfree(H(void *, HSL_SCREEN_DIB));
    if (H(void *, HSL_IFACE_DIB))
        H(void *, HSL_IFACE_DIB) = winfree(H(void *, HSL_IFACE_DIB));
    screen_dib_create(H(int32_t, HSL_WIDTH), H(int32_t, HSL_HEIGHT));
    return H(uint32_t, HSL_SCREEN_DIB);
}

void __stdcall set_8c0(uint8_t *hsl, int a, int b, int c)
{
    g_hsl = hsl;
    if (a == 5 && b == 5 && c == 5)
        H(uint32_t, HSL_8C0) = 0;
    else if (a == 5 && b == 6 && c == 5)
        H(uint32_t, HSL_8C0) = 1;
}

void __stdcall set_b60(uint8_t *hsl, uint32_t v)
{
    g_hsl = hsl;
    H(uint32_t, HSL_B60) = v;
}

void __stdcall SpriteRegisterOverlaySprite(uint8_t *hsl, uint32_t s)
{
    g_hsl = hsl;
    H(uint32_t, HSL_OVERLAY) = s;
}

/* Fits the screen bitmap header to w x h, growing its memory when needed */
int __stdcall screen_dib_fit(int w, int h)
{
    uint32_t pitch = (w + 3) & ~3u;
    uint32_t size = (H(uint32_t, HSL_DEPTH) >> 3) * h * pitch + 0x24;
    uint8_t *p = H(uint8_t *, HSL_SCREEN_DIB);
    S(int32_t, p, 4) = w;
    S(uint32_t, p, 8) = pitch;
    S(int32_t, p, 0xc) = h;
    if (size <= H(uint32_t, HSL_SCREEN_SIZE))
        return 1;
    p = winrealloc(p, size, 0);
    if (!p)
        return 0;
    S(uint8_t *, p, 0x20) = p + 0x24;
    H(uint8_t *, HSL_SCREEN_DIB) = p;
    H(uint32_t, HSL_SCREEN_SIZE) = size;
    return 1;
}

/* Copies bitmap bm to the window at x, y */
void __stdcall background_draw(int x, int y, uint8_t *bm)
{
    SetRect(&g_draw_rect, x, y, S(int32_t, bm, 8), S(int32_t, bm, 0xc));
    screen_blit(bm);
}

/* ---- showing, hiding, moving ---- */

/* Command 1 step: show the next cel when its time has come; returns 1 when done */
uint32_t __stdcall cmd_cels(uint8_t *s)
{
    DWORD now = timeGetTime();
    if (now < S(uint32_t, s, SP_NEXT_MOVE))
        return 0;
    uint8_t *c = S(uint8_t *, s, SP_QUEUE_CUR);
    if (!S(int32_t, c, 10))
        return 1;
    if (S(int32_t, c, 0xe) != -1)
        S(int32_t, s, SP_MOTION) = sprite_delay(S(int32_t, c, 0xe), S(char, c, 0x14));
    if (!S(uint32_t, s, SP_PRIORITY) || !S(uint32_t, s, SP_NEXT_MOVE))
        S(uint32_t, s, SP_NEXT_MOVE) = now;
    S(int32_t, s, SP_NEXT_MOVE) += S(int32_t, s, SP_MOTION);
    S(uint32_t, s, SP_CC) = 0;
    S(uint32_t, s, SP_UPDATED) = 1;
    S(uint32_t, s, SP_VISIBLE) = S(uint32_t, s, SP_UPDATED);
    sprite_reposition(s);
    S(int32_t, c, 10)--;
    return S(int32_t, c, 10) == 0 && S(int32_t, s, SP_MOTION) == 0;
}

/* ---- sprites made of parts (0x10062320, 0x1006254a) ----
 * A part is 20 bytes at SP_PARTS: its bitmap and its rectangle on screen. */

#define SP_FLIP_X 0x7c
#define SP_FLIP_Y 0x7d
#define SP_Y 0x40
#define SP_CALLBACK 0xac
#define SP_NEXT_STEP 0x64
#define SP_SPEED 0x6c
#define SP_LOOPS 0xd8
#define INFO_RECT_PROC 0x24    /* proc(hsl, sprite, rect): may change a part's rectangle */
#define INFO_PART_OFFSETS 0x34 /* x, y offsets of the first 8 parts */
#define HSL_INDEX_TABLE 0x484
typedef void(__stdcall *RectProc)(uint8_t *, uint8_t *, int32_t *);

/* 0x10062320: works out each part's rectangle and the sprite's bounds around them */
void __stdcall parts_update_bounds(uint8_t *s)
{
    if (!S(uint32_t, s, SP_POSITIONED))
        return;
    int32_t *part = S(int32_t *, s, SP_PARTS);
    uint8_t *info = S(uint8_t *, s, SP_INFO);
    for (uint32_t i = 0; i < S(uint32_t, s, SP_NPARTS); i++, part += 5) {
        uint8_t *bm = (uint8_t *)(uintptr_t)part[0];
        int hw = S(int32_t, bm, 4) - 1, hh = S(int32_t, bm, 0xc) - 1;
        int ox = S(int32_t, bm, 0x14), oy = S(int32_t, bm, 0x18);
        if (S(uint32_t, info, INFO_PART_OFFSETS) && i < 8) {
            ox += S(int32_t *, info, INFO_PART_OFFSETS)[i * 2];
            oy += S(int32_t *, info, INFO_PART_OFFSETS)[i * 2 + 1];
        }
        part[1] = S(int32_t, s, SP_X) - hw / 2 + (S(char, s, SP_FLIP_X) ? -ox : ox);
        part[3] = part[1] + S(int32_t, bm, 4);
        part[2] = S(int32_t, s, SP_Y) - hh / 2 + (S(char, s, SP_FLIP_Y) ? -oy : oy);
        part[4] = part[2] + S(int32_t, bm, 0xc);
        if (S(uint32_t, s, SP_SCROLLS))
            OffsetRect((RECT *)(part + 1), H(int32_t, HSL_SCROLL_X), H(int32_t, HSL_SCROLL_Y));
        if (S(uint32_t, info, INFO_RECT_PROC))
            S(RectProc, info, INFO_RECT_PROC)(g_hsl, s, part + 1);
        if (i == 0)
            memcpy(s + SP_RECT, part + 1, 16);
        else
            union_rect(s + SP_RECT, s + SP_RECT, part + 1);
    }
}

/* Command 2 (0x1006254a): show a set of cels as the sprite's parts. If the first parts
 * are the ones already shown, only the changed ones are redrawn. Returns 1 when done. */
uint32_t __stdcall cmd_parts(uint8_t *s)
{
    static int32_t before[8][4]; /* 0x1008f858 */
    DWORD now = timeGetTime();
    if (now < S(uint32_t, s, SP_NEXT_MOVE))
        return 0;
    uint8_t *c = S(uint8_t *, s, SP_QUEUE_CUR);
    if (S(char, c, 10))
        return 1;
    if (S(int32_t, c, 2) != -1)
        S(int32_t, s, SP_MOTION) = sprite_delay(S(int32_t, c, 2), S(char, c, 0xb));
    if (!S(uint32_t, s, SP_PRIORITY) || !S(uint32_t, s, SP_NEXT_MOVE))
        S(uint32_t, s, SP_NEXT_MOVE) = now;
    S(int32_t, s, SP_NEXT_MOVE) += S(int32_t, s, SP_MOTION);
    int32_t old[4];
    memcpy(old, s + SP_RECT, 16);
    if (!S(uint32_t, s, SP_PARTS))
        S(HLOCAL, s, SP_PARTS) = LocalAlloc(LMEM_ZEROINIT, 0xa0);
    if (!S(uint32_t, s, SP_PARTS))
        return 1;
    int32_t bm[8];
    uint32_t n = 0;
    uint8_t indexed = S(uint8_t, c, 0xd);
    for (uint32_t i = 0; i < S(uint32_t, c, 6); i++, indexed >>= 1) {
        uint32_t cel = S(uint32_t, c, 0xe + i * 4);
        if (indexed & 1)
            cel = H(uint32_t *, HSL_INDEX_TABLE)[cel];
        if (cel == 0xffffffff)
            continue;
        if (S(uint32_t, s, SP_NCELS) <= cel)
            return 1;
        bm[n++] = S(int32_t *, s, SP_CELS)[cel];
    }
    int32_t *part = S(int32_t *, s, SP_PARTS);
    int same = S(uint32_t, s, SP_CC) && S(uint32_t, s, SP_NPARTS) == n;
    if (same)
        for (uint32_t i = 0; i < n; i++)
            memcpy(before[i], part + i * 5 + 1, 16);
    uint32_t kept = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (!same || part[i * 5] != bm[i]) {
            part[i * 5] = bm[i];
            same = 0;
        } else
            kept++;
    }
    if (S(uint32_t, s, SP_NPARTS) == n)
        same = kept != 0;
    else
        S(uint32_t, s, SP_NPARTS) = n;
    parts_update_bounds(s);
    uint32_t changed = 1;
    if (!same)
        S(uint32_t, s, SP_OVERLAPS) = intersect_rect(old, old, s + SP_RECT);
    else {
        changed = n - kept;
        for (uint32_t i = kept; i < n; i++) {
            if (i == kept)
                memcpy(old, before[i], 16);
            else
                union_rect(old, old, before[i]);
            union_rect(old, old, part + i * 5 + 1);
        }
    }
    S(uint32_t, s, SP_CC) = 1;
    S(uint32_t, s, SP_VISIBLE) = 1;
    S(uint8_t, c, 10) = 1;
    S(uint32_t, s, SP_UPDATED) = 0;
    if (S(uint32_t, s, SP_CALLBACK))
        sprite_callback(s);
    if (!same) {
        if (changed)
            sprite_reposition(s);
    } else {
        draw_view_sprites(old, REDRAW_ALL);
        memcpy(s + SP_OLD_RECT, s + SP_RECT, 16);
        S(uint32_t, s, SP_DRAWN) = 1;
    }
    return S(int32_t, s, SP_MOTION) == 0;
}

/* ---- moving along a line or curve (0x10063180-0x100636b0) ---- */

/* The animation part shared by the move commands: 1 when the next cel is due */
static int cel_due(uint8_t *s, DWORD now)
{
    if (!S(uint32_t, s, SP_LOOPS) || now < S(uint32_t, s, SP_NEXT_MOVE))
        return 0;
    S(uint32_t, s, SP_UPDATED) = 1;
    if (!S(uint32_t, s, SP_PRIORITY) || !S(uint32_t, s, SP_NEXT_MOVE))
        S(uint32_t, s, SP_NEXT_MOVE) = now;
    S(int32_t, s, SP_NEXT_MOVE) += S(int32_t, s, SP_MOTION);
    return 1;
}

/* 1 when a movement step is due; schedules the next one */
static int step_due(uint8_t *s, DWORD now)
{
    if (now < S(uint32_t, s, SP_NEXT_STEP))
        return 0;
    if (!S(uint32_t, s, SP_PRIORITY) || !S(uint32_t, s, SP_NEXT_STEP))
        S(uint32_t, s, SP_NEXT_STEP) = now;
    S(int32_t, s, SP_NEXT_STEP) += S(int32_t, s, SP_SPEED);
    return 1;
}

/* 0x100632cc: the next point of a line command (step at +0x24 of +0x28) */
uint32_t __stdcall line_step(uint8_t *s)
{
    uint8_t *c = S(uint8_t *, s, SP_QUEUE_CUR);
    int32_t pt[2];
    if (++S(uint32_t, c, 0x24) <= S(uint32_t, c, 0x28)) {
        line_point(S(uint32_t, c, 0x24), S(uint32_t, c, 0x28), S(int32_t, c, 0x14), S(int32_t, c, 0x18),
                   S(int32_t, c, 0x1c), S(int32_t, c, 0x20), pt);
        sprite_move(s, pt[0], pt[1], 0, 1);
    }
    return S(uint32_t, c, 0x28) <= S(uint32_t, c, 0x24);
}

/* 0x100635fc: the next point of a curve command (step at +0x34 of +0x38) */
uint32_t __stdcall curve_step(uint8_t *s)
{
    uint8_t *c = S(uint8_t *, s, SP_QUEUE_CUR);
    int32_t pt[2];
    if (++S(uint32_t, c, 0x34) <= S(uint32_t, c, 0x38)) {
        G(uint32_t, 0x1007bd24) =
            curve_point(S(uint32_t, c, 0x34), S(uint32_t, c, 0x38), S(int32_t, c, 0x1c), S(int32_t, c, 0x20),
                        S(int32_t, c, 0x24), S(int32_t, c, 0x28), S(int32_t, c, 0x2c), S(int32_t, c, 0x30), pt);
        sprite_move(s, pt[0], pt[1], 0, 1);
    }
    return S(uint32_t, c, 0x38) <= S(uint32_t, c, 0x34);
}

static uint32_t cmd_path(uint8_t *s, uint32_t(__stdcall *step)(uint8_t *))
{
    DWORD now = timeGetTime();
    if (!step_due(s, now)) {
        if (cel_due(s, now)) {
            S(uint32_t, s, SP_OVERLAPS) = 1;
            sprite_reposition(s);
        }
        return 0;
    }
    cel_due(s, now);
    return step(s);
}

/* Commands 9 and 5 (0x10063180, 0x100634b0): move along a line or a curve, animating */
uint32_t __stdcall cmd_line(uint8_t *s) { return cmd_path(s, line_step); }
uint32_t __stdcall cmd_curve(uint8_t *s) { return cmd_path(s, curve_step); }

/* Command 10 (0x10063368): jump to a position when the step is due */
uint32_t __stdcall cmd_10(uint8_t *s)
{
    DWORD now = timeGetTime();
    int cel = cel_due(s, now);
    if (!step_due(s, now)) {
        if (cel)
            sprite_reposition(s);
        return 0;
    }
    uint8_t *c = S(uint8_t *, s, SP_QUEUE_CUR);
    sprite_move(s, sprite_delay(S(int32_t, c, 6), S(char, c, 3)), sprite_delay(S(int32_t, c, 10), S(char, c, 4)),
                S(uint8_t, c, 2), 1);
    return 1;
}

/* Brings s up to date on screen: next cel, then redraw where it was and where it is */
void __stdcall sprite_reposition(uint8_t *s)
{
    S(uint32_t, s, SP_MOVED) = 0;
    if (!S(uint32_t, s, SP_VISIBLE) || !S(uint32_t, s, SP_POSITIONED))
        return;
    if (S(uint32_t, s, SP_UPDATED))
        sprite_next_cel(s);
    if (S(uint32_t, s, SP_EXTERNAL_DRAW))
        return;
    if (!S(uint32_t, s, SP_DRAWN) || !S(uint32_t, s, SP_OVERLAPS) || !sprite_redraw_both(s)) {
        if (S(uint32_t, s, SP_DRAWN))
            sprite_erase(s, 1);
        draw_view_sprites(s + SP_RECT, REDRAW_ALL);
    }
    memcpy(s + SP_OLD_RECT, s + SP_RECT, 16);
    S(uint32_t, s, SP_DRAWN) = 1;
}

/* Redraws the union of the old and new bounds */
int __stdcall sprite_redraw_both(uint8_t *s)
{
    RECT r;
    union_rect(&r, s + SP_RECT, s + SP_OLD_RECT);
    return draw_view_sprites(&r, REDRAW_ALL);
}

/* Redraws where s was drawn without s; clip: not over its new bounds (GDI) */
void __stdcall sprite_erase(uint8_t *s, int clip)
{
    int saved = 0;
    if (clip && !H(uint32_t, 0x954)) {
        saved = SaveDC(H(HDC, HSL_DC));
        ExcludeClipRect(H(HDC, HSL_DC), S(int32_t, s, SP_RECT), S(int32_t, s, SP_RECT + 4), S(int32_t, s, SP_RECT + 8),
                        S(int32_t, s, SP_RECT + 12));
    }
    uint32_t visible = S(uint32_t, s, SP_VISIBLE);
    S(uint32_t, s, SP_VISIBLE) = 0;
    draw_view_sprites(s + SP_OLD_RECT, REDRAW_ALL);
    S(uint32_t, s, SP_VISIBLE) = visible;
    if (saved)
        RestoreDC(H(HDC, HSL_DC), saved);
}

/* Moves s to x, y (or by x, y when relative); redraw: update the screen now */
void __stdcall sprite_move(uint8_t *s, int x, int y, int relative, int redraw)
{
    RECT old;
    memcpy(&old, s + SP_RECT, 16);
    /* sprites on the interface panel mark its dirty rectangles before and after */
    int panel = H(uint32_t, HSL_DIRECTDRAW) && !H(uint32_t, HSL_B5C) && H(uint32_t, HSL_IFACE_ON) &&
                !S(uint32_t, s, SP_SCROLLS) && old.bottom > H(int32_t, HSL_VIEW_BOTTOM);
    if (panel)
        dirty_add_if_shown(s);
    if (S(uint32_t, s, SP_UPDATED) && S(uint32_t, s, SP_POSITIONED))
        sprite_next_cel(s);
    int32_t *pos = S(uint32_t, s, SP_POSITIONED) ? (int32_t *)(s + SP_X) : S(int32_t *, s, SP_INFO);
    if (relative) {
        x += pos[0];
        y += pos[1];
    }
    int dx = x - pos[0], dy = y - pos[1];
    pos[0] = x;
    pos[1] = y;
    OffsetRect((RECT *)(s + SP_RECT), dx, dy);
    if (!S(uint32_t, s, SP_POSITIONED))
        return;
    S(uint32_t, s, SP_CC) = 0;
    S(uint32_t, s, SP_OVERLAPS) = intersect_rect(&old, &old, s + SP_RECT);
    for (uint32_t i = 0; i < S(uint32_t, s, SP_NPARTS); i++)
        OffsetRect((RECT *)(S(uint8_t *, s, SP_PARTS) + i * 0x14 + 4), dx, dy);
    if (redraw)
        sprite_reposition(s);
    if (panel)
        dirty_add_if_shown(s);
}

void __stdcall SpriteHide(uint8_t *hsl, uint8_t *s, int erase)
{
    g_hsl = hsl;
    if (S(uint32_t, s, SP_VISIBLE) && erase && !S(uint32_t, s, SP_EXTERNAL_DRAW))
        sprite_erase(s, 0);
    S(uint32_t, s, SP_DRAWN) = 0;
    S(uint32_t, s, SP_VISIBLE) = 0;
    S(uint32_t, s, SP_CC) = 0;
}

/* Replaces bitmap old by new in the cels and parts of s */
void __stdcall SpriteReplaceBitmap(uint8_t *hsl, uint8_t *s, uint32_t old, uint32_t new)
{
    g_hsl = hsl;
    uint32_t *cel = S(uint32_t *, s, SP_CELS);
    if (!cel)
        return;
    int found = 0;
    for (uint32_t i = 0; i < S(uint32_t, s, SP_NCELS); i++, cel++)
        if (*cel == old) {
            *cel = new;
            found = 1;
        }
    if (!found)
        return;
    if (!S(uint32_t, s, SP_NPARTS)) {
        if (S(uint32_t, s, SP_BITMAP) != old)
            return;
        S(uint32_t, s, SP_BITMAP) = new;
        sprite_update_bounds(s);
    } else {
        found = 0;
        for (uint32_t i = 0; i < S(uint32_t, s, SP_NPARTS); i++)
            if (*(uint32_t *)(S(uint8_t *, s, SP_PARTS) + i * 0x14) == old) {
                *(uint32_t *)(S(uint8_t *, s, SP_PARTS) + i * 0x14) = new;
                found = 1;
            }
        if (!found)
            return;
        parts_update_bounds(s);
    }
    S(uint32_t, s, SP_UPDATED) = 0;
    S(uint32_t, s, SP_CC) = 0;
    sprite_reposition(s);
}

/* Shows cel `cel` of s (or keeps the current one); redraw: update the screen now */
void __stdcall SpriteShow(uint8_t *hsl, uint8_t *s, uint32_t cel, int redraw)
{
    g_hsl = hsl;
    if (S(uint32_t, s, SP_VISIBLE) && S(uint32_t, s, SP_SHOWN_CEL) == cel)
        return;
    sprite_set_positioned(s, 1, 0);
    S(uint32_t, s, SP_VISIBLE) = S(uint32_t, s, SP_NCELS) != 0;
    S(uint32_t, s, SP_CC) = 0;
    if (cel < S(uint32_t, s, SP_NCELS)) {
        S(uint32_t, s, SP_CEL) = cel;
        S(uint32_t, s, SP_UPDATED) = 1;
    } else if (!S(uint32_t, s, SP_BITMAP)) {
        S(uint32_t, s, SP_CEL) = S(uint32_t, s, SP_SHOWN_CEL);
        S(uint32_t, s, SP_UPDATED) = 1;
    }
    if (redraw)
        sprite_reposition(s);
}

/* Shows s and redraws its bounds, passing its level to the drawing pass */
void __stdcall SpriteShowOpaque(uint8_t *hsl, uint8_t *s)
{
    g_hsl = hsl;
    sprite_set_positioned(s, 1, 0);
    S(uint32_t, s, SP_VISIBLE) = S(uint32_t, s, SP_NCELS) != 0;
    S(uint32_t, s, SP_CC) = 0;
    draw_view_sprites(s + SP_RECT, S(uint32_t, s, SP_LEVEL));
}
