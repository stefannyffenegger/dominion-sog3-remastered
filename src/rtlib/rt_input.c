/* Engine core, 0x1000b3e0-0x1000cf7e: input. Mouse buttons pick the sprite or click
 * rectangle under the pointer and run its handler; sprites can be dragged; the pointer
 * entering and leaving sprites and hover regions raises events; keys run handlers from
 * a table with separate entries for Shift, Ctrl and both. Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_input.h"

#define HSL R(uint8_t *, RT_HSL)

/* state fields */
#define RT_HOVER_REGIONS 0x0000  /* 0x24 bytes each: rect, enter handler, arg, leave handler, arg, sprite id */
#define RT_CLICK_RECTS 0x0b40    /* array of 0x19 bytes: rect, handler, arg, disabled */
#define RT_COLLIDE_PAIRS 0x0b54  /* array of 0x18 bytes */
#define RT_AUDIO 0x0c90
#define RT_KEY_HOOK_ARG 0x0c94
#define RT_KEY_HOOK 0x18b7       /* proc(arg, message, key, 0): non-zero swallows the key */
#define RT_KEYS 0x194b           /* 0x19 bytes per key: handlers plain, shift, ctrl, both, up; disabled */
#define RT_DRAG_X 0x36af
#define RT_DRAG_Y 0x36b3
#define RT_VIEW_X 0x36c7
#define RT_VIEW_Y 0x36cb
#define RT_ZOOM 0x3c43
#define RT_ZOOM_X 0x3c53
#define RT_ZOOM_Y 0x3c57
#define RT_MODE 0x3dd3
#define RT_N_HOVER_REGIONS 0x3e1b
#define RT_N_OBJECTS 0x3e43
#define RT_HOVER_HANDLER 0x3e67  /* event when the pointer enters or leaves a sprite */
#define RT_HOVER_SPRITE 0x3e6b
#define RT_DRAG_SPRITE 0x3e73
#define RT_3E77 0x3e77
#define RT_KEY_HANDLER 0x3f07    /* script handlers that see every key, mouse event, pause */
#define RT_MOUSE_HANDLER 0x3f17
#define RT_PAUSE_HANDLER 0x3f2b
#define RT_HOVER_REGION 0x3f33   /* current hover region + 1 */
#define RT_RESULT 0x3f3b
#define RT_BUTTON 0x8d73
#define RT_MOUSE_X 0x8d83
#define RT_MOUSE_Y 0x8d87
#define RT_DOWN_X 0x8d8b
#define RT_DOWN_Y 0x8d8f
#define RT_UP_X 0x8d93
#define RT_UP_Y 0x8d97
#define RT_MOUSE_ON 0x95cd       /* bytes */
#define RT_KEY_FILTER 0x95e0
#define RT_MOUSE_FILTER 0x95e1
#define RT_LEFT_DOWN 0x95e6
#define RT_PAUSED 0x95f1
#define RT_RIGHT_DOWN 0x95f5
#define RT_95FB 0x95fb
#define RT_95FE 0x95fe
#define RT_ZOOMED 0x9601

#define event_send ORIG(int(__stdcall *)(uint32_t, uint32_t, uint32_t), 0x10025e97) /* handler, id, arg */
#define handler_record_run ORIG(void(__stdcall *)(uint8_t *), 0x100220dd)
#define mouse_filter ORIG(int(__stdcall *)(uint32_t, uint32_t), 0x10014dc0)
#define fn_1002799a ORIG(void(__stdcall *)(int, int), 0x1002799a)
#define fn_10027bbd ORIG(void(__stdcall *)(int), 0x10027bbd)
#define fn_10028012 ORIG(void(__stdcall *)(int, int), 0x10028012)

#define CLICKS ((RtArray *)(g_rt + RT_CLICK_RECTS))
#define PAIRS ((RtArray *)(g_rt + RT_COLLIDE_PAIRS))
#define KEY(k) (g_rt + RT_KEYS + (k) * 0x19)
#define OBJ(id) ((uint8_t *)(uintptr_t)*obj_entry(id)) /* an object's data */

