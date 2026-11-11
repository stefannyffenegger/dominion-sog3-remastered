/* Engine core, 0x10001000-0x10003039: the background (bitmap, color, tile map), palette
 * commands, growable arrays, error text, screen capture to .bmp, and the transitions
 * that bring a new background in. Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_bg.h"
#include <mmsystem.h>

#define HSL R(uint8_t *, RT_HSL)

/* ---- background ---- */

/* 0x10001166: drops the background and the tile map */
void __cdecl bg_clear(void)
{
    if (R(uint32_t, RT_TILE_MAP)) {
        if (P(uint32_t, R(uint8_t *, RT_TILE_MAP), 0x3c))
            obj_release(P(uint32_t, R(uint8_t *, RT_TILE_MAP), 0x3c), 0x16);
        R(uint32_t, RT_TILE_MAP) = 0;
    }
    if (R(uint32_t, RT_BACKGROUND))
        winfree(R(void *, RT_BACKGROUND));
    R(uint32_t, RT_BACKGROUND) = 0;
    R(uint32_t, RT_BKG_COLOR) = 0;
    SpriteSetBackground(HSL, NULL, NULL);
}

/* 0x100012ca: the background's size: the screen's, or bm's; a bitmap larger than the
 * screen scrolls */
void __stdcall bg_size(uint8_t *bm)
{
    R(uint32_t, RT_BKG_W) = R(uint32_t, RT_SCREEN_W);
    R(uint32_t, RT_BKG_H) = R(uint32_t, RT_SCREEN_H);
    R(uint8_t, RT_BKG_SCROLLS) = R(uint8_t, RT_SCROLLABLE);
    if (!bm)
        return;
    R(uint32_t, RT_BKG_W) = P(uint32_t, bm, 4);
    R(uint32_t, RT_BKG_H) = P(uint32_t, bm, 0xc);
    R(uint8_t, RT_BKG_SCROLLS) = !(!R(uint8_t, RT_SCROLLABLE) && R(uint32_t, RT_SCREEN_W) <= R(uint32_t, RT_BKG_W) &&
                                  R(uint32_t, RT_SCREEN_H) <= R(uint32_t, RT_BKG_H));
}

/* 0x100013c0: a private copy of bitmap resource id (type 0xf: a reference to one) */
uint8_t *__stdcall bg_bitmap(uint32_t id)
{
    if (!id)
        return NULL;
    if (ResGetType(R(ResFile *, RT_RES), id) == 0xf) {
        uint32_t *ref = (uint32_t *)obj_load(id, NULL);
        if (!ref)
            return NULL;
        id = *ref;
    }
    if (!obj_12000(id, 1))
        return NULL;
    if (!obj_14409(id, 1))
        return obj_load(id, NULL);
    uint8_t *bm = winptrcpy((void *)(uintptr_t)*obj_entry(id));
    if (bm)
        P(uint8_t *, bm, 0x20) = bm + 0x24;
    return bm;
}

/* 0x10001498: repaints the window; with a composed sprite, without drawing over it */
void __stdcall bg_repaint(RECT *r)
{
    HWND wnd = R(HWND, RT_HWND);
    if (!R(uint32_t, RT_COMPOSE_SPRITE) || !R(uint8_t, RT_COMPOSE)) {
        if (R(uint8_t, RT_PALETTE_DEVICE) || !R(uint8_t, RT_COMPOSE)) {
            InvalidateRect(wnd, NULL, 1);
            SendMessageA(wnd, WM_PAINT, 0, 0);
        }
        return;
    }
    uint32_t info[24];
    SpriteGetInfo(R(uint8_t *, RT_COMPOSE_SPRITE), info);
    RECT *box = (RECT *)&info[15];
    if (!info[10] || !IntersectRect(box, box, r)) {
        InvalidateRect(wnd, NULL, 1);
        SendMessageA(wnd, WM_PAINT, 0, 0);
        return;
    }
    ExcludeClipRect(R(HDC, RT_DC), box->left, box->top, box->right, box->bottom);
    InvalidateRect(wnd, NULL, 1);
    SendMessageA(wnd, WM_PAINT, 0, 0);
    SelectClipRgn(R(HDC, RT_DC), NULL);
    SpritePaint(HSL, R(uint8_t *, RT_COMPOSE_SPRITE), 0);
}

