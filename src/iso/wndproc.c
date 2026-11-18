/* The game logic, 0x1005e5e0-0x100600b8: the game window's message handler (the window is
 * subclassed; what it does not take goes on to the engine's handler) and the drag box drawn
 * with GDI or DirectDraw while the mouse is held down. */
#include <string.h>
#include "iso_core.h"
#include "view.h"
#include "cursor.h"
#include "records.h"
#include "popup.h"
#include "click.h"
#include "buttons.h"
#include "healthbar.h"
#include "wndproc.h"
#include "rubber.h"

#define APP I(void *, ISO_APP)
#define ISO_SCROLL_X 0xcd82
#define ISO_SCROLL_Y 0xcd86
#define ISO_VIEW_H 0xd72a
#define CUR_ENABLED 0xd75d
#define CUR_SPRITE 0xd761

#define KEY_CTRL 0xd797
#define KEY_ALT 0xd79b
#define KEY_SHIFT 0xd7af
#define MOUSE_X 0xd76d          /* the mouse on the screen, mapped */
#define MOUSE_Y 0xd771
#define MOUSE_OP 0xd77d         /* the cursor's operation */
#define MOUSE_UNIT 0xd781       /* short: the unit under the mouse */
#define MAP_RECT 0xd787         /* the map's part of the screen (RECT) */
#define DIRECTDRAW 0xd79f       /* the box is drawn by the engine (DirectDraw), not with GDI */
#define HANDLE_MOUSE 0xd7a3     /* the map takes mouse input */
#define RIGHT_DOWN 0xd7a7
#define DRAGGING 0xd7ab
#define DRAG_DC 0xd7b3
#define DRAG_PEN 0xd7b7
#define DRAG_BRUSH 0xd7bb
#define DRAG_LAST 0xd7c7        /* the last point (x, y) */
#define OLD_PROC 0xd7cf         /* the engine's window handler */
#define DRAG_ORIGIN 0xd7df
#define DRAG_BOX 0xd7e7         /* left, top, right, bottom */
#define DRAG_HANDLER 0xd7f7     /* the script's handler for a finished drag */
#define RIGHT_DOWN_AT 0xd7ff    /* timeGetTime of the last right press */
#define POPUP_DELAY 0xdac8      /* a right click shorter than this does not open the popup */
#define POPUP_ENABLED 0xdacc
#define POPUP_SHOWN 0xdaf0
#define POPUP_RECT 0xdaf8

#define debug_print ORIG(void(__cdecl *)(int, const char *), 0x10095e98)
#define EDITPATH_GetEditMode ORIG(int(__cdecl *)(void), 0x1009b89f)
#define IsoBrowsePopupMenu ORIG(uint32_t(__stdcall *)(uint8_t *, int, int, int), 0x100d3c1e)
#define IsoHidePopupMenu ORIG(void(__stdcall *)(uint8_t *), 0x100d4263)
#define IsoActivateSelectionMode ORIG(void(__cdecl *)(uint8_t *, int, uint32_t, uint32_t), 0x10073acc)
#define IsoHideBeaconDirections ORIG(void(__cdecl *)(uint8_t *), 0x1007c111)
#define unit_selectable ORIG(int(__cdecl *)(uint32_t), 0x100f4167)

typedef void(__cdecl *click_hook)(uint32_t, int32_t, int32_t, uint32_t, uint32_t, int);

static int key_down(int vk) { return (int16_t)GetAsyncKeyState(vk) & 0x8000; }

static void read_modifiers(void)
{
    I(uint32_t, KEY_CTRL) = key_down(VK_CONTROL);
    I(uint32_t, KEY_SHIFT) = key_down(VK_SHIFT);
    I(uint32_t, KEY_ALT) = key_down(VK_MENU);
}

/* a key message for the script: handler at +2, kind 1 at +9, the key at +10 */
static int send_key(uint32_t handler, WPARAM key)
{
    uint8_t rec[0x22];
    memset(rec, 0, sizeof rec);
    P(uint32_t, rec, 2) = handler;
    rec[9] = 1;
    P(uint32_t, rec, 10) = (uint32_t)key;
    return RtRunMsg(APP, rec);
}

static int in_map_rect(void)
{
    int x = I(int32_t, MOUSE_X), y = I(int32_t, MOUSE_Y);
    return I(int32_t, MAP_RECT) <= x && x < I(int32_t, MAP_RECT + 8) &&
           I(int32_t, MAP_RECT + 4) <= y && y < I(int32_t, MAP_RECT + 12);
}