/* 0x1000b921: runs a script handler with one argument; returns its result */
int __stdcall handler_run(uint32_t handler, uint32_t arg)
{
    uint8_t rec[0x22];
    memset(rec, 0, sizeof rec);
    P(uint32_t, rec, 2) = handler;
    P(uint32_t, rec, 10) = arg;
    rec[9] = 1;
    uint32_t saved = R(uint32_t, RT_RESULT);
    R(uint32_t, RT_RESULT) = 0;
    handler_record_run(rec);
    int r = R(int, RT_RESULT);
    R(uint32_t, RT_RESULT) = saved;
    return r;
}

/* 0x1000b8fa, 0x1000c21c: the handlers that see every mouse event (1 down, 2 up, 4 move)
 * and every key; 0 from them swallows the event */
int __stdcall mouse_handler(uint32_t what)
{
    return handler_run(R(uint32_t, RT_MOUSE_HANDLER), what);
}

int __stdcall key_handler(uint32_t key)
{
    return handler_run(R(uint32_t, RT_KEY_HANDLER), key);
}

/* ---- click rectangles and clickable objects ---- */

/* 0x1000b3e0: adds, changes, enables, disables or (handler 0) removes the rectangle at
 * +0xe of the command */
void __stdcall click_set(uint8_t *c)
{
    int32_t *rc = (int32_t *)(c + 0xe);
    uint8_t *e = CLICKS->data;
    uint32_t i = 0;
    for (; i < CLICKS->count && memcmp(e, rc, 16); i++)
        e += 0x19;
    if (i >= CLICKS->count && !(e = array_add(CLICKS)))
        return;
    if (c[0x20])
        e[0x18] = 1;
    else if (c[0x1f])
        e[0x18] = 0;
    else if (!P(uint32_t, c, 6))
        array_remove(CLICKS, i);
    else {
        memcpy(e, rc, 16);
        P(uint32_t, e, 0x10) = P(uint32_t, c, 6);
        P(uint32_t, e, 0x14) = P(uint32_t, c, 10);
        e[0x18] = 0;
    }
}

/* 0x1000bb67: one handler for all rectangles (0 removes them all) */
void __stdcall click_rects_handler(uint32_t handler)
{
    if (!handler) {
        array_free(CLICKS);
        return;
    }
    uint8_t *e = CLICKS->data;
    for (uint32_t i = 0; i < CLICKS->count; i++, e += 0x19) {
        P(uint32_t, e, 0x10) = handler;
        e[0x18] = 0;
    }
}

/* 0x1000bbe5: one handler for all clickable objects (resource type 0x14) */
void __stdcall click_objects_handler(uint32_t handler)
{
    for (uint32_t id = 0; id < R(uint32_t, RT_N_OBJECTS); id++)
        if (ResGetType(R(ResFile *, RT_RES), id) == 0x14 && OBJ(id))
            P(uint32_t, OBJ(id), 0) = handler;
}

/* 0x1000bd76 */
void __stdcall click_all_handler(uint32_t handler)
{
    click_rects_handler(handler);
    click_objects_handler(handler);
}

/* 0x1000be5c: disables (1) or enables everything clickable */
void __stdcall click_all_disable(uint32_t off)
{
    uint8_t *e = CLICKS->data;
    for (uint32_t i = 0; i < CLICKS->count; i++, e += 0x19)
        e[0x18] = (uint8_t)off;
    for (uint32_t id = 0; id < R(uint32_t, RT_N_OBJECTS); id++)
        if (ResGetType(R(ResFile *, RT_RES), id) == 0x14 && OBJ(id))
            OBJ(id)[0x20] = (uint8_t)off;
}

/* 0x1000bc62: the click command: +2 object (0: the rectangle at +0xe), +6 handler,
 * +10 argument, +0x1e all, +0x1f enable, +0x20 disable */
