/* Engine core, 0x10016800-0x10017f60: the script commands' argument resolvers.
 *
 * A command's fields can be constants or variables: a flag byte per field says which.
 * Each command has a small wrapper that copies the packed record, replaces the flagged
 * fields by the variables' values and calls the command's handler with the copy.
 * Most wrappers are generated from the disassembly (work/tmp/wrap.py lists the field and
 * flag offsets); the irregular ones are written out below. Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_cmdargs.h"

#define RES R(ResFile *, RT_RES)
#define CALL1(addr) ORIG(uint32_t(__stdcall *)(uint8_t *), addr)

/* 0x10016800: v itself, or the value of variable v when flagged */
uint32_t __stdcall script_value(uint32_t v, uint32_t is_var)
{
    return (uint8_t)is_var ? (uint32_t)*script_var(v) : v;
}

/* 0x100169e3: resolves a rectangle's four values (flag bits 1, 2, 4, 8) and puts its
 * corners in order */
void __stdcall cmd_rect(uint32_t *r, uint32_t flags)
{
    for (int i = 0; i < 4; i++)
        r[i] = script_value(r[i], (uint8_t)flags & (1 << i));
    if ((int32_t)r[2] < (int32_t)r[0]) {
        uint32_t t = r[0];
        r[0] = r[2], r[2] = t;
    }
    if ((int32_t)r[3] < (int32_t)r[1]) {
        uint32_t t = r[1];
        r[1] = r[3], r[3] = t;
    }
}

#define FIELD(p, off) P(uint32_t, p, off)
#define V(off, flag) FIELD(b, off) = script_value(FIELD(b, off), b[flag])
#define RECT_AT(off, flag) cmd_rect((uint32_t *)(b + (off)), b[flag])
#define W(name, size, body, call)                                                                                 \
    uint32_t __stdcall name(const uint8_t *c)                                                                     \
    {                                                                                                             \
        uint8_t b[0x40];                                                                                          \
        memcpy(b, c, size);                                                                                       \
        body return call;                                                                                         \
    }
#define WV(name, size, body, call)                                                                                \
    uint32_t __stdcall name(const uint8_t *c)                                                                     \
    {                                                                                                             \
        uint8_t b[0x40];                                                                                          \
        memcpy(b, c, size);                                                                                       \
        body call;                                                                                                \
        return 0;                                                                                                 \
    }

/* ---- generated: copy, resolve, call ---- */

