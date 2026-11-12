/* Engine core, 0x10017f60-0x10018ea0: device bitmaps into sprite bitmaps (with their
 * colors added to the palette), a file's icon drawn into a sprite, and printing a
 * bitmap resource on the default printer. Dominion uses none of it (text objects use
 * the first). Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_print.h"

#define RU(off) R(U, off)
#define RB(off) R(uint8_t, off)
#define HSL R(uint8_t *, RT_HSL)

#define RT_TRANSPARENT 0x95ed /* icons get a transparent background */
#define RT_COLORS 0x3e3b      /* colors added after the 10 fixed ones */
#define RT_COLORS_KEPT 0x3df7 /* palette entries the added colors may not take */
#define FIRST_COLOR 10

#define g_map ((uint8_t *)ORIG_PTR(0x1007c5d0)) /* old color -> new color */
#define g_hist ((U *)ORIG_PTR(0x1007c6d0))      /* pixels of each color */
#define g_printing G(U, 0x1007cad0)
#define g_print_ok G(U, 0x1007cad4) /* cleared when the print is cancelled */

#define g_module G(HMODULE, 0x1007cef0)
#define crt_strrchr ORIG(char *(__cdecl *)(const char *, int), 0x100292b0)

static int iabs(int v) { return v < 0 ? -v : v; }

static uint8_t *pal(U i) { return g_rt + RT_PALETTE + i * 4; }

/* 0x10018209: counts the pixels of each color */
void __stdcall dib_histogram(uint8_t *dib)
{
    U w = P(U, dib, 4), stride = P(U, dib, 8), h = P(U, dib, 0xc);
    uint8_t *p = dib + 0x24;
    memset(g_hist, 0, 0x400);
    for (U n = h * stride; n; n -= stride) {
        for (U x = 0; x < w; x++)
            g_hist[*p++]++;
        p += stride - w;
    }
}

/* 0x100186a5: the palette entry already added for `rgb`, or -1 */
int __stdcall color_find(U rgb)
{
    U want = (rgb & 0xffffff) | 0x01000000;
    U *e = (U *)pal(FIRST_COLOR);
    for (U i = 0; i < RU(RT_COLORS); i++)
        if (e[i] == want)
            return (int)(i + FIRST_COLOR);
    return -1;
}

/* 0x10018655: adds `entry` to the palette */
int __stdcall color_add(U entry)
{
    U n = RU(RT_COLORS);
    ((U *)pal(FIRST_COLOR))[n] = entry;
    RU(RT_COLORS) = n + 1;
    return (int)(n + FIRST_COLOR);
}

/* 0x10018715: the added color nearest to `rgb` (sum of differences); 0xff if none */
U __stdcall color_nearest(U rgb)
{
    int best = 1000;
    U found = 0xff;
    for (U i = FIRST_COLOR; i < RU(RT_COLORS) + FIRST_COLOR; i++) {
        int d = iabs((int)pal(i)[2] - (int)(rgb >> 16 & 0xff)) + iabs((int)pal(i)[1] - (int)(rgb >> 8 & 0xff)) +
                iabs((int)pal(i)[0] - (int)(rgb & 0xff));
        if (d < best) {
            found = i;
            best = d;
        }
    }
    return found;
}

/* 0x100185c9: a palette index for `rgb`: the added one, a new one while there is room,
 * or the nearest */
U __stdcall color_index(U rgb)
{
    U entry = (rgb & 0xffffff) | 0x01000000;
    int i = color_find(entry);
    if (i < 0 && 0xecu - RU(RT_COLORS_KEPT) > RU(RT_COLORS))
        i = color_add(entry);
    if (i < 0)
        i = (int)color_nearest(entry);
    return (uint8_t)i;
}

/* 0x10018121: maps the used colors, most used first, to palette entries; with
 * `transparent`, color `key` becomes 0 */
void __stdcall colors_map(const RGBQUAD *colors, U transparent, U key)
{
    for (;;) {
        U most = 0, c = 0;
        for (U i = 0; i < 0x100; i++)
            if (g_hist[i] > most) {
                most = g_hist[i];
                c = i;
            }
        if (!most)
            return;
        if (transparent && (key & 0xff) == c)
            g_map[c] = 0;
        else
            g_map[c] = (uint8_t)color_index(colors[c].rgbRed | colors[c].rgbGreen << 8 | colors[c].rgbBlue << 16);
        g_hist[c] = 0;
    }
}

/* 0x100180a5: moves the bitmap's colors into the palette; nonzero when it grew */
int __stdcall dib_remap(uint8_t *dib, const RGBQUAD *colors, U transparent, U key)
{
    U before = RU(RT_COLORS);
    dib_histogram(dib);
    colors_map(colors, transparent, key);
    MapBytes(P(uint8_t *, dib, 0x20), P(int, dib, 8) * P(int, dib, 0xc), g_map);
    return RU(RT_COLORS) != before;
}