/* 0x1005fab2: draws (or, XORed again, erases) the drag box with GDI */
void __stdcall drag_box_draw(HDC dc)
{
    int rop = SetROP2(dc, I(int, DIRECTDRAW) == 0 ? R2_XORPEN : 0x10);
    HGDIOBJ pen = SelectObject(dc, I(HGDIOBJ, DRAG_PEN));
    HGDIOBJ brush = SelectObject(dc, I(HGDIOBJ, DRAG_BRUSH));
    Rectangle(dc, I(int, DRAG_BOX), I(int, DRAG_BOX + 4), I(int, DRAG_BOX + 8), I(int, DRAG_BOX + 12));
    SetROP2(dc, rop);
    SelectObject(dc, pen);
    SelectObject(dc, brush);
}

/* 0x1005f8c3: the left button goes down: starts a drag box there (inside the map's part of
 * the screen), hiding the cursor sprite. 1 if taken */
int __stdcall drag_start(LPARAM pos)
{
    POINT pt = { (int16_t)pos, (int16_t)((uint32_t)pos >> 16) };
    RtMapPoint(APP, &pt);
    if (I(int32_t, MAP_RECT) < I(int32_t, MAP_RECT + 8) && !PtInRect((RECT *)(g_iso + MAP_RECT), pt))
        return 0;
    if (I(void *, CUR_SPRITE) && !I(int, DIRECTDRAW)) {
        SpriteEmptyQueue(I(void *, ISO_HSL), I(void *, CUR_SPRITE));
        RtRegisterCursorSprite(APP, 0);
        SpriteHide(I(void *, ISO_HSL), I(void *, CUR_SPRITE), 1);
    }
    I(int, DRAGGING) = 1;
    I(int32_t, DRAG_LAST) = pt.x;
    I(int32_t, DRAG_LAST + 4) = pt.y;
    I(int32_t, DRAG_ORIGIN) = pt.x;
    I(int32_t, DRAG_ORIGIN + 4) = pt.y;
    I(int32_t, DRAG_BOX + 8) = pt.x;
    I(int32_t, DRAG_BOX + 12) = pt.y;
    I(int32_t, DRAG_BOX) = pt.x;
    I(int32_t, DRAG_BOX + 4) = pt.y;
    if (!I(int, DIRECTDRAW)) {
        I(HPEN, DRAG_PEN) = CreatePen(PS_SOLID, 2, 0xffffff);
        I(HGDIOBJ, DRAG_BRUSH) = GetStockObject(NULL_BRUSH);
        drag_box_draw(I(HDC, DRAG_DC));
    }
    return 1;
}

/* 0x1005fb7f: the drag ends: the box is erased and, unless `cancel` (Escape), ordered and
 * handed to the map's click handler and the script's handler. 1 if a drag was going on */
int __stdcall drag_end(int cancel)
{
    if (!I(int, DRAGGING))
        return 0;
    if (!I(int, DIRECTDRAW))
        drag_box_draw(I(HDC, DRAG_DC));
    else
        RtDirectDrawRect(APP, 0, 0, 0, 0);
    if (I(HGDIOBJ, DRAG_PEN))
        DeleteObject(I(HGDIOBJ, DRAG_PEN));
    I(int, DRAGGING) = 0;
    if (I(void *, CUR_SPRITE) && !I(int, DIRECTDRAW))
        IsoSetMouseCursorState(g_iso, I(uint32_t, MOUSE_OP), 1);
    if (!cancel) {
        for (int k = 0; k < 2; k++) {
            int32_t *lo = &I(int32_t, DRAG_BOX + 4 * k), *hi = &I(int32_t, DRAG_BOX + 8 + 4 * k);
            if (*hi < *lo) {
                int32_t t = *lo;
                *lo = *hi;
                *hi = t;
            }
        }
        for (int k = 0; k < 4; k++)
            I(int32_t, 0xbf2c + 4 * k) = I(int32_t, DRAG_BOX + 4 * k);
        map_click();
        if (I(uint32_t, DRAG_HANDLER)) {
            uint8_t rec[0x22];
            memset(rec, 0, sizeof rec);
            P(uint32_t, rec, 2) = I(uint32_t, DRAG_HANDLER);
            for (int k = 0; k < 4; k++)
                P(uint32_t, rec, 10 + 4 * k) = I(uint32_t, DRAG_BOX + 4 * k);
            P(uint32_t, rec, 0x1a) = I(uint32_t, KEY_CTRL);
            P(uint32_t, rec, 0x1e) = I(uint32_t, KEY_SHIFT);
            rec[9] = 6;
            RtRunMsg(APP, rec);
        }
    }
    return 1;
}

