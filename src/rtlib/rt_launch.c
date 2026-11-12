/* Engine core, 0x10022e5c-0x100236a0: running one command of a handler, launching an
 * external program (the engine hides its window and comes back once the program's
 * windows are gone), and run mode 2 (/k), whose window logs Windows off when it closes.
 * None of this but the first is used by Dominion. Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_launch.h"
#include <shellapi.h>

#define RU(off) R(U, off)
#define RB(off) R(uint8_t, off)

#define RT_WNDPROC 0x0c8c
#define RT_CALL_DEPTH 0x3e07
#define RT_SCRIPT_INDEX 0x3f47
#define RT_SCRIPT_DEBUG 0x95d8
#define RT_LAUNCHED 0x95e9


/* the windows seen while a launched program runs; 0x10c bytes each */
typedef struct Watched {
    HWND hwnd;
    U alive;    /* seen in the last scan */
    U existing; /* was there before the launch */
    char cls[0x100];
} Watched;

#define g_watch_next G(U, 0x10086cd0)  /* time of the next scan */
#define g_watch_grace G(U, 0x10086cd4) /* scans left to wait for the program's first window */
#define g_watch G(Watched *, 0x10086cd8)
#define g_watch_count G(U, 0x10086cdc)
#define g_watch_handler G(U, 0x10086ce0) /* script handler told when it is over */

/* 0x10022e5c: runs command number `index` of handler `id` */
U __stdcall handler_run_at(U id, int index)
{
    if (!obj_is(id, 0xe))
        return 0;
    U *e = obj_entry(id);
    const uint8_t *c = (const uint8_t *)(uintptr_t)e[0];
    int left = (int)e[1];
    while (index > 0 && left > 0) {
        U size = script_cmd_size((const uint16_t *)c);
        if (!size)
            return 0;
        c += size;
        left -= (int)size;
        index--;
    }
    if (left < 1)
        return 0;
    RU(RT_SCRIPT_INDEX) = id;
    if (++RU(RT_CALL_DEPTH) < 30) {
        U r = cmd_exec(c, NULL);
        RU(RT_CALL_DEPTH)--;
        return r;
    }
    if (!RB(RT_SCRIPT_DEBUG)) {
        rt_fatal(0x3ea, 0);
        return 0;
    }
    return rt_script_error(0x6a, 0);
}

/* 0x10023082 */
U __stdcall watch_add(HWND hwnd, U existing)
{
    Watched *w = winrealloc(g_watch, (g_watch_count + 1) * sizeof(Watched), 0);
    if (!w)
        return 0;
    g_watch = w;
    w[g_watch_count].hwnd = hwnd;
    w[g_watch_count].existing = existing;
    w[g_watch_count].alive = 1;
    GetClassNameA(hwnd, w[g_watch_count].cls, 0x100);
    g_watch_count++;
    return 1;
}

/* 0x10022fa0: EnumWindows callback: records visible windows (all of them at the start,
 * then marks the ones still there and adds new ones) */
BOOL __stdcall watch_enum(HWND hwnd, LPARAM first)
{
    if (!IsWindowVisible(hwnd) || hwnd == R(HWND, RT_HWND))
        return 1;
    if (first)
        return (BOOL)watch_add(hwnd, 1);
    for (U i = 0; i < g_watch_count; i++)
        if (g_watch[i].hwnd == hwnd)
            return (BOOL)(g_watch[i].alive = 1);
    return (BOOL)watch_add(hwnd, 0);
}

/* 0x100232ad: the program is gone: the engine's window comes back and the handler runs */
U __cdecl watch_end(void)
{
    RB(RT_LAUNCHED) = 0;
    ShowWindow(R(HWND, RT_HWND), SW_SHOWNORMAL);
    SetActiveWindow(R(HWND, RT_HWND));
    SetForegroundWindow(R(HWND, RT_HWND));
    SetFocus(R(HWND, RT_HWND));
    if (g_watch)
        winfree(g_watch);
    g_watch = NULL;
    g_watch_count = 0;
    if (g_watch_handler)
        event_send(g_watch_handler, 0, 0);
    return 0;
}

/* 0x10023160: once a second while a program runs: still running while one of its own
 * windows is up; before the first one appears it waits up to 60 scans */
U __cdecl watch_poll(void)
{
    if (!RB(RT_LAUNCHED))
        return 0;
    U now = timeGetTime();
    if (now < g_watch_next)
        return 1;
    g_watch_next = now + 1000;
    for (U i = 0; i < g_watch_count; i++)
        g_watch[i].alive = 0;
    if (!EnumWindows(watch_enum, 0))
        return watch_end();
    for (U i = 0; i < g_watch_count; i++) {
        if (!g_watch[i].existing && g_watch[i].alive) {
            g_watch_grace = 0;
            return 1;
        }
    }
    if (!g_watch_grace)
        return watch_end();
    g_watch_grace--;
    return 1;
}

/* 0x1002335d: run mode 2 (/k): subclass the window */
U __cdecl input_hook_2(void)
{
    RU(RT_WNDPROC) = (U)SetWindowLongA(R(HWND, RT_HWND), GWL_WNDPROC, (LONG)(uintptr_t)(void *)kiosk_wndproc);
    return RU(RT_WNDPROC) != 0;
}

/* 0x1002355d: hides the engine and notes the windows already open */
U __stdcall watch_start(U handler)
{
    g_watch_handler = handler;
    g_watch_next = timeGetTime() + 1000;
    g_watch_grace = 60;
    SendMessageA(R(HWND, RT_HWND), WM_ACTIVATEAPP, 0, 0);
    ShowWindow(R(HWND, RT_HWND), SW_HIDE);
    if (!EnumWindows(watch_enum, 1))
        return watch_end();
    PostMessageA(R(HWND, RT_HWND), 0x5f5, 0, 0);
    RB(RT_LAUNCHED) = 1;
    return 1;
}

/* 0x100233b0: launches `cmdline` (a program, quoted or not, then its arguments) in
 * `dir`; `handler` runs when it is over. 0 when it could not be started. */
U __stdcall program_launch(LPCSTR cmdline, LPCSTR dir, U handler)
{
    if (!watch_start(handler))
        return 1;
    char buf[0x204];
    lstrcpyA(buf, cmdline);
    char *file = buf, *args;
    while (*file == ' ')
        file++;
    if (*file == '"') {
        file++;
        char *q = file;
        while (*q && *q != '"')
            q++;
        args = file;
        if (*q) {
            *q = 0;
            args = q + 1;
        }
    } else {
        args = file;
    }
    while (*args && *args != ' ')
        args++;
    if (*args == ' ') {
        *args = 0;
        do
            args++;
        while (*args && *args == ' ');
    }
    if (!*args)
        args = NULL;
    HINSTANCE r = ShellExecuteA(NULL, (LPCSTR)ORIG_PTR(0x100755fc), file, args, dir, SW_SHOWNORMAL);
    if ((uintptr_t)r < 32) {
        SendMessageA(R(HWND, RT_HWND), WM_ACTIVATEAPP, 1, 0);
        watch_end();
        return 0;
    }
    return 1;
}

/* 0x10023612: run mode 2's window procedure: closing the window logs Windows off */
LRESULT __stdcall kiosk_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DESTROY)
        ExitWindowsEx(0, 0);
    return CallWindowProcA(R(WNDPROC, RT_WNDPROC), hwnd, msg, wp, lp);
}
