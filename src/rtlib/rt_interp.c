/* Engine core, 0x10021960-0x10022e5c: the script interpreter's command loop. A handler
 * is a list of commands (a 16-bit opcode, then fixed or self-sized arguments); running
 * one executes its commands in order until one stops it or the scene changes. Handlers
 * call each other up to 30 deep, with their locals on a stack in the state block.
 * Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_interp.h"

#define RU(off) R(U, off)
#define RB(off) R(uint8_t, off)
#define F32(c, off) P(U, c, off)

#define RT_SCRIPT_PROC 0x3c37 /* proc(command, arg) sees every command while debugging */
#define RT_PROC_ARG 0x3c3f
#define RT_LOCALS_SP 0x36cf
#define RT_ERR_LINE 0x3e0b
#define RT_CALL_DEPTH 0x3e07
#define RT_STOPPED 0x3ddb /* set when the scene changes: no more commands run */
#define RT_RESULT 0x3f3b
#define RT_SCRIPT_NAME 0x3f43
#define RT_SCRIPT_INDEX 0x3f47
#define RT_SCRIPT_DEBUG 0x95d8
#define RT_NEXT_SCENE 0x9703
#define RT_MSG_ARG 0x8db3


#define MAX_DEPTH 30

/* 0x100219fb: the size of a command, 0 for an unknown one */
U __stdcall script_cmd_size(const uint16_t *c)
{
    switch (*c) {
    case 1: return ((const uint8_t *)c)[9] * 4 + 10;
    case 2: case 0x39: case 0xa2: case 0xa3: return 2;
    case 0x1b: case 0x7a: case 0x8e: case 0xd2: return 4;
    case 0x23: case 0x3c: case 0x3d: case 0x3e: case 0x68: case 0x72: return 6;
    case 4: case 0xd: case 0x14: case 0x6d: case 0x74: case 0x75: case 0xdf: case 0x78: case 0x8a:
    case 0xb4: case 0xd3: return 8;
    case 0xc6: return 10;
    case 0x1a: case 0xca: case 0x1c: case 0x1e: case 0x5c: case 0x61: case 0x65: case 0x6a:
    case 0x71: case 0xb7: case 0xc3: case 0xd4: case 0xe1: return 0xc;
    case 0xb9: case 0xd5: case 0xd6: return 0xe;
    case 0x4d: case 0x60: case 0x8f: case 0x99: case 0x9c: case 0xe4: return 0x10;
    case 0x2b: case 0x42: case 0x53: case 0x6b: case 0x7c: case 0x95: case 0xb3: case 0xb8:
    case 0xc2: case 0xc4: case 0xd9: case 0xde: case 0xe6: return 0x12;
    case 0x11: case 0x63: return 0x14;
    case 0x2c: case 0x3f: return 0x18;
    case 0xb: case 0x59: return 0x1a;
    case 0x5e: return 0x1c;
    case 0x9a: case 0xa4: case 0xa7: return 0x1e;
    case 0x17: return 0x20;
    case 0xa9: return 0x22;
    case 0x10: case 0x91: return 0x26;
    case 0x40: return 0x28;
    case 0xe: case 0xe0: return 0x2e;
    case 0x48: return 0x32;
    case 0xc0: return 0x34;
    case 0x3a: return 0x102;
    case 0x36: return 0x104;
    case 0x5b: return F32(c, 6);
    case 0xaf: return F32(c, 2);
    }
    return 0;
}

/* 0x10021960: the number of commands in handler `id`, 0 at an unknown one */
U __stdcall handler_count(U id)
{
    if (!obj_is(id, 0xe))
        return 0;
    U *e = obj_entry(id);
    const uint8_t *c = (const uint8_t *)(uintptr_t)e[0];
    int left = (int)e[1];
    U n = 0;
    while (left > 0) {
        U size = script_cmd_size((const uint16_t *)c);
        if (!size)
            return 0;
        c += size;
        n++;
        left -= (int)size;
    }
    return n;
}

/* 0x100220dd: runs a handler record: +2 handler, +8 number of locals, +9 number of
 * arguments, +10 the arguments, which become its first locals */
U __stdcall handler_record_run(const uint8_t *rec)
{
    U locals = rec[8] * 4u;
    if (rec[9]) {
        U *dst = (U *)(uintptr_t)(RU(RT_LOCALS_SP) - locals);
        const U *src = (const U *)(rec + 10);
        for (U n = rec[9]; n; n--)
            *dst-- = *src++;
    }
    RU(RT_LOCALS_SP) -= locals;
    U r = script_run(F32(rec, 2));
    RU(RT_LOCALS_SP) += locals;
    return r;
}

/* 0x10022075: a queued handler call: +2 handler, +6 argument count, +10 arguments */
void __stdcall handler_post_run(const uint8_t *p)
{
    uint8_t rec[10 + 4 * 64] = {0};
    P(U, rec, 2) = F32(p, 2);
    rec[8] = 0;
    rec[9] = p[6];
    U n = F32(p, 6);
    if (n > 64)
        n = 64; /* the original's record holds 6; more would overrun its stack */
    for (U i = 0; i < n; i++)
        P(U, rec, 10 + i * 4) = F32(p, 10 + i * 4);
    handler_record_run(rec);
}

