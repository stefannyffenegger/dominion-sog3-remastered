/* Engine core, 0x100265d0-0x100278d0: the engine window's procedure and what it
 * calls: activation (switching away pauses the game unless AltTab is set), painting,
 * input routing, closing, and the engine's own messages. Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_wndproc.h"

#define C1(a, x) ORIG(U(__stdcall *)(U), a)((U)(x))
#define RU(off) R(U, off)
#define RB(off) R(uint8_t, off)
#define HSL R(uint8_t *, RT_HSL)
#define AUDIO R(uint8_t *, RT_AUDIO)

#define RT_IDLE_DELAY 0x0b7c
#define RT_IDLE_DUE 0x0b80
#define RT_LAST_INPUT 0x0b84
#define RT_NEW_SCENE 0x0b8a
#define RT_AUDIO 0x0c90
#define RT_HOOK_ARG 0x0c98
#define RT_HRL 0x0cc4
#define RT_TOPMOST 0x0cd4
#define RT_MSG_HOOK 0x18bb   /* proc(arg, msg, wp, lp) for messages 0x6bc-0x71f */
#define RT_BKG_INDEX 0x3dc3
#define RT_CURSOR_MODE 0x3dd3
#define RT_STOPPED 0x3ddb
#define RT_SAVED_CURSOR 0x3e03
#define RT_RUN_MODE 0x3e5b
#define RT_ON_ACTIVATE 0x3e5f
#define RT_ON_RESUME 0x3e63
#define RT_ON_EXIT 0x3e6f
#define RT_FOCUS_TEXT 0x3e77
#define RT_IDLE_EVENT 0x3f3f
#define RT_DEFERRED 0x3f0f
#define RT_SYS_PALETTE 0x32af
#define RT_ACTIVE 0x95cd     /* the game runs (the window is active) */
#define RT_APP_ACTIVE 0x95ce /* the application is active */
#define RT_ALT_TAB 0x95d0    /* AltTab setting: keep running when switched away */
#define RT_CLOSING 0x95d4
#define RT_COMPONENT 0x95d6
#define RT_CURSOR_SHOWN 0x95d7
#define RT_DIRECTDRAW 0x95d9
#define RT_JOY_CAPTURED 0x95e3
#define RT_PALETTE_REALIZED 0x95ee
#define RT_PAUSED 0x95f1
#define RT_RUNNING 0x95f6
#define RT_NO_PAINT 0x95fa
#define RT_WINDOW_BOX 0x9600


/* 0x100268f0: fills the window with the background color (palette entry RT_BKG_INDEX,
 * black for 0) */
void __stdcall paint_black(HDC dc)
{
    if (RB(RT_NO_PAINT))
        return;
    RECT r;
    GetClientRect(R(HWND, RT_HWND), &r);
    OffsetRect(&r, -R(int, 0x36c7), -R(int, 0x36cb));
    U i = RU(RT_BKG_INDEX);
    if (!i) {
        FillRect(dc, &r, GetStockObject(BLACK_BRUSH));
        return;
    }
    COLORREF c;
    if (!RB(RT_PALETTE_DEVICE)) {
        const uint8_t *p = g_rt + RT_SYS_PALETTE + i * 4;
        c = RGB(p[0], p[1], p[2]);
    } else {
        c = R(uint16_t, RT_BKG_INDEX) | 0x1000000u;
    }
    HBRUSH b = CreateSolidBrush(c);
    if (b) {
        FillRect(dc, &r, b);
        DeleteObject(b);
    }
}

/* 0x100265d0: the window became active or inactive. Unless AltTab is set, an inactive
 * game stops running: sound and joysticks off, the palette given back; coming back
 * restores the display mode and runs the resume handler. */
