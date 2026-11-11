/* Engine core, 0x1001e610-0x1001f550: the command line, the per-frame service calls,
 * the small Rt* getters and setters, and the screensaver window procedure. Exports
 * set the current state block first and return what the original leaves in EAX.
 * Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_service.h"

#define SET(rt) (g_rt = (uint8_t *)(rt))
#define C0(a) ORIG(U(__cdecl *)(void), a)()
#define C2(a, x, y) ORIG(U(__stdcall *)(U, U), a)((U)(x), (U)(y))
#define C3(a, x, y, z) ORIG(U(__stdcall *)(U, U, U), a)((U)(x), (U)(y), (U)(z))
#define C4(a, x, y, z, w) ORIG(U(__stdcall *)(U, U, U, U), a)((U)(x), (U)(y), (U)(z), (U)(w))
#define RU(off) R(U, off)
#define RB(off) R(uint8_t, off)
/* a byte setter leaves the state block pointer in EAX with the byte in AL */
#define BYTE_SET(rt, off, v) (RB(off) = (v), ((U)(uintptr_t)(rt) & ~0xffu) | (v))

#define RT_WNDPROC 0x0c8c
#define RT_CLICK_RECTS 0x0b40
#define RT_CLICK_COUNT 0x0b44
#define RT_COLLIDE 0x0b58
#define RT_MOUSE_POS 0x0b6c
#define RT_SCRIPT_OFFSETS 0x0cbc
#define RT_SCRIPT_TEXT 0x329f
#define RT_COPY_MAP 0x32a3
#define RT_VIEW_ORG 0x36c7
#define RT_SCRIPT_PROC 0x3c37
#define RT_CALL_PROC 0x3c3b
#define RT_PROC_ARG 0x3c3f
#define RT_PLUGINS 0x3c47
#define RT_BKG_INDEX 0x3dc3
#define RT_CURSOR_ID 0x3dd3
#define RT_FIRST_COPY 0x3dfb
#define RT_TRACK 0x3e07
#define RT_ERR_LINE 0x3e0b
#define RT_BROWSE 0x3e1b
#define RT_SVC_3E27 0x3e27
#define RT_SVC_3E37 0x3e37
#define RT_RES_COUNT 0x3e43
#define RT_EVENTS 0x3e47
#define RT_SVC_3E57 0x3e57
#define RT_RUN_MODE 0x3e5b /* 1 /w, 2 /k, 3 /s (screensaver) from the command line */
#define RT_SVC_3F3F 0x3f3f
#define RT_SCRIPT_NAME 0x3f43
#define RT_SCRIPT_INDEX 0x3f47
#define RT_SCRIPT_DEBUG 0x95d8
#define RT_DIRECTDRAW 0x95d9
#define RT_MOVED_ONCE 0x95dd
#define RT_INTERACTIVE 0x95e2
#define RT_PAUSED 0x95f1
#define RT_PLAYER 0x95f2
#define RT_RUNNING 0x95f6
#define RT_TRACKING_RECT 0x95fb
#define RT_CLOSE_ASKED 0x95fc
#define RT_CLOSE_DONE 0x95fd
#define RT_WINDOW_BOX 0x9600
#define RT_ERR_FILE 0x9602
#define RT_START_SCENE 0x9954
#define RT_SIGNON_PATH 0x9a54
#define RT_TITLE 0x9d56
#define RT_DISC_TITLE 0x9d96 /* from /t "..." on the command line */

#define g_saver_active G(U, 0x10075568)

static int is_blank(char c) { return c == ' ' || c == '\t'; }

/* 0x1001e92f: /t "title": the disc title shown in "Please insert ..." messages */
char *__stdcall cmdline_title(char *p)
{
    while (*p && is_blank(*p))
        p++;
    if (*p == '"') {
        char *out = (char *)(g_rt + RT_DISC_TITLE);
        p++;
        while (*p && *p != '"')
            *out++ = *p++;
        *out = 0;
        if (*p == '"')
            p++;
    }
    return p;
}