/* 0x10021fab: the sprite engine's message callback: script messages 0x13-0x16 */
void __stdcall script_message(const uint16_t *msg, U arg)
{
    if (RU(RT_STOPPED))
        return;
    RU(RT_MSG_ARG) = arg;
    switch (*msg) {
    case 0x13:
        sprite_extra_release((U)(F32(msg, 2)));
        break;
    case 0x14:
        PostMessageA(R(HWND, RT_HWND), 0x4c8, 0, 0);
        break;
    case 0x15:
        cmd_exec((const uint8_t *)msg + 6, NULL);
        break;
    case 0x16:
        handler_post_run((const uint8_t *)msg);
        break;
    }
}

/* 0x100221a7: executes one command; `*len` gets its size (0: don't go on). Returns 0
 * when the handler should stop. */
U __stdcall cmd_exec(const uint8_t *c, int *len)
{
    U ok = 1;
    int n = 0;
    uint8_t *cw = (uint8_t *)(uintptr_t)c;
    if (RU(RT_SCRIPT_PROC) && RU(RT_SCRIPT_NAME)) {
        U a[2] = {(U)(uintptr_t)c, RU(RT_PROC_ARG)};
        orig_stdcall_unsafe(R(void *, RT_SCRIPT_PROC), 2, a);
    }
    switch (P(uint16_t, c, 0)) {
    case 1: ok = cmdw_handler_call(c); n = c[9] * 4 + 10; break;
    case 2: n = 2; break;
    case 4: cmdw_set_3e63(c); n = 8; break;
    case 0xb: cmdw_100168c0(c); n = 0x1a; break;
    case 0xd: cmdw_1001692d(c); n = 8; break;
    case 0xe: cmdw_10016969(c); n = 0x2e; break;
    case 0x10: ok = cmdw_10016ac7(c); n = 0x26; break;
    case 0x11: cmdw_clip_box(c); n = 0x14; break;
    case 0x14: cmdw_10016b8d(c); n = 8; break;
    case 0x17: ok = cmdw_10016bc9(c); n = 0x20; break;
    case 0x1a: case 0xca: cmdw_10017c6e(c); n = 0xc; break;
    case 0x1b: cursor_set(c[2]); n = 4; break;
    case 0x1c: cmdw_set_cursor_pos(c); n = 0xc; break;
    case 0x1e:
        if (RB(RT_SCRIPT_DEBUG))
            cmdw_10016cca(c);
        n = 0xc;
        break;
    case 0x23: cmd_ini_delete_section(c); n = 6; break;
    case 0x2b: ok = cmdw_10016d0e(c); n = 0x12; break;
    case 0x2c: ok = cmdw_10016d80(c); n = 0x18; break;
    case 0x36: cmd_new_scene(cw); n = 0x104; break;
    case 0x39:
        /* quit, or go to the scene set by command 0x3a */
        if (!R(char, RT_NEXT_SCENE)) {
            PostMessageA(R(HWND, RT_HWND), WM_CLOSE, 1, 0);
        } else {
            uint8_t rec[0x104];
            memset(rec, 0, sizeof rec);
            P(uint16_t, rec, 0) = 0x36;
            rec[0x102] = 1;
            lstrcpyA((LPSTR)(rec + 2), (LPCSTR)(g_rt + RT_NEXT_SCENE));
            cmd_new_scene(rec);
        }
        n = 2;
        break;
    case 0x3a: lstrcpyA((LPSTR)(g_rt + RT_NEXT_SCENE), (LPCSTR)(c + 2)); n = 0x102; break;
    case 0x3c: case 0x3d: scene_cmd((const int16_t *)c); n = 6; break;
    case 0x3e: RU(RT_SCRIPT_NAME) = F32(c, 2); n = 6; break;
    case 0x3f: cmdw_10016ddb(c); n = 0x18; break;
    case 0x40: cmdw_10016e16(c); n = 0x28; break;
    case 0x42: cmd_get_date(c); n = 0x12; break;
    case 0x48: cmdw_10016e51(c); n = 0x32; break;
    case 0x4d: cmdw_10016efe(c); n = 0x10; break;
    case 0x53: cmd_get_time(c); n = 0x12; break;
    case 0x59: ok = cmdw_10016f48(c, (U)(uintptr_t)&n); break;
    case 0x5a: n = (int)cmdw_10016fb9(c); break;
    case 0x5b: n = (int)cmd_if_expr(c); break;
    case 0x5c: n = cmdw_if_loaded(c) ? 0xc : 0; break;
    case 0x5e: cmdw_100170ac(c); n = 0x1c; break;
    case 0x60: cmdw_10017142(c); n = 0x10; break;
    case 0x61: cmdw_1001719c(c); n = 0xc; break;
    case 0x63: cmdw_10017107(c); n = 0x14; break;
    case 0x65: cmdw_set_slot(c); n = 0xc; break;
    case 0x68: n = (int)F32(c, 2); break;
    case 0x6a: cmdw_100171f0(c); n = 0xc; break;
    case 0x6b: cmdw_10017234(c); n = 0x12; break;
    case 0x6d: cmdw_100172ce(c); n = 8; break;
    case 0x71: cmdw_100172f5(c); n = 0xc; break;
    case 0x72: RU(RT_ERR_LINE) = F32(c, 2); n = 6; break;
    case 0x74: cmdw_set_3f0f(c); n = 8; break;
    case 0x75: case 0xdf: ok = cmdw_load_release(c); n = 8; break;
    case 0x78: cmdw_set_mouse_handler(c); n = 8; break;
    case 0x7a: AudioMute(R(uint8_t *, 0xc90), (int16_t)(int8_t)c[2]); n = 4; break;
    case 0x7c: cmdw_1001744d(c); n = 0x12; break;
    case 0x8a: cmdw_set_pause_handler(c); n = 8; break;
    case 0x8e: cmd_cd_track(c[2]); n = 4; break;
    case 0x8f: cmdw_100174f0(c); n = 0x10; break;
    case 0x91: ok = cmdw_1001754a(c); n = 0x26; break;
    case 0x95: cmdw_event(c); n = 0x12; break;
    case 0x99: cmdw_100175ee(c); n = 0x10; break;
    case 0x9a: ok = cmdw_10017648(cw); n = 0x1e; break;
    case 0x9c: case 0xe4: cmd_ini(c); n = 0x10; break;
    case 0xa1: n = 0; RU(RT_RESULT) = (U)script_eval(c + 6); break;
    case 0xa2: n = 0; break;
    case 0xa3: ini_backup((U)(uintptr_t)c); n = 2; break;
    case 0xa4: cmdw_100177a0(c); n = 0x1e; break;
    case 0xa7: ok = cmdw_100177ed(cw); n = 0x1e; break;
    case 0xa9: cmdw_10017899(c); n = 0x22; break;
    case 0xaf: script_eval(c + 6); n = (int)F32(c, 2); break;
    case 0xb3: cmdw_1001794a(c); n = 0x12; break;
    case 0xb4: cmdw_100179ac(c); n = 8; break;
    case 0xb7: cmdw_100179e8(c); n = 0xc; break;
    case 0xb8: ok = cmdw_10017a2c(c); n = 0x12; break;
    case 0xb9: ok = cmdw_10017902(c); n = 0xe; break;
    case 0xc0: cmdw_10017a9e(c); n = 0x34; break;
    case 0xc2: ok = cmdw_10017b32(c); n = 0x12; break;
    case 0xc3: cmdw_10017ba4(c); n = 0xc; break;
    case 0xc4: cmdw_10017bf8(c); n = 0x12; break;
    case 0xc6: cmd_volume(c); n = 10; break;
    case 0xd2: cd_stop(); n = 4; break;
    case 0xd3: cmdw_10017cb2(c); n = 8; break;
    case 0xd4: cmdw_10017cdd(c); n = 0xc; break;
    case 0xd5: case 0xd6: cmd_strcmp(c); n = 0xe; break;
    case 0xd9: cmdw_10017d0f(c); n = 0x12; break;
    case 0xda: n = (int)cmd_switch(c); break;
    case 0xdb: n = (int)cmd_switch_expr(c); break;
    case 0xde: cmdw_10017da2(c); n = 0x12; break;
    case 0xe0: cmdw_10017dfe(c); n = 0x2e; break;
    case 0xe1: cmdw_10017e9b(c); n = 0xc; break;
    case 0xe3: n = (int)cmd_if_expr2(c); break;
    case 0xe6: cmdw_10017edf(c); n = 0x12; break;
    default:
        if (!RB(RT_SCRIPT_DEBUG))
            rt_fatal(0x3ed, 0);
        else
            rt_script_error(0x73, 0);
        ok = 0;
        n = 0;
    }
    if (len)
        *len = n;
    return ok && !RU(RT_STOPPED);
}