/* 0x10001620: hands the background to the sprite engine */
void __stdcall bg_apply(RECT *out)
{
    if (!R(uint8_t, RT_PALETTE_DEVICE) && R(uint8_t, RT_COMPOSE) && R(uint32_t, RT_DEPTH) != 0x10) {
        if (R(uint32_t, RT_SCROLL_DIB))
            R(void *, RT_SCROLL_DIB) = winfree(R(void *, RT_SCROLL_DIB));
        R(uint8_t *, RT_SCROLL_DIB) = SpriteDibCreate(R(int, RT_BKG_W), R(int, RT_BKG_H), 1, 8);
        if (R(uint32_t, RT_SCROLL_DIB))
            SpriteSetBackground(HSL, R(uint8_t *, RT_SCROLL_DIB), out);
    } else if (!R(uint32_t, RT_TILE_MAP))
        SpriteSetBackground(HSL, R(uint8_t *, RT_BACKGROUND), out);
    else
        SpriteSetTileBackground(HSL, R(uint32_t, RT_TILE_MAP), out);
}

/* 0x10001218: p is a background command from its type field on (+4 bitmap, +8 another
 * one for the alternative mode) */
void __stdcall bg_load(uint8_t *p)
{
    RECT rc;
    uint32_t id = ((!R(uint32_t, RT_8D77) && !R(uint8_t, RT_95CC)) || !P(uint32_t, p, 8)) ? P(uint32_t, p, 4)
                                                                                    : P(uint32_t, p, 8);
    uint8_t *bm = bg_bitmap(id);
    if (!bm)
        return;
    bg_clear();
    R(uint8_t *, RT_BACKGROUND) = bm;
    bg_size(bm);
    bg_apply(&rc);
    bg_repaint(&rc);
}

/* 0x10001760: a plain color as the background (palette displays) */
void __stdcall bg_color(uint32_t color)
{
    RECT rc;
    bg_size(NULL);
    SpriteSetBackground(HSL, (uint8_t *)(uintptr_t)color, &rc);
    R(uint32_t, RT_BKG_COLOR) = color;
    R(uint8_t, RT_BKG_IS_COLOR) = 1;
    bg_repaint(&rc);
}

/* 0x100017b9: a plain color as a filled bitmap */
void __stdcall bg_fill(uint32_t color)
{
    RECT rc;
    bg_size(NULL);
    uint8_t *bm = SpriteDibCreate(R(int, RT_BKG_W), R(int, RT_BKG_H), 0, 8);
    R(uint8_t *, RT_BACKGROUND) = bm;
    if (!bm)
        return;
    SetHugeBytes(P(void *, bm, 0x20), (uint8_t)color, P(uint32_t, bm, 8) * R(uint32_t, RT_BKG_H));
    R(uint32_t, RT_BKG_COLOR) = color;
    R(uint8_t, RT_BKG_IS_COLOR) = 1;
    R(uint8_t, RT_BKG_SCROLLS) = R(uint8_t, RT_SCROLLABLE);
    bg_size(bm);
    bg_apply(&rc);
    bg_repaint(&rc);
}

/* 0x100018a8: a tile map as the background */
void __stdcall bg_tiles(uint32_t map, int repaint)
{
    RECT rc;
    if (map && R(uint32_t, RT_TILE_MAP) != map)
        bg_clear();
    R(uint32_t, RT_TILE_MAP) = map;
    bg_size(NULL);
    R(uint8_t, RT_BKG_SCROLLS) = R(uint8_t, RT_SCROLLABLE);
    bg_apply(&rc);
    if (repaint)
        bg_repaint(&rc);
}