uint32_t __stdcall click_cmd(uint8_t *c)
{
    if (c[0x1e]) {
        if (c[0x1f])
            click_all_disable(0);
        else if (c[0x20])
            click_all_disable(1);
        else
            click_all_handler(P(uint32_t, c, 6));
        return 1;
    }
    if (!P(uint32_t, c, 2)) {
        click_set(c);
        return 1;
    }
    obj_ensure(P(uint32_t, c, 2));
    uint8_t *o = OBJ(P(uint32_t, c, 2));
    P(uint32_t, o, 4) = P(uint32_t, c, 10);
    o[0x20] = 0;
    if (c[0x20])
        o[0x20] = 1;
    else if (c[0x1f])
        o[0x20] = 0;
    else
        P(uint32_t, o, 0) = P(uint32_t, c, 6);
    return 1;
}

/* ---- dragging ---- */

/* 0x1000bae5: the dragged sprite is dropped at x, y */
void __stdcall drag_end(int x, int y)
{
    R(int, RT_UP_X) = x;
    R(int, RT_UP_Y) = y;
    uint8_t *o = OBJ(R(uint32_t, RT_DRAG_SPRITE));
    if (P(uint32_t, o, 0x14))
        event_send(P(uint32_t, o, 0x14), R(uint32_t, RT_DRAG_SPRITE), 0);
    R(uint32_t, RT_DRAG_SPRITE) = 0;
}

/* 0x1000bf1d: the drag command: +2 object, +6 handler when picked up, +10 when dropped,
 * +0xe stop dragging */
uint32_t __stdcall drag_cmd(uint8_t *c)
{
    obj_ensure(P(uint32_t, c, 2));
    uint8_t *o = OBJ(P(uint32_t, c, 2));
    P(uint32_t, o, 0x14) = P(uint32_t, c, 10);
    P(uint32_t, o, 0x10) = P(uint32_t, c, 6);
    o[0x22] = c[0xe] == 0;
    if (R(uint32_t, RT_DRAG_SPRITE) == P(uint32_t, c, 2) && c[0xe]) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(R(HWND, RT_HWND), &pt);
        drag_end(pt.x, pt.y);
    }
    return 1;
}

/* ---- mouse ---- */

/* 0x1000c402: window coordinates to the sprite engine's */
void __stdcall mouse_to_view(POINT *pt)
{
    if (!SpriteGetStretch(HSL)) {
        pt->x -= R(int, RT_VIEW_X);
        pt->y -= R(int, RT_VIEW_Y);
    } else
        DPtoLP(R(HDC, RT_DC), pt, 1);
    if (R(uint8_t, RT_ZOOMED)) {
        int half = R(int, RT_ZOOM) / 2;
        pt->x = R(int, RT_ZOOM_X) + (pt->x + half) / R(int, RT_ZOOM);
        pt->y = R(int, RT_ZOOM_Y) + (pt->y + half) / R(int, RT_ZOOM);
    }
}