W(cmdw_100168c0, 0x1a, V(0x6, 0x16); V(0xa, 0x17); V(0xe, 0x18); V(0x12, 0x19);, CALL1(0x10014acc)(b)) /* -> 0x10014acc */
WV(cmdw_1001692d, 0x8, V(0x2, 0x7);, cmd_break_loops(b)) /* -> 0x10018ea0 */
W(cmdw_10016969, 0x2e, RECT_AT(0x2, 0x2c); V(0x12, 0x28); V(0x1a, 0x2a); V(0x16, 0x29); V(0x1e, 0x2b);, hover_region_cmd(b)) /* -> 0x1000d097 */
W(cmdw_10016ac7, 0x26, RECT_AT(0xe, 0x24); V(0x2, 0x21); V(0x6, 0x22); V(0xa, 0x23);, click_cmd(b)) /* -> 0x1000bc62 */
WV(cmdw_10016b8d, 0x8, V(0x2, 0x7);, cmd_comp_to_background(b)) /* -> 0x10018ee3 */
W(cmdw_10016bc9, 0x20, V(0x2, 0x1e); V(0x6, 0x1f); V(0xa, 0x1d); V(0xe, 0x1b); V(0x12, 0x1c);, cmd_collide(b)) /* -> 0x10018f4b */
WV(cmdw_10016cca, 0xc, V(0x2, 0xa);, cmd_debug_string(b)) /* -> 0x10018f6b */
W(cmdw_10016d0e, 0x12, V(0x2, 0xf); V(0x6, 0x10); V(0xa, 0x11);, drag_cmd(b)) /* -> 0x1000bf1d */
W(cmdw_10016d80, 0x18, V(0x2, 0x15); V(0x6, 0x16); V(0xa, 0x17);, CALL1(0x100242b0)(b)) /* -> 0x100242b0 */
WV(cmdw_10016ddb, 0x18, V(0x2, 0x6);, cmd_get_bounds(b)) /* -> 0x1001910c */
WV(cmdw_10016e16, 0x28, V(0x2, 0x6);, cmd_get_bitmap_ids(b)) /* -> 0x100191de */
W(cmdw_10016e51, 0x32, V(0xa, 0x2a); V(0xe, 0x2b); V(0x12, 0x2c); V(0x16, 0x2d); V(0x1a, 0x2e); V(0x1e, 0x2f); V(0x22, 0x30); V(0x26, 0x31);, CALL1(0x10014b23)(b)) /* -> 0x10014b23 */
WV(cmdw_10016efe, 0x10, V(0x2, 0x6);, cmd_get_position(b)) /* -> 0x1001938e */
W(cmdw_10016fb9, 0x12, V(0x6, 0xf); V(0xa, 0x10);, cmd_if_goto(b)) /* -> 0x10019706 */
W(cmdw_100170ac, 0x1c, V(0x2, 0x19); V(0x6, 0x1a); V(0xa, 0x1b);, CALL1(0x1000e51b)(b)) /* -> 0x1000e51b */
W(cmdw_10017107, 0x14, V(0x2, 0x12);, CALL1(0x1000e959)(b)) /* -> 0x1000e959 */
W(cmdw_10017142, 0x10, V(0x2, 0xd); V(0x6, 0xe);, CALL1(0x1000ea42)(b)) /* -> 0x1000ea42 */
W(cmdw_1001719c, 0xc, V(0x2, 0xa); V(0x6, 0xb);, CALL1(0x1000eef7)(b)) /* -> 0x1000eef7 */
WV(cmdw_100171f0, 0xc, V(0x6, 0xa);, cmd_key_state(b)) /* -> 0x1001979d */
WV(cmdw_10017234, 0x12, V(0x2, 0xb); V(0x6, 0xa);, key_cmd(b)) /* -> 0x1000ca4b */
WV(cmdw_100172f5, 0xc, V(0x2, 0xa); V(0x6, 0xb);, cmd_set_level(b)) /* -> 0x1001980b */
WV(cmdw_1001744d, 0x12, V(0x2, 0xe); V(0x6, 0xf); V(0xa, 0x10);, cmd_offset(b)) /* -> 0x10019869 */
WV(cmdw_100174f0, 0x10, V(0x2, 0x6); V(0xc, 0xb);, cmd_play_midi(b)) /* -> 0x100198f8 */
WV(cmdw_100175ee, 0x10, V(0x6, 0xe); V(0xa, 0xf);, cmd_random(b)) /* -> 0x100199c7 */
WV(cmdw_100177a0, 0x1e, V(0x6, 0xe); V(0xa, 0xf);, bg_cmd(b)) /* -> 0x10001000 */
WV(cmdw_1001794a, 0x12, V(0x2, 0xb); V(0x6, 0xc);, palette_cmd(b)) /* -> 0x100019f9 */
WV(cmdw_100179ac, 0x8, V(0x2, 0x6);, palette_load_cmd(b)) /* -> 0x10001b06 */
WV(cmdw_100179e8, 0xc, V(0x2, 0xb);, cmd_save_resource(b)) /* -> 0x10019b54 */
W(cmdw_10017a2c, 0x12, V(0x2, 0xe); V(0x6, 0xf); V(0xa, 0x10);, cmd_set_position(b)) /* -> 0x10019c89 */
W(cmdw_10017b32, 0x12, V(0x2, 0xe); V(0x6, 0xf); V(0xa, 0x10);, CALL1(0x10024171)(b)) /* -> 0x10024171 */
W(cmdw_10017ba4, 0xc, V(0x2, 0xa); V(0x6, 0xb);, CALL1(0x10024225)(b)) /* -> 0x10024225 */
W(cmdw_10017c6e, 0xc, V(0x6, 0xa);, CALL1(0x10014bba)(b)) /* -> 0x10014bba */
W(cmdw_10017dfe, 0x2e, V(0xa, 0x26); V(0xe, 0x27); V(0x12, 0x28); V(0x16, 0x29); V(0x1a, 0x2a); V(0x1e, 0x2b); V(0x22, 0x2c);, CALL1(0x10014c1f)(b)) /* -> 0x10014c1f */
WV(cmdw_10017e9b, 0xc, V(0x6, 0xa);, cmd_is_visible(b)) /* -> 0x1001a93f */
WV(cmdw_10017edf, 0x12, V(0x2, 0xe); V(0x6, 0xf); V(0xa, 0x10);, cmd_zoom(b)) /* -> 0x1001aa0b */

