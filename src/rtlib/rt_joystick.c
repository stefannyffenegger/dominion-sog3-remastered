/* Engine core, 0x10010037-0x10011430: two script built-ins that play sound, and the
 * joystick and key input of the two-player controller layer: joystick capture,
 * mapping joystick moves and buttons to per-player slots, and the eight keys per
 * player. Dominion only runs the joystick capture at startup.
 * Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_joystick.h"

#define RU(off) R(U, off)
#define RB(off) R(uint8_t, off)

#define RT_KEY_HOOK_ARG 0x0c94
#define RT_KEYS 0x0ce0         /* 256 x {player, armed, code} in 4 bytes */
#define RT_SLOT_ARMED 0x1893   /* per slot: may fire again */
#define RT_DIRS 0x1891         /* per player: direction keys held, one bit each */
#define RT_LAST_SLOT 0x18b5    /* per player */
#define RT_KEY_HOOK 0x18b7
#define RT_JOY_CENTER1 0x36b7  /* x, y as 32-bit values */
#define RT_JOY_CENTER2 0x36bf
#define RT_JOYSTICKS 0x3df3    /* MaxJoySticks */
#define RT_JOY_COUNT 0x3e2b
#define RT_SP 0x3e4f
#define RT_SLOT_HANDLERS 0x3e7f /* handler per slot, 17 per player */
#define RT_CONTROLLERS 0x95df
#define RT_JOY_CAPTURED 0x95e3
#define RT_JOY_SWAP 0x95e4
#define RT_JOY_4WAY 0x95e5
#define RT_PAUSED 0x95f1

#define g_stack ((int32_t *)ORIG_PTR(0x1007c408))
#define g_slot_codes ((const uint8_t *)ORIG_PTR(0x100751c0)) /* 17 per player */
#define g_keymap ((uint8_t *)ORIG_PTR(0x100751d8))           /* 8 x {key, code} per player */
#define g_dir_codes ((const uint8_t *)ORIG_PTR(0x100751f8))  /* by direction bits; 0x88 none */

#define dirs_changed(player) ctimer_remove((U)(player))
#define handler_fire(h) (U)script_run((U)(h))

#define SLOTS 0x11
#define NO_DIR 0x88

static int32_t pop(void)
{
    RU(RT_SP)--;
    return g_stack[RU(RT_SP)];
}
static uint8_t pop8(void) { return (uint8_t)pop(); }

static void put16(uint8_t *p, U v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, U v) { put16(p, v), put16(p + 2, v >> 16); }

/* 0x10010037: built-in: plays a MIDI resource through a command record */
U __cdecl builtin_play_midi(void)
{
    uint8_t c[16] = {0};
    put16(c, 0x8f);
    put16(c + 8, (U)pop());
    U a = pop() != 0;
    int32_t b = pop();
    c[10] = a || b;
    put32(c + 12, (U)b);
    put32(c + 2, (U)pop());
    cmd_play_midi(c);
    return 0;
}

/* 0x10010122: built-in: plays a wave resource, looping or not */
U __cdecl builtin_play_wave(void)
{
    uint8_t p[32] = {0};
    p[0x19] = pop() != 0;
    U id = (U)pop();
    return cmd_play_wave(id, (int32_t *)p);
}

/* 0x100101a3: built-in: plays a wave resource with all its settings */
U __cdecl builtin_play_wave_ex(void)
{
    uint8_t p[32] = {0};
    int32_t a = pop(), b = pop(), c = pop();
    put32(p + 0xc, (U)a);
    put32(p + 8, (U)b);
    if ((b || a) && !c)
        c = -1;
    put32(p + 0x10, (U)c);
    put32(p + 4, (U)pop());
    put32(p, (U)pop());
    uint8_t vol = pop8();
    p[0x1c] = vol > 0x10 ? 0x10 : vol;
    p[0x1a] = pop8();
    p[0x18] = pop() != 0;
    U flag = pop() != 0;
    int32_t d = pop();
    put32(p + 0x14, (U)d);
    U id = (U)pop();
    p[0x19] = flag || d;
    return cmd_play_wave(id, (int32_t *)p);
}