/* 0x10017f60: a sprite bitmap from a device bitmap; `remap` takes its colors into the
 * palette, otherwise it is read as palette indexes */
uint8_t *__stdcall dib_from_bitmap(HBITMAP bm, U remap, U transparent, U key)
{
    BITMAP info;
    if (!GetObjectA(bm, sizeof info, &info))
        return NULL;
    BITMAPINFO *bmi = winmalloc(0x428, 1);
    if (!bmi)
        return NULL;
    bmi->bmiHeader.biSize = 0x28;
    bmi->bmiHeader.biWidth = info.bmWidth;
    bmi->bmiHeader.biHeight = info.bmHeight;
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biBitCount = 8;
    bmi->bmiHeader.biCompression = 0;
    uint8_t *dib = SpriteDibCreate(info.bmWidth, info.bmHeight, 0, 8);
    if (dib) {
        int rgb = (remap & 0xff) != 0;
        GetDIBits(R(HDC, RT_DC), bm, 0, (UINT)info.bmHeight, dib + 0x24, bmi, rgb ? DIB_RGB_COLORS : DIB_PAL_COLORS);
        if (rgb && dib_remap(dib, bmi->bmiColors, transparent & 0xff, key)) {
            SpriteSetPalette(HSL, g_rt + RT_PALETTE);
            SpriteRealizePalette(HSL);
        }
    }
    winfree(bmi);
    return dib;
}

/* 0x100183f8: a w x h device bitmap with the icon drawn on white (or on the
 * transparent color) */
HBITMAP __stdcall icon_bitmap(HICON icon, HDC dc, int w, int h)
{
    HBITMAP bm = CreateCompatibleBitmap(R(HDC, RT_DC), w, h);
    if (!bm)
        return NULL;
    HPALETTE old_pal = SpriteSelectPalette(HSL, dc);
    HGDIOBJ old = SelectObject(dc, bm);
    HBRUSH brush = CreateSolidBrush(RB(RT_TRANSPARENT) ? PALETTEINDEX(FIRST_COLOR) : RGB(0xff, 0xff, 0xff));
    if (brush) {
        RECT r = {0, 0, w, h};
        FillRect(dc, &r, brush);
        DeleteObject(brush);
    }
    DrawIcon(dc, 0, 0, icon);
    if (old)
        SelectObject(dc, old);
    if (old_pal)
        SelectPalette(dc, old_pal, FALSE);
    return bm;
}

/* 0x10018352: the icon as a w x h sprite bitmap */
uint8_t *__stdcall icon_dib(HICON icon, int w, int h)
{
    HDC dc = CreateCompatibleDC(R(HDC, RT_DC));
    if (!dc)
        return NULL;
    uint8_t *dib = NULL;
    HBITMAP bm = icon_bitmap(icon, dc, w, h);
    if (bm) {
        dib = dib_from_bitmap(bm, 1, RB(RT_TRANSPARENT), FIRST_COLOR);
        DeleteObject(bm);
    }
    DeleteDC(dc);
    return dib;
}

/* 0x10018520: a .ico file, or icon `index` of a program; the engine's own if none */
HICON __stdcall icon_load(char *path, UINT index)
{
    HICON icon;
    if (!lstrcmpiA(crt_strrchr(path, '.'), ".ico")) {
        icon = LoadImageA(NULL, path, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    } else {
        icon = ExtractIconA(g_module, path, index);
        if ((uintptr_t)icon < 2)
            icon = NULL;
    }
    if (!icon)
        icon = LoadIconA(g_module, MAKEINTRESOURCEA(0x3f0));
    return icon;
}

/* 0x100182b0: draws the icon of `path` over the sprite bitmap */
U __stdcall dib_draw_icon(uint8_t *dib, char *path, UINT index)
{
    HICON icon = icon_load(path, index);
    if (!icon)
        return 0;
    int w = P(int, dib, 4), h = P(int, dib, 0xc);
    uint8_t *src = icon_dib(icon, w, h);
    DeleteObject(icon);
    if (!src)
        return 0;
    SpriteDibMerge((uint32_t)(uintptr_t)dib, (uint32_t)(uintptr_t)src, w, h, 0, 0, 0, 0);
    winfree(src);
    return 1;
}

/* 0x10018800: the printer's abort procedure: keeps sprites moving; an error cancels */
BOOL __stdcall print_abort(HDC dc, int error)
{
    (void)dc;
    if (error && g_print_ok)
        g_print_ok = 0;
    SpriteAnimate(HSL);
    return (BOOL)g_print_ok;
}

/* 0x10018920: a DC for the default printer ("device" in WIN.INI's [windows]) in
 * `orientation` (1 portrait, 2 landscape). The original crashed without a printer;
 * here it then fails. */
HDC __stdcall printer_dc(U orientation)
{
    char dev[100];
    GetProfileStringA("windows", "device", "", dev, 100);
    char *driver = dev, *port;
    while (*driver && *driver != ',')
        driver++;
    if (!*driver)
        return NULL;
    *driver++ = 0;
    for (port = driver; *port && *port != ','; port++) {}
    if (!*port)
        return NULL;
    *port++ = 0;

    HANDLE printer = NULL;
    OpenPrinterA(dev, &printer, NULL);
    LONG size = DocumentPropertiesA(NULL, printer, dev, NULL, NULL, 0);
    if (size < 0)
        return NULL;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, (SIZE_T)size);
    if (!mem)
        return NULL;
    DEVMODEA *dm = GlobalLock(mem), in = {0};
    lstrcpynA((LPSTR)in.dmDeviceName, dev, sizeof in.dmDeviceName);
    in.dmSpecVersion = 0x300;
    in.dmSize = 0x9c;
    DocumentPropertiesA(NULL, printer, dev, dm, &in, DM_OUT_BUFFER);
    dm->dmOrientation = (short)orientation;
    HDC dc = CreateDCA(driver, dev, port, dm);
    if (dc)
        SetAbortProc(dc, print_abort);
    ClosePrinter(printer);
    GlobalUnlock(mem);
    GlobalFree(mem);
    return dc;
}

