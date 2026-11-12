/* Engine core, 0x10025880-0x100265d0: cursors, strings, error messages, the script
 * event queue and message flushing. Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_msg.h"

#define RU(off) R(U, off)
#define RB(off) R(uint8_t, off)
#define RES R(ResFile *, RT_RES)

#define RT_CURSOR_MODE 0x3dd3
#define RT_CURSOR_SHOWN 0x95d7
#define RT_SCENES 0x329b
#define RT_SCENE 0x3dcf
#define RT_SCRIPT_TEXT 0x329f
#define RT_SCRIPT_OFFSETS 0x0cbc
#define RT_SCRIPT_NAME 0x3f43
#define RT_SCRIPT_INDEX 0x3f47
#define RT_STOPPED 0x3ddb
#define RT_RUNNING 0x95f6
#define RT_SCRIPT_DEBUG 0x95d8
#define RT_COMPONENT 0x95d6
#define RT_DIRECTDRAW 0x95d9
#define RT_STOP_CD_ON_LOAD 0x95e8
#define RT_CD_PLAYING 0x95d2
#define RT_ERR_FILE 0x9602
#define RT_RES_TITLE 0x9904
#define RT_TITLE 0x9d56
#define RT_EVENTS 0x36df /* 40 queued events of 0x22 bytes (handler records) */
#define RT_EVENT_COUNT 0x3e47
#define MAX_EVENTS 40
#define EVENT_SIZE 0x22

#define g_module G(HMODULE, 0x1007cef0)

/* 0x10025a6a: string variable n (1-based; bit 31 marks a constant: none here) */
char *__stdcall string_var(U n)
{
    if (!n || (n & 0x80000000u))
        return NULL;
    U i = (n & 0x7fffffff) - 1;
    uint8_t *scene = R(uint8_t *, RT_SCENES) + RU(RT_SCENE) * 0x10c;
    if (i >= P(U, scene, 0x104))
        return NULL;
    return P(char *, scene, 0x108) + i * 0x201;
}

/* 0x100259a6: a string variable, or with bit 31 a constant from the resource file */
char *__stdcall string_get(U n)
{
    if (!n)
        return NULL;
    if (n & 0x80000000u)
        return (char *)ResGetConstStr(RES, n & 0x7fffffff);
    return string_var(n);
}

/* 0x1002657e: shows or hides the cursor for good (ShowCursor counts) */
void __stdcall cursor_show(int on)
{
    if (!on)
        while (ShowCursor(FALSE) >= 0) {}
    else
        while (ShowCursor(TRUE) < 0) {}
}

/* 0x100263da: sets the cursor for the cursor mode: 1 arrow, 2-9 the engine's own */
U __cdecl cursor_update(void)
{
    static const uint16_t field[10] = {0, 0, 0xc9c, 0xcb0, 0, 0xcac, 0xcb4, 0xca8, 0xca4, 0xca0};
    U mode = RU(RT_CURSOR_MODE);
    if (!mode || !RU(RT_HWND))
        return 1;
    HCURSOR c = NULL;
    if (mode == 1)
        c = LoadCursorA(NULL, IDC_ARROW);
    else if (mode < 10 && field[mode])
        c = R(HCURSOR, field[mode]);
    SetCursor(c);
    if (!RB(RT_CURSOR_SHOWN))
        cursor_show(1);
    RB(RT_CURSOR_SHOWN) = 1;
    return 1;
}

/* 0x10025880: sets the cursor mode; leaving modes 3 and 4 drops pending clicks */
void __stdcall cursor_set(U mode)
{
    if (RU(RT_CURSOR_MODE) == 3 || RU(RT_CURSOR_MODE) == 4)
        flush_messages(WM_LBUTTONDOWN);
    RU(RT_CURSOR_MODE) = mode;
    if (RB(RT_CURSOR_SHOWN))
        cursor_update();
}

/* 0x1002635d: called while the resource file loads: keeps sprites moving and stops
 * the CD when asked to */
void __cdecl load_progress(void)
{
    if (!RB(RT_DIRECTDRAW))
        SpriteAnimate(R(uint8_t *, RT_HSL));
    if (RB(RT_STOP_CD_ON_LOAD) && RB(RT_CD_PLAYING))
        RB(RT_CD_PLAYING) = AudioStopCD(R(uint8_t *, 0xc90)) != 0;
}

/* 0x100258e9: writes resource `id` to the file `path`; a failed write deletes it */
U __stdcall res_extract_to_file(LPSTR path, U id)
{
    HMMIO h = mmioOpenA(path, NULL, MMIO_CREATE | MMIO_WRITE);
    if (!h)
        return 0;
    void *old = ResSetCallBack(RES, (void(__stdcall *)(void))(void *)load_progress, 0x2000);
    U r = (U)ResExtractFile(RES, id & 0xffff, h);
    ResSetCallBack(RES, (void(__stdcall *)(void))old, 0x2000);
    mmioClose(h, 0);
    if (!r)
        mmioOpenA(path, NULL, MMIO_DELETE);
    return r;
}

