/* Engine core, 0x100278d0-0x10028370: run mode 1 (/w), "7th Level Living Wall Paper":
 * the engine window lies behind all others as an animated desktop, and a dragged
 * rectangle (or the whole screen) can be saved as a .bmp in the Windows directory and
 * made the Windows wallpaper. Also screen captures to numbered files. Dominion uses
 * none of it. Rewritten from the original. */
#include "rtcore.h"
#include "crt.h"
#include "rt.h"
#include "rt_wallpaper.h"

#define RU(off) R(U, off)
#define RB(off) R(uint8_t, off)

#define RT_WNDPROC 0x0c8c
#define RT_RUN_MODE 0x3e5b
#define RT_NO_ZORDER 0x95d3
#define RT_BAND_MOVES 0x95f5 /* dragging moves the rectangle instead of sizing it */
#define RT_RUNNING 0x95f6
#define RT_TRACKING_RECT 0x95fb

/* the rubber band: its rectangle, the last mouse position, the anchor, saved DC state */
#define g_band ((RECT *)ORIG_PTR(0x100871b0))
#define g_band_last_x G(int, 0x100871c0)
#define g_band_last_y G(int, 0x100871c4)
#define g_band_anchor_x G(int, 0x100871c8)
#define g_band_anchor_y G(int, 0x100871cc)
#define g_band_rop G(int, 0x100871d0)
#define g_band_old_brush G(HGDIOBJ, 0x100871d4)
#define g_band_old_pen G(HGDIOBJ, 0x100871d8)
#define g_band_pen G(HPEN, 0x100871dc)

#define g_module G(HMODULE, 0x1007cef0)
#define crt_abs(v) ((v) < 0 ? -(v) : (v))

/* 0x100278d0: "<name>NNNN.bmp" in the current directory (name cut to 4 characters;
 * without one the engine's own, string 201) */
void __stdcall capture_name(char *out, char *name, U unused, U n)
{
    (void)unused;
    char dir[0x100], file[0x100], def[0x100];
    GetCurrentDirectoryA(0x100, dir);
    if (!name) {
        LoadStringA(g_module, 0xc9, def, 0x100);
        def[4] = 0;
        wsprintfA(file, "%s%04i", def, n);
    } else {
        name[4] = 0;
        wsprintfA(file, "%s%04i", name, n);
    }
    lstrcatA(file, ".bmp");
    MakeFullPath(out, dir, file);
}

/* 0x10027a91: draws the band (XOR) */
void __stdcall band_draw(const RECT *r)
{
    RECT b = *r;
    OffsetRect(&b, -R(int, 0x36c7), -R(int, 0x36cb));
    Rectangle(R(HDC, RT_DC), b.left, b.top, b.right, b.bottom);
}

/* 0x1002799a: starts a rubber band at (x, y); the game stops meanwhile */
void __stdcall band_start(int x, int y)
{
    RB(RT_TRACKING_RECT) = 1;
    RB(RT_RUNNING) = 0;
    g_band_rop = SetROP2(R(HDC, RT_DC), R2_XORPEN);
    g_band_pen = CreatePen(PS_SOLID, 0, 0xffff);
    if (g_band_pen)
        g_band_old_pen = SelectObject(R(HDC, RT_DC), g_band_pen);
    g_band_old_brush = SelectObject(R(HDC, RT_DC), GetStockObject(NULL_BRUSH));
    g_band_anchor_x = g_band_last_x = g_band->right = g_band->left = x;
    g_band_anchor_y = g_band_last_y = g_band->bottom = g_band->top = y;
    band_draw(g_band);
}

/* 0x10028012: the mouse moved to (x, y) */
void __stdcall band_move(int x, int y)
{
    band_draw(g_band);
    if (!RB(RT_BAND_MOVES)) {
        SetRect(g_band, g_band_anchor_x, g_band_anchor_y, x, y);
    } else {
        g_band_anchor_x += x - g_band_last_x;
        g_band_anchor_y += y - g_band_last_y;
        OffsetRect(g_band, x - g_band_last_x, y - g_band_last_y);
    }
    g_band_last_x = x;
    g_band_last_y = y;
    band_draw(g_band);
}

/* 0x10027d97: the first free "<name>NNNN.bmp" (1-100) in the Windows directory, or
 * "<name>.bmp" */
void __stdcall wallpaper_name(char *out, U unused)
{
    (void)unused;
    char dir[0x100], file[0x100], name[0x100];
    GetWindowsDirectoryA(dir, 0x100);
    LoadStringA(g_module, 0xc9, name, 0x100);
    name[4] = 0;
    for (uint16_t n = 0;;) {
        if (n > 99) {
            lstrcpyA(file, name);
            lstrcatA(file, ".bmp");
            MakeFullPath(out, dir, file);
            return;
        }
        n++;
        wsprintfA(file, "%s%04i", name, n);
        lstrcatA(file, ".bmp");
        MakeFullPath(out, dir, file);
        if (!mmioOpenA(out, NULL, MMIO_EXIST))
            return;
    }
}