/* ---- written out ---- */

#define VAL(off, flag) script_value(FIELD(c, off), c[flag])

/* 0x10016864: resolves field +2 in the command itself */
void __stdcall cmdw_inplace(uint8_t *c)
{
    FIELD(c, 2) = VAL(2, 0xb);
}

/* 0x1001688f, 0x10017296, 0x10017349, 0x1001741c, 0x100174bf: set a state field */
void __stdcall cmdw_set_3e63(const uint8_t *c)
{
    R(uint32_t, 0x3e63) = VAL(2, 6);
}

void __stdcall cmdw_set_slot(const uint8_t *c)
{
    R(uint32_t, 0x3e7f + FIELD(c, 2) * 4) = VAL(6, 10);
}

void __stdcall cmdw_set_3f0f(const uint8_t *c)
{
    if (R(uint8_t, 0xc8a))
        PostMessageA(R(HWND, RT_HWND), 0x4c8, 0, 0);
    R(uint32_t, 0x3f0f) = VAL(2, 6);
}

void __stdcall cmdw_set_mouse_handler(const uint8_t *c)
{
    R(uint32_t, 0x3f17) = VAL(2, 6);
}

void __stdcall cmdw_set_pause_handler(const uint8_t *c)
{
    R(uint32_t, 0x3f2b) = VAL(2, 6);
}

/* 0x10016b31: the clip box command */
void __stdcall cmdw_clip_box(const uint8_t *c)
{
    uint8_t b[0x14];
    memcpy(b, c, 0x14);
    RECT_AT(2, 0x12);
    RECT box;
    memcpy(&box, b + 2, 16);
    SpriteSetClipBox(R(uint8_t *, RT_HSL), &box);
}

/* 0x10016c44: moves the pointer */
void __stdcall cmdw_set_cursor_pos(const uint8_t *c)
{
    R(uint32_t, 0x8d83) = VAL(2, 10);
    R(uint32_t, 0x8d87) = VAL(6, 0xb);
    SetCursorPos(R(int, 0x36c7) + R(int, 0x8d83), R(int, 0x36cb) + R(int, 0x8d87));
}

/* 0x10016f48 */
uint32_t __stdcall cmdw_10016f48(const uint8_t *c, uint32_t arg)
{
    uint8_t b[0x1c];
    memcpy(b, c, 0x1a);
    V(2, 0xb);
    V(6, 0xc);
    V(0xe, 0x17);
    V(0x12, 0x18);
    return (uint32_t)cmd_if(b, (uint32_t *)(uintptr_t)arg);
}

/* 0x1001701b: runs script +6 if object +2 is loaded; 0 when it is loaded and there is
 * no script */
uint32_t __stdcall cmdw_if_loaded(const uint8_t *c)
{
    uint32_t id = VAL(2, 10), script = VAL(6, 0xb);
    if (!obj_loaded(id, ResGetType(RES, id)))
        return 1;
    if (!script)
        return 0;
    script_run(script);
    return 1;
}

/* 0x100172ce */
void __stdcall cmdw_100172ce(const uint8_t *c)
{
    timer_remove(FIELD(c, 2), c[6]);
}

/* 0x100173a9: commands 0x75 (load an object) and its partner (release it) */
uint32_t __stdcall cmdw_load_release(const uint8_t *c)
{
    uint32_t id = VAL(2, 6), t = ResGetType(RES, id);
    if (P(int16_t, c, 0) == 0x75)
        return (uint32_t)obj_is(id, t);
    return (uint32_t)obj_release(id, t);
}

/* 0x1001754a */
uint32_t __stdcall cmdw_1001754a(const uint8_t *c)
{
    uint8_t b[0x28];
    memcpy(b, c, 0x26);
    V(2, 6);
    V(0x1c, 0x23);
    return cmd_play_wave(FIELD(b, 2), (int32_t *)(b + 8));
}

/* 0x1001759b: sends an event: handler, id, argument */
void __stdcall cmdw_event(const uint8_t *c)
{
    uint32_t arg = VAL(10, 0x10), id = VAL(6, 0xf), handler = VAL(2, 0xe);
    event_send(handler, id, arg);
}