/* 0x100103c0: captures up to MaxJoySticks joysticks and records their centers; with
 * none found, MaxJoySticks becomes 0 */
void __cdecl joystick_capture(void)
{
    JOYINFO ji;
    RU(RT_JOY_COUNT) = 0;
    if (RU(RT_JOY_COUNT) >= RU(RT_JOYSTICKS))
        return;
    if (joySetCapture(R(HWND, RT_HWND), JOYSTICKID1, 10, TRUE) == JOYERR_NOERROR) {
        RB(RT_JOY_CAPTURED) = 1;
        joyGetPos(JOYSTICKID1, &ji);
        RU(RT_JOY_CENTER1) = ji.wXpos;
        RU(RT_JOY_CENTER1 + 4) = ji.wYpos;
        RU(RT_JOY_COUNT)++;
    }
    if (RU(RT_JOY_COUNT) >= RU(RT_JOYSTICKS))
        return;
    if (joySetCapture(R(HWND, RT_HWND), JOYSTICKID2, 10, TRUE) == JOYERR_NOERROR) {
        RB(RT_JOY_CAPTURED) = 1;
        joyGetPos(JOYSTICKID2, &ji);
        RU(RT_JOY_CENTER2) = ji.wXpos;
        RU(RT_JOY_CENTER2 + 4) = ji.wYpos;
        RU(RT_JOY_COUNT)++;
    }
    if (!RU(RT_JOY_COUNT))
        RU(RT_JOYSTICKS) = 0;
}

/* the first slot of joystick 1 or 2; the two can be swapped */
static U joy_base(int second) { return (RB(RT_JOY_SWAP) != 0) != (second != 0) ? SLOTS : 0; }

/* 0x10010666: the slot of a button change: 9-16 (pressed/released for buttons 1-4), or -1 */
int __stdcall joy_button_slot(U buttons)
{
    if (buttons & JOY_BUTTON1CHG)
        return buttons & JOY_BUTTON1 ? 10 : 9;
    if (buttons & JOY_BUTTON2CHG)
        return buttons & JOY_BUTTON2 ? 12 : 11;
    if (buttons & JOY_BUTTON3CHG)
        return buttons & JOY_BUTTON3 ? 14 : 13;
    if (buttons & JOY_BUTTON4CHG)
        return buttons & JOY_BUTTON4 ? 16 : 15;
    return -1;
}

/* 0x10010785: four-way: 0 centered, 1 right, 2 left, 3 up, 6 down */
int __stdcall joy_dir4(U pos, const uint16_t *center)
{
    int dx = (int)(pos & 0xffff) - center[0];
    int dy = (int)(pos >> 16) - center[2];
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if (ax <= 8000 && ay <= 8000)
        return 0;
    if (ay > ax)
        return dy > 0 ? 6 : 3;
    return dx < 0 ? 2 : 1;
}

/* 0x10010869: eight-way: the sum of right 1, left 2, up 3, down 6 */
int __stdcall joy_dir8(U pos, const uint16_t *center)
{
    int x = (int)(pos & 0xffff), y = (int)(pos >> 16);
    int up = y < center[2] - 16000 ? 3 : 0;
    int down = center[2] + 16000 < y ? 6 : 0;
    int right = center[0] + 16000 < x ? 1 : 0;
    int left = x < center[0] - 16000 ? 2 : 0;
    return left + down + right + up;
}

/* 0x10010730 */
int __stdcall joy_axis_slot(U pos, const uint16_t *center, int base)
{
    return (RB(RT_JOY_4WAY) ? joy_dir4(pos, center) : joy_dir8(pos, center)) + base;
}