/* 0x1001e832: one switch after '-' or '/'; the rest of the word is skipped */
char *__stdcall cmdline_switch(char *p)
{
    if (!*p)
        return p;
    switch (*p++) {
    case 'k':
        RU(RT_RUN_MODE) = 2;
        break;
    case 's':
        RU(RT_RUN_MODE) = 3;
        break;
    case 't':
        p = cmdline_title(p);
        break;
    case 'w':
        RU(RT_RUN_MODE) = 1;
        break;
    }
    while (*p && *p != ' ')
        p++;
    return p;
}

/* 0x1001e610: lowercases everything outside quotes, then reads the switches; a word
 * that is not a switch is copied to `path` (the last one wins). Nothing is read when
 * the quotes do not pair up. */
void __stdcall cmdline_parse(LPCSTR cmdline, char *path)
{
    char buf[128];
    U quoted = 0;
    lstrcpynA(buf, cmdline, sizeof buf);
    for (int i = 0; i < lstrlenA(buf); i++) {
        if (buf[i] == '"')
            quoted = !quoted;
        else if (!quoted)
            buf[i] = (char)(uintptr_t)CharLowerA((LPSTR)(uintptr_t)(uint16_t)(int16_t)buf[i]);
    }
    if (quoted)
        return;
    char *p = buf;
    while (*p) {
        while (*p && is_blank(*p))
            p++;
        char c = *p;
        if (!c)
            return;
        if (c == '-' || c == '/') {
            p = cmdline_switch(p + 1);
        } else if (c == '"') {
            quoted = !quoted;
            p++;
        } else {
            char *out = path;
            while (*p && (*p != ' ' || quoted) && *p != '"')
                *out++ = *p++;
            *out = 0;
        }
    }
}

/* 0x1001e9f0: the full per-frame service: sprites, then each subsystem that has work */
void __cdecl RtService(uint8_t *rt)
{
    SET(rt);
    if (!RB(RT_RUNNING))
        return;
    SpriteAnimate(R(uint8_t *, RT_HSL));
    if (RU(RT_SVC_3F3F))
        C0(0x10024db2);
    if (RU(RT_SVC_3E57))
        C0(0x10024e10);
    if (RU(RT_SVC_3E27))
        C0(0x10024f1c);
    if (RU(RT_COLLIDE))
        collide_check();
    if (RU(RT_SVC_3E37))
        C0(0x10015150);
    if (RU(RT_EVENTS))
        events_flush();
    if (RU(RT_PLUGINS))
        plugins_service();
}

/* 0x1001eacf: Dominion's own version: no sprite animation with DirectDraw, fewer steps */
void __cdecl RtDominionService(uint8_t *rt)
{
    SET(rt);
    if (!RB(RT_RUNNING))
        return;
    if (!RB(RT_DIRECTDRAW))
        SpriteAnimate(R(uint8_t *, RT_HSL));
    if (RU(RT_PLUGINS))
        plugins_service();
    if (RU(RT_SVC_3E57))
        C0(0x10024e10);
    if (RU(RT_SVC_3E27))
        C0(0x10024f1c);
    if (RU(RT_COLLIDE))
        collide_check();
    if (RU(RT_EVENTS))
        events_flush();
}

U __cdecl RtGetBrowseEvents(uint8_t *rt, U *out)
{
    SET(rt);
    if (out)
        *out = RU(RT_BROWSE);
    return (U)(uintptr_t)g_rt;
}

U __cdecl RtGetClickRects(uint8_t *rt, U *count)
{
    SET(rt);
    if (count)
        *count = RU(RT_CLICK_COUNT);
    return RU(RT_CLICK_RECTS);
}

/* a run-time copy's id maps to the resource it was copied from */
U __cdecl RtGetCopyId(uint8_t *rt, U id)
{
    SET(rt);
    if (id >= RU(RT_FIRST_COPY) && RU(RT_COPY_MAP))
        id = R(U *, RT_COPY_MAP)[id - RU(RT_FIRST_COPY)];
    return id;
}