/* 0x10001929: brings the command's bitmap into the current background with the
 * transition at +0x12; 0 when there is nothing to do it with */
int __stdcall bg_transition(uint8_t *c)
{
    if (!R(uint32_t, RT_BACKGROUND) || P(uint32_t, c, 2))
        return 0;
    uint32_t id = ((!R(uint8_t, RT_95E7) && !R(uint8_t, RT_95CC)) || !P(uint32_t, c, 10)) ? P(uint32_t, c, 6)
                                                                                    : P(uint32_t, c, 10);
    uint8_t *bm = bg_bitmap(id);
    if (!bm)
        return 0;
    bg_effect(R(uint32_t, RT_BACKGROUND), (uint32_t)(uintptr_t)bm, (const uint32_t *)(c + 0x12));
    winfree(bm);
    return 1;
}

/* 0x10001039: the background command: +2 kind (0 bitmap or tile map, 1 color, 2 none
 * kept transparent, 3 tile map), +6 resource, +0x10 color */
void __stdcall bg_set(uint8_t *c)
{
    R(uint8_t, RT_BKG_IS_COLOR) = 0;
    switch (P(uint32_t, c, 2)) {
    case 0:
        if (ResGetType(R(ResFile *, RT_RES), P(uint32_t, c, 6)) == 1) {
            bg_load(c + 2);
            return;
        }
        /* fall through */
    case 3: {
        uint32_t id = P(uint32_t, c, 6);
        if (obj_is(id, 0x16))
            bg_tiles(obj_entry(id)[1], 1);
        break;
    }
    case 1:
        bg_clear();
        if (!R(uint8_t, RT_PALETTE_DEVICE))
            bg_fill(P(uint8_t, c, 0x10));
        else
            bg_color(P(uint8_t, c, 0x10));
        break;
    case 2:
        bg_clear();
        SpriteSetBackground(HSL, (uint8_t *)(uintptr_t)0xffffffff, NULL);
        break;
    default:
        bg_clear();
    }
}

/* 0x10001000 */
void __stdcall bg_cmd(uint8_t *c)
{
    if (P(uint32_t, c, 0x12) && bg_transition(c))
        return;
    bg_set(c);
}

/* ---- palette commands ---- */

static int palette_animates(void)
{
    return R(uint8_t, RT_95DB) && !R(uint8_t, RT_95F9);
}

/* 0x100019f9: sets colors +2.. (count +6, at most 236) from the command or from the
 * stored palette (+10); only above the 10 system colors, on palette displays */
void __stdcall palette_cmd(uint8_t *c)
{
    uint32_t first = P(uint32_t, c, 2);
    if (!R(uint8_t, RT_PALETTE_DEVICE) || first <= 9)
        return;
    uint32_t n = P(uint32_t, c, 6);
    if (n > 0xec)
        n = 0xec;
    int animate = palette_animates();
    if (!P(uint8_t, c, 10))
        SpriteChangePalette(HSL, first, n, (const uint32_t *)(c + 0xe), animate, 1);
    else
        SpriteChangePalette(HSL, first, n, (const uint32_t *)(g_rt + RT_PALETTE + first * 4), animate, 0);
}

/* 0x10001b06: loads palette resource +2 (colors 10-245) */
void __stdcall palette_load_cmd(uint8_t *c)
{
    if (!obj_is(P(uint32_t, c, 2), 0x11))
        return;
    memcpy(g_rt + RT_PALETTE + 40, (void *)(uintptr_t)*obj_entry(P(uint32_t, c, 2)), 0x3b0);
    SpriteChangePalette(HSL, 10, 0xec, (const uint32_t *)(g_rt + RT_PALETTE + 40),
                        palette_animates() && !R(uint8_t, RT_COMPOSE), 0);
    plugins_palette();
}

/* ---- exports ---- */

void __cdecl RtGetScrollBox(uint8_t *rt, int32_t *r)
{
    g_rt = rt;
    SpriteGetScrollBox(P(uint8_t *, rt, RT_HSL), r);
}