void __stdcall window_activate(HWND hwnd, U active_arg, U unused)
{
    (void)unused;
    if (RB(RT_CLOSING))
        return;
    U active = active_arg && GetForegroundWindow() == hwnd;
    U changed = RB(RT_APP_ACTIVE) != active;
    if (changed) {
        plugins_notify_18(active);
        controllers_focus(active == 0);
        if (RU(RT_RES))
            AudioActivate(AUDIO, (int)active);
        if (RU(RT_ON_ACTIVATE)) {
            uint8_t rec[0x22];
            memset(rec, 0, sizeof rec);
            P(U, rec, 2) = RU(RT_ON_ACTIVATE);
            rec[9] = 1;
            P(U, rec, 10) = active;
            handler_record_run(rec);
        }
        RB(RT_APP_ACTIVE) = (uint8_t)active;
    }
    if (RB(RT_ALT_TAB) || !RU(RT_RES) || RB(RT_ACTIVE) == active)
        return;
    RB(RT_ACTIVE) = (uint8_t)active;
    if (!RB(RT_ACTIVE)) {
        if (RB(RT_JOY_CAPTURED))
            joystick_release();
    } else {
        if (RU(RT_ON_RESUME))
            script_run(RU(RT_ON_RESUME));
        if (RU(RT_SAVED_CURSOR))
            RU(RT_CURSOR_MODE) = RU(RT_SAVED_CURSOR);
        RU(RT_SAVED_CURSOR) = 0;
        cursor_update();
        if (!RB(RT_JOY_CAPTURED))
            joystick_capture();
    }
    if (!changed || !RB(RT_DIRECTDRAW)) {
        if (!active) {
            SpriteRealizePalette2(HSL, 1);
        } else {
            dd_set_mode(RU(0x3de3), RU(0x3e0f));
            PostMessageA(hwnd, WM_SIZE, SIZE_MAXIMIZED, 0);
        }
    } else {
        display_activate(HSL, RB(RT_ACTIVE));
    }
    RB(RT_RUNNING) = RB(RT_ACTIVE) && RU(RT_RES);
    if (changed && active)
        PostMessageA(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
}

/* 0x10026a57: registers the window class "VIEW_CLASS" (unless `skip`) */
ATOM __stdcall window_class_register(U skip)
{
    if (skip)
        return 1;
    WNDCLASSA wc;
    HINSTANCE inst = G(HINSTANCE, 0x1007cef0);
    if (GetClassInfoA(inst, "VIEW_CLASS", &wc))
        UnregisterClassA("VIEW_CLASS", inst);
    memset(&wc, 0, sizeof wc);
    wc.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
    wc.lpfnWndProc = (WNDPROC)ORIG_PTR(0x10026b17);
    wc.hInstance = inst;
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(1000));
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.lpszClassName = "VIEW_CLASS";
    wc.hbrBackground = GetStockObject(BLACK_BRUSH);
    return RegisterClassA(&wc);
}

/* 0x100277df: input came: the idle event moves on */
void __cdecl idle_rearm(void) { RU(RT_IDLE_DUE) = RU(RT_LAST_INPUT) + RU(RT_IDLE_DELAY); }

static void input_seen(void)
{
    RU(RT_LAST_INPUT) = timeGetTime();
    if (RU(RT_IDLE_EVENT))
        idle_rearm();
}

/* 0x10027604: after a fatal stop (messages 0x590, 0x464) a last message box */
void __cdecl window_stopped(void)
{
    paint_black(R(HDC, RT_DC));
    if (RU(RT_STOPPED) == 0x590 || RU(RT_STOPPED) == 0x464)
        rt_message_box(NULL, RU(RT_STOPPED) == 0x590 ? 0x6e : 100, 0);
}

/* 0x10027328: WM_CLOSE: exit handler, then everything shut down and the window gone */
void __cdecl window_close(void)
{
    static const uint16_t cursors[7] = {0xc9c, 0xcb0, 0xcac, 0xcb4, 0xca8, 0xca4, 0xca0};
    if (RB(RT_CLOSING) || RB(RT_COMPONENT))
        return;
    if (RU(RT_ON_EXIT)) {
        uint8_t rec[0x22];
        memset(rec, 0, sizeof rec);
        P(U, rec, 2) = RU(RT_ON_EXIT);
        RU(RT_ON_EXIT) = 0;
        handler_record_run(rec);
    }
    RB(RT_CLOSING) = 1;
    RB(RT_ACTIVE) = 0;
    RB(RT_RUNNING) = 0;
    if (RB(RT_JOY_CAPTURED))
        joystick_release();
    if (RU(RT_RES))
        scene_exit();
    flush_engine_messages();
    if (RU(RT_HRL))
        ResExitLibrary(0);
    if (RU(RT_HSL))
        SpriteExitLibrary(HSL);
    if (RU(RT_AUDIO))
        AudioExitLibrary(AUDIO);
    cursor_set(1);
    if (!RB(RT_CURSOR_SHOWN))
        cursor_show(1);
    cursor_set(1);
    if (RU(RT_STOPPED))
        window_stopped();
    for (int i = 0; i < 7; i++)
        if (RU(cursors[i]))
            DestroyCursor(R(HCURSOR, cursors[i]));
    ReleaseDC(R(HWND, RT_HWND), R(HDC, RT_DC));
    DestroyWindow(R(HWND, RT_HWND));
    RU(RT_HWND) = 0;
    PostQuitMessage(0);
}

/* 0x10027687: message 0x4c8: runs the deferred handler */
void __cdecl deferred_run(void)
{
    U top = RU(RT_TOPMOST);
    if (RU(RT_DEFERRED)) {
        script_run(RU(RT_DEFERRED));
        SpriteAnimate(HSL);
        RU(RT_DEFERRED) = 0;
    }
    if (top)
        compose_reset(1);
}

/* 0x10027713: WM_PAINT */
void __cdecl window_paint(void)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(R(HWND, RT_HWND), &ps);
    if (RB(RT_WINDOW_BOX) && !RB(RT_DIRECTDRAW))
        paint_black(dc);
    if (!RB(RT_PALETTE_REALIZED) && !RB(RT_DIRECTDRAW))
        SpriteRealizePalette(HSL);
    SpritePaint(HSL, NULL, dc);
    EndPaint(R(HWND, RT_HWND), &ps);
}