U __cdecl RtGetErrFile(uint8_t *rt, U *line)
{
    SET(rt);
    if (line)
        *line = RU(RT_ERR_LINE);
    return (U)(uintptr_t)(g_rt + RT_ERR_FILE);
}

U __cdecl RtGetNumResources(uint8_t *rt) { SET(rt); return RU(RT_RES_COUNT); }

U __cdecl RtGetResName(uint8_t *rt, U id)
{
    SET(rt);
    if (id < RU(RT_RES_COUNT) && RU(RT_SCRIPT_TEXT) && RU(RT_SCRIPT_OFFSETS))
        return R(U *, RT_SCRIPT_OFFSETS)[id] + RU(RT_SCRIPT_TEXT);
    return 0;
}

U __cdecl RtGetResNames(uint8_t *rt) { SET(rt); return RU(RT_SCRIPT_TEXT); }
U __cdecl RtGetResOffsets(uint8_t *rt) { SET(rt); return RU(RT_SCRIPT_OFFSETS); }
U __cdecl RtGetResType(uint8_t *rt, U id) { SET(rt); return (U)ResGetType(R(ResFile *, RT_RES), id); }
U __cdecl RtGetSignonPath(uint8_t *rt) { SET(rt); return (U)(uintptr_t)(rt + RT_SIGNON_PATH); }
U __cdecl RtGetTitleString(uint8_t *rt) { SET(rt); return (U)(uintptr_t)(rt + RT_TITLE); }

U __cdecl RtGetTrackInfo(uint8_t *rt, U *out)
{
    SET(rt);
    out[0] = RU(RT_TRACK);
    out[1] = RU(RT_SCRIPT_INDEX);
    return out[2] = RU(RT_SCRIPT_NAME);
}

U __cdecl RtGetViewportOrg(uint8_t *rt) { SET(rt); return (U)(uintptr_t)(rt + RT_VIEW_ORG); }
U __cdecl RtIsPaused(uint8_t *rt) { SET(rt); return RB(RT_PAUSED); }
U __cdecl RtIsRunning(uint8_t *rt) { SET(rt); return RB(RT_RUNNING); }
U __cdecl RtIsTrackingRect(uint8_t *rt) { SET(rt); return RB(RT_TRACKING_RECT); }
U __cdecl RtMessage(uint8_t *rt, U hwnd, U id, U arg) { SET(rt); return C3(0x10025da8, hwnd, id, arg); }

U __cdecl RtProcessRunMsgs(uint8_t *rt)
{
    SET(rt);
    if (RU(RT_EVENTS))
        events_flush();
    return (U)(uintptr_t)g_rt;
}

U __cdecl RtSceneIn(uint8_t *rt, U scene, U arg) { SET(rt); return C2(0x100208b3, scene, arg); }
U __cdecl RtSceneInCallback(uint8_t *rt, U scene, U arg, U fn, U data)
{
    SET(rt);
    return C4(0x10021387, scene, arg, fn, data);
}
U __cdecl RtSceneOut(uint8_t *rt) { SET(rt); return C0(0x100215c5); }

/* leaves the scene and starts the first one again */
U __cdecl RtSceneRestart(uint8_t *rt)
{
    SET(rt);
    C0(0x100215c5);
    SpriteResetLibrary(R(uint8_t *, RT_HSL));
    RB(RT_WINDOW_BOX) = 1;
    RU(RT_BKG_INDEX) = 0;
    InvalidateRect(R(HWND, RT_HWND), NULL, TRUE);
    UpdateWindow(R(HWND, RT_HWND));
    return C2(0x100208b3, g_rt + RT_START_SCENE, 1);
}

