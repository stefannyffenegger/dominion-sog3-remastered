/* Engine core, 0x1000dfb0-0x1000f130: the controller layer. Two players, each bound
 * to a device slot (1-2 joysticks, 3-4 keyboard sets, 5 the mouse). Button presses
 * and releases fire the player's handler; the codes pressed in a short time make a
 * combination, matched (code under a mask, best priority first) against the player's
 * list to fire that entry's handler. Dominion does not use it. Rewritten from the
 * original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_controller.h"

#define RT_PLAYERS 0x10e0
#define PLAYER_SIZE 0x3c5
#define RT_SLOT_PLAYER 0x188b /* the player each device slot drives, by slot */
#define RT_KEYSETS 0x1893     /* 17 bytes per keyboard set */
#define RT_HANDLER_RUNNING 0x3e77
#define RT_COMBO_TIME 0x3deb
#define RT_MOUSE_USED 0x95e1
#define RT_ANY_JOYSTICK 0x95e0
#define RT_ANY_KEYBOARD 0x95df
#define MAX_COMBOS 50

/* a player, at rt + 0x10e0 + n * 0x3c5 */
#define PL_BUFFER 0x0  /* recent codes, 4 bits apart */
#define PL_LAST 0x4    /* the last direction code */
#define PL_HELD 0x8    /* buttons held */
#define PL_HANDLER 0xc
#define PL_COUNT 0x10
#define PL_COUNT2 0x14
#define PL_COMBOS 0x18  /* 50 entries */
#define PL_COMBOS2 0x36a
#define PL_RECORD 0x3ae /* the matched entry, kept instead of fired */
#define PL_TYPE 0x3bf   /* 0 none, 1 keyboard, 2 joystick, 5 mouse */
#define PL_REMAP 0x3c0
#define PL_RECORDING 0x3c1
#define PL_SLOT 0x3c3
#define PL_ACTIVE 0x3c4

/* a combination entry, 17 bytes */
#define CO_ARG 0
#define CO_HANDLER 4
#define CO_CODE 8
#define CO_MASK 0xc
#define CO_ENABLED 0x10
#define CO_SIZE 0x11

#define g_remap ((const uint8_t *)ORIG_PTR(0x100751b0))

static uint8_t *player(U n) { return g_rt + RT_PLAYERS + n * PLAYER_SIZE; }
static uint8_t *slot_player(U slot) { return g_rt + RT_SLOT_PLAYER + slot; }
static uint8_t *combo(uint8_t *list, U i) { return list + i * CO_SIZE; }

static int combo_matches(const uint8_t *e, U buf)
{
    return (P(U, e, CO_MASK) & buf) == P(U, e, CO_CODE) && e[CO_ENABLED] && P(U, e, CO_HANDLER);
}

/* 0x1000e0ed: the second list's turn. (Recording copies the entry of the first list
 * at the same place, as in the original.) */
void __stdcall combo_check2(uint8_t n)
{
    uint8_t *b = player(n);
    U buf = P(U, b, PL_BUFFER);
    P(U, b, PL_BUFFER) = 0;
    for (U i = 0; i < P(U, b, PL_COUNT2); i++) {
        uint8_t *e = combo(b + PL_COMBOS2, i);
        if (!combo_matches(e, buf))
            continue;
        if (b[PL_RECORDING])
            memcpy(b + PL_RECORD, combo(b + PL_COMBOS, i), CO_SIZE);
        else
            event_send(P(U, e, CO_HANDLER), n, P(U, e, CO_ARG));
        return;
    }
}

/* 0x1000dfb0: the combination timer ran out (key: player + 2): fires the first entry
 * matching the codes pressed */
void __stdcall combo_check(U key)
{
    uint8_t n = (uint8_t)(key - 2);
    uint8_t *b = player(n);
    if (P(U, b, PL_COUNT2)) {
        combo_check2(n);
        return;
    }
    U buf = P(U, b, PL_BUFFER);
    P(U, b, PL_BUFFER) = 0;
    for (U i = 0; i < P(U, b, PL_COUNT); i++) {
        uint8_t *e = combo(b + PL_COMBOS, i);
        if (!combo_matches(e, buf))
            continue;
        if (b[PL_RECORDING])
            memcpy(b + PL_RECORD, e, CO_SIZE);
        else
            event_send(P(U, e, CO_HANDLER), n, P(U, e, CO_ARG));
        return;
    }
}

static void fire(uint8_t *b, uint8_t n, U code)
{
    if (!R(U, RT_HANDLER_RUNNING) && P(U, b, PL_HANDLER))
        event_send(P(U, b, PL_HANDLER), n, code);
}

/* 0x1000e208: a code from device slot `slot`: 0 centred, 1/2/4/8 buttons pressed,
 * their complements released, anything else a direction (which also starts a
 * combination) */