/* 0x10010509: the slot of a joystick message, -1 for an unknown button change */
int __stdcall joy_slot(U msg, U buttons, U pos)
{
    int s = 0;
    switch (msg) {
    case MM_JOY1MOVE:
        s = joy_axis_slot(pos, (const uint16_t *)(g_rt + RT_JOY_CENTER1), (int)joy_base(0));
        break;
    case MM_JOY2MOVE:
        s = joy_axis_slot(pos, (const uint16_t *)(g_rt + RT_JOY_CENTER2), (int)joy_base(1));
        break;
    case MM_JOY1BUTTONDOWN:
    case MM_JOY1BUTTONUP:
        s = joy_button_slot(buttons);
        if (s != -1)
            s += (int)joy_base(0);
        break;
    case MM_JOY2BUTTONDOWN:
    case MM_JOY2BUTTONUP:
        s = joy_button_slot(buttons);
        if (s != -1)
            s += (int)joy_base(1);
        break;
    }
    return s;
}

/* 0x10010b12, 0x10010b57: the script handler of a button or an axis slot */
U __stdcall joy_button_handler(U buttons, int base)
{
    int s = joy_button_slot(buttons);
    return s == -1 ? 0 : RU(RT_SLOT_HANDLERS + (s + base) * 4);
}

U __stdcall joy_axis_handler(U pos, const uint16_t *center, int base)
{
    return RU(RT_SLOT_HANDLERS + joy_axis_slot(pos, center, base) * 4);
}

/* 0x10010cae: re-arms the player's previous slot and remembers this one */
void __stdcall joy_slot_remember(uint8_t slot, uint8_t player)
{
    RB(RT_SLOT_ARMED + RB(RT_LAST_SLOT + player)) = 1;
    RB(RT_LAST_SLOT + player) = slot;
}

/* 0x10010b87: joystick input in controller mode: an armed slot fires once and sends
 * the player's controller event. (Button down and up set the flag of the neighbouring
 * slot, as the original does.) */
void __stdcall joy_controller(U msg, U buttons, U pos)
{
    U s = (U)joy_slot(msg, buttons, pos);
    if (s == (U)-1 || !RB(RT_SLOT_ARMED + s))
        return;
    U second = s > 0x10;
    if (msg == MM_JOY1BUTTONDOWN || msg == MM_JOY2BUTTONDOWN)
        RB(RT_SLOT_ARMED - 1 + s) = 1;
    else if (msg == MM_JOY1BUTTONUP || msg == MM_JOY2BUTTONUP)
        RB(RT_SLOT_ARMED + 1 + s) = 1;
    else
        joy_slot_remember((uint8_t)s, (uint8_t)second);
    RB(RT_SLOT_ARMED + s) = 0;
    controller_event(second + 3, second ? g_slot_codes[s - SLOTS] : g_slot_codes[s]);
}

/* 0x10010950: a joystick message: the key hook first, then a script handler per slot
 * or, in controller mode, a controller event */
void __stdcall joy_message(U msg, U buttons, U pos)
{
    if (RU(RT_KEY_HOOK)) {
        U a[4] = {RU(RT_KEY_HOOK_ARG), msg, buttons, pos};
        if (orig_stdcall_unsafe(R(void *, RT_KEY_HOOK), 4, a))
            return;
    }
    if (RB(RT_CONTROLLERS)) {
        joy_controller(msg, buttons, pos);
        return;
    }
    U h = 0;
    switch (msg) {
    case MM_JOY1MOVE:
        h = joy_axis_handler(pos, (const uint16_t *)(g_rt + RT_JOY_CENTER1), (int)joy_base(0));
        break;
    case MM_JOY2MOVE:
        h = joy_axis_handler(pos, (const uint16_t *)(g_rt + RT_JOY_CENTER2), (int)joy_base(1));
        break;
    case MM_JOY1BUTTONDOWN:
    case MM_JOY1BUTTONUP:
        h = joy_button_handler(buttons, (int)joy_base(0));
        break;
    case MM_JOY2BUTTONDOWN:
    case MM_JOY2BUTTONUP:
        h = joy_button_handler(buttons, (int)joy_base(1));
        break;
    }
    if (h)
        handler_fire(h);
}