/* 0x1000b55d: a mouse button went down (msg 0x201 left, 0x204 right) */
void __stdcall mouse_down(uint32_t msg, uint32_t keys, uint32_t lparam)
{
    if (R(uint8_t, RT_MOUSE_FILTER) && R(uint8_t, RT_MOUSE_ON) && mouse_filter(msg, lparam))
        return;
    SetCapture(R(HWND, RT_HWND));
    if (msg == WM_LBUTTONDOWN)
        R(uint8_t, RT_LEFT_DOWN) = 1;
    else if (msg == WM_RBUTTONDOWN)
        R(uint8_t, RT_RIGHT_DOWN) = 1;
    if (R(uint8_t, RT_PAUSED))
        return;
    POINT pt = {(int16_t)lparam, (int16_t)(lparam >> 16)};
    if (msg == WM_LBUTTONDOWN && R(uint8_t, RT_95FE) && ((uint8_t)keys & 0xc) == 0xc)
        fn_1002799a(pt.x, pt.y);
    if (R(uint8_t, RT_95FB) || R(uint32_t, RT_MODE) == 3 || R(uint32_t, RT_MODE) == 4 || R(uint32_t, RT_DRAG_SPRITE))
        return;
    mouse_to_view(&pt);
    R(uint32_t, RT_BUTTON) = msg == WM_LBUTTONDOWN ? 1 : 2;
    R(int, RT_DOWN_X) = pt.x;
    R(int, RT_DOWN_Y) = pt.y;
    if (R(uint32_t, RT_MOUSE_HANDLER) && !mouse_handler(1))
        return;
    uint8_t *s = SpritePicked(HSL, pt.x, pt.y);
    uint32_t handler = 0, id = 0, arg = 0;
    if (s) {
        id = SpriteGetId(s);
        uint8_t *o = OBJ(id);
        if (o[0x22]) { /* draggable: pick it up */
            R(uint32_t, RT_DRAG_SPRITE) = id;
            R(int, RT_DRAG_X) = pt.x;
            R(int, RT_DRAG_Y) = pt.y;
            handler = P(uint32_t, o, 0x10);
        } else if (!o[0x20]) {
            handler = P(uint32_t, o, 0);
            arg = P(uint32_t, o, 4);
        }
    }
    if (!handler) {
        id = 0;
        uint8_t *e = CLICKS->data + CLICKS->count * 0x19;
        for (uint32_t n = 0; n < CLICKS->count && !handler; n++) { /* the newest rectangle first */
            e -= 0x19;
            if (!e[0x18] && P(int, e, 0) <= pt.x && pt.x < P(int, e, 8) && P(int, e, 4) <= pt.y && pt.y < P(int, e, 12)) {
                handler = P(uint32_t, e, 0x10);
                arg = P(uint32_t, e, 0x14);
            }
        }
    }
    if (handler)
        event_send(handler, id, arg);
}

/* 0x1000b99c: a mouse button went up (0x202 left, 0x205 right) */
void __stdcall mouse_up(uint32_t msg, uint32_t lparam)
{
    if (R(uint8_t, RT_MOUSE_FILTER) && mouse_filter(msg, lparam))
        return;
    ReleaseCapture();
    if (msg == WM_LBUTTONUP)
        R(uint8_t, RT_LEFT_DOWN) = 0;
    else if (msg == WM_RBUTTONUP)
        R(uint8_t, RT_RIGHT_DOWN) = 0;
    if (R(uint8_t, RT_PAUSED))
        return;
    if (R(uint8_t, RT_95FB)) {
        if (msg == WM_LBUTTONUP && R(uint8_t, RT_95FE))
            fn_10027bbd(0);
        return;
    }
    if ((!R(uint32_t, RT_MOUSE_HANDLER) || mouse_handler(2)) && R(uint32_t, RT_DRAG_SPRITE)) {
        POINT pt = {(int16_t)lparam, (int16_t)(lparam >> 16)};
        mouse_to_view(&pt);
        drag_end(pt.x, pt.y);
    }
}

/* 0x1000c6ff: the pointer left the hovered sprite */
uint32_t __cdecl hover_sprite_leave(void)
{
    event_send(R(uint32_t, RT_HOVER_HANDLER), R(uint32_t, RT_HOVER_SPRITE), 0);
    R(uint32_t, RT_HOVER_SPRITE) = 0;
    return 0;
}

/* 0x1000c96c: events for the pointer entering and leaving sprites */
uint32_t __stdcall hover_sprite(int x, int y)
{
    uint8_t *s = SpritePicked(HSL, x, y);
    if (!s)
        return R(uint32_t, RT_HOVER_SPRITE) && hover_sprite_leave();
    uint32_t id = SpriteGetId(s);
    if (R(uint32_t, RT_HOVER_SPRITE) == id)
        return 1;
    if (R(uint32_t, RT_HOVER_SPRITE))
        hover_sprite_leave();
    R(uint32_t, RT_HOVER_SPRITE) = id;
    event_send(R(uint32_t, RT_HOVER_HANDLER), id, 1);
    return 1;
}