/* 0x1005fe4b: the mouse moves during a drag: the box follows (kept to the map's part of the
 * screen), or moves as a whole while the right button is down. Returns 1 */
int __stdcall drag_move(LPARAM pos)
{
    POINT pt = { (int16_t)pos, (int16_t)((uint32_t)pos >> 16) };
    RtMapPoint(APP, &pt);
    if (!I(int, DIRECTDRAW))
        drag_box_draw(I(HDC, DRAG_DC));
    if (I(int32_t, MAP_RECT) < I(int32_t, MAP_RECT + 8)) {
        if (pt.x < I(int32_t, MAP_RECT))
            pt.x = I(int32_t, MAP_RECT);
        if (pt.y < I(int32_t, MAP_RECT + 4))
            pt.y = I(int32_t, MAP_RECT + 4);
        if (I(int32_t, MAP_RECT + 8) < pt.x)
            pt.x = I(int32_t, MAP_RECT + 8);
        if (I(int32_t, MAP_RECT + 12) < pt.y)
            pt.y = I(int32_t, MAP_RECT + 12);
    }
    if (!I(int, RIGHT_DOWN)) {
        I(int32_t, DRAG_BOX + 8) = pt.x;
        I(int32_t, DRAG_BOX + 12) = pt.y;
    } else {
        int dx = pt.x - I(int32_t, DRAG_LAST), dy = pt.y - I(int32_t, DRAG_LAST + 4);
        I(int32_t, DRAG_ORIGIN) += dx;
        I(int32_t, DRAG_ORIGIN + 4) += dy;
        I(int32_t, DRAG_BOX) += dx;
        I(int32_t, DRAG_BOX + 8) += dx;
        I(int32_t, DRAG_BOX + 4) += dy;
        I(int32_t, DRAG_BOX + 12) += dy;
    }
    I(int32_t, DRAG_LAST) = pt.x;
    I(int32_t, DRAG_LAST + 4) = pt.y;
    if (!I(int, DIRECTDRAW))
        drag_box_draw(I(HDC, DRAG_DC));
    else if (I(int32_t, DRAG_BOX + 8) != I(int32_t, DRAG_BOX) || I(int32_t, DRAG_BOX + 12) != I(int32_t, DRAG_BOX + 4))
        RtDirectDrawRect(APP, I(int, DRAG_BOX), I(int, DRAG_BOX + 4), I(int, DRAG_BOX + 8), I(int, DRAG_BOX + 12));
    return 1;
}

/* the right button comes up over the map: selects the unit under the mouse and opens the
 * popup menu for it, or leaves the selection mode */
static int right_click_popup(void)
{
    int player = I(int32_t, ISO_LOCAL_PLAYER);
    uint32_t id = I(uint16_t, MOUSE_UNIT);
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
    int selectable = unit_selectable(id);
    int open = 1;
    uint32_t category = (id && u) ? unit_category(id) : 0;
    int others = 0;
    for (int p = 0; p < I(int32_t, 0x1b1f); p++)
        if (I(int32_t, ISO_LOCAL_PLAYER) != p && I(int32_t, ISO_SELECTED_N + p * 4)) {
            others = 1;
            break;
        }
    int foreign = category && u && P(uint8_t, u, 0x14) != I(uint32_t, ISO_LOCAL_PLAYER);
    int quick = timeGetTime() - I(uint32_t, RIGHT_DOWN_AT) < I(uint32_t, POPUP_DELAY);
    int count = I(int32_t, ISO_SELECTED_N + player * 4);
    int op = I(int32_t, MOUSE_OP);
    if (!I(int, POPUP_ENABLED) || I(uint8_t, 0x17d) || foreign || (!category && (others || quick || count)) ||
        (category && !selectable) || op == 6 || op == 10 || op == 8 || op == 99 || I(int, 0xc303))
        open = 0;
    if ((!category && (others || count)) || (id && u && !P(uint8_t, u, 0x2e) && selectable)) {
        for (int p = 0; p < I(int32_t, 0x1b1f); p++)
            selection_clear(p);
        count = 0;
    }
    if (I(int, POPUP_ENABLED) && id && u && !P(uint8_t, u, 0x2e) && selectable && !foreign) {
        player = I(int32_t, ISO_LOCAL_PLAYER);
        I(uint16_t *, ISO_SELECTED + player * 4)[count] = (uint16_t)id;
        I(int32_t, ISO_SELECTED_N + I(int32_t, ISO_LOCAL_PLAYER) * 4)++;
        P(uint8_t, u, 0x2e) = 1;
        unit_health_bar(id, 0);
    }
    if (!open) {
        IsoActivateSelectionMode(g_iso, 0, 0, 0);
        return 0;
    }
    popup_menu_open(I(int32_t, ISO_SCROLL_X) + I(int32_t, MOUSE_X), I(int32_t, ISO_SCROLL_Y) + I(int32_t, MOUSE_Y),
                    id, (int)category);
    DebugStr("Popup for type = %d, slot = %d", category, id & 0xffff);
    return 1;
}