U __stdcall controller_event(U slot, U code_arg)
{
    uint8_t n = *slot_player(slot & 0xff);
    uint8_t *b = player(n);
    uint8_t code = (uint8_t)code_arg;
    switch (code) {
    case 0:
        P(U, b, PL_LAST) = 0;
        fire(b, n, code);
        return 1;
    case 1:
    case 2:
    case 4:
    case 8:
        P(U, b, PL_HELD) |= code;
        break;
    case 0xf7:
    case 0xfb:
    case 0xfd:
    case 0xfe:
        P(U, b, PL_HELD) &= code;
        fire(b, n, code);
        return 1;
    default:
        P(U, b, PL_LAST) = code;
        if (b[PL_REMAP] == 1)
            code = g_remap[code];
        break;
    }
    if (R(U, RT_HANDLER_RUNNING))
        return 1;
    fire(b, n, code);
    if (!P(U, b, PL_BUFFER))
        ctimer_set(n + 2u, R(U, RT_COMBO_TIME), (U)(uintptr_t)combo_check, 0);
    P(U, b, PL_BUFFER) &= 0xff;
    P(U, b, PL_BUFFER) = P(U, b, PL_BUFFER) * 0x10 + code;
    return 1;
}

/* 0x1000e610: gives every entry the handler */
void __stdcall combos_set_handler(uint8_t *list, U count, U handler)
{
    for (U i = 0; i < count; i++)
        P(U, combo(list, i), CO_HANDLER) = handler;
}

/* 0x1000e659: enables or disables every entry */
void __stdcall combos_enable(uint8_t *list, U count, U on)
{
    for (U i = 0; i < count; i++)
        combo(list, i)[CO_ENABLED] = (uint8_t)on;
}

/* 0x1000e8e7: removes entry i; the new count */
U __stdcall combo_remove(U i, U count, uint8_t *list)
{
    if (i < count) {
        count--;
        if (count - i)
            memmove(combo(list, i), combo(list, i + 1), (count - i) * CO_SIZE);
    }
    return count;
}

/* 0x1000e7e4: adds the command's entry in priority order; the new count */
U __stdcall combo_insert(const uint8_t *c, uint8_t *list, U count)
{
    U i = 0;
    while (i < count && P(U, c, 0xe) < P(U, combo(list, i), CO_CODE))
        i++;
    if (count - i)
        memmove(combo(list, i + 1), combo(list, i), (count - i) * CO_SIZE);
    uint8_t *e = combo(list, i);
    P(U, e, CO_CODE) = P(U, c, 0xe);
    P(U, e, CO_HANDLER) = P(U, c, 10);
    P(U, e, CO_MASK) = P(U, c, 0x12);
    P(U, e, CO_ARG) = P(U, c, 6);
    e[CO_ENABLED] = 1;
    return count + 1;
}

/* 0x1000e6a2: one entry by argument: enable (+0x17), disable (+0x18), remove (no
 * handler), change the handler (no code) or add; the new count. (Enabling, disabling
 * or changing one that is not there writes the slot past the end, as the original
 * did.) */
U __stdcall combo_update(const uint8_t *c, uint8_t *list, U count, U max)
{
    U i = 0;
    while (i < count && P(U, combo(list, i), CO_ARG) != P(U, c, 6))
        i++;
    if (i >= max)
        return max & 0xffff;
    if (c[0x17]) {
        combo(list, i)[CO_ENABLED] = 1;
    } else if (c[0x18]) {
        combo(list, i)[CO_ENABLED] = 0;
    } else if (!P(U, c, 10)) {
        count = combo_remove(i, count, list);
    } else if (!P(U, c, 0xe)) {
        P(U, combo(list, i), CO_HANDLER) = P(U, c, 10);
        P(U, combo(list, i), CO_ARG) = P(U, c, 6);
    } else if (count < max) {
        count = combo_insert(c, list, count);
    }
    return count & 0xffff;
}

/* 0x1000e51b: the combination command for player +2: one entry (+0x16 clear), or
 * all: enable, disable, a new handler, or (no handler) clear */
void __stdcall cmd_combo(const uint8_t *c)
{
    uint8_t *b = player(P(U, c, 2));
    uint8_t *list = b + PL_COMBOS;
    if (!c[0x16])
        P(U, b, PL_COUNT) = combo_update(c, list, P(U, b, PL_COUNT), MAX_COMBOS) & 0xffff;
    else if (c[0x17])
        combos_enable(list, P(U, b, PL_COUNT), 1);
    else if (c[0x18])
        combos_enable(list, P(U, b, PL_COUNT), 0);
    else if (!P(U, c, 10))
        P(U, b, PL_COUNT) = 0;
    else
        combos_set_handler(list, P(U, b, PL_COUNT), P(U, c, 10));
}

/* 0x1000e959: the player's state into variables: direction, buttons, and whether a
 * combination was recorded */
void __stdcall cmd_controller_state(const uint8_t *c)
{
    uint8_t *b = player(P(U, c, 2));
    int32_t *v = script_var(P(U, c, 6));
    *v = b[PL_REMAP] == 1 ? g_remap[P(U, b, PL_LAST)] : (int32_t)P(U, b, PL_LAST);
    *script_var(P(U, c, 10)) = (int32_t)P(U, b, PL_HELD);
    *script_var(P(U, c, 0xe)) = b[PL_RECORDING] && P(U, b, PL_RECORD + CO_HANDLER);
}