/* 0x1000c73f: hover regions (rectangles or sprites), the newest first: leave the old
 * one, enter the new one; a handler returning 0 keeps the old state */
void __stdcall hover_regions(int x, int y)
{
    int i = R(int, RT_N_HOVER_REGIONS);
    while (--i >= 0) {
        uint8_t *g = g_rt + RT_HOVER_REGIONS + i * 0x24;
        if (P(uint32_t, g, 0x20)) {
            uint32_t sprite = obj_entry(P(uint32_t, g, 0x20))[1];
            if (sprite && SpriteIsPicked(HSL, (uint8_t *)(uintptr_t)sprite, x, y))
                break;
        } else if (P(int, g, 0) <= x && x < P(int, g, 8) && P(int, g, 4) <= y && y < P(int, g, 12))
            break;
    }
    int cur = R(int, RT_HOVER_REGION) - 1;
    if (cur != i) {
        uint8_t *g = g_rt + RT_HOVER_REGIONS + cur * 0x24;
        if (cur >= 0 && P(uint32_t, g, 0x18) && !event_send(P(uint32_t, g, 0x18), 0, P(uint32_t, g, 0x1c)))
            return;
        R(int, RT_HOVER_REGION) = 0;
    }
    if (i >= 0 && cur != i) {
        uint8_t *g = g_rt + RT_HOVER_REGIONS + i * 0x24;
        if (!P(uint32_t, g, 0x10) || event_send(P(uint32_t, g, 0x10), 1, P(uint32_t, g, 0x14)))
            R(int, RT_HOVER_REGION) = i + 1;
    }
}

/* 0x1000c4f3: the pointer moved */
void __stdcall mouse_move(uint32_t lparam)
{
    if (!R(uint8_t, RT_MOUSE_ON) || (R(uint8_t, RT_MOUSE_FILTER) && mouse_filter(WM_MOUSEMOVE, lparam)))
        return;
    if (R(uint8_t, RT_95FB)) {
        if (R(uint8_t, RT_95FE))
            fn_10028012((int16_t)lparam, (int16_t)(lparam >> 16));
        return;
    }
    POINT pt = {(int16_t)lparam, (int16_t)(lparam >> 16)};
    mouse_to_view(&pt);
    R(int, RT_MOUSE_X) = pt.x;
    R(int, RT_MOUSE_Y) = pt.y;
    if (R(uint8_t, RT_PAUSED))
        return;
    if (R(uint32_t, RT_MOUSE_HANDLER) && !mouse_handler(4))
        return;
    if (R(uint32_t, RT_MODE) == 4) {
        if (R(uint32_t, RT_HOVER_SPRITE))
            hover_sprite_leave();
    } else if (!R(uint32_t, RT_DRAG_SPRITE)) {
        if (R(uint32_t, RT_HOVER_HANDLER))
            hover_sprite(pt.x, pt.y);
        if (R(uint32_t, RT_N_HOVER_REGIONS))
            hover_regions(pt.x, pt.y);
    } else { /* drag the sprite along */
        int dy = pt.y - R(int, RT_DRAG_Y), dx = pt.x - R(int, RT_DRAG_X);
        SpriteOffset(HSL, (uint8_t *)(uintptr_t)obj_entry(R(uint32_t, RT_DRAG_SPRITE))[1], dx, dy);
        R(int, RT_DRAG_X) = pt.x;
        R(int, RT_DRAG_Y) = pt.y;
    }
}

/* ---- keys ---- */