/* 0x100260b4: drops pending messages `msg` */
void __stdcall flush_messages(UINT msg)
{
    MSG m;
    while (PeekMessageA(&m, R(HWND, RT_HWND), msg, msg, PM_REMOVE | PM_NOYIELD)) {}
}

/* 0x10025ff9: drops the engine's own pending messages */
void __cdecl flush_engine_messages(void)
{
    for (U m = 0x4c8; m < 0x4cc; m++)
        flush_messages(m);
    for (U m = 0x52c; m < 0x52f; m++)
        flush_messages(m);
    flush_messages(0x464);
    flush_messages(0x590);
}

static LPCSTR msg_title(void)
{
    return (LPCSTR)(g_rt + (R(char, RT_RES_TITLE) ? RT_RES_TITLE : RT_TITLE));
}

/* the running script's name and the file it came from, appended with `fmt` */
static void append_script(char *out, char *tmp, const char *fmt)
{
    CopyHugeStr(tmp, R(char *, RT_SCRIPT_TEXT) + R(U *, RT_SCRIPT_OFFSETS)[RU(RT_SCRIPT_INDEX)]);
    lstrcpyA((LPSTR)(g_rt + RT_ERR_FILE), string_get(RU(RT_SCRIPT_NAME)));
    wsprintfA(out + lstrlenA(out), fmt, tmp, g_rt + RT_ERR_FILE);
}

/* shows the message, then stops: screen mode back, messages dropped, window closed */
static void show_and_quit(const char *text)
{
    RtShowGDISurface(g_rt);
    if (!RB(RT_COMPONENT))
        MessageBoxA(R(HWND, RT_HWND), text, msg_title(), 0);
    else
        MessageBoxA(GetForegroundWindow(), text, msg_title(), MB_SETFOREGROUND);
    RtRestoreFromGDISurface(g_rt);
    RtSetScreenRes(g_rt, 0, 0, 0);
    flush_engine_messages();
    PostMessageA(R(HWND, RT_HWND), WM_CLOSE, 0, 0);
}

/* 0x10025b0f: a fatal error with code `code` (the second argument is not used) */
void __stdcall rt_fatal(U code, U unused)
{
    (void)unused;
    char fmt[300], text[300];
    RU(RT_STOPPED) = code;
    RB(RT_RUNNING) = 0;
    if (!RB(RT_SCRIPT_DEBUG) || !RU(RT_SCRIPT_TEXT) || !RU(RT_SCRIPT_NAME)) {
        LoadStringA(g_module, 0x77, fmt, 300);
        wsprintfA(text, fmt, code, RU(RT_SCRIPT_INDEX));
    } else {
        LoadStringA(g_module, 0x78, fmt, 300);
        wsprintfA(text, fmt, code, RU(RT_SCRIPT_INDEX));
        append_script(text, fmt, (const char *)ORIG_PTR(0x10075610));
    }
    show_and_quit(text);
}

/* 0x100260f6: a script error: message `id` from the resources (with `arg`), the
 * running script added; always 0 */
U __stdcall rt_script_error(UINT id, U arg)
{
    char fmt[300], text[300];
    RU(RT_STOPPED) = id;
    RB(RT_RUNNING) = 0;
    LoadStringA(g_module, id, arg ? fmt : text, 300);
    if (arg)
        wsprintfA(text, fmt, arg);
    if (RU(RT_SCRIPT_TEXT) && RU(RT_SCRIPT_NAME))
        append_script(text, fmt, (const char *)ORIG_PTR(0x1007562c));
    show_and_quit(text);
    return 0;
}

/* 0x10025da8: a message box with text `id` from the resources (with `arg`) */
void __stdcall rt_message_box(HWND owner, UINT id, U arg)
{
    char fmt[300], text[300];
    LoadStringA(g_module, id, arg ? fmt : text, 300);
    if (arg)
        wsprintfA(text, fmt, arg);
    RtShowGDISurface(g_rt);
    MessageBoxA(owner, text, msg_title(), 0);
    RtRestoreFromGDISurface(g_rt);
}

static U queue_full(void)
{
    if (!RB(RT_SCRIPT_DEBUG)) {
        rt_fatal(0x3ec, 0);
        return 0;
    }
    return rt_script_error(0x70, 0);
}

/* 0x10025e97: queues handler `handler` with two arguments */
U __stdcall event_send(U handler, U a, U b)
{
    if (RU(RT_EVENT_COUNT) >= MAX_EVENTS)
        return queue_full();
    uint8_t *e = g_rt + RT_EVENTS + RU(RT_EVENT_COUNT) * EVENT_SIZE;
    memset(e, 0, EVENT_SIZE);
    P(U, e, 2) = handler;
    P(U, e, 10) = a;
    P(U, e, 14) = b;
    e[9] = 2;
    RU(RT_EVENT_COUNT)++;
    return 1;
}

/* 0x10025f5d: queues a prepared handler record */
U __stdcall event_post(const uint8_t *rec)
{
    if (RU(RT_EVENT_COUNT) >= MAX_EVENTS)
        return queue_full();
    memcpy(g_rt + RT_EVENTS + RU(RT_EVENT_COUNT) * EVENT_SIZE, rec, EVENT_SIZE);
    RU(RT_EVENT_COUNT)++;
    return 1;
}
