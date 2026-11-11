/* Engine core, 0x1001d82a-0x1001e610: starting and shutting the engine down. RtStartup
 * (the game's first call) creates the state block, the window, the cursors and the
 * sound, sprite, resource and utility libraries, reads the engine's settings and
 * opens the game's resource file. Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_startup.h"

typedef uint32_t U;
#define C1(a, x) ORIG(U(__stdcall *)(U), a)((U)(x))
#define C2(a, x, y) ORIG(U(__stdcall *)(U, U), a)((U)(x), (U)(y))
#define C3(a, x, y, z) ORIG(U(__stdcall *)(U, U, U), a)((U)(x), (U)(y), (U)(z))
#define C0(a) ORIG(U(__cdecl *)(void), a)()
#define RU(off) R(U, off)

#define g_static_rt ((uint8_t *)ORIG_PTR(0x1007cef8)) /* the first state block */
#define g_static_used G(U, 0x1007ceec)
#define g_module G(HMODULE, 0x1007cef0)
#define RT_SIZE 0x9dd6

#define RT_WNDPROC 0x0c8c
#define RT_AUDIO 0x0c90
#define RT_CURSORS 0x0c9c       /* seven cursors: 0xc9c, 0xcb0, 0xcac, 0xcb4, 0xca8, 0xca4, 0xca0 */
#define RT_HRL 0x0cc4
#define RT_MODE 0x3dd3
#define RT_INPUT_TIMEOUT 0x3deb
#define RT_JOYSTICKS 0x3df3
#define RT_DROP_FRAMES 0x3dff
#define RT_INPUT_KIND 0x3e5b
#define RT_ON_EXIT 0x3e6f
#define RT_95CB 0x95cb
#define RT_ALLOW_DD 0x95cf
#define RT_ALT_TAB 0x95d0
#define RT_COMPONENT 0x95d6
#define RT_SCRIPT_DEBUG 0x95d8
#define RT_95DE 0x95de
#define RT_95E3 0x95e3
#define RT_95FE 0x95fe
#define RT_APP_NAME 0x9d56

static const U CURSOR_FIELDS[7] = {0xc9c, 0xcb0, 0xcac, 0xcb4, 0xca8, 0xca4, 0xca0};

/* 0x1001de3b: a cleared state block; the first one is static */
uint8_t *__cdecl rt_alloc(void)
{
    uint8_t *rt = g_static_used ? LocalAlloc(0, RT_SIZE) : g_static_rt;
    g_static_used = 1;
    memset(rt, 0, RT_SIZE);
    return rt;
}

/* 0x1001da9a */
U __stdcall rt_block_free(uint8_t *rt)
{
    if (rt) {
        if (rt == g_static_rt)
            g_static_used = 0;
        else
            LocalFree(rt);
    }
    return 0;
}

/* 0x1001de97: hands joystick n's calibration from SYSTEM.INI (joycal0/joycal1) to the
 * joystick driver */
void __stdcall joystick_calibrate(int n)
{
    uint32_t v[6], out[6];
    char line[200], fmt[100];
    HDRVR drv = OpenDriver(L"joystick", NULL, n);
    if (!drv)
        return;
    GetPrivateProfileStringA("joystick.drv", n ? "joycal1" : "joycal0", "", line, sizeof line, "system.ini");
    lstrcpyA(fmt, "%x %x %x %x %x %x");
    ORIG(int(__cdecl *)(const char *, const char *, ...), 0x10029970)(line, fmt, &v[0], &v[1], &v[2], &v[3], &v[4],
                                                                       &v[5]);
    SendDriverMessage(drv, 0x902, (LPARAM)v, (LPARAM)out);
    CloseDriver(drv, 0, 0);
}

/* 0x1001df85: the full-screen window, its DC and the engine's seven cursors */
U __stdcall window_create(U arg)
{
    if (!(uint16_t)C1(0x10026a57, arg))
        return 0;
    HWND w = CreateWindowExA(WS_EX_APPWINDOW, "VIEW_CLASS", (LPCSTR)(g_rt + RT_APP_NAME), WS_POPUP | WS_VISIBLE, 0, 0,
                             GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, g_module, NULL);
    R(HWND, RT_HWND) = w;
    if (!w)
        return 0;
    R(HDC, RT_DC) = GetDC(w);
    if (!R(HDC, RT_DC))
        return 0;
    SetStretchBltMode(R(HDC, RT_DC), COLORONCOLOR);
    for (int i = 0; i < 7; i++)
        R(HCURSOR, CURSOR_FIELDS[i]) = LoadCursorA(g_module, MAKEINTRESOURCEA(0x3e9 + i));
    for (int i = 0; i < 7; i++)
        if (!R(HCURSOR, CURSOR_FIELDS[i]))
            return 0;
    return 1;
}