static int key_hook(uint32_t msg, uint32_t key)
{
    if (!R(uint32_t, RT_KEY_HOOK))
        return 0;
    uint32_t a[4] = {R(uint32_t, RT_KEY_HOOK_ARG), msg, key, 0};
    return (int)orig_stdcall_unsafe(R(void *, RT_KEY_HOOK), 4, a);
}

/* 0x1000c243: Pause key: the pause handler decides, or the sound is paused; then the
 * pause state flips */
void __cdecl pause_toggle(void)
{
    if (!R(uint32_t, RT_PAUSE_HANDLER))
        AudioPause(R(uint8_t *, RT_AUDIO), R(uint8_t, RT_PAUSED) == 0);
    else {
        if (handler_run(R(uint32_t, RT_PAUSE_HANDLER), R(uint8_t, RT_PAUSED) == 0))
            return;
        SpriteAnimate(HSL);
    }
    R(uint8_t, RT_PAUSED) = R(uint8_t, RT_PAUSED) == 0;
    plugins_pause(R(uint8_t, RT_PAUSED));
    controllers_focus((int8_t)R(uint8_t, RT_PAUSED));
}

/* 0x1000bfe9: a key went down */
void __stdcall key_down(uint32_t key)
{
    if (key_hook(WM_KEYDOWN, key) || (R(uint8_t, RT_KEY_FILTER) && !key_filter_down(key)))
        return;
    if (key == VK_PAUSE)
        pause_toggle();
    if (R(uint8_t, RT_PAUSED))
        return;
    if (R(uint8_t, RT_95FB) && key == VK_ESCAPE) {
        if (R(uint8_t, RT_95FE))
            fn_10027bbd(1);
        return;
    }
    if ((R(uint32_t, RT_KEY_HANDLER) && !key_handler(key)) || R(uint32_t, RT_3E77) || key >= 0x100 || KEY(key)[0x14])
        return;
    int shift = GetKeyState(VK_SHIFT) < 0, ctrl = GetKeyState(VK_CONTROL) < 0;
    uint32_t handler = P(uint32_t, KEY(key), shift && ctrl ? 0xc : shift ? 4 : ctrl ? 8 : 0);
    if (handler)
        handler_run(handler, key);
}

/* 0x1000c32a: a key went up */
void __stdcall key_up(uint32_t key)
{
    if (key_hook(WM_KEYUP, key) || (R(uint8_t, RT_KEY_FILTER) && !key_filter_up(key)))
        return;
    if (R(uint8_t, RT_PAUSED) || key >= 0x100 || !P(uint32_t, KEY(key), 0x10))
        return;
    handler_run(P(uint32_t, KEY(key), 0x10), key);
}

/* 0x1000ca4b: the key command: +2 key (0: all keys), +6 handler, +0xc with Ctrl,
 * +0xd with Shift, +0x10 on release, +0xe enable, +0xf disable */
void __stdcall key_cmd(uint8_t *c)
{
    uint32_t key = P(uint32_t, c, 2);
    if (key >= 0x100)
        return;
    uint32_t first = key ? key : 0, last = key ? key : 0xff;
    for (uint32_t k = first; k <= last; k++) {
        uint8_t *e = KEY(k);
        if (c[0xe])
            e[0x14] = 0;
        else if (c[0xf])
            e[0x14] = 1;
        else {
            int slot = c[0xc] && c[0xd] ? 0xc : c[0xc] ? 8 : c[0xd] ? 4 : c[0x10] ? 0x10 : 0;
            P(uint32_t, e, slot) = P(uint32_t, c, 6);
            e[0x14] = 0;
        }
    }
}

/* ---- collisions ---- */

/* 0x1000cd69: the collision command: watch sprites +2 and +6 (stored lower id first)
 * and run handler +10 when they touch; handler 0 removes the pair, +0x16 removes all */
