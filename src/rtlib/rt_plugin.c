/* Engine core, 0x10011430-0x10011f54: plugins.
 *
 * The game's .r32 files are plugins of the engine. The resource file lists their names
 * (key resource 10); each <name>.r32 is loaded from the game or the data directory and
 * its RtLibInit(rt, info) returns the plugin's context. The functions the scripts call in
 * plugins are listed by name (key resource 11) with their plugin's number (12) and
 * looked up with GetProcAddress. A plugin can fill callbacks into its info block: called
 * every frame, on pause, on palette changes and so on, always with its context first.
 * Rewritten from the original.
 *
 * A plugin record is 300 bytes: +0 module, +4 context, +8 the info block handed to
 * RtLibInit (0x24, 0x200, then the callbacks at +0x14 to +0x24 of the record), +0x28
 * argument count of the running call, +0x2c name. */
#include "rtcore.h"
#include "rt.h"
#include "rt_plugin.h"

#define RT_EXTERN_TABLE 0x324f  /* 8 bytes per extern function: 0 if its name starts with '_', address */
#define RT_PLUGINS 0x325b
#define RT_OLD_PLUGINS 0x325f   /* unloaded, their DLLs still to be freed */
#define RT_EXTERN_PLUGIN 0x32ab /* plugin number of each extern function */
#define RT_N_SERVICED 0x3c47    /* plugins with a per-frame callback */
#define RT_N_PLUGINS 0x3c4b
#define RT_N_OLD_PLUGINS 0x3c4f
#define RT_SCRIPT_DEBUG 0x95d8
#define RT_DATA_DIR 0x9803
#define RT_GAME_DIR 0x9a54

#define PL_SERVICE 0x14
#define PL_18 0x18
#define PL_PAUSE 0x1c
#define PL_20 0x20
#define PL_PALETTE 0x24

#define fn_10025ff9 flush_engine_messages

#define PLUGIN(i) (R(uint8_t **, RT_PLUGINS)[i])

/* Calls a plugin function (C convention; the original leaves the arguments on the stack) */
static uint32_t pcall(void *fn, uint32_t n, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t args[3] = {a, b, c};
    return orig_stdcall_unsafe(fn, n, args);
}

/* Calls callback `off` of every plugin, the last loaded first */
static void notify(uint32_t off, uint32_t n, uint32_t a, uint32_t b)
{
    for (int i = R(int, RT_N_PLUGINS) - 1; i >= 0; i--) {
        uint8_t *p = PLUGIN(i);
        if (P(uint32_t, p, off))
            pcall(P(void *, p, off), n, P(uint32_t, p, 4), a, b);
    }
}

/* 0x10011430 */
uint32_t __stdcall rt_set_cef0(uint32_t a, uint32_t unused1, uint32_t unused2)
{
    (void)unused1, (void)unused2;
    G(uint32_t, 0x1007cef0) = a;
    return 1;
}

/* 0x10011450, 0x100114c3, 0x10011535, 0x10011633 */
void __stdcall plugins_notify_18(uint32_t arg)
{
    notify(PL_18, 2, arg, 0);
}

void __stdcall plugins_pause(uint32_t paused)
{
    notify(PL_PAUSE, 2, paused, 0);
}

void __stdcall plugins_notify_20(uint32_t a, uint32_t b)
{
    notify(PL_20, 3, a, b);
}

void __cdecl plugins_palette(void)
{
    notify(PL_PALETTE, 2, (uint32_t)(uintptr_t)(g_rt + RT_PALETTE), 0);
}

/* 0x100115ac: the per-frame callbacks; stops once all that have one were called */
void __cdecl plugins_service(void)
{
    int left = R(int, RT_N_SERVICED);
    for (int i = R(int, RT_N_PLUGINS) - 1; i >= 0 && left; i--) {
        uint8_t *p = PLUGIN(i);
        if (P(uint32_t, p, PL_SERVICE)) {
            pcall(P(void *, p, PL_SERVICE), 1, P(uint32_t, p, 4), 0, 0);
            left--;
        }
    }
}

/* 0x1001180a */
void __stdcall plugin_free(HMODULE dll, uint32_t ctx)
{
    FARPROC f = GetProcAddress(dll, "RtLibFree");
    if (f)
        pcall((void *)f, 1, ctx, 0, 0);
}

/* 0x100116aa: tells every plugin to free itself. The DLLs are freed later (message
 * 0x5f4 to the window), because a plugin may be on the call stack right now. */
void __cdecl plugins_unload(void)
{
    if (!R(uint32_t, RT_N_PLUGINS))
        return;
    for (int i = R(int, RT_N_PLUGINS) - 1; i >= 0; i--)
        plugin_free(P(HMODULE, PLUGIN(i), 0), P(uint32_t, PLUGIN(i), 4));
    if (R(uint32_t, RT_EXTERN_TABLE))
        winfree(R(void *, RT_EXTERN_TABLE));
    if (R(uint32_t, RT_EXTERN_PLUGIN))
        winfree(R(void *, RT_EXTERN_PLUGIN));
    R(uint32_t, RT_EXTERN_TABLE) = 0;
    R(uint32_t, RT_EXTERN_PLUGIN) = 0;
    R(uint32_t, RT_OLD_PLUGINS) = R(uint32_t, RT_PLUGINS);
    R(uint32_t, RT_PLUGINS) = 0;
    R(uint32_t, RT_N_OLD_PLUGINS) = R(uint32_t, RT_N_PLUGINS);
    R(uint32_t, RT_N_SERVICED) = 0;
    R(uint32_t, RT_N_PLUGINS) = 0;
    PostMessageA(R(HWND, RT_HWND), 0x5f4, 0, 0);
}