/* 0x10018da3: a BITMAPINFO with the palette, color 0 white */
BITMAPINFO *__stdcall print_bmi(uint8_t *dib)
{
    BITMAPINFO *bmi = winmalloc(0x42c, 1);
    if (!bmi)
        return NULL;
    bmi->bmiHeader.biSize = 0x28;
    bmi->bmiHeader.biWidth = P(LONG, dib, 8);
    bmi->bmiHeader.biHeight = P(LONG, dib, 0xc);
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biBitCount = 8;
    bmi->bmiColors[0].rgbRed = bmi->bmiColors[0].rgbGreen = bmi->bmiColors[0].rgbBlue = 0xff;
    for (U i = 1; i < 0x100; i++) {
        bmi->bmiColors[i].rgbRed = pal(i)[0];
        bmi->bmiColors[i].rgbGreen = pal(i)[1];
        bmi->bmiColors[i].rgbBlue = pal(i)[2];
    }
    return bmi;
}

/* 0x10018b7e: prints the bitmap as one page, as large as fits, centred; nonzero
 * unless cancelled */
U __stdcall print_dib(HDC dc, uint8_t *dib)
{
    BITMAPINFO *bmi = print_bmi(dib);
    if (!bmi)
        return 0;
    g_printing = 1;
    g_print_ok = 1;
    DOCINFOA doc = {0};
    doc.cbSize = 0x14;
    doc.lpszDocName = (LPCSTR)(g_rt + (R(char, 0x9904) ? 0x9904 : 0x9d56));
    StartDocA(dc, &doc);
    StartPage(dc);
    int bw = P(int, dib, 4), bh = P(int, dib, 0xc);
    int ox = GetDeviceCaps(dc, PHYSICALOFFSETX), oy = GetDeviceCaps(dc, PHYSICALOFFSETY);
    int pw = GetDeviceCaps(dc, HORZRES) - ox * 2, ph = GetDeviceCaps(dc, VERTRES) - oy * 2;
    int w = pw, h = ph;
    if (ph * bw < pw * bh)
        w = ph * bw / bh;
    if (pw * bh < ph * bw)
        h = pw * bh / bw;
    StretchDIBits(dc, ox + (pw - w) / 2, oy + (ph - h) / 2, w, h, 0, 0, bw, bh, P(void *, dib, 0x20), bmi,
                  DIB_RGB_COLORS, SRCCOPY);
    winfree(bmi);
    EndPage(dc);
    if (!g_print_ok)
        AbortDoc(dc);
    else
        EndDoc(dc);
    g_printing = 0;
    return g_print_ok;
}

/* 0x10018aaa: prints bitmap resource `id` */
U __stdcall print_bitmap_dc(HDC dc, U id)
{
    if (!obj_is(id, 1))
        return 0;
    U *e = obj_entry(id);
    e[1]++;
    uint8_t *dib = (uint8_t *)(uintptr_t)e[0];
    U packed = P(U, dib, 0x10) & 0x80;
    if (packed)
        dib = ResExpandBitmapDib(R(ResFile *, RT_RES), (ResDib *)dib);
    U r = 0;
    if (dib) {
        r = print_dib(dc, dib);
        if (packed)
            winfree(dib);
    }
    if (--e[1] == 0)
        obj_release(id, 1);
    return r;
}

/* 0x10018849: prints bitmap resource `id`, across the page with `landscape` */
U __stdcall print_bitmap(U id, U landscape)
{
    if (g_printing)
        return 0;
    HDC dc = printer_dc(landscape ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT);
    if (!dc)
        return 0;
    int msgs = SpriteSetSendMsg(HSL, 0);
    U r = 0;
    if (ResGetType(R(ResFile *, RT_RES), id) == 1)
        r = print_bitmap_dc(dc, id);
    SpriteSetSendMsg(HSL, msgs);
    DeleteDC(dc);
    return r;
}