uint32_t __cdecl RtGetBackground(uint8_t *rt)
{
    g_rt = rt;
    return P(uint32_t, rt, RT_BACKGROUND);
}

void __cdecl RtSetBackground(uint8_t *rt, uint32_t bm, int repaint)
{
    RECT rc;
    g_rt = rt;
    bg_clear();
    R(uint32_t, RT_BACKGROUND) = bm;
    bg_size(R(uint8_t *, RT_BACKGROUND));
    bg_apply(&rc);
    if (repaint)
        bg_repaint(&rc);
}

void __cdecl RtSetComposeBackground(uint8_t *rt, int repaint)
{
    g_rt = rt;
    uint8_t *bm = screen_copy(P(uint8_t *, rt, RT_HSL));
    if (bm)
        RtSetBackground(rt, (uint32_t)(uintptr_t)bm, repaint);
}

void __cdecl RtSetScrollBox(uint8_t *rt, const int32_t *r)
{
    g_rt = rt;
    SpriteSetScrollBox(P(uint8_t *, rt, RT_HSL), (const RECT *)r);
    R(uint8_t, RT_SCROLLABLE) = !(!r || (r[2] <= r[0] && r[3] <= r[1]));
    R(uint8_t, RT_BKG_SCROLLS) = R(uint8_t, RT_SCROLLABLE);
}

void __cdecl RtSetTileBackground(uint8_t *rt, uint32_t map, int repaint)
{
    g_rt = rt;
    bg_tiles(map, repaint);
}

/* ---- growable arrays (0x10001da0-0x10002010) ---- */

/* Adds a zeroed element; NULL when out of memory or past 65,000 bytes */
void *__stdcall array_add(RtArray *a)
{
    if (a->capacity <= a->count) {
        uint32_t cap = a->capacity + a->grow, size = a->elem * cap;
        if (size > 65000)
            return NULL;
        void *p = winrealloc(a->data, size, 0);
        if (!p)
            return NULL;
        a->data = p;
        a->capacity = cap;
    }
    void *e = a->data + a->count * a->elem;
    memset(e, 0, a->elem);
    a->count++;
    return e;
}

/* Removes element i; returns the index to go on from (i - 1, or i for the last one,
 * or 0 when i was out of range) */
uint32_t __stdcall array_remove(RtArray *a, uint32_t i)
{
    if (i >= a->count)
        return 0;
    a->count--;
    if (a->count != i) {
        uint8_t *e = a->data + a->elem * i;
        memcpy(e, e + a->elem, a->elem * (a->count - i));
    }
    return i < a->count ? i - 1 : i;
}

void __stdcall array_free(RtArray *a)
{
    if (a->data)
        a->data = winfree(a->data);
    a->data = NULL;
    a->capacity = 0;
    a->count = 0;
}

void *__stdcall array_at(RtArray *a, uint32_t i)
{
    return i < a->count ? a->data + a->elem * i : NULL;
}

/* n: elements to allocate at once (0 none). The capacity field is not set, so the
 * first add allocates again. */
int __stdcall array_init(RtArray *a, uint32_t elem, uint32_t n, uint32_t grow)
{
    if (a->data)
        array_free(a);
    a->elem = elem;
    a->grow = grow ? grow : 1;
    if (!n)
        return 1;
    a->data = winmalloc(n * elem, 0);
    return a->data != NULL;
}

/* ---- error text (0x10002010, 0x100020d6) ---- */

/* "RtError:NNNN " plus the message for code 1-3, to the debug output. (The text is then
 * used as a format string once more, as in the original.) */
void __cdecl rt_error(int code, ...)
{
    static const char *const text[] = {"plugin '%s' has no RtLibInit function", "plugin '%s' initialization failed",
                                       "extern function '%s' not found"};
    char buf[256];
    wsprintfA(buf, "RtError:%04i ", code);
    if (code >= 1 && code <= 3) {
        va_list args;
        va_start(args, code);
        wvsprintfA(buf + lstrlenA(buf), text[code - 1], args);
        va_end(args);
    }
    DebugStr(buf);
}