/* 0x10022dde: runs the commands of handler `id` */
U __stdcall handler_exec(U id)
{
    U *e = obj_entry(id);
    const uint8_t *c = (const uint8_t *)(uintptr_t)e[0];
    int left = (int)e[1], n;
    U r;
    for (;;) {
        r = cmd_exec(c, &n);
        if (!r || !n || (left -= n) < 1)
            break;
        c += n;
    }
    return r;
}

/* 0x10022ce9: runs script handler `id`, at most 30 deep. (Past the limit the depth
 * count is not taken back, as in the original.) */
U __stdcall script_run(U id)
{
    if (RU(RT_STOPPED) || !obj_is(id, 0x13))
        return 0;
    if (++RU(RT_CALL_DEPTH) < MAX_DEPTH) {
        RU(RT_SCRIPT_NAME) = 0;
        U old = RU(RT_SCRIPT_INDEX);
        RU(RT_SCRIPT_INDEX) = id;
        U r = handler_exec(id);
        RU(RT_SCRIPT_INDEX) = old;
        RU(RT_CALL_DEPTH)--;
        return r;
    }
    if (!RB(RT_SCRIPT_DEBUG)) {
        rt_fatal(0x3ea, 0);
        return 0;
    }
    return rt_script_error(0x6a, 0);
}