/* 0x1001e1b6: sound, sprites (8 bits), resources, utilities, and the callbacks */
U __cdecl libraries_init(void)
{
    RU(RT_AUDIO) = C2(0x1003ec6e, RU(RT_HWND), 0x400);
    if (!RU(RT_AUDIO))
        return 0;
    RU(RT_DEPTH) = 8;
    RU(RT_HSL) = C3(0x100554be, RU(RT_HWND), RU(RT_DC), RU(RT_DEPTH));
    if (!RU(RT_HSL))
        return 0;
    RU(RT_HRL) = C1(0x100699a2, g_module);
    if (!RU(RT_HRL))
        return 0;
    if (!C0(0x1003e97e))
        return 0;
    C2(0x10054cb8, RU(RT_HSL), ORIG_PTR(0x10021fab));
    C2(0x1005bfe2, RU(RT_HSL), ORIG_PTR(0x100159ec));
    C3(0x1004d6e8, RU(RT_HSL), ORIG_PTR(0x10044600), RU(RT_AUDIO));
    return 1;
}

/* RtInit (0x1001dae5): creates a state block and everything the engine needs. The
 * display is taken as a 256-color palette device (Phase 1a's "palette" patch, built
 * in). Script debug mode is always on: the Debug setting is read and then
 * overridden. */
uint8_t *__cdecl RtInit(U unused, U arg)
{
    (void)unused;
    g_rt = rt_alloc();
    if (!g_rt)
        return NULL;
    if (!LoadStringA(g_module, 200, (LPSTR)(g_rt + RT_APP_NAME), 0x40) || !window_create(arg) || !libraries_init()) {
        if (RU(RT_HWND))
            SendMessageA(R(HWND, RT_HWND), WM_CLOSE, 0, 0);
        g_rt = (uint8_t *)(uintptr_t)rt_block_free(g_rt);
        return g_rt;
    }
    LPCSTR app = (LPCSTR)(g_rt + RT_APP_NAME);
    R(uint8_t, RT_ALLOW_DD) = (uint8_t)GetPrivateProfileIntA("DirectDraw", "AllowDirectDraw", 1, "DOMINION.INI");
    GetViewportOrgEx(R(HDC, RT_DC), (POINT *)(g_rt + 0x36c7));
    R(uint8_t, RT_95CB) = (uint8_t)C2(0x100412ed, RU(RT_AUDIO), 0);
    R(uint8_t, RT_95FE) = 1;
    R(uint8_t, RT_BKG_SCROLLS) = R(uint8_t, RT_95FE);
    R(uint8_t, RT_PALETTE_DEVICE) = 1; /* original: (RASTERCAPS & RC_PALETTE) != 0 */
    R(uint8_t, RT_95DE) = R(uint8_t, RT_PALETTE_DEVICE) == 0;
    RU(RT_SCREEN_W) = GetDeviceCaps(R(HDC, RT_DC), HORZRES);
    RU(RT_SCREEN_H) = GetDeviceCaps(R(HDC, RT_DC), VERTRES);
    R(uint8_t, RT_95CC) = GetSystemMetrics(SM_CXSCREEN) < 0x281;
    R(uint8_t, RT_ALT_TAB) = (uint8_t)GetPrivateProfileIntA(app, "AltTab", 0, "DOMINION.INI");
    R(uint8_t, RT_SCRIPT_DEBUG) = (uint8_t)GetPrivateProfileIntA(app, "Debug", 0, "DOMINION.INI");
    R(uint8_t, RT_SCRIPT_DEBUG) = 1;
    RU(RT_JOYSTICKS) = GetPrivateProfileIntA(app, "MaxJoySticks", 2, "DOMINION.INI");
    RU(RT_DROP_FRAMES) = GetPrivateProfileIntA(app, "MaxDropFrames", 1, "DOMINION.INI");
    RU(RT_MODE) = 1;
    MemSetNotify(RU(RT_HWND));
    if (RU(RT_JOYSTICKS))
        joystick_calibrate(0);
    if (RU(RT_JOYSTICKS) > 1)
        joystick_calibrate(1);
    if (!R(uint8_t, RT_COMPONENT))
        C0(0x100103c0);
    RU(RT_INPUT_TIMEOUT) = GetPrivateProfileIntA("Input", "Timeout", 200, "DOMINION.INI");
    return g_rt;
}

