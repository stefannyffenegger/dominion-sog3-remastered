/* The game logic, 0x10060700-0x10060d40: the plugin's start (RtLibInit) and end (RtLibFree).
 * The state block is allocated, the window subclassed (iso_window_proc), the drag box's paint
 * hook installed, the other plugins looked up, and the network told the build's checksum and
 * the dominion.ini settings SyncThreshold, NetworkPing and EnableTerrainAnalysis. */
#include "iso_core.h"
#include "plugin.h"

#define DRAG_DC 0xd7b3
#define WINDOW 0xd7c3
#define OLD_PROC 0xd7cf
#define OLD_PAINT_HOOK 0xd7d3
#define OLD_PAINT_DATA 0xd7bf
#define DIRECTDRAW 0xd79f
#define LIB_BMPFONT 0x1812
#define LIB_NETPLAY 0x1816
#define LIB_WAVEPLAY 0x181a

#define S(a) ((const char *)ORIG_PTR(a))
#define g_module G(HMODULE, 0x1017c580)
#define g_game_info (*(RtGameInfo *)ORIG_PTR(0x1017c578))
#define crt_sprintf ORIG(int(__cdecl *)(char *, const char *, ...), 0x100f6f40)
#define crt_strlen ORIG(uint32_t(__cdecl *)(const char *), 0x100f5750)
#define crt_time ORIG(int32_t(__cdecl *)(int32_t *), 0x100f4df0)
#define crt_srand ORIG(void(__cdecl *)(uint32_t), 0x100f4d60)
#define calc_crc32 ORIG(uint32_t(__cdecl *)(const void *, uint32_t), 0x10112044) /* WAVEPLAY ordinal 48 */
#define DPSetGameData ORIG(void(__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t), 0x101120b0)
#define SpriteSetPaintHook ORIG(uint32_t(__stdcall *)(void *, uint32_t, uint32_t, uint32_t), 0x100f4a3c)
#define RtGetDirectDrawMode ORIG(uint32_t(__cdecl *)(void *), 0x100f4a54)
#define RtGetLibraryState ORIG(uint32_t(__cdecl *)(void *, const char *), 0x100f4a4e)
#define IsoStopNetProcess ORIG(void(__stdcall *)(uint8_t *), 0x10045961)
#define IsoStopMoveProcess ORIG(void(__stdcall *)(uint8_t *), 0x1008d188)
#define init_tables ORIG(void(__cdecl *)(void), 0x10091790)
#define init_state ORIG(void(__cdecl *)(uint8_t *), 0x100d2d70)

/* 0x1006071d: undoes the start: the engine's game info, paint hook and window handler are
 * restored, the network and movement processes stopped, the state freed. Returns 0 */
uint32_t __cdecl plugin_free(void)
{
    if (g_iso) {
        g_game_info.hook = 0;
        RtSetGameInfo(I(void *, ISO_APP), &g_game_info);
        SpriteSetPaintHook(I(void *, ISO_HSL), I(uint32_t, OLD_PAINT_HOOK), I(uint32_t, OLD_PAINT_DATA), 0);
        if (I(LONG, OLD_PROC))
            SetWindowLongA(I(HWND, WINDOW), GWL_WNDPROC, I(LONG, OLD_PROC));
        if (I(int, 0x175))
            IsoStopNetProcess(g_iso);
        IsoStopMoveProcess(g_iso);
        LocalFree(g_iso);
        g_iso = 0;
    }
    return 0;
}

/* RtLibFree */
void __cdecl lib_free(uint8_t *iso)
{
    g_iso = iso;
    plugin_free();
}