void __cdecl rt_debug_str(LPCSTR s)
{
    DebugStr(s);
}

/* ---- screen capture (0x10002100-0x10002910) ---- */

/* 0x10002542: size of a bitmap header's color table */
int __stdcall bmi_colors_size(const uint8_t *bi)
{
    int n = 0;
    if (P(int32_t, bi, 0x20))
        n = P(int32_t, bi, 0x20);
    else if (P(int16_t, bi, 0xe) == 1)
        n = 2;
    else if (P(int16_t, bi, 0xe) == 4)
        n = 0x10;
    else if (P(int16_t, bi, 0xe) == 8)
        n = 0x100;
    return n << 2;
}

/* 0x100022ba: the part of the screen in r (which it puts in order) as a bitmap */
HBITMAP __stdcall screen_grab_bitmap(int32_t *r)
{
    HBITMAP bm = NULL;
    if (r[2] < r[0]) {
        int32_t t = r[0];
        r[0] = r[2], r[2] = t;
    }
    if (r[3] < r[1]) {
        int32_t t = r[1];
        r[1] = r[3], r[3] = t;
    }
    HDC screen = GetDC(NULL);
    if (!screen)
        return NULL;
    HDC mem = CreateCompatibleDC(screen);
    if (mem) {
        int sw = GetDeviceCaps(screen, HORZRES), sh = GetDeviceCaps(screen, VERTRES);
        int x0 = r[0] < 0 ? 0 : r[0], y0 = r[1] < 0 ? 0 : r[1];
        int x1 = sw < r[2] ? sw : r[2], y1 = sh < r[3] ? sh : r[3];
        bm = CreateCompatibleBitmap(screen, x1 - x0, y1 - y0);
        if (bm) {
            HGDIOBJ old = SelectObject(mem, bm);
            BitBlt(mem, 0, 0, x1 - x0, y1 - y0, screen, x0, y0, SRCCOPY);
            SelectObject(mem, old);
        }
        DeleteDC(mem);
    }
    ReleaseDC(NULL, screen);
    return bm;
}

/* 0x10002488: a GDI palette with the engine's colors */
HPALETTE __cdecl palette_copy(void)
{
    int n = R(uint8_t, RT_PALETTE_DEVICE) ? GetDeviceCaps(R(HDC, RT_DC), SIZEPALETTE) : 0x100;
    LOGPALETTE *lp = winmalloc(n * 4 + 8, 1);
    if (!lp)
        return NULL;
    lp->palVersion = 0x300;
    lp->palNumEntries = (WORD)n;
    SpriteGetPalette(HSL, lp->palPalEntry, 0, 0x100);
    HPALETTE pal = CreatePalette(lp);
    winfree(lp);
    return pal;
}

/* 0x100021a2: an 8-bit packed DIB of bm (the palette argument is not used) */
BITMAPINFO *__stdcall bitmap_to_dib(HBITMAP bm, uint32_t unused)
{
    (void)unused;
    BITMAP info;
    if (!GetObjectA(bm, sizeof info, &info))
        return NULL;
    BITMAPINFOHEADER h;
    memset(&h, 0, sizeof h);
    h.biSize = sizeof h;
    h.biWidth = info.bmWidth;
    h.biHeight = info.bmHeight;
    h.biPlanes = 1;
    h.biBitCount = 8;
    h.biCompression = 0;
    h.biSizeImage = ((info.bmWidth + 3) / 4) * 4 * info.bmHeight;
    SIZE_T size = h.biSizeImage + bmi_colors_size((const uint8_t *)&h) + h.biSize;
    BITMAPINFO *bi = winmalloc(size, 0);
    if (!bi)
        return NULL;
    bi->bmiHeader = h;
    uint8_t *bits = (uint8_t *)bi + bmi_colors_size((const uint8_t *)bi) + (uint16_t)bi->bmiHeader.biSize;
    if (!GetDIBits(R(HDC, RT_DC), bm, 0, h.biHeight & 0xffff, bits, bi, DIB_RGB_COLORS))
        return winfree(bi);
    return bi;
}