uint32_t __stdcall collide_cmd(uint8_t *c)
{
    if (c[0x16]) {
        array_free(PAIRS);
        return 1;
    }
    if (!obj_12000(P(uint32_t, c, 2), 0x14) || !obj_12000(P(uint32_t, c, 6), 0x14))
        return 0;
    uint32_t a = P(uint32_t, c, 2), b = P(uint32_t, c, 6);
    uint8_t fa = c[0x17], fb = c[0x18];
    if (b < a) {
        uint32_t t = a;
        a = b, b = t;
        uint8_t f = fa;
        fa = fb, fb = f;
    }
    uint32_t *e = (uint32_t *)PAIRS->data;
    uint32_t i = 0;
    for (; i < PAIRS->count && !(e[0] == a && e[1] == b); i++)
        e += 6;
    if (i >= PAIRS->count && !(e = array_add(PAIRS)))
        return 0;
    if (!P(uint32_t, c, 10)) {
        array_remove(PAIRS, i);
        return 1;
    }
    e[0] = a;
    e[1] = b;
    e[2] = P(uint32_t, c, 10);
    e[3] = P(uint32_t, c, 0xe);
    e[4] = P(uint32_t, c, 0x12);
    P(uint8_t, e, 0x14) = c[0x1a];
    P(uint8_t, e, 0x16) = fa;
    P(uint8_t, e, 0x17) = fb;
    P(uint8_t, e, 0x15) = c[0x19];
    return 1;
}

/* ---- 0x1000cf7e-0x1000d7c0: collisions, hover commands, queued events ---- */

#define RT_EVENT_QUEUE 0x36df /* queued handler records, 0x22 bytes each */
#define RT_N_EVENTS 0x3e47

/* 0x1000cf7e: runs the handler of every watched pair of sprites that touch. One-shot
 * pairs are removed first; a handler returning 0 ends the pass. */
void __cdecl collide_check(void)
{
    uint32_t *e = (uint32_t *)PAIRS->data;
    for (uint32_t i = 0; (int32_t)i < (int32_t)PAIRS->count; i++, e += 6) {
        uint32_t a = obj_entry(e[0])[1];
        if (!a)
            continue;
        uint32_t b = obj_entry(e[1])[1];
        if (!b || !SpriteHitDetect((uint8_t *)(uintptr_t)a, (uint8_t *)(uintptr_t)b, P(uint8_t, e, 0x16),
                                   P(uint8_t, e, 0x17), P(uint8_t, e, 0x15)))
            continue;
        uint32_t handler = e[2], id = e[3], arg = e[4];
        if (!P(uint8_t, e, 0x14))
            i = array_remove(PAIRS, i);
        if (!event_send(handler, id, arg))
            return;
    }
}

/* Index of the hover region with this rectangle or sprite; the count when there is none */
static uint32_t hover_region_find(const uint8_t *c)
{
    uint32_t sprite = P(uint32_t, c, 0x22), i = 0;
    for (; i < R(uint32_t, RT_N_HOVER_REGIONS); i++) {
        uint8_t *g = g_rt + RT_HOVER_REGIONS + i * 0x24;
        if (sprite ? P(uint32_t, g, 0x20) == sprite : !memcmp(g, c + 2, 16))
            break;
    }
    return i;
}

static void hover_at_pointer(int sprites, int regions)
{
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(R(HWND, RT_HWND), &pt);
    mouse_to_view(&pt);
    if (sprites)
        hover_sprite(pt.x, pt.y);
    if (regions)
        hover_regions(pt.x, pt.y);
}

/* 0x1000d112: adds or changes a hover region (at most 80) and checks the pointer */
uint32_t __stdcall hover_region_add(uint8_t *c)
{
    uint32_t i = hover_region_find(c);
    if (R(uint32_t, RT_N_HOVER_REGIONS) <= i) {
        if (R(uint32_t, RT_N_HOVER_REGIONS) > 0x4f)
            return 0;
        i = R(uint32_t, RT_N_HOVER_REGIONS)++;
    }
    uint8_t *g = g_rt + RT_HOVER_REGIONS + i * 0x24;
    memcpy(g, c + 2, 16);
    memcpy(g + 0x10, c + 0x12, 16); /* enter handler, arg, leave handler, arg */
    P(uint32_t, g, 0x20) = P(uint32_t, c, 0x22);
    hover_at_pointer(0, 1);
    return 1;
}