/* 0x100608c3: sets up the state; nonzero if the window could be subclassed */
int __stdcall plugin_setup(void *app, uint8_t *info)
{
    char line[128], path[256];
    uint32_t crc = 0, net = 0, terrain = 0;

    init_tables();
    P(uint32_t, info, 0x10) = (uint32_t)ORIG_PTR(0x10060166); /* rubber_hook_off */
    I(void *, ISO_APP) = app;
    I(HWND, WINDOW) = RtGetWindow(app);
    I(HDC, DRAG_DC) = RtGetDC(app);
    I(void *, ISO_HSL) = RtGetHSL(app);
    I(uint32_t, OLD_PAINT_HOOK) = SpriteSetPaintHook(I(void *, ISO_HSL), (uint32_t)ORIG_PTR(0x10060191),
                                                     (uint32_t)g_iso, (uint32_t)(g_iso + OLD_PAINT_DATA));
    I(uint32_t, DIRECTDRAW) = RtGetDirectDrawMode(app);
    I(LONG, OLD_PROC) = SetWindowLongA(I(HWND, WINDOW), GWL_WNDPROC, (LONG)ORIG_PTR(0x1005e5e0));
    I(int, 0x148) = 0;
    I(int, 0x175) = 0;
    I(uint32_t, LIB_WAVEPLAY) = RtGetLibraryState(I(void *, ISO_APP), S(0x1013f854));
    I(uint32_t, LIB_NETPLAY) = RtGetLibraryState(I(void *, ISO_APP), S(0x1013f860));
    I(uint32_t, LIB_BMPFONT) = RtGetLibraryState(I(void *, ISO_APP), S(0x1013f868));
    if (I(uint32_t, LIB_NETPLAY)) {
        if (I(uint32_t, LIB_WAVEPLAY)) {
            /* the build's checksum: games only connect with the same build */
            crt_sprintf(line, S(0x1013f88c), S(0x1013f870));
            crc = calc_crc32(line, crt_strlen(line));
        }
        UINT sync = GetPrivateProfileIntA(S(0x1013f8e8), S(0x1013f8d8), 0, S(0x1013f8c8));
        UINT ping = GetPrivateProfileIntA(S(0x1013f8bc), S(0x1013f8b0), 3000, S(0x1013f8a0));
        net = sync << 16 | (ping & 0xffff);
        terrain = GetPrivateProfileIntA(S(0x1013f91c), S(0x1013f904), 0, S(0x1013f8f4)) & 0xffff;
        DPSetGameData(I(uint32_t, LIB_NETPLAY), crc, net, terrain, 0);
    }
    I(int, 0x170) = 0;
    DebugStr(I(uint32_t, LIB_WAVEPLAY) ? S(0x1013f928) : S(0x1013f934));
    DebugStr(I(uint32_t, LIB_NETPLAY) ? S(0x1013f944) : S(0x1013f950));
    DebugStr(I(uint32_t, LIB_BMPFONT) ? S(0x1013f95c) : S(0x1013f968));
    if (GetModuleFileNameA(g_module, path, sizeof path))
        DebugStr(S(0x1013f974), path);
    for (int k = 0; k < 3; k++)
        I(uint32_t, 0xd807 + k * 4) = 0;
    I(int, 0x1ac7) = 0;
    I(int, 0xd9da) = 1;
    crt_srand((uint32_t)crt_time(0));
    init_state(g_iso);
    I(uint32_t, 0xd7fb) = timeGetTime();
    SetWindowTextA(I(HWND, WINDOW), S(0x1013f984));
    return I(LONG, OLD_PROC) != 0;
}

/* RtLibInit (lib_init; hooks.c imports the name): the engine loads the plugin; returns the state block (or 0) */
uint8_t *__cdecl lib_init(void *app, uint8_t *info)
{
    uint8_t *iso = LocalAlloc(LMEM_ZEROINIT, 0xe12f);
    if (!iso)
        return 0;
    g_iso = iso;
    if (!plugin_setup(app, info))
        return (uint8_t *)(uintptr_t)plugin_free();
    I(void *, ISO_HSL) = RtGetHSL(I(void *, ISO_APP));
    I(uint32_t, 0x184e) = 0x5a;
    P(uint32_t, info, 0xc) = 0;
    RtGameInfo gi = { iso, (void(__stdcall *)(void *))ORIG_PTR(0x100af179) };
    RtSetGameInfo(I(void *, ISO_APP), &gi);
    return iso;
}