/* 0x10010d00 */
void __cdecl joystick_release(void)
{
    if (!RB(RT_JOY_CAPTURED))
        return;
    joyReleaseCapture(JOYSTICKID1);
    joyReleaseCapture(JOYSTICKID2);
    RB(RT_JOY_CAPTURED) = 0;
}

static int is_dir(uint8_t code) { return code == 0x10 || code == 0x20 || code == 0x40 || code == 0x80; }

/* 0x10010d50: a controller key went down; 0 means it was used. Direction keys combine
 * into one of eight directions. (Key 0x100 is let through, one past the table.) */
U __stdcall key_filter_down(U key)
{
    if (RB(RT_PAUSED) || key > 0x100)
        return 1;
    uint8_t *k = g_rt + RT_KEYS + key * 4;
    if (!k[1])
        return 1;
    k[1] = 0;
    uint8_t code = k[2], player = k[0];
    if (is_dir(code)) {
        uint8_t before = g_dir_codes[RB(RT_DIRS + player)];
        RB(RT_DIRS + player) |= (uint8_t)(code >> 4);
        dirs_changed(player);
        code = g_dir_codes[RB(RT_DIRS + player)];
        if (code == NO_DIR) {
            if (before == NO_DIR)
                return 0;
            code = 0;
        }
    }
    controller_event(player + 1, code);
    return 0;
}

/* 0x10011195: timer callback: the direction still held after a key went up */
void __stdcall key_dir_settle(uint8_t player)
{
    uint8_t code = g_dir_codes[RB(RT_DIRS + player)];
    if (code != NO_DIR)
        controller_event(player + 1, code);
}

/* 0x10010f54: a controller key went up; 0 means it was used. Releasing one of two
 * direction keys waits 200 ms before reporting the remaining direction. */
U __stdcall key_filter_up(U key)
{
    if (RB(RT_PAUSED) || key > 0x100)
        return 1;
    uint8_t *k = g_rt + RT_KEYS + key * 4;
    if (!k[2])
        return 1;
    k[1] = 1;
    uint8_t code = k[2], player = k[0];
    if (is_dir(code)) {
        uint8_t before = g_dir_codes[RB(RT_DIRS + player)];
        RB(RT_DIRS + player) &= (uint8_t)~(code >> 4);
        if (RB(RT_DIRS + player) && before != NO_DIR) {
            ctimer_set((U)(player), (U)(200), (U)((void *)key_dir_settle), (uint8_t)(0));
            return 0;
        }
        dirs_changed(player);
        code = g_dir_codes[RB(RT_DIRS + player)];
        if (code == NO_DIR)
            return 0;
    } else {
        code = (uint8_t)~code;
    }
    controller_event(player + 1, code);
    return 0;
}

/* 0x100111ed: binds or unbinds the player's eight keys; held keys count as pressed */
void __stdcall keys_bind(uint8_t player, uint8_t on)
{
    for (U i = 0; i < 0x100; i++) {
        uint8_t *k = g_rt + RT_KEYS + i * 4;
        if (k[0] == player)
            k[1] = 0, k[2] = 0;
    }
    if (!on)
        return;
    RB(RT_DIRS + player) = 0;
    for (U i = 0; i < 8; i++) {
        U vk = g_keymap[player * 16 + i * 2];
        uint8_t *k = g_rt + RT_KEYS + vk * 4;
        k[0] = player;
        k[1] = on;
        k[2] = g_keymap[player * 16 + i * 2 + 1];
        if (GetKeyState((int)vk) < 0)
            key_filter_down(vk);
    }
}

/* 0x1001131f: sets one of the player's eight keys (1-8, in the keymap's own order) */
void __stdcall keymap_set(uint8_t player, uint8_t which, uint8_t vk)
{
    static const uint8_t pos[9] = {0xff, 8, 10, 0, 12, 2, 6, 4, 14};
    if (which >= 1 && which <= 8)
        g_keymap[player * 16 + pos[which]] = vk;
}