/* 0x1005e5e0: the game window's message handler */
LRESULT __stdcall iso_window_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    MSG m;
    int taken = 0;

    if (msg == WM_CHAR && I(int, 0x14c)) {
        debug_print(0x1a, (const char *)ORIG_PTR(0x1013f800));
        return 0;
    }
    if (msg == WM_MOUSEMOVE) {
        while (PeekMessageA(&m, 0, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE))
            lp = m.lParam;
        if (wp & (MK_SHIFT | MK_CONTROL)) {
            /* drop the key presses waiting, up to the first digit, letter or 0x1b-0x2f */
            while (PeekMessageA(&m, 0, WM_KEYDOWN, WM_KEYDOWN, PM_NOREMOVE) &&
                   (m.wParam < 0x30 || m.wParam > 0x39) && (m.wParam < 0x41 || m.wParam > 0x7a) &&
                   (m.wParam < 0x1b || m.wParam > 0x2f))
                PeekMessageA(&m, 0, WM_KEYDOWN, WM_KEYDOWN, PM_REMOVE);
            if (wp & MK_CONTROL)
                I(uint32_t, KEY_CTRL) = 1;
            if (wp & MK_SHIFT)
                I(uint32_t, KEY_SHIFT) = 1;
        }
        if (key_down(VK_MENU)) {
            while (PeekMessageA(&m, 0, WM_SYSKEYDOWN, WM_SYSKEYDOWN, PM_REMOVE))
                ;
            I(uint32_t, KEY_ALT) = 1;
        }
    }
    if (I(uint8_t, 0xd90c) || !I(int, 0x1960) || !I(int, ISO_HAVE_SPRITES)) {
        read_modifiers();
        if ((msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) && wp == VK_F10)
            return 0;
        int f4 = (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) && wp == VK_F4;
        if (f4 || (msg == WM_CHAR && I(int, 0x124))) {
            send_key(f4 ? I(uint32_t, 0xe094) : I(uint32_t, 0xa1a), wp);
            return 0;
        }
        if (msg == WM_KEYDOWN && wp == VK_PAUSE)
            return 0;
        if (!I(int, ISO_HAVE_SPRITES) && EDITPATH_GetEditMode())
            editpath_message(msg, wp, lp);
        if (!I(int, HANDLE_MOUSE) || I(int, ISO_HAVE_SPRITES))
            return CallWindowProcA(I(WNDPROC, OLD_PROC), wnd, msg, wp, lp);
    }

    taken = 0;
    switch (msg) {
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN:
        goto modifiers;
    case WM_LBUTTONUP: case WM_RBUTTONUP:
        if (I(int, ISO_HAVE_SPRITES) && I(int, POPUP_SHOWN)) {
            POINT pt = { (int16_t)lp, (int16_t)((uint32_t)lp >> 16) };
            RtMapPoint(APP, &pt);
            int x = I(int32_t, ISO_SCROLL_X) + pt.x, y = I(int32_t, ISO_SCROLL_Y) + pt.y;
            if (x < I(int32_t, POPUP_RECT) || I(int32_t, POPUP_RECT + 8) <= x || y < I(int32_t, POPUP_RECT + 4) ||
                I(int32_t, POPUP_RECT + 12) <= y) {
                IsoHidePopupMenu(g_iso);
            } else {
                uint32_t item = IsoBrowsePopupMenu(g_iso, x, y, 1);
                if (msg == WM_LBUTTONUP && item != 0xfffffffe && item != 0xffffffff) {
                    IsoProcessAction(g_iso, (int)item);
                    IsoHidePopupMenu(g_iso);
                }
            }
            taken = 1;
        }
        goto modifiers;
    default:
        goto dispatch;
    }
modifiers:
    if (msg == WM_RBUTTONDOWN)
        I(uint32_t, RIGHT_DOWN_AT) = timeGetTime();
    read_modifiers();