/* 0x10011842: frees the DLLs of unloaded plugins */
void __cdecl plugins_free_dlls(void)
{
    if (!R(uint32_t, RT_N_OLD_PLUGINS))
        return;
    uint8_t **old = R(uint8_t **, RT_OLD_PLUGINS);
    for (int i = R(int, RT_N_OLD_PLUGINS) - 1; i >= 0; i--) {
        rt_debug_str("freeing %04x"); /* (the module handle is not passed along) */
        FreeLibrary(P(HMODULE, old[i], 0));
        winfree(old[i]);
    }
    if (old)
        winfree(old);
    R(uint32_t, RT_OLD_PLUGINS) = 0;
    R(uint32_t, RT_N_OLD_PLUGINS) = 0;
}

/* 0x10011bfb: registers a loaded DLL and calls its RtLibInit; returns the plugin count,
 * 0 on failure */
uint32_t __stdcall plugin_add(HMODULE dll, LPCSTR name)
{
    void *list = winrealloc(R(void *, RT_PLUGINS), R(uint32_t, RT_N_PLUGINS) * 4 + 4, 0);
    if (!list)
        return 0;
    R(void *, RT_PLUGINS) = list;
    uint32_t *p = winmalloc(300, 0);
    if (!p)
        return 0;
    PLUGIN(R(uint32_t, RT_N_PLUGINS)) = (uint8_t *)p;
    memset(p, 0, 300);
    p[0] = (uint32_t)(uintptr_t)dll;
    lstrcpyA((LPSTR)(p + 0xb), name);
    p[2] = 0x24;
    p[3] = 0x200;
    FARPROC init = GetProcAddress(dll, "RtLibInit");
    if (!init) {
        rt_error(1);
        return 0;
    }
    p[1] = pcall((void *)init, 2, (uint32_t)(uintptr_t)g_rt, (uint32_t)(uintptr_t)(p + 2), 0);
    if (!p[1]) {
        rt_error(2);
        return 0;
    }
    if (p[5])
        R(uint32_t, RT_N_SERVICED)++;
    return ++R(uint32_t, RT_N_PLUGINS);
}

/* 0x10011d6e: looks up the address of every extern function the scripts use */
int __cdecl plugins_bind_functions(void)
{
    uint32_t size = 0;
    R(uint8_t *, RT_EXTERN_PLUGIN) = ResLoadKeyResource(R(ResFile *, RT_RES), 0xc, &size);
    if (!R(uint32_t, RT_EXTERN_PLUGIN))
        return 0;
    uint32_t *table = winrealloc(R(void *, RT_EXTERN_TABLE), (size >> 2) << 3, 0);
    if (!table)
        return 0;
    char *names = (char *)ResLoadKeyResource(R(ResFile *, RT_RES), 0xb, &size);
    if (!names) {
        winfree(table);
        return 0;
    }
    R(uint32_t *, RT_EXTERN_TABLE) = table;
    uint32_t pos = 0;
    for (int i = 0; pos < size; i++) {
        const char *name = names + pos;
        table[i * 2 + 1] = (uint32_t)(uintptr_t)GetProcAddress(P(HMODULE, PLUGIN(R(uint32_t *, RT_EXTERN_PLUGIN)[i]), 0), name);
        if (!table[i * 2 + 1]) {
            rt_error(3);
            break;
        }
        table[i * 2] = name[0] != '_';
        pos += lstrlenA(name) + 1;
    }
    winfree(names);
    return size <= pos;
}

/* 0x10011925: loads the plugins the resource file names. A plugin that fails to
 * initialize ends the game with a message box. */
uint32_t __cdecl plugins_load(void)
{
    uint32_t size = 0;
    char *names = (char *)ResLoadKeyResource(R(ResFile *, RT_RES), 10, &size);
    if (!names)
        return size == 0;
    uint32_t ok = 1, pos = 0;
    char file[256], path[256], msg[288];
    while (pos < size) {
        const char *name = names + pos;
        wsprintfA(file, "%s.r32", name);
        MakeFullPath(path, (char *)(g_rt + RT_GAME_DIR), file);
        if (!FileExist(path)) {
            MakeFullPath(path, (char *)(g_rt + RT_DATA_DIR), file);
            if (!FileExist(path)) {
                rt_script_error(0x79, (U)(uintptr_t)file);
                return 0; /* (the name list is not freed) */
            }
        }
        lstrcpyA(file, path);
        UINT mode = SetErrorMode(SEM_NOOPENFILEERRORBOX);
        HMODULE dll = LoadLibraryA(file);
        wsprintfA(msg, "Loading: %04x - %s", dll, file);
        DebugStr(msg);
        SetErrorMode(mode);
        if ((uintptr_t)dll <= 0x20) {
            if (R(uint8_t, RT_SCRIPT_DEBUG))
                rt_script_error(0x7a, (U)(uintptr_t)file);
            else
                rt_fatal(0x3f2, 0);
            return 0;
        }
        ok = plugin_add(dll, name);
        if (!ok) {
            FreeLibrary(dll);
            MessageBoxA(R(HWND, RT_HWND), "Error initializing Dominion.\nPlease see the troubleshooting guide.",
                        "Dominion: Storm Over Gift3", MB_ICONINFORMATION);
            RtSetScreenRes(g_rt, 0, 0, 0);
            fn_10025ff9();
            PostMessageA(R(HWND, RT_HWND), WM_CLOSE, 0, 0);
            break;
        }
        pos += lstrlenA(name) + 1;
    }
    if (ok) {
        ok = plugins_bind_functions();
        if (!ok) {
            if (R(uint8_t, RT_SCRIPT_DEBUG))
                rt_script_error(0x7c, 0);
            else
                rt_fatal(0x3f4, 0);
        }
    }
    winfree(names);
    return ok;
}