/* 0x1000d33e: removes a hover region. (It moves one region too many down, reading past
 * the last one.) */
uint32_t __stdcall hover_region_remove(uint8_t *c)
{
    uint32_t i = hover_region_find(c);
    if (i >= R(uint32_t, RT_N_HOVER_REGIONS))
        return 1;
    uint8_t *g = g_rt + RT_HOVER_REGIONS + i * 0x24;
    memmove(g, g + 0x24, (R(uint32_t, RT_N_HOVER_REGIONS) - i) * 0x24);
    R(uint32_t, RT_N_HOVER_REGIONS)--;
    if (R(uint32_t, RT_HOVER_REGION)) {
        uint32_t cur = R(uint32_t, RT_HOVER_REGION) - 1;
        if (cur == i)
            R(uint32_t, RT_HOVER_REGION) = 0;
        else if (i < cur)
            R(uint32_t, RT_HOVER_REGION)--;
    }
    return 1;
}

/* 0x1000d097: the hover region command: +2 rectangle or +0x22 sprite, +0x12 enter
 * handler (0 removes the region), +0x26 remove all */
uint32_t __stdcall hover_region_cmd(uint8_t *c)
{
    if (c[0x26]) {
        R(uint32_t, RT_HOVER_REGION) = 0;
        R(uint32_t, RT_N_HOVER_REGIONS) = 0;
        return 1;
    }
    return P(uint32_t, c, 0x12) ? hover_region_add(c) : hover_region_remove(c);
}

/* 0x1000d511: runs the handlers queued so far (ones queued meanwhile stay for next time) */
void __cdecl events_flush(void)
{
    uint32_t n = R(uint32_t, RT_N_EVENTS);
    for (uint32_t i = 0; i < n; i++)
        handler_record_run(g_rt + RT_EVENT_QUEUE + i * 0x22);
    R(uint32_t, RT_N_EVENTS) -= n;
    if (R(uint32_t, RT_N_EVENTS))
        memmove(g_rt + RT_EVENT_QUEUE, g_rt + RT_EVENT_QUEUE + n * 0x22, R(uint32_t, RT_N_EVENTS) * 0x22);
}

/* 0x1000d5d4: sets the handler for the pointer entering and leaving sprites (a
 * resource of type 0x13, or 0 for none) */
void __stdcall hover_handler_set(uint32_t handler)
{
    if (handler && ResGetType(R(ResFile *, RT_RES), handler) != 0x13)
        return;
    if (R(uint32_t, RT_HOVER_HANDLER) && R(uint32_t, RT_HOVER_SPRITE))
        hover_sprite_leave();
    R(uint32_t, RT_HOVER_HANDLER) = handler;
    if (handler)
        hover_at_pointer(1, 0);
}

/* 0x1000d693: checks what is under the pointer now */
void __cdecl hover_update(void)
{
    if (R(uint32_t, RT_MODE) == 4) {
        if (R(uint32_t, RT_HOVER_SPRITE))
            hover_sprite_leave();
        return;
    }
    hover_at_pointer(R(uint32_t, RT_HOVER_HANDLER) != 0, R(uint32_t, RT_N_HOVER_REGIONS) != 0);
}

/* 0x1000d73b: starts dragging sprite id from where the pointer is */
int __stdcall drag_start(uint32_t id)
{
    if (!obj_14409(id, 0x14))
        return 0;
    R(uint32_t, RT_DRAG_SPRITE) = id;
    R(int, RT_DRAG_X) = R(int, RT_MOUSE_X);
    R(int, RT_DRAG_Y) = R(int, RT_MOUSE_Y);
    return 1;
}
