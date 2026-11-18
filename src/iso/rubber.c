/* The game logic, 0x100600b8-0x10060700: the script's drag box ("rubber band") exports, the
 * sprite paint hooks that keep the box on screen, the script's keyboard hooks, and the map
 * editor's path mouse handling. */
#include "iso_core.h"
#include "relay.h"
#include "wndproc.h"
#include "rubber.h"

#define DRAG_HANDLER 0xd7f7
#define HANDLE_MOUSE 0xd7a3
#define DRAGGING 0xd7ab
#define DRAG_BOX 0xd7e7
#define MAP_RECT 0xd787
#define OLD_PAINT_HOOK 0xd7d3  /* the paint hook in place before ours, and its data */
#define OLD_PAINT_DATA 0xd7bf
#define ISO_SCROLL_X 0xcd82
#define ISO_SCROLL_Y 0xcd86

#define debug_print ORIG(void(__cdecl *)(int, const char *, ...), 0x10095e98)
#define IsoHideBeaconDirections ORIG(void(__cdecl *)(uint8_t *), 0x1007c111)
#define editpath_redraw ORIG(void(__cdecl *)(void), 0x1009b4fc)
#define editpath_pick ORIG(int(__cdecl *)(POINT *, uint32_t), 0x1009b0fd)
#define editpath_add ORIG(void(__cdecl *)(int, int), 0x1009b2c0)
#define editpath_drag ORIG(void(__cdecl *)(int, int), 0x1009b30c)
#define editpath_drop ORIG(void(__cdecl *)(int, int), 0x1009b3cb)
#define editpath_picked ORIG(int(__cdecl *)(void), 0x1009b8b4)

typedef void(__stdcall *paint_hook_fn)(uint32_t, HDC, RECT *, int);

/* RubberBand: sets the script's handler for a finished drag; 0 cancels the drag going on */
void __stdcall RubberBand(uint8_t *iso, uint32_t handler)
{
    g_iso = iso;
    if (!handler) {
        P(uint32_t, iso, DRAG_HANDLER) = 0;
        drag_end(1);
    } else {
        P(uint32_t, iso, DRAG_HANDLER) = handler;
    }
}

/* RubberSetActiveBox: the part of the screen a drag is kept to */
void __stdcall RubberSetActiveBox(uint8_t *iso, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    g_iso = iso;
    P(int32_t, iso, MAP_RECT) = left;
    I(int32_t, MAP_RECT + 4) = top;
    I(int32_t, MAP_RECT + 8) = right;
    I(int32_t, MAP_RECT + 12) = bottom;
}

/* 0x10060166: an engine hook (stdcall, 2 arguments): when switched off, the drag ends */
void __stdcall rubber_hook_off(uint8_t *iso, int on)
{
    g_iso = iso;
    if (!on)
        drag_end(1);
}

/* 0x10060191: the sprite paint hook: redraws the drag box where the engine painted over it
 * (not in pass 1), then calls the hook that was there before */
void __stdcall rubber_paint_hook(uint8_t *iso, HDC dc, RECT *area, int pass)
{
    g_iso = iso;
    if (P(int, iso, DRAGGING) && pass != 1) {
        RECT r = { P(LONG, iso, DRAG_BOX), P(LONG, iso, DRAG_BOX + 4), P(LONG, iso, DRAG_BOX + 8),
                   P(LONG, iso, DRAG_BOX + 12) };
        if (IntersectRect(&r, area, &r))
            drag_box_draw(dc);
    }
    if (I(paint_hook_fn, OLD_PAINT_HOOK))
        I(paint_hook_fn, OLD_PAINT_HOOK)(I(uint32_t, OLD_PAINT_DATA), dc, area, pass);
}

/* 0x1006025a: the map editor's paint hook: redraws the edited path after pass 1 */
void __stdcall editpath_paint_hook(uint8_t *iso, uint32_t a, uint32_t b, int pass)
{
    (void)a;
    (void)b;
    g_iso = iso;
    if (pass == 1)
        editpath_redraw();
}

/* IsoRubberBand: sets the drag handler and whether the map takes mouse input */
void __stdcall IsoRubberBand(uint8_t *iso, uint32_t handler, int on)
{
    g_iso = iso;
    P(uint32_t, iso, DRAG_HANDLER) = handler ? handler : 0;
    if (!on) {
        I(int, HANDLE_MOUSE) = 0;
        drag_end(1);
    } else {
        I(int, HANDLE_MOUSE) = 1;
        IsoHideBeaconDirections(iso);
        if (G(int16_t, 0x10118d84))
            ring_hide(G(int16_t, 0x10118d84));
    }
}

/* IsoEditTextHook: the script's handler for typed characters (0: none) */
int __stdcall IsoEditTextHook(uint8_t *iso, uint32_t handler)
{
    g_iso = iso;
    P(uint32_t, iso, 0xa1a) = handler ? handler : 0;
    I(int, 0x124) = I(uint32_t, 0xa1a) != 0;
    return I(uint32_t, 0xa1a) != 0;
}

/* IsoKeyboardHook: the script's handler for keys */
int __stdcall IsoKeyboardHook(uint8_t *iso, uint32_t handler)
{
    g_iso = iso;
    P(uint32_t, iso, 0xa1e) = handler ? handler : 0;
    return I(uint32_t, 0xa1e) != 0;
}

/* IsoExitGameHook: the script's handler for Alt+F4 */
int __stdcall IsoExitGameHook(uint8_t *iso, uint32_t handler)
{
    g_iso = iso;
    P(uint32_t, iso, 0xe094) = handler ? handler : 0;
    return I(uint32_t, 0xe094) != 0;
}

uint32_t __stdcall IsoCtrlPressed(uint8_t *iso)
{
    g_iso = iso;
    return P(uint32_t, iso, 0xd797);
}

uint32_t __stdcall IsoShiftPressed(uint8_t *iso)
{
    g_iso = iso;
    return P(uint32_t, iso, 0xd7af);
}

/* IsoSetIgnoreInputFlag: while set, the window handler drops typed characters */
void __stdcall IsoSetIgnoreInputFlag(uint8_t *iso, uint32_t flag)
{
    g_iso = iso;
    debug_print(0x1a, (const char *)ORIG_PTR(0x1013f838), flag);
    I(uint32_t, 0x14c) = flag;
}

static POINT map_point(LPARAM lp)
{
    POINT pt = { (int16_t)lp, (int16_t)((uint32_t)lp >> 16) };
    RtMapPoint(I(void *, ISO_APP), &pt);
    pt.x += I(int32_t, ISO_SCROLL_X);
    pt.y += I(int32_t, ISO_SCROLL_Y);
    return pt;
}

/* 0x10060521: the map editor's path editing: a click picks a point of the path (or adds
 * one), dragging moves it, letting go drops it */
void __stdcall editpath_message(UINT msg, WPARAM wp, LPARAM lp)
{
    (void)wp;
    if (msg == WM_LBUTTONDOWN) {
        POINT pt = map_point(lp);
        if (editpath_pick(&pt, I(uint32_t, 0xd7af)) < 0)
            editpath_add(pt.x, pt.y);
    } else if (msg == WM_MOUSEMOVE && editpath_picked() == 1) {
        POINT pt = map_point(lp);
        editpath_drag(pt.x, pt.y);
    } else if (msg == WM_LBUTTONUP && editpath_picked() == 1) {
        POINT pt = map_point(lp);
        editpath_drop(pt.x, pt.y);
    }
}