/* 0x10002100: the part of the screen in r as a packed DIB; a message box when it fails */
BITMAPINFO *__stdcall screen_grab(int32_t *r)
{
    BITMAPINFO *bi = NULL;
    MemSetNotify(0);
    HBITMAP bm = screen_grab_bitmap(r);
    if (bm) {
        HPALETTE pal = palette_copy();
        bi = bitmap_to_dib(bm, (uint32_t)(uintptr_t)pal);
        if (pal)
            DeleteObject(pal);
        DeleteObject(bm);
    }
    MemSetNotify((uint32_t)(uintptr_t)R(HWND, RT_HWND));
    if (!bi)
        rt_message_box(R(HWND, RT_HWND), 0x6d, 0);
    return bi;
}

static int bmp_write(LPSTR path, const void *file_head, const void *a, LONG na, const void *b, LONG nb,
                     const void *c, LONG nc, int exact)
{
    HMMIO f = mmioOpenA(path, NULL, MMIO_CREATE | MMIO_WRITE);
    if (!f)
        return -1;
    int ok;
    if (exact) /* 0x100027e9 checks the byte counts, 0x100025d4 only for 0 */
        ok = mmioWrite(f, file_head, 0xe) == 0xe && mmioWrite(f, a, na) == na;
    else
        ok = mmioWrite(f, file_head, 0xe) && mmioWrite(f, a, na) && mmioWrite(f, b, nb) && mmioWrite(f, c, nc);
    mmioClose(f, 0);
    if (!ok)
        mmioOpenA(path, NULL, MMIO_DELETE);
    return ok;
}

#pragma pack(push, 1)
typedef struct {
    char type[2];
    int32_t size;
    int32_t reserved;
    int32_t off_bits;
} BmpFileHead;
#pragma pack(pop)

/* 0x100025d4: saves an engine bitmap as an 8-bit .bmp with the engine's palette. The
 * palette buffer is only freed when the file can't be created. */
int __stdcall dib_save_bmp(uint8_t *bm, LPSTR path)
{
    uint8_t *pal = winmalloc(0x400, 1);
    if (!pal)
        return 0;
    for (uint32_t i = 0; i < 0x100; i++) {
        pal[i * 4 + 2] = R(uint8_t, RT_PALETTE + i * 4);
        pal[i * 4 + 1] = R(uint8_t, RT_PALETTE + i * 4 + 1);
        pal[i * 4] = R(uint8_t, RT_PALETTE + i * 4 + 2);
    }
    int32_t bytes = P(int32_t, bm, 8) * P(int32_t, bm, 0xc);
    BITMAPINFOHEADER h;
    memset(&h, 0, sizeof h);
    h.biSize = sizeof h;
    h.biWidth = P(int32_t, bm, 4);
    h.biHeight = P(int32_t, bm, 0xc);
    h.biPlanes = 1;
    h.biBitCount = 8;
    h.biSizeImage = bytes;
    BmpFileHead fh;
    memset(&fh, 0, sizeof fh);
    fh.type[0] = 'B', fh.type[1] = 'M';
    fh.off_bits = 0x400 + 0x36;
    fh.size = fh.off_bits + bytes;
    int ok = bmp_write(path, &fh, &h, 0x28, pal, 0x400, P(void *, bm, 0x20), bytes, 0);
    if (ok < 0) {
        winfree(pal);
        return 0;
    }
    return ok;
}

/* 0x100027e9: saves a packed DIB as a .bmp */
int __stdcall bmi_save_bmp(int32_t *bi, LPSTR path)
{
    int32_t head = bi[0] + bmi_colors_size((const uint8_t *)bi);
    int32_t bits = P(uint16_t, bi, 0xe) * bi[1] + 0x1f;
    int32_t image = (bits / 32) * bi[2] * 4;
    bi[5] = image;
    BmpFileHead fh;
    memset(&fh, 0, sizeof fh);
    fh.type[0] = 'B', fh.type[1] = 'M';
    fh.size = image + head + 0xe;
    fh.off_bits = head + 0xe;
    int ok = bmp_write(path, &fh, bi, image + head, NULL, 0, NULL, 0, 1);
    return ok < 0 ? 0 : ok;
}