U __cdecl RtSetBkgIndex(uint8_t *rt, U v) { SET(rt); return RU(RT_BKG_INDEX) = v; }
U __cdecl RtSetCallProc(uint8_t *rt, U proc, U arg)
{
    SET(rt);
    RU(RT_CALL_PROC) = proc;
    return RU(RT_PROC_ARG) = arg;
}
U __cdecl RtSetCursorId(uint8_t *rt, U v) { SET(rt); return RU(RT_CURSOR_ID) = v; }
U __cdecl RtSetDebug(uint8_t *rt, uint8_t v) { SET(rt); return BYTE_SET(rt, RT_SCRIPT_DEBUG, v); }
U __cdecl RtSetInteractive(uint8_t *rt, uint8_t v) { SET(rt); return BYTE_SET(rt, RT_INTERACTIVE, v); }
U __cdecl RtSetPlayer(uint8_t *rt, uint8_t v) { SET(rt); return BYTE_SET(rt, RT_PLAYER, v); }
U __cdecl RtSetScriptProc(uint8_t *rt, U proc, U arg)
{
    SET(rt);
    RU(RT_SCRIPT_PROC) = proc;
    return RU(RT_PROC_ARG) = arg;
}
U __cdecl RtSetWindowBox(uint8_t *rt, uint8_t v) { SET(rt); return BYTE_SET(rt, RT_WINDOW_BOX, v); }

/* 0x1001f4d3: whether the screensaver may close now. The first call allows it and
 * switches the screensaver handling off; with RT_CLOSE_ASKED set it never does. */
U __cdecl saver_may_close(void)
{
    if (!RB(RT_CLOSE_ASKED)) {
        RB(RT_CLOSE_ASKED) = 0;
        g_saver_active = RB(RT_CLOSE_ASKED);
        return 1;
    }
    if (!RB(RT_CLOSE_DONE))
        RB(RT_CLOSE_DONE) = 1;
    return 0;
}

/* 0x1001f427 */
void __cdecl saver_quit(void)
{
    if (saver_may_close())
        PostMessageA(R(HWND, RT_HWND), WM_CLOSE, 0, 0);
}

/* 0x1001f45c: the first mouse move only records the position; later ones quit */
U __stdcall saver_mouse_move(U pos)
{
    if (!RB(RT_MOVED_ONCE))
        RB(RT_MOVED_ONCE) = 1;
    else if (!RB(RT_INTERACTIVE) && RU(RT_MOUSE_POS) != pos)
        saver_quit();
    RU(RT_MOUSE_POS) = pos;
    return 0;
}

/* 0x1001f190: the window procedure for run mode 3 (/s): any key, click or mouse move
 * closes the window unless the scene is interactive. Unhandled messages go on to the
 * engine's own window procedure. */
LRESULT __stdcall saver_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res = 0;
    U handled = g_saver_active;
    if (!handled)
        goto pass;
    switch (msg) {
    case WM_CLOSE:
        handled = (wp && !saver_may_close()) ? 1 : 0;
        break;
    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
    case WM_NCACTIVATE:
        if (!wp)
            saver_quit();
        return DefWindowProcA(hwnd, msg, wp, lp);
    case WM_SETCURSOR:
        handled = !RB(RT_INTERACTIVE);
        if (handled)
            SetCursor(NULL);
        break;
    case WM_SYSKEYDOWN:
        if (wp != VK_F10) {
            saver_quit();
            break; /* still handled */
        }
        /* fall through */
    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        handled = !RB(RT_INTERACTIVE);
        if (handled)
            saver_quit();
        break;
    case WM_SYSCOMMAND:
        if (wp == SC_SCREENSAVE) {
            handled = 1;
            res = 1;
        } else {
            handled = wp == SC_CLOSE;
        }
        break;
    case WM_MOUSEMOVE:
        handled = saver_mouse_move((U)lp);
        break;
    default:
        handled = 0;
    }
pass:
    if (!handled)
        res = CallWindowProcA(R(WNDPROC, RT_WNDPROC), hwnd, msg, wp, lp);
    return res;
}