/* 0x10027812: Alt+Tab and Alt+F4. Unless AltTab is set, Alt+Tab stops the game first. */
void __stdcall window_alt_key(HWND hwnd, WPARAM wp, LPARAM lp)
{
    if (wp == VK_TAB && !RB(RT_ALT_TAB)) {
        RB(RT_RUNNING) = 0;
        RB(RT_ACTIVE) = 0;
        plugins_notify_18(0);
        AudioActivate(AUDIO, 0);
        RU(RT_SAVED_CURSOR) = RU(RT_CURSOR_MODE);
        RU(RT_CURSOR_MODE) = 1;
        cursor_update();
    }
    DefWindowProcA(hwnd, WM_SYSKEYDOWN, wp, lp);
}

static void key_pressed(WPARAM wp)
{
    input_seen();
    if (!RU(RT_FOCUS_TEXT) || wp != VK_DELETE)
        key_down((U)wp);
    else
        edit_key(VK_DELETE);
}

/* 0x10026b17: the engine window's procedure */
LRESULT __stdcall window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res = 0;
    int handled = 1;
    switch (msg) {
    case WM_ACTIVATEAPP:
        handled = RU(RT_RES) != 0;
        if (handled && RB(RT_ACTIVE) != wp)
            window_activate(hwnd, (U)wp, 0);
        break;
    case WM_GETTEXT:
        *(char *)lp = 0;
        return 0;
    case WM_PAINT:
        window_paint();
        break;
    case WM_CLOSE:
        window_close();
        break;
    case WM_NCPAINT:
        return 0;
    case WM_SETCURSOR:
        res = (LRESULT)cursor_update();
        break;
    case WM_PALETTECHANGED:
        if ((HWND)wp == hwnd) {
            RB(RT_PALETTE_REALIZED) = 1;
            handled = 0;
            break;
        }
        res = 1;
        /* fall through */
    case WM_QUERYNEWPALETTE:
        if (SpriteRealizePalette(HSL))
            InvalidateRect(hwnd, NULL, TRUE);
        break;
    case WM_MOUSEMOVE:
        input_seen();
        mouse_move((U)lp);
        break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        input_seen();
        mouse_down(msg, (uint8_t)wp, (U)lp);
        break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        input_seen();
        mouse_up(msg, (U)lp);
        break;
    case MM_JOY1MOVE:
    case MM_JOY2MOVE:
    case MM_JOY1BUTTONDOWN:
    case MM_JOY2BUTTONDOWN:
    case MM_JOY1BUTTONUP:
    case MM_JOY2BUTTONUP:
        RU(RT_LAST_INPUT) = timeGetTime();
        if (!RB(RT_PAUSED))
            joy_message(msg, (U)wp, (U)lp);
        break;
    case 0x464:
    case 0x590:
        flush_engine_messages();
        RU(RT_STOPPED) = msg;
        PostMessageA(hwnd, WM_CLOSE, 0, 0);
        break;
    case 0x52c:
        if (!(lp & 0xffff))
            obj_release((U)wp, ResGetType(R(ResFile *, RT_RES), (U)wp));
        else
            event_send((U)lp & 0xffff, (U)wp, 0);
        break;
    case 0x52d:
    case 0x52e:
        if (lp) {
            U *call = (U *)lp;
            U a[2] = {call[1], call[2]};
            orig_stdcall_unsafe((void *)(uintptr_t)call[0], 2, a);
            winfree(call);
        }
        break;
    case 0x4c8:
        deferred_run();
        break;
    case 0x4c9:
        scene_change((char *)(g_rt + RT_NEW_SCENE));
        break;
    case 0x5f4:
        plugins_free_dlls();
        break;
    case 0x5f5:
        if (watch_poll())
            PostMessageA(hwnd, msg, wp, lp);
        break;
    case WM_SYSCOMMAND:
        handled = wp == SC_SCREENSAVE && RU(RT_RUN_MODE) != 1;
        if (handled)
            res = 1;
        else if (wp == SC_PREVWINDOW) {
            PostMessageA(hwnd, WM_ACTIVATEAPP, 0, 0);
            G(U, 0x100871ac) = 1;
        }
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        input_seen();
        key_up((U)wp);
        break;
    case WM_CHAR:
        input_seen();
        handled = RU(RT_FOCUS_TEXT) != 0;
        if (handled)
            edit_char((U)wp);
        break;
    case WM_SYSKEYDOWN:
        input_seen();
        if (wp == VK_TAB || wp == VK_F4) {
            window_alt_key(hwnd, wp, lp);
            break;
        }
        key_pressed(wp);
        break;
    case WM_KEYDOWN:
        key_pressed(wp);
        break;
    default:
        if (RU(RT_MSG_HOOK) && msg >= 0x6bc && msg <= 0x71f) {
            U a[4] = {RU(RT_HOOK_ARG), msg, (U)wp, (U)lp};
            orig_stdcall_unsafe(R(void *, RT_MSG_HOOK), 4, a);
        } else {
            handled = 0;
        }
    }
    if (!handled)
        res = DefWindowProcA(hwnd, msg, wp, lp);
    return res;
}