/* ---- background transitions (0x10002910-0x10003039) ----
 * The new picture is copied into the current background bit by bit, painting each
 * piece, with the sprites animating in between. */

#define g_fx_dst G(uint8_t *, 0x1007c400)
#define g_fx_src G(uint8_t *, 0x1007c404)

/* 0x10002d33: one piece */
void __stdcall fx_show(const int32_t *r)
{
    SpriteCopyRect(g_fx_dst, g_fx_src, (int32_t *)r);
    SpritePaintRect(HSL, (const RECT *)r, 1);
}

/* 0x10002d71: the strips left and right of b, between a's sides */
void __stdcall fx_sides(const int32_t *a, const int32_t *b)
{
    int32_t r[4] = {a[0], b[1], b[0], b[3]};
    fx_show(r);
    r[0] = b[2];
    r[2] = a[2];
    fx_show(r);
}

/* 0x10002dcc: the strips above and below b, as wide as a */
void __stdcall fx_bands(const int32_t *a, const int32_t *b)
{
    int32_t r[4] = {a[0], a[1], a[2], b[1]};
    fx_show(r);
    r[1] = b[3];
    r[3] = a[3];
    fx_show(r);
}

/* 0x10002e28: the rectangle of a step, 0 the whole background, steps - 1 the smallest */
void __stdcall fx_rect(int step, int32_t *r, int steps, int sides)
{
    uint32_t div = (uint32_t)(steps * 2 - 1);
    r[0] = sides ? (int32_t)((uint32_t)(R(int32_t, RT_BKG_W) * step) / div) : 0;
    r[2] = R(int32_t, RT_BKG_W) - r[0];
    r[1] = (int32_t)((uint32_t)(R(int32_t, RT_BKG_H) * step) / div);
    r[3] = R(int32_t, RT_BKG_H) - r[1];
}

/* 0x10002ab2: blinds: every n-th row, then the rows after them, and so on */
void __stdcall fx_blinds(uint32_t n)
{
    if (!n)
        n = 10;
    int32_t h = R(int32_t, RT_BKG_H);
    int32_t r[4] = {0, 0, R(int32_t, RT_BKG_W), 0};
    for (uint32_t first = 0; first < n; first++) {
        uint32_t row = first;
        for (uint32_t i = 0; i < (uint32_t)(h - 1 + (int32_t)n) / n && (int32_t)row <= R(int32_t, RT_BKG_H); i++) {
            r[1] = (int32_t)row;
            r[3] = (int32_t)row + 1;
            fx_show(r);
            row += n;
            SpriteAnimate(HSL);
        }
    }
}

/* 0x10002b90: from the edges inwards */
void __stdcall fx_close(uint32_t steps, int sides)
{
    int32_t r[4], outer[4];
    fx_rect((int)steps - 1, r, (int)steps, sides);
    fx_rect(0, outer, (int)steps, sides);
    for (uint32_t i = 1; i < steps; i++) {
        fx_rect((int)i, r, (int)steps, sides);
        fx_bands(outer, r);
        if (sides)
            fx_sides(outer, r);
        SpriteAnimate(HSL);
    }
    fx_show(r);
}

/* 0x10002c5d: from the middle outwards */
void __stdcall fx_open(int steps, int sides)
{
    int32_t first[4], r[4];
    fx_rect(steps - 1, first, steps, sides);
    fx_show(first);
    for (int i = steps - 2; i >= 0; i--) {
        SpriteAnimate(HSL);
        fx_rect(i, r, steps, sides);
        if (sides)
            fx_sides(r, first);
        fx_bands(r, first);
    }
}