/* 0x1000edc5: turns a keyboard set on or off */
void __stdcall keyset_enable(U n, U on) { memset(g_rt + RT_KEYSETS + (n & 0xff) * 0x11, on & 0xff, 0x11); }

/* 0x1000ed20: turns the player's device on or off */
void __stdcall device_enable(uint8_t *b, U on)
{
    uint8_t slot = b[PL_SLOT];
    switch (b[PL_TYPE]) {
    case 1:
        keyset_enable((uint8_t)(slot - 3), on & 0xff);
        break;
    case 2:
        keys_bind((uint8_t)(slot - 1), (uint8_t)on);
        break;
    case 5:
        mouse_controller_reset((uint8_t)(slot - 5), on & 0xff);
        break;
    }
}

/* 0x1000ee08: releases what the player holds (the handler hears the releases and the
 * centring) */
void __stdcall release_all(uint8_t *b, U n)
{
    U held = P(U, b, PL_HELD), last = P(U, b, PL_LAST);
    P(U, b, PL_HELD) = 0;
    P(U, b, PL_LAST) = 0;
    U h = P(U, b, PL_HANDLER);
    if (!h)
        return;
    n &= 0xff;
    if (held & 1)
        event_send(h, n, 0xfe);
    if (held & 2)
        event_send(h, n, 0xfd);
    if (held & 4)
        event_send(h, n, 0xfb);
    if (held & 8)
        event_send(h, n, 0xf7);
    if (last)
        event_send(h, n, 0);
}

/* 0x1000ea42: the device command: player +2, slot number +6, type +10; +0xb turns the
 * device on or off, +0xc releases everything; with neither the device is assigned */
void __stdcall cmd_controller_device(const uint8_t *c)
{
    uint8_t n = c[2];
    uint8_t *b = player(n);
    if (b[PL_SLOT] && *slot_player(b[PL_SLOT]) == n) {
        device_enable(b, c[0xb]);
        if (b[PL_TYPE] == 5)
            R(uint8_t, RT_MOUSE_USED) = c[0xb];
        b[PL_ACTIVE] = c[0xb];
    }
    if (c[0xc])
        release_all(b, n);
    if (c[0xb] || c[0xc])
        return;
    if (c[10] == 1) {
        b[PL_SLOT] = c[6] + 3;
    } else if (c[10] == 2) {
        b[PL_SLOT] = c[6] + 1;
    } else if (c[10] == 5) {
        b[PL_SLOT] = 5;
        R(uint8_t, RT_MOUSE_USED) = 1;
    }
    b[PL_TYPE] = c[10];
    if (!b[PL_TYPE]) {
        if (*slot_player(b[PL_SLOT]) == n && b[PL_SLOT] == 5)
            R(uint8_t, RT_MOUSE_USED) = 0;
    } else {
        *slot_player(b[PL_SLOT]) = c[2];
    }
    b[PL_ACTIVE] = b[PL_TYPE] != 0;
    device_enable(b, b[PL_ACTIVE]);
    uint8_t t0 = player(0)[PL_TYPE], t1 = player(1)[PL_TYPE];
    R(uint8_t, RT_ANY_JOYSTICK) = t0 == 2 || t1 == 2;
    R(uint8_t, RT_ANY_KEYBOARD) = t0 == 1 || t1 == 1;
}

/* 0x1000eef7: sets the player's handler; the old one */
U __stdcall cmd_controller_handler(const uint8_t *c)
{
    U *h = (U *)(player(P(U, c, 2)) + PL_HANDLER);
    U old = *h;
    *h = P(U, c, 6);
    return old;
}

/* 0x1000ef5a: the window lost (`lost`) or got the focus: held buttons are released,
 * or the devices turned back on */
void __stdcall controllers_focus(U lost)
{
    for (U n = 0; n < 2; n++) {
        uint8_t *b = player(n);
        if (!b[PL_ACTIVE])
            continue;
        if (!(lost & 0xff))
            device_enable(b, 1);
        else
            release_all(b, n);
    }
}

/* 0x1000efe3: the player takes its device slot back; `exclusive` turns the other
 * player's device off if it has the slot */
void __stdcall controller_take(U n, U exclusive)
{
    uint8_t *b = player(n);
    uint8_t slot = b[PL_SLOT];
    *slot_player(slot) = (uint8_t)n;
    if (exclusive) {
        U o = (n + 1) & 1;
        uint8_t *ob = player(o);
        if (ob[PL_SLOT] && *slot_player(ob[PL_SLOT]) == o) {
            ob[PL_ACTIVE] = 0;
            device_enable(ob, 0);
            if (ob[PL_TYPE] == 5)
                R(uint8_t, RT_MOUSE_USED) = slot == 5;
        }
    }
    b[PL_ACTIVE] = 1;
    device_enable(b, 1);
    if (b[PL_TYPE] == 5)
        R(uint8_t, RT_MOUSE_USED) = 1;
}