/* 0x10027eba: makes `path` the Windows wallpaper, tiled or not */
void __stdcall wallpaper_set(char *path, U tiled)
{
    char *file = SkipPath(path);
    const char *tile = tiled ? "1" : "0";
    WriteProfileStringA("Desktop", "TileWallPaper", tile);
    WriteProfileStringA("Desktop", "Wallpaper", file);
    HKEY key;
    if (!RegOpenKeyA(HKEY_CURRENT_USER, "Control Panel\\Desktop", &key)) {
        RegSetValueExA(key, "TileWallPaper", 0, REG_SZ, (const BYTE *)tile, 1);
        RegSetValueExA(key, "Wallpaper", 0, REG_SZ, (const BYTE *)file, (DWORD)lstrlenA(file));
        RegCloseKey(key);
    }
    SendMessageA(HWND_BROADCAST, WM_WININICHANGE, 0, (LPARAM)ORIG_PTR(0x10075698));
    SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, file, 0);
    if (!file || !*file)
        InvalidateRect(NULL, NULL, TRUE);
    SpriteRealizePalette(R(uint8_t *, RT_HSL));
}

/* 0x10027d22: captures `r` (screen coordinates) and makes it the wallpaper */
void __stdcall wallpaper_capture(int32_t *r, U tiled)
{
    char path[0x100];
    BITMAPINFO *bmi = screen_grab(r);
    if (!bmi)
        return;
    wallpaper_name(path, 0x100);
    if (bmi_save_bmp((int32_t *)bmi, path))
        wallpaper_set(path, tiled);
    winfree(bmi);
}

static void client_screen_rect(RECT *r)
{
    GetClientRect(R(HWND, RT_HWND), r);
    ClientToScreen(R(HWND, RT_HWND), (POINT *)r);
    ClientToScreen(R(HWND, RT_HWND), (POINT *)r + 1);
}

/* 0x10027bbd: ends the band; unless cancelled, the band (tiled) or, when it is under
 * 5 pixels, the whole window becomes the wallpaper */
void __stdcall band_end(U cancel)
{
    band_draw(g_band);
    SetROP2(R(HDC, RT_DC), g_band_rop);
    if (g_band_old_pen)
        SelectObject(R(HDC, RT_DC), g_band_old_pen);
    if (g_band_old_brush)
        SelectObject(R(HDC, RT_DC), g_band_old_brush);
    if (g_band_pen)
        DeleteObject(g_band_pen);
    RB(RT_TRACKING_RECT) = 0;
    RB(RT_RUNNING) = 1;
    if (cancel)
        return;
    int w = g_band->right - g_band->left, h = g_band->top - g_band->bottom;
    if (crt_abs(w) < 5 || crt_abs(h) < 5) {
        RECT r;
        client_screen_rect(&r);
        wallpaper_capture((int32_t *)&r, 0);
    } else {
        wallpaper_capture((int32_t *)g_band, 1);
    }
}

/* 0x10027b0a: saves the window as "<name>NNNN.bmp" in the current directory */
void __stdcall screen_capture_file(char *name, U n)
{
    RECT r;
    char path[0x100];
    client_screen_rect(&r);
    BITMAPINFO *bmi = screen_grab((int32_t *)&r);
    if (!bmi)
        return;
    capture_name(path, name, 0x100, n);
    bmi_save_bmp((int32_t *)bmi, path);
    winfree(bmi);
}

/* 0x100280c1: closes another living wallpaper */
BOOL __stdcall wallpaper_close_other(HWND hwnd, LPARAM unused)
{
    (void)unused;
    char title[0x80], cls[0x80];
    if (R(HWND, RT_HWND) != hwnd && GetWindowTextA(hwnd, title, 0x80) && GetClassNameA(hwnd, cls, 0x80) &&
        !lstrcmpA(cls, "VIEW_CLASS") && !lstrcmpA(title, (LPCSTR)ORIG_PTR(0x10075678)))
        SendMessageA(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

/* 0x10027feb: only one living wallpaper runs */
void __cdecl wallpaper_single(void) { EnumWindows(wallpaper_close_other, R(LPARAM, RT_HWND)); }

/* 0x1002818a: run mode 1's window procedure: stays at the bottom of the window order
 * and covers the screen; a double click opens the Windows task list */
LRESULT __stdcall wallpaper_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND z = RU(RT_RUN_MODE) == 1 ? HWND_BOTTOM : HWND_TOPMOST;
    int handled = 0;
    if (msg == WM_PAINT) {
        if (!RB(RT_NO_ZORDER))
            SetWindowPos(hwnd, z, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    } else if (msg == WM_WINDOWPOSCHANGED) {
        handled = !RB(RT_NO_ZORDER);
        if (handled)
            SetWindowPos(hwnd, z, -1, -1, R(int, 0x3de3) + 1, R(int, 0x3e0f) + 1, SWP_NOACTIVATE);
    } else if (msg == WM_LBUTTONDBLCLK) {
        PostMessageA(GetDesktopWindow(), WM_SYSCOMMAND, SC_TASKLIST, 0);
        handled = 1;
    }
    if (handled)
        return 0;
    return CallWindowProcA(R(WNDPROC, RT_WNDPROC), hwnd, msg, wp, lp);
}