/* 0x10002ebc: rolls down in bands of n rows over about ms milliseconds */
void __stdcall fx_roll(uint32_t n, uint32_t ms)
{
    uint32_t h = R(uint32_t, RT_BKG_H);
    if (!n)
        n = 10;
    else if (h < n)
        n = h;
    int32_t r[4] = {0, 0, R(int32_t, RT_BKG_W), 0};
    uint32_t stride = n == 1 ? 1 : n - 1;
    uint32_t done = 0, end = stride;
    uint32_t next = timeGetTime();
    while (done < R(uint32_t, RT_BKG_H)) {
        if (timeGetTime() < next) {
            SpriteAnimate(HSL);
            continue;
        }
        next += ms / ((n + h / n) - 1);
        for (uint32_t row = done; row < end; row += stride) {
            r[1] = (int32_t)row;
            r[3] = (int32_t)row + 1;
            fx_show(r);
            SpriteAnimate(HSL);
        }
        done += done < stride ? 1 : n;
        uint32_t e = n + end;
        end = e <= R(uint32_t, RT_BKG_H) ? e : R(uint32_t, RT_BKG_H);
    }
}

/* 0x10003039 and 0x1000316e: sweep across in n strips over about ms milliseconds, from
 * the left or top (dir 1) or from the right or bottom (dir -1) */
static void fx_slide(uint32_t n, uint32_t ms, int dir, int vertical)
{
    uint32_t len = R(uint32_t, vertical ? RT_BKG_H : RT_BKG_W);
    if (!n)
        n = 0x3c;
    else if (len < n)
        n = len;
    int32_t step = (int32_t)(len / n) * dir;
    int32_t r[4];
    int lo = vertical ? 1 : 0, hi = lo + 2;
    r[lo] = step < 0 ? (int32_t)len + step : 0;
    r[hi] = step < 0 ? (int32_t)len : step;
    r[1 - lo] = 0;
    r[3 - lo] = R(int32_t, vertical ? RT_BKG_W : RT_BKG_H);
    uint32_t wait = ms / n, next = timeGetTime();
    while ((uint32_t)r[lo] < R(uint32_t, vertical ? RT_BKG_H : RT_BKG_W) && r[hi] >= 0) {
        SpriteAnimate(HSL);
        if (next <= timeGetTime()) {
            next += wait;
            fx_show(r);
            r[lo] += step;
            r[hi] += step;
        }
    }
}

void __stdcall fx_slide_x(uint32_t n, uint32_t ms, int dir)
{
    fx_slide(n, ms, dir, 0);
}

void __stdcall fx_slide_y(uint32_t n, uint32_t ms, int dir)
{
    fx_slide(n, ms, dir, 1);
}

/* 0x10002910: copies src into dst with transition params[0] (1 blinds, 2-5 open or
 * close with or without sides, 6 roll, 7-10 slides). Sprite messages are off meanwhile. */
void __stdcall bg_effect(uint32_t dst, uint32_t src, const uint32_t *params)
{
    g_fx_dst = (uint8_t *)(uintptr_t)dst;
    g_fx_src = (uint8_t *)(uintptr_t)src;
    int msgs = SpriteSetSendMsg(HSL, 0);
    switch (params[0]) {
    case 1:
        fx_blinds(params[1]);
        break;
    case 2:
        fx_close(0x1e, 1);
        break;
    case 3:
        fx_open(0x1e, 1);
        break;
    case 4:
        fx_close(0x1e, 0);
        break;
    case 5:
        fx_open(0x1e, 0);
        break;
    case 6:
        fx_roll(params[1], params[2]);
        break;
    case 7:
        fx_slide_x(params[1], params[2], -1);
        break;
    case 8:
        fx_slide_x(params[1], params[2], 1);
        break;
    case 9:
        fx_slide_y(params[1], params[2], -1);
        break;
    case 10:
        fx_slide_y(params[1], params[2], 1);
        break;
    }
    SpriteSetSendMsg(HSL, msgs);
}