/* 0x10017648, 0x100177ed: +4 set: one value at +6 (flag +5); else six values at +6 with
 * flag bits in +5. The handler's byte +3 is copied back into the command. */
static uint32_t six_values(uint8_t *c, uint32_t handler)
{
    uint8_t b[0x20];
    memcpy(b, c, 0x1e);
    if (b[4])
        V(6, 5);
    else
        for (int i = 0; i < 6; i++)
            if (b[5] & (uint8_t)(1 << i))
                FIELD(b, 6 + i * 4) = script_value(FIELD(b, 6 + i * 4), 1);
    uint32_t r = CALL1(handler)(b);
    c[3] = b[3];
    return r;
}

uint32_t __stdcall cmdw_10017648(uint8_t *c)
{
    return six_values(c, 0x10019a62);
}

uint32_t __stdcall cmdw_100177ed(uint8_t *c)
{
    return six_values(c, 0x1001a3f0);
}

/* 0x100176f4: a handler call with +9 arguments at +10 (flag bits in +7) */
uint32_t __stdcall cmdw_handler_call(const uint8_t *c)
{
    uint8_t b[0x30];
    memcpy(b, c, c[9] * 4 + 10);
    V(2, 6);
    for (uint32_t i = 0; i < c[9]; i++)
        if (b[7] & (uint8_t)(1 << i))
            FIELD(b, 10 + i * 4) = script_value(FIELD(b, 10 + i * 4), 1);
    return (uint32_t)handler_record_run(b);
}

/* 0x10017899: +6 entries of 8 bytes at +10 (value, ..., flag at +7) */
uint32_t __stdcall cmdw_10017899(const uint8_t *c)
{
    uint8_t b[0x28];
    memcpy(b, c, 0x22);
    for (uint32_t i = 0; i < FIELD(b, 6); i++)
        FIELD(b, 0xa + i * 8) = script_value(FIELD(b, 0xa + i * 8), b[0x11 + i * 8]);
    cmd_math(b);
    return 0;
}

/* 0x10017902 */
uint32_t __stdcall cmdw_10017902(const uint8_t *c)
{
    uint32_t b = VAL(6, 0xc), a = VAL(2, 0xb);
    return cmd_set_queue(a, b, c[10]);
}

/* 0x10017a9e: +0xc entries of 6 bytes at +0x10 (value, used, flag) */
uint32_t __stdcall cmdw_10017a9e(const uint8_t *c)
{
    uint8_t b[0x38];
    memcpy(b, c, 0x34);
    V(6, 0xb);
    for (uint32_t i = 0; i < FIELD(b, 0xc); i++)
        if (b[0x14 + i * 6])
            FIELD(b, 0x10 + i * 6) = script_value(FIELD(b, 0x10 + i * 6), b[0x15 + i * 6]);
    cmd_format(b);
    return 0;
}

/* 0x10017bf8 */
void __stdcall cmdw_10017bf8(const uint8_t *c)
{
    uint32_t v = c[0xe] ? VAL(10, 0x10) : FIELD(c, 10);
    timer_set(FIELD(c, 2), VAL(6, 0xf), v, c[0x11]);
}

/* 0x10017cb2, 0x10017cdd */
void __stdcall cmdw_10017cb2(const uint8_t *c)
{
    cmd_stop_midi(VAL(2, 6));
}

void __stdcall cmdw_10017cdd(const uint8_t *c)
{
    cmd_stop_wave(VAL(2, 6), (int32_t *)(c + 8));
}

/* 0x10017d0f: field +10 names a variable: its value goes in, the handler's result for
 * that field goes back into the variable */
uint32_t __stdcall cmdw_10017d0f(const uint8_t *c)
{
    uint8_t b[0x18];
    memcpy(b, c, 0x12);
    FIELD(b, 10) = (uint32_t)*script_var(FIELD(c, 10));
    V(2, 0x10);
    V(6, 0x11);
    cmd_list_entry(b);
    *script_var(FIELD(c, 10)) = (int32_t)FIELD(b, 10);
    return FIELD(b, 10);
}

/* 0x10017da2 */
void __stdcall cmdw_10017da2(const uint8_t *c)
{
    uint32_t a3 = VAL(10, 0x11), a2 = VAL(6, 0x10), a1 = VAL(2, 0xf);
    sprite_transfer(a1, a2, a3, c[0xe]);
}