dispatch:
    if (msg == WM_KEYDOWN || msg == WM_KEYUP) {
        if (I(uint8_t, 0x180) && I(int, 0x148)) {
            /* the host's Escape: everyone still playing (bit 9) and in (bit 0 or 2), not out
             * (bit 3), is asked to leave */
            if (wp == VK_ESCAPE && I(int, 0x148)) {
                for (int p = 0; p < I(int32_t, 0x1b1f); p++) {
                    uint16_t f = I(uint16_t, p * ISO_PLAYER_SIZE + 0x2fc7);
                    if ((f >> 9 & 1) && ((f & 1) || (f >> 2 & 1)) && !(f >> 3 & 1))
                        I(uint16_t, p * ISO_PLAYER_SIZE + 0x2fc5) = 1;
                }
                if (I(int, 0x1af1))
                    I(int, 0x1b09) = 1;
            }
        } else {
            if (wp == VK_ESCAPE)
                taken = drag_end(1);
            if (msg == WM_KEYDOWN && wp == VK_PAUSE && !I(int, 0xa1e))
                return 0;
            if ((msg == WM_KEYDOWN && I(int, 0xa1e) && !I(int, 0x124)) || (msg == WM_KEYUP && wp == VK_SNAPSHOT)) {
                if ((!I(uint8_t, 0x17e) || wp == VK_PAUSE) && (!I(uint8_t, 0x180) || wp == VK_ESCAPE))
                    send_key(I(uint32_t, 0xa1e), wp);
                taken = 1;
            }
        }
    } else if (msg == WM_SYSKEYDOWN && wp != VK_MENU) {
        I(uint32_t, KEY_ALT) = 1;
        if (I(int, 0xa1e)) {
            if (!I(uint8_t, 0x17e))
                send_key(I(uint32_t, 0xa1e), wp);
            taken = 1;
            I(uint32_t, KEY_ALT) = 0;
        }
    } else if (msg == WM_SYSKEYUP) {
        I(uint32_t, KEY_ALT) = 0;
    } else if (msg == WM_RBUTTONDOWN) {
        I(int, RIGHT_DOWN) = 1;
    } else if (msg == WM_RBUTTONUP) {
        I(int, RIGHT_DOWN) = 0;
        if (!taken && in_map_rect() && I(int, ISO_HAVE_SPRITES) && !I(int, POPUP_SHOWN))
            taken = right_click_popup();
    } else if (msg == WM_LBUTTONDOWN && I(int, HANDLE_MOUSE) && !I(int, POPUP_SHOWN)) {
        taken = drag_start(lp);
    } else {
        if (msg == WM_MOUSEMOVE && I(int, CUR_ENABLED) && I(void *, CUR_SPRITE)) {
            POINT pt = { (int16_t)lp, (int16_t)((uint32_t)lp >> 16) };
            if (I(int32_t, ISO_VIEW_H) <= pt.y)
                IsoHideBeaconDirections(g_iso);
            RtMapPoint(APP, &pt);
            I(int32_t, MOUSE_X) = pt.x;
            I(int32_t, MOUSE_Y) = pt.y;
            SpriteSetPosition(I(void *, ISO_HSL), I(void *, CUR_SPRITE), I(int32_t, MOUSE_X), I(int32_t, MOUSE_Y), 0);
            if (I(int, POPUP_SHOWN))
                IsoBrowsePopupMenu(g_iso, I(int32_t, ISO_SCROLL_X) + I(int32_t, MOUSE_X),
                                   I(int32_t, ISO_SCROLL_Y) + I(int32_t, MOUSE_Y), 0);
        }
        if (!I(int, DRAGGING)) {
            if (!taken && I(int, 0x134) && msg == WM_LBUTTONUP && in_map_rect())
                I(click_hook, 0xc303)(
                    I(uint32_t, 0xc30f), I(int32_t, MOUSE_X), I(int32_t, MOUSE_Y), I(uint32_t, 0xc307),
                    I(uint32_t, 0xc30b), 0);
        } else if (msg == WM_MOUSEMOVE) {
            taken = drag_move(lp);
        } else if (msg == WM_LBUTTONUP) {
            taken = drag_end(taken);
        }
        if (msg == WM_CHAR && I(int, 0x124))
            send_key(I(uint32_t, 0xa1a), wp);
    }
    if (!taken)
        return CallWindowProcA(I(WNDPROC, OLD_PROC), wnd, msg, wp, lp);
    return 0;
}