/* RtFree (0x1001d82a): runs the exit handler, shuts the libraries down, destroys the
 * cursors and the window, and frees the state block */
U __cdecl RtFree(uint8_t *rt)
{
    g_rt = rt;
    if (RU(RT_HWND) && IsWindow(R(HWND, RT_HWND)))
        SendMessageA(R(HWND, RT_HWND), WM_CLOSE, 0, 0);
    if (R(uint8_t, RT_COMPONENT)) {
        if (RU(RT_ON_EXIT)) {
            uint8_t rec[0x22];
            memset(rec, 0, sizeof rec);
            P(U, rec, 2) = RU(RT_ON_EXIT);
            RU(RT_ON_EXIT) = 0;
            C1(0x100220dd, rec);
        }
        if (R(uint8_t, RT_95E3))
            C0(0x10010d00);
        C0(0x10025ff9);
        C1(0x10069990, 0);
        C1(0x10055321, RU(RT_HSL));
        C1(0x1003ebf1, RU(RT_AUDIO));
        for (int i = 0; i < 7; i++)
            if (RU(CURSOR_FIELDS[i]))
                DestroyCursor(R(HCURSOR, CURSOR_FIELDS[i]));
        ReleaseDC(R(HWND, RT_HWND), R(HDC, RT_DC));
        DestroyWindow(R(HWND, RT_HWND));
        RU(RT_HWND) = 0;
    }
    if (g_rt)
        plugins_free_dlls();
    g_rt = (uint8_t *)(uintptr_t)rt_block_free(g_rt);
    return (U)(uintptr_t)g_rt;
}

/* 0x1001e3e0: at least 256 colors (a message box otherwise) */
U __cdecl colors_check(void)
{
    int bits = GetDeviceCaps(R(HDC, RT_DC), BITSPIXEL);
    U n = 1;
    for (; bits && n < 0x200; n <<= 1)
        bits--;
    if (n < 0x101)
        n *= GetDeviceCaps(R(HDC, RT_DC), PLANES);
    if (n <= 0xff)
        rt_message_box(NULL, 0x65, 0);
    return n > 0xff;
}

/* 0x1001e512, 0x1001e583: subclass the window for input kinds 3 and 1 */
U __cdecl input_hook_3(void)
{
    RU(RT_WNDPROC) = (U)SetWindowLongA(R(HWND, RT_HWND), GWL_WNDPROC, (LONG)(uintptr_t)ORIG_PTR(0x1001f190));
    if (!RU(RT_WNDPROC))
        return 0;
    SetWindowPos(R(HWND, RT_HWND), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    return 1;
}

U __cdecl input_hook_1(void)
{
    RU(RT_WNDPROC) = (U)SetWindowLongA(R(HWND, RT_HWND), GWL_WNDPROC, (LONG)(uintptr_t)ORIG_PTR(0x1002818a));
    if (!RU(RT_WNDPROC))
        return 0;
    SetClassLongA(R(HWND, RT_HWND), GCL_STYLE, GetClassLongA(R(HWND, RT_HWND), GCL_STYLE) | CS_DBLCLKS);
    C0(0x10027feb);
    return 1;
}

/* 0x1001e49a */
U __cdecl input_hook(void)
{
    switch (RU(RT_INPUT_KIND)) {
    case 1:
        return input_hook_1();
    case 3:
        return input_hook_3();
    case 2:
        return C0(0x1002335d) & 0xff;
    }
    return 1;
}

/* RtStartup (0x1001e2f5): RtInit, then the resource file (the game's own name with
 * ".rsf"-style extension from the original's string) */
U __cdecl RtStartup(HMODULE module, U arg, LPCSTR cmdline)
{
    char path[256];
    if (!RtInit((U)(uintptr_t)module, arg))
        return 0;
    GetModuleFileNameA(module, path, sizeof path);
    char *dot = ORIG(char *(__cdecl *)(const char *, int), 0x100292b0)(path, '.');
    if (dot)
        lstrcpyA(dot, (LPCSTR)ORIG_PTR(0x10075560));
    C2(0x1001e610, cmdline, path);
    if (!colors_check() || !rt_measure_memory() || !input_hook() || !C2(0x100208b3, path, 0))
        PostMessageA(R(HWND, RT_HWND), WM_CLOSE, 0, 0);
    return (U)(uintptr_t)g_rt;
}
