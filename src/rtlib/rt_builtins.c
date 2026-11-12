/* Engine core, 0x10003f6d-0x1000b16f: the script's built-in functions, one big switch
 * on the function number. Arguments are on the script stack, the last on top; a
 * result replaces the slot below the arguments (the function number). Many built-ins
 * just pack their arguments into a script command record and run that command.
 * Rewritten from the original's assembly (its decompile of this function is wrong in
 * places). Numbers not handled here still go to the original. */
#include "rtcore.h"
#include "crt.h"
#include "rt.h"
#include "math87.h"
#include "rt_builtins.h"

#define RU(off) R(U, off)
#define RT_SP 0x3e4f
#define RT_AUDIO 0x0c90
#define RT_FIRST_COPY 0x3dfb      /* ids from here on are copies made at run time */
#define RT_SCRIPT_DEBUG 0x95d8
#define RT_ON_ACTIVATE 0x3e5f
#define RT_HOVER_HANDLER 0x3e67
#define RT_KEY_HANDLER 0x3f07
#define RT_MOUSE_HANDLER 0x3f17
#define RT_DRIVE_TYPE 0x3dd7
#define RT_JOY_SWAP 0x95e4

#define TYPE_DIB 1
#define TYPE_ARRAY 0xb
#define TYPE_WAVE 0xc
#define TYPE_MOVIE 0xd
#define TYPE_SPRITE 0x14

#define g_stack ((int32_t *)ORIG_PTR(0x1007c408))
#define REG_PATH "Software\\7th Level, Inc."

#define AudioSetWaveSoundPriority ORIG(void(__stdcall *)(uint8_t *, U, U), 0x100402fa)
#define RtSetScreenRes ORIG(U(__cdecl *)(uint8_t *, U, U, U), 0x1001d3b3) /* too short to hook */
#define AudioGetWaveSoundTime ORIG(U(__stdcall *)(uint8_t *, U), 0x1003fbf5)

static int32_t pop(void)
{
    RU(RT_SP)--;
    return g_stack[RU(RT_SP)];
}
static U popu(void) { return (U)pop(); }
static int32_t *pop_ptr(void) { return (int32_t *)(uintptr_t)popu(); }
static char *pop_str(void) { return string_get(popu()); }
static int32_t top(void) { return g_stack[RU(RT_SP) - 1]; }
static void ret(int32_t v) { g_stack[RU(RT_SP) - 1] = v; }

static uint8_t *hsl(void) { return R(uint8_t *, RT_HSL); }
static uint8_t *audio(void) { return R(uint8_t *, RT_AUDIO); }
static uint8_t *sprite_of(U id) { return (uint8_t *)(uintptr_t)obj_entry(id)[1]; }
static U res_type(U id) { return ResGetType(R(ResFile *, RT_RES), id); }

static void put16(uint8_t *p, U v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, U v) { put16(p, v), put16(p + 2, v >> 16); }

/* a script error when debugging, else the fatal error `code` */
static void array_error(U code)
{
    if (R(uint8_t, RT_SCRIPT_DEBUG))
        rt_script_error(0x73, 0);
    else
        rt_fatal(code, 0);
}

/* the cell of an array object at the indices on the stack (argc of them), or NULL
 * after an error */
static U *array_cell(U id, const int32_t *idx, int n, U code)
{
    U *a = (U *)(uintptr_t)obj_entry(id)[0]; /* data, size, ?, ?, dims, sizes[dims]... */
    if (a[4] != (U)n) {
        array_error(code);
        return NULL;
    }
    U p = a[0];
    for (int i = 0; i < n; i++)
        p += a[5 + i] * (U)idx[i] * 4;
    if (a[0] > p || a[1] + a[0] <= p) {
        array_error(code);
        return NULL;
    }
    return (U *)(uintptr_t)p;
}

/* fixed-point arithmetic: (whole, part) pairs with the part in 1/10000 */
static void fixed(int op)
{
    int32_t *part = pop_ptr(), *whole = pop_ptr();
    int32_t b_part = pop(), b_whole = pop();
    int32_t a = *whole * 10000 + *part, b = b_whole * 10000 + b_part, t;
    if (op == 0)
        t = a + b;
    else if (op == 1)
        t = (int32_t)((U)a * (U)b);
    else
        t = a / b;
    *whole = t / 10000;
    *part = t % 10000;
}

/* the background command (0xa4) */
static void bg_rgb(U fn, uint8_t *c)
{
    if (fn == 0xbc) {
        put32(c + 0x1a, popu());
        put32(c + 0x16, popu());
        put32(c + 0x12, popu());
    }
    U b = (uint16_t)pop(), g = (uint16_t)pop(), r = (uint16_t)pop();
    c[0x10] = (uint8_t)color_index(b << 16 | (g & 0xff) << 8 | (r & 0xff));
}

static void new_scene(const char *name, uint8_t arg, uint8_t flag)
{
    uint8_t c[0x104] = {0};
    put16(c, 0x36);
    c[0x102] = arg;
    lstrcpyA((char *)c + 2, name);
    c[0x103] = flag;
    cmd_new_scene(c);
}

static void reg_get_str(HKEY root, const char *path, int n, U argc)
{
    U crypt = argc > (U)n ? popu() : 0;
    char *out = string_var(popu());
    char *name = pop_str(), *key = pop_str();
    if (!path)
        path = pop_str();
    reg_get_string(root ? root : (HKEY)(uintptr_t)popu(), path, key, name, (LPBYTE)out, crypt);
}

void __stdcall builtin_call(U fn, U argc)
{
    uint8_t c[0x30];
    memset(c, 0, sizeof c);
    switch (fn) {
    case 0x2:
        break;
    case 0x3:
        {
            int32_t v = pop();
            ret(v < 0 ? (int32_t)(0u - (U)v) : v);
        }
        break;
    case 0x5:
        RU(RT_ON_ACTIVATE) = (U)top();
        break;
    case 0x6:
        fixed(0);
        break;
    case 0x79:
        fixed(1);
        break;
    case 0x2a:
        fixed(2);
        break;
    case 0xb: {
        int32_t *s = g_stack + RU(RT_SP);
        int dx = s[-2] - s[-4], dy = s[-1] - s[-3];
        RU(RT_SP) -= 4;
        ret(GetAngle(dx, dy));
        break;
    }
    case 0xc: {
        char *s = pop_str();
        ret(s ? crt_atoi(s) : 0);
        break;
    }
    case 0xf: {
        U old = RU(RT_HOVER_HANDLER);
        hover_handler_set(popu());
        ret((int32_t)old);
        break;
    }
    case 0x12: {
        int32_t v[8];
        for (int i = 7; i >= 0; i--)
            v[i] = pop();
        Collide(v[0], v[1], v[2], v[3], (int *)(uintptr_t)(U)v[4], (int *)(uintptr_t)(U)v[5],
                (int *)(uintptr_t)(U)v[6], (int *)(uintptr_t)(U)v[7]);
        break;
    }
    case 0x16: {
        U d = argc == 4 ? popu() : 0;
        U e = argc >= 3 ? popu() : 0;
        U src = popu(), dst = popu();
        ret(bitmap_merge_objects(dst, src, (int)e, (int)d, 0));
        break;
    }
    case 0x18: {
        int32_t same = pop(), h2 = pop(), h1 = pop();
        U b = popu(), a = popu();
        ret(obj_loaded(b, TYPE_SPRITE) && obj_loaded(a, TYPE_SPRITE) &&
            SpriteHitDetect(sprite_of(a), sprite_of(b), h1, h2, same));
        break;
    }
    case 0x19: {
        U r = obj_copy(popu());
        ret((int32_t)r);
        if (!r && R(uint8_t, RT_SCRIPT_DEBUG))
            rt_script_error(0x75, 0);
        break;
    }
    case 0x1a:
        ret(Cosine(pop()));
        break;
    case 0xca:
        ret(Sine(pop()));
        break;
    case 0x1d:
        builtin_curve_point();
        break;
    case 0x73:
        builtin_line_point();
        break;
    case 0x1e: { /* debug print: printf-style, arguments on the stack */
        RU(RT_SP) -= argc;
        char *fmt = string_get((U)g_stack[RU(RT_SP)]);
        if (!fmt)
            break;
        char buf[1025]; /* the original's 340 bytes could overflow */
        wvsprintfA(buf, fmt, (va_list)(g_stack + RU(RT_SP) + 1));
        RU(RT_SP)--;
        if (R(uint8_t, RT_SCRIPT_DEBUG))
            DebugStr("%s", buf);
        break;
    }
    case 0x1f:
        put16(c, 0xe);
        c[0x26] = 1;
        hover_region_cmd(c);
        break;
    case 0x20:
        put16(c, 0xe);
        put32(c + 0xe, popu());
        put32(c + 0xa, popu());
        put32(c + 6, popu());
        put32(c + 2, popu());
        hover_region_cmd(c);
        break;
    case 0x84: {
        put16(c, 0xe);
        U v = popu();
        put32(c + 0x1e, v);
        put32(c + 0x16, v);
        v = popu();
        put32(c + 0x1a, v);
        put32(c + 0x12, v);
        put32(c + 0xe, popu());
        put32(c + 0xa, popu());
        put32(c + 6, popu());
        put32(c + 2, popu());
        hover_region_cmd(c);
        break;
    }
    case 0x85:
        put16(c, 0x10);
        put32(c + 0xa, popu());
        put32(c + 6, popu());
        put32(c + 0x1a, popu());
        put32(c + 0x16, popu());
        put32(c + 0x12, popu());
        put32(c + 0xe, popu());
        click_cmd(c);
        break;
    case 0x22:
    case 0xdf: {
        U id = popu();
        if (RU(RT_FIRST_COPY) > id)
            ret(obj_release(id, res_type(id)));
        else
            ret(obj_delete(id));
        break;
    }
    case 0x29: {
        int32_t y2 = pop(), x2 = pop(), y1 = pop(), x1 = pop();
        ret(Distance(x1, y1, x2, y2));
        break;
    }
    case 0x37: {
        uint8_t arg = (uint8_t)pop();
        new_scene(pop_str(), arg, 0);
        break;
    }
    case 0x38:
        new_scene("", (uint8_t)pop(), 1);
        break;
    case 0x39:
        if (R(char, 0x9703))
            new_scene((char *)g_rt + 0x9703, 1, 0);
        else
            PostMessageA(R(HWND, RT_HWND), WM_CLOSE, 1, 0);
        break;
    case 0x3b: {
        U index = popu();
        char *path = pop_str();
        U id = popu();
        if (path && obj_loaded(id, TYPE_DIB))
            ret(dib_draw_icon((uint8_t *)(uintptr_t)obj_entry(id)[0], path, index));
        else
            ret(0);
        break;
    }
    case 0x3c:
        scene_show_fn(popu());
        break;
    case 0x3d:
        scene_hide_fn(popu());
        break;
    case 0x41: {
        int32_t i = pop();
        char *s = pop_str();
        ret(s && lstrlenA(s) >= i ? s[i] : 0);
        break;
    }
    case 0x44:
    case 0x45: { /* the size of an array's dimension */
        int32_t i = pop();
        U id = popu();
        if (obj_is(id, TYPE_ARRAY))
            ret((int32_t)((U *)(uintptr_t)obj_entry(id)[0])[10 + i]);
        break;
    }
    case 0x46:
        if (argc == 1) {
            char *s = pop_str();
            char root[4] = {s[0] == '"' ? s[1] : s[0], ':', '\\', 0};
            ret((int32_t)GetDriveTypeA(root));
        } else {
            ret((int32_t)RU(RT_DRIVE_TYPE));
        }
        break;
    case 0x47:
        ret((int32_t)(RU(0xb70) >> 16));
        break;
    case 0x49:
        ret((int32_t)RU(RT_MOUSE_HANDLER));
        break;
    case 0x4a: {
        U id = popu();
        ret(obj_is(id, TYPE_SPRITE) ? (int32_t)SpriteGetNumCels(sprite_of(id)) : 0);
        break;
    }
    case 0x4b: { /* an array's number of dimensions */
        U id = popu();
        if (obj_is(id, TYPE_ARRAY))
            ret((int32_t)((U *)(uintptr_t)obj_entry(id)[0])[4]);
        break;
    }
    case 0x4c: {
        DWORD v = GetVersion();
        ret(((v >> 8) & 0xff) == 0x5f || (v & 0xff) == 4 ? 0x5f : 0x1f);
        break;
    }
    case 0x4e:
        ret(GetQuadrant(pop()));
        break;
    case 0x4f: {
        int32_t *b = pop_ptr(), *r = pop_ptr(), *t = pop_ptr(), *l = pop_ptr();
        int32_t box[4];
        SpriteGetScrollBox(hsl(), box);
        *l = box[0];
        *t = box[1];
        *r = box[2];
        *b = box[3];
        break;
    }
    case 0x50: {
        int32_t *py = pop_ptr(), *px = pop_ptr();
        int32_t pos[2];
        SpriteGetScrollPos(hsl(), pos);
        *px = pos[0];
        *py = pos[1];
        break;
    }
    case 0x52: { /* read an array cell */
        int n = (int)argc - 1;
        int32_t idx[256];
        for (int i = n - 1; i >= 0; i--)
            idx[i] = pop();
        U id = popu();
        if (!obj_is(id, TYPE_ARRAY))
            break;
        U *p = array_cell(id, idx, n, 0x3ef);
        ret(p ? (int32_t)*p : 0);
        break;
    }
    case 0xc1: { /* write an array cell */
        int n = (int)argc - 2;
        int32_t idx[256];
        for (int i = n - 1; i >= 0; i--)
            idx[i] = pop();
        U v = popu(), id = popu();
        if (!obj_is(id, TYPE_ARRAY))
            break;
        U *p = array_cell(id, idx, n, 0x3ee);
        if (p)
            *p = v;
        ret(p != NULL);
        break;
    }
    case 0x55: {
        ATOM a = AddAtomA(pop_str());
        PostMessageA(R(HWND, RT_HWND), 0xf000, 0, a);
        break;
    }
    case 0x56: {
        char *out = string_var(popu());
        if (!out)
            break;
        DWORD n = 0x200;
        if (!GetUserNameA(out, &n))
            lstrcpyA(out, "guest");
        break;
    }
    case 0x57: {
        U id = popu();
        ret(obj_is(id, TYPE_WAVE) ? (int32_t)AudioGetWaveSoundTime(audio(), obj_entry(id)[0]) : 0);
        break;
    }
    case 0x5d:
        builtin_list_entry();
        break;
    case 0x5f: {
        U vk = popu(), which = popu(), player = popu();
        keymap_set((uint8_t)player, (uint8_t)which, (uint8_t)vk);
        break;
    }
    case 0x62: {
        U exclusive = popu();
        controller_take(popu(), exclusive);
        break;
    }
    case 0x64: {
        U id = popu();
        ret((int32_t)obj_loaded(id, res_type(id)));
        break;
    }
    case 0x66:
        R(uint8_t, RT_JOY_SWAP) = pop() != 0;
        break;
    case 0x67:
        R(uint8_t, 0x95e5) = pop() != 0;
        break;
    case 0x69:
    case 0x6b: {
        U old = RU(RT_KEY_HANDLER);
        RU(RT_KEY_HANDLER) = popu();
        ret((int32_t)old);
        break;
    }
    case 0x6a:
        ret(GetKeyState(pop()) < 0);
        break;
    case 0x6f: { /* run a program; with a handler, it hears when the program ends */
        U handler = argc > 2 ? popu() : 0;
        char *dir = argc > 1 ? pop_str() : NULL;
        char *cmd = pop_str();
        int32_t r;
        if (!cmd)
            r = 0;
        else if (handler)
            r = (int32_t)program_launch(cmd, dir, handler);
        else
            r = WinExec(cmd, SW_SHOWNORMAL) >= 32;
        ret(r);
        break;
    }
    case 0x74:
        if (R(char, 0xc8a))
            PostMessageA(R(HWND, RT_HWND), 0x4c8, 0, 0);
        RU(0x3f0f) = popu();
        break;
    case 0x75: {
        U id = popu();
        ret(obj_is(id, res_type(id)));
        break;
    }
    case 0x76: {
        int32_t b = pop(), a = pop();
        ret(a > b ? a : b);
        break;
    }
    case 0x77: {
        int32_t b = pop(), a = pop();
        ret(a < b ? a : b);
        break;
    }
    case 0x89:
        AudioPause(audio(), pop());
        break;
    case 0x8b:
        AudioPauseMidi(audio(), pop());
        break;
    case 0x8c: {
        int32_t pause = pop();
        U id = popu();
        if (id && !obj_loaded(id, TYPE_SPRITE))
            break;
        uint8_t *s = id ? sprite_of(id) : NULL;
        SpritePause(hsl(), s, pause);
        if (!s)
            movies_pause((U)pause);
        break;
    }
    case 0x8d:
        timers_pause((uint8_t)pop());
        break;
    case 0x94:
        RU(0x3f2f) = (U)top();
        break;
    case 0x96: {
        U b2 = popu(), b1 = popu(), handler = popu();
        event_send(handler, b1, b2);
        break;
    }
    case 0x97: {
        U landscape = argc > 1 ? popu() : 0;
        ret((int32_t)print_bitmap(popu(), landscape));
        break;
    }
    case 0x98: {
        int32_t y = pop(), x = pop();
        U id = popu();
        if (obj_loaded(id, TYPE_SPRITE))
            ret(SpriteIsPicked(hsl(), sprite_of(id), x, y));
        break;
    }
    case 0x99: { /* a random number from lo to hi */
        int32_t hi = pop(), lo = pop();
        if (hi < lo) {
            int32_t t = hi;
            hi = lo;
            lo = t;
        }
        ret(lo + (int32_t)((U)crt_rand() % (U)(hi - lo + 1)));
        break;
    }
    case 0x9b:
        builtin_random_script();
        break;
    case 0xa8:
        builtin_sequence();
        break;
    case 0x9d:
    case 0xf5:
        reg_get_str(HKEY_LOCAL_MACHINE, REG_PATH, 3, argc);
        break;
    case 0xf9:
        reg_get_str(NULL, NULL, 5, argc);
        break;
    case 0xf4: {
        U crypt = argc > 3 ? popu() : 0;
        int32_t *out = pop_ptr();
        char *name = pop_str(), *key = pop_str();
        reg_get_int(HKEY_LOCAL_MACHINE, REG_PATH, key, name, out, crypt);
        break;
    }
    case 0xf8: {
        U crypt = argc > 5 ? popu() : 0;
        int32_t *out = pop_ptr();
        char *name = pop_str(), *key = pop_str(), *path = pop_str();
        reg_get_int((HKEY)(uintptr_t)popu(), path, key, name, out, crypt);
        break;
    }
    case 0xe5:
    case 0xf7: {
        U crypt = argc > 3 ? popu() : 0;
        char *v = pop_str(), *name = pop_str(), *key = pop_str();
        reg_set_string(HKEY_LOCAL_MACHINE, REG_PATH, key, name, (const BYTE *)v, crypt);
        break;
    }
    case 0xfb: {
        U crypt = argc > 5 ? popu() : 0;
        char *v = pop_str(), *name = pop_str(), *key = pop_str(), *path = pop_str();
        reg_set_string((HKEY)(uintptr_t)popu(), path, key, name, (const BYTE *)v, crypt);
        break;
    }
    case 0xf6: {
        U crypt = argc > 4 ? popu() : 0;
        U del = argc > 3 ? popu() : 0;
        U v = popu();
        char *name = pop_str(), *key = pop_str();
        reg_set_int(HKEY_LOCAL_MACHINE, REG_PATH, key, name, v, del, crypt);
        break;
    }
    case 0xfa: {
        U crypt = argc > 6 ? popu() : 0;
        U del = argc > 5 ? popu() : 0;
        U v = popu();
        char *name = pop_str(), *key = pop_str(), *path = pop_str();
        reg_set_int((HKEY)(uintptr_t)popu(), path, key, name, v, del, crypt);
        break;
    }
    case 0xfe: {
        U crypt = popu();
        char *out = pop_str(), *key = pop_str(), *sec = pop_str(), *file = pop_str();
        ini_get_string(file, sec, key, out, crypt);
        break;
    }
    case 0xff: {
        U crypt = popu();
        char *v = pop_str(), *key = pop_str(), *sec = pop_str(), *file = pop_str();
        ini_set_string(file, sec, key, v, crypt);
        break;
    }
    case 0xfc: {
        U crypt = popu();
        int32_t *out = pop_ptr();
        char *key = pop_str(), *sec = pop_str(), *file = pop_str();
        ini_get_int(file, sec, key, out, crypt);
        break;
    }
    case 0xfd: {
        U crypt = popu();
        int32_t v = pop();
        char *key = pop_str(), *sec = pop_str(), *file = pop_str();
        ini_set_int(file, sec, key, v, crypt);
        break;
    }
    case 0x101: {
        char *key = pop_str(), *sec = pop_str(), *file = pop_str();
        ini_delete_key(file, sec, key);
        break;
    }
    case 0x102: {
        char *sec = pop_str(), *file = pop_str();
        ini_delete_section(file, sec);
        break;
    }
    case 0x9e: {
        int32_t mirror = pop();
        ret(ReflectAngle(pop(), mirror));
        break;
    }
    case 0xa0:
        put16(c, 0xb3);
        c[0xa] = 1;
        put32(c + 6, popu());
        put32(c + 2, popu());
        palette_cmd(c);
        break;
    case 0xb3:
        put16(c, 0xb3);
        c[0x10] = (uint8_t)pop();
        c[0xf] = (uint8_t)pop();
        c[0xe] = (uint8_t)pop();
        put32(c + 6, popu());
        put32(c + 2, popu());
        palette_cmd(c);
        break;
    case 0xb4:
        put16(c, 0xb4);
        put32(c + 2, popu());
        palette_load_cmd(c);
        break;
    case 0xa5: { /* a timeout in seconds */
        U secs = popu();
        RU(0x3f3f) = popu();
        if (RU(0x3f3f)) {
            RU(0xb7c) = secs * 1000;
            RU(0xb80) = RU(0xb7c) + timeGetTime();
        }
        break;
    }
    case 0xa6: {
        int32_t dy = pop(), dx = pop();
        SpriteScroll(hsl(), dx, dy, 1);
        break;
    }
    case 0xaa:
    case 0xab: { /* background: a bitmap */
        put16(c, 0xa4);
        if (fn == 0xab) {
            put32(c + 0x1a, popu());
            put32(c + 0x16, popu());
            put32(c + 0x12, popu());
        }
        U v = popu();
        put32(c + 0xa, v);
        put32(c + 6, v);
        bg_cmd(c);
        break;
    }
    case 0xb5:
    case 0xb6: /* background: a palette color */
        put16(c, 0xa4);
        put32(c + 2, 1);
        if (fn == 0xb6) {
            put32(c + 0x1a, popu());
            put32(c + 0x16, popu());
            put32(c + 0x12, popu());
        }
        c[0x10] = (uint8_t)pop();
        bg_cmd(c);
        break;
    case 0xbb:
    case 0xbc: /* background: an RGB color */
        put16(c, 0xa4);
        put32(c + 2, 1);
        bg_rgb(fn, c);
        bg_cmd(c);
        break;
    case 0xad:
    case 0xbd:
    case 0xbf: { /* clip and scroll boxes */
        int32_t r[4];
        r[3] = pop();
        r[2] = pop();
        r[1] = pop();
        r[0] = pop();
        if (fn == 0xbd) {
            RtSetScrollBox(g_rt, r);
        } else if (fn == 0xad) {
            SpriteSetClipBox(hsl(), (const RECT *)r);
        } else {
            U id = popu();
            if (obj_is(id, TYPE_SPRITE))
                SpriteSetSpriteClipBox(sprite_of(id), r);
        }
        break;
    }
    case 0xac: { /* set a character of a string */
        int32_t ch = pop();
        U i = popu(), n = popu();
        if (n & 0x80000000u)
            break;
        char *s = string_get(n);
        if (s && i < 0x200)
            s[i] = (char)ch;
        break;
    }
    case 0xae:
        ret(drag_start(popu()));
        break;
    case 0xb2: {
        U speed = popu(), id = popu();
        if (obj_loaded(id, TYPE_MOVIE))
            movie_set_speed((Movie *)(uintptr_t)obj_entry(id)[2], speed);
        break;
    }
    case 0xb9: {
        U hide = argc > 2 ? popu() : 0;
        U queue = popu(), id = popu();
        ret((int32_t)cmd_set_queue(id, queue, (int)hide));
        break;
    }
    case 0xbe: {
        int32_t y = pop(), x = pop();
        SpriteSetScrollPos(hsl(), x, y);
        break;
    }
    case 0xc7:
        put16(c, 0xb7);
        c[0xa] = (uint8_t)pop();
        put32(c + 6, popu());
        put32(c + 2, popu());
        cmd_save_resource(c);
        break;
    case 0xc8: {
        int32_t dy = pop(), dx = pop();
        SpriteScreenShake(hsl(), dx, dy);
        break;
    }
    case 0xcc: {
        U id = popu();
        ret(obj_loaded(id, TYPE_SPRITE) ? (int32_t)SpriteGetExtraId(sprite_of(id)) : 0);
        break;
    }
    case 0xcd:
    case 0xce: { /* post or send a message to a sprite; the arguments are the message */
        RU(RT_SP) -= argc;
        U id = (U)g_stack[RU(RT_SP)];
        if (!obj_is(id, TYPE_SPRITE))
            break;
        const uint32_t *msg = (const uint32_t *)(g_stack + RU(RT_SP) + 1);
        if (fn == 0xce)
            SpriteSendMessage(hsl(), sprite_of(id), argc, msg);
        else
            SpritePostMessage(hsl(), sprite_of(id), argc, msg);
        RU(RT_SP)--;
        break;
    }
    case 0xcf: {
        U v = popu(), id = popu();
        if (obj_is(id, TYPE_SPRITE))
            SpriteSetExtraId(sprite_of(id), v);
        break;
    }
    case 0xd1:
        ret(SquareRoot(pop()));
        break;
    case 0xd5:
    case 0xd6: {
        char *b = pop_str(), *a = pop_str();
        if (a && b)
            ret(fn == 0xd6 ? lstrcmpiA(a, b) : lstrcmpA(a, b));
        break;
    }
    case 0xd7: {
        char *s = pop_str();
        if (s)
            ret(lstrlenA(s));
        break;
    }
    case 0xd8: { /* where a string is found in another from `start`, or -1 */
        int32_t start = pop();
        char *needle = pop_str(), *hay = pop_str(), *r = NULL;
        if (hay && needle && lstrlenA(hay) > start)
            r = crt_strstr(hay + start, needle);
        ret(r ? (int32_t)(r - hay) : -1);
        break;
    }
    case 0xe1: {
        U id = popu();
        ret(obj_loaded(id, TYPE_SPRITE) && SpriteIsVisible(sprite_of(id)));
        break;
    }
    case 0xe2:
        R(uint8_t, 0x95fe) = (uint8_t)top();
        break;
    case 0xed:
    case 0xee: {
        char *src = pop_str(), *dst = pop_str();
        if (!dst || !src)
            break;
        if (fn == 0xed)
            lstrcatA(dst, src);
        else
            lstrcpyA(dst, src);
        break;
    }
    case 0xef: {
        int32_t n = pop();
        char *src = pop_str(), *dst = pop_str();
        if (n && dst && src)
            lstrcpynA(dst, src, n);
        break;
    }
    case 0x7:
    case 0x8:
    case 0x9:
    case 0xa: /* volume of wave (1), MIDI (2), CD (3) or all (0) */
        put16(c, 0xc6);
        c[3] = fn == 0x9 ? 0 : fn == 0x7 ? 1 : fn == 0x8 ? 2 : 3;
        c[2] = pop() < 0;
        cmd_volume(c);
        break;
    case 0xd:
        put16(c, 0xd);
        c[6] = (uint8_t)pop();
        put32(c + 2, popu());
        cmd_break_loops(c);
        break;
    case 0x15:
    case 0x10e: {
        U b = argc == 4 || fn == 0x15 ? popu() : 0;
        U a = argc >= 3 || fn == 0x15 ? popu() : 0;
        U src = popu(), dst = popu();
        ret(bitmap_merge_objects(dst, src, (int)a, (int)b, fn == 0x10e));
        break;
    }
    case 0x1b:
        cursor_set(popu());
        break;
    case 0x21:
        put16(c, 0x10);
        put32(c + 0x1a, popu());
        put32(c + 0x16, popu());
        put32(c + 0x12, popu());
        put32(c + 0xe, popu());
        click_cmd(c);
        break;
    case 0x24:
        put16(c, 0x10);
        c[0x1e] = 1;
        click_cmd(c);
        break;
    case 0x27:
        put16(c, 0x10);
        put32(c + 2, popu());
        click_cmd(c);
        break;
    case 0x2e:
        put16(c, 0x10);
        c[0x1e] = 1;
        c[pop() ? 0x1f : 0x20] = 1;
        click_cmd(c);
        break;
    case 0x2f:
        put16(c, 0x10);
        c[pop() ? 0x1f : 0x20] = 1;
        put32(c + 0x1a, popu());
        put32(c + 0x16, popu());
        put32(c + 0x12, popu());
        put32(c + 0xe, popu());
        click_cmd(c);
        break;
    case 0x34:
        put16(c, 0x10);
        c[pop() ? 0x1f : 0x20] = 1;
        put32(c + 2, popu());
        click_cmd(c);
        break;
    case 0x7d:
        put16(c, 0x10);
        c[0x1e] = 1;
        put32(c + 6, popu());
        click_cmd(c);
        break;
    case 0x86:
        put16(c, 0x10);
        put32(c + 0xa, popu());
        put32(c + 6, popu());
        put32(c + 2, popu());
        click_cmd(c);
        break;
    case 0x7e:
        click_rects_handler(popu());
        break;
    case 0x7f:
        click_objects_handler(popu());
        break;
    /* key commands; the record's number is left 0 */
    case 0x25:
        put32(c + 2, popu());
        key_cmd(c);
        break;
    case 0x26:
        c[0xc] = (uint8_t)pop();
        c[0xd] = (uint8_t)pop();
        put32(c + 2, popu());
        key_cmd(c);
        break;
    case 0x32:
        c[pop() ? 0xe : 0xf] = 1;
        put32(c + 2, popu());
        key_cmd(c);
        break;
    case 0x33:
        c[pop() ? 0xe : 0xf] = 1;
        c[0xc] = (uint8_t)pop();
        c[0xd] = (uint8_t)pop();
        put32(c + 2, popu());
        key_cmd(c);
        break;
    case 0x81:
        put32(c + 6, popu());
        put32(c + 2, popu());
        key_cmd(c);
        break;
    case 0x82:
        put32(c + 6, popu());
        c[0xc] = (uint8_t)pop();
        c[0xd] = (uint8_t)pop();
        put32(c + 2, popu());
        key_cmd(c);
        break;
    case 0x28:
        builtin_collide();
        break;
    case 0x87:
        builtin_collide_ex();
        break;
    case 0x2d: {
        U hide = popu(), id = popu();
        ret((int32_t)cmd_set_queue(id, 0, (int)hide));
        break;
    }
    case 0xba: {
        U queue = popu(), id = popu();
        ret((int32_t)cmd_set_queue(id, queue, 0));
        break;
    }
    case 0x30:
        put16(c, 0x60);
        c[pop() ? 0xb : 0xc] = 1;
        put32(c + 2, popu());
        cmd_controller_device(c);
        break;
    case 0xb0: {
        put16(c, 0x60);
        put32(c + 6, popu());
        U dev = popu();
        c[0xa] = dev > 5 ? 2 : (uint8_t)dev;
        put32(c + 2, popu());
        cmd_controller_device(c);
        break;
    }
    case 0x80:
        put16(c, 0x61);
        put32(c + 6, popu());
        put32(c + 2, popu());
        ret((int32_t)cmd_controller_handler(c));
        break;
    case 0x35:
        put16(c, 0x2b);
        c[0xe] = (uint8_t)pop();
        put32(c + 2, popu());
        drag_cmd(c);
        break;
    case 0x88:
        put16(c, 0x2b);
        put32(c + 0xa, popu());
        put32(c + 6, popu());
        put32(c + 2, popu());
        drag_cmd(c);
        break;
    case 0x43:
        builtin_date();
        break;
    case 0x53:
        builtin_time();
        break;
    case 0x48:
        builtin_intersect2();
        break;
    case 0xe0:
        builtin_ray_to_rect2();
        break;
    case 0x4d: { /* a sprite's position */
        int32_t *py = pop_ptr(), *px = pop_ptr();
        U id = popu();
        if (!obj_check_type(id, TYPE_SPRITE))
            break;
        uint8_t *s = sprite_of(id);
        int32_t pos[2] = {0, 0};
        if (s)
            SpriteGetPosition(s, pos);
        if (px)
            *px = pos[0];
        if (py)
            *py = pos[1];
        break;
    }
    case 0x51: { /* the bitmap ids of a sprite's (up to four) parts */
        int32_t *p[4];
        for (int i = 3; i >= 0; i--)
            p[i] = pop_ptr();
        U id = popu();
        if (!obj_check_type(id, TYPE_SPRITE))
            break;
        for (int i = 0; i < 4; i++)
            *p[i] = 0;
        if (!obj_loaded(id, TYPE_SPRITE))
            break;
        uint32_t info[0x17];
        SpriteGetInfo(sprite_of(id), info);
        /* the original wrote past its four pointers for sprites of more parts */
        for (U i = 0; i < info[0xb] && i < 4; i++)
            if (info[1 + i])
                *p[i] = *(int32_t *)(uintptr_t)(info[1 + i] + 0x1c);
        break;
    }
    case 0xf1: { /* a sprite's bounds */
        int32_t *b = pop_ptr(), *r = pop_ptr(), *t = pop_ptr(), *l = pop_ptr();
        U id = popu();
        if (!obj_check_type(id, TYPE_SPRITE))
            break;
        uint8_t *s = sprite_of(id);
        if (s) {
            RECT box;
            SpriteGetBounds(hsl(), s, &box);
            *l = box.left;
            *t = box.top;
            *r = box.right;
            *b = box.bottom;
        }
        break;
    }
    case 0x6e: {
        U id = popu();
        timer_remove(id, id == 0xffffffffu);
        break;
    }
    case 0xc5: {
        uint8_t flag = pop() != 0;
        U handler = popu(), interval = popu(), id = popu();
        timer_set(id, handler, interval, flag);
        break;
    }
    case 0x71:
        put16(c, 0x71);
        put32(c + 6, popu());
        put32(c + 2, popu());
        cmd_set_level(c);
        break;
    case 0x7c:
        put16(c, 0x7c);
        put32(c + 0xa, popu());
        put32(c + 6, popu());
        put32(c + 2, popu());
        cmd_offset(c);
        break;
    case 0xb8:
        put16(c, 0xb8);
        put32(c + 0xa, popu());
        put32(c + 6, popu());
        put32(c + 2, popu());
        cmd_set_position(c);
        break;
    case 0x7b:
        AudioMute(audio(), pop() != 0);
        break;
    case 0xdd:
        AudioMute(audio(), -1);
        break;
    case 0x83: {
        U handler = popu();
        ret((int32_t)RU(RT_MOUSE_HANDLER));
        RU(RT_MOUSE_HANDLER) = handler;
        break;
    }
    case 0x8e:
        cmd_cd_track((uint8_t)pop());
        break;
    case 0x90:
        builtin_play_midi();
        break;
    case 0x92:
        builtin_play_wave();
        break;
    case 0x93:
        builtin_play_wave_ex();
        break;
    case 0xcb: {
        put16(c, 0x14);
        c[6] = pop() != 0;
        put32(c + 2, popu());
        cmd_comp_to_background(c);
        break;
    }
    case 0xd0: {
        U destroy = pop() == 0;
        U queue = popu(), to = popu(), from = popu();
        sprite_transfer(from, to, queue, destroy);
        break;
    }
    case 0xd2:
        cd_stop();
        break;
    case 0xd3:
        cmd_stop_midi(popu());
        break;
    case 0xd4: {
        int32_t params[1] = {pop()};
        cmd_stop_wave(popu(), params);
        break;
    }
    case 0xe7:
        R(uint8_t, 0x95fa) = (uint8_t)pop();
        SpriteSetSync(hsl(), R(uint8_t, 0x95fa));
        break;
    case 0x109:
        R(uint8_t, 0x95fa) = pop() == 0;
        SpriteSetSync(hsl(), R(uint8_t, 0x95fa));
        break;
    case 0xe8: {
        U unused = popu();
        res_cache_file(R(ResFile *, RT_RES), pop(), unused);
        break;
    }
    case 0xf2:
        res_drop_cache(R(ResFile *, RT_RES), pop());
        break;
    case 0xe9:
        RU(0x3e6f) = popu();
        break;
    case 0xec:
        RU(0x3f2b) = popu();
        break;
    case 0xea:
        scenes_trim();
        break;
    case 0xeb: {
        U c3 = popu(), c2 = popu(), c1 = popu();
        ret((int32_t)RtSetScreenRes(g_rt, c1, c2, c3));
        break;
    }
    case 0xf0: { /* the full path of the current resource file */
        char *out = string_var(popu());
        if (out)
            MakeFullPath(out, (char *)g_rt + 0x9a54, R(char *, 0x329b));
        break;
    }
    case 0xf3: { /* move the mouse pointer */
        RU(0x8d87) = popu();
        RU(0x8d83) = popu();
        SetCursorPos(RU(0x36c7) + RU(0x8d83), RU(0x36cb) + RU(0x8d87));
        break;
    }
    case 0x100: {
        U priority = popu(), id = popu();
        if (obj_is(id, TYPE_WAVE))
            AudioSetWaveSoundPriority(audio(), obj_entry(id)[0], priority);
        break;
    }
    case 0x103:
        ret((int32_t)(uintptr_t)FileExist(string_var(popu())));
        break;
    case 0x104: { /* 1 when this is the only copy of the game running */
        U n = 0;
        HWND w = NULL;
        for (;;) {
            const char *title = R(char, 0x9904) ? (char *)g_rt + 0x9904 : (char *)g_rt + 0x9d56;
            w = FindWindowExA(NULL, w, "VIEW_CLASS", title);
            if (!w)
                break;
            n++;
        }
        ret(n <= 1);
        break;
    }
    case 0x105: {
        U b = popu(), a = popu();
        if (b && !obj_is(b, TYPE_SPRITE))
            break;
        if (!obj_is(a, TYPE_SPRITE))
            break;
        uint8_t *sb = b ? sprite_of(b) : NULL;
        SpriteCompToSprite(hsl(), (U)(uintptr_t)sprite_of(a), (U)(uintptr_t)sb);
        break;
    }
    case 0x106: { /* assert */
        char *msg = pop_str();
        if (!pop())
            rt_script_error(0x76, (U)(uintptr_t)msg);
        break;
    }
    case 0x107: { /* a message box */
        char *msg = pop_str();
        RtShowGDISurface(g_rt);
        MessageBoxA(R(HWND, RT_HWND), msg, "Studio7", 0);
        RtRestoreFromGDISurface(g_rt);
        break;
    }
    case 0x108: { /* the background's size, or the screen's when there is none */
        int32_t *h = pop_ptr(), *w = pop_ptr();
        uint8_t *bg = R(uint8_t *, RT_BACKGROUND);
        if (bg) {
            if (w)
                *w = P(int32_t, bg, 4);
            if (h)
                *h = P(int32_t, bg, 0xc);
            ret(1);
        } else {
            if (w)
                *w = R(int32_t, RT_SCREEN_W);
            if (h)
                *h = R(int32_t, RT_SCREEN_H);
            ret(0);
        }
        break;
    }
    case 0x10a: {
        U level = popu(), b = popu(), a = popu();
        if (a && !obj_is(a, TYPE_SPRITE))
            break;
        if (b && !obj_is(b, TYPE_SPRITE))
            break;
        uint8_t *sa = a ? sprite_of(a) : NULL, *sb = b ? sprite_of(b) : NULL;
        sprite_set_level_relative(hsl(), (U)(uintptr_t)sa, (U)(uintptr_t)sb, level);
        break;
    }
    case 0x10b: {
        U id = popu();
        SpriteSetTopMost(hsl(), id ? sprite_of(id) : NULL);
        break;
    }
    case 0x10c: /* seconds on the game clock */
        ret((int32_t)((timeGetTime() - RU(0xb84)) / 1000));
        break;
    case 0x10d:
        RU(0xb84) = timeGetTime() - popu() * 1000;
        if (RU(0x3f3f))
            RU(0xb80) = RU(0xb84) + RU(0xb7c);
        break;
    case 0x10f: { /* add a bitmap (or rendered text) to a sprite */
        U bm = popu(), spr = popu();
        U t = res_type(bm);
        if ((t != 1 && t != 0x15) || res_type(spr) != TYPE_SPRITE || !obj_is(spr, TYPE_SPRITE))
            break;
        uint32_t *se = obj_entry(spr);
        if (!se)
            break;
        uint32_t *e = obj_entry(bm);
        if (!e)
            break;
        if (t == 0x15 && !e[0] &&
            (!obj_is(bm, 0x15) || !text_render((uint8_t *)(uintptr_t)obj_entry(bm)[2])))
            break;
        e[1]++;
        SpriteAppendBitmaps(hsl(), (uint8_t *)(uintptr_t)se[1], e, 1);
        break;
    }
    /* numbers that do nothing */
    case 0x4: case 0xe: case 0x10: case 0x11: case 0x13: case 0x14: case 0x17: case 0x1c:
    case 0x23: case 0x2b: case 0x2c: case 0x31: case 0x36: case 0x3a: case 0x3e: case 0x3f:
    case 0x40: case 0x42: case 0x54: case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c:
    case 0x5e: case 0x60: case 0x61: case 0x63: case 0x65: case 0x68: case 0x6c: case 0x6d:
    case 0x70: case 0x72: case 0x78: case 0x7a: case 0x8a: case 0x8f: case 0x91: case 0x95:
    case 0x9a: case 0x9c: case 0x9f: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa7:
    case 0xa9: case 0xaf: case 0xb1: case 0xb7: case 0xc0: case 0xc2: case 0xc3: case 0xc4:
    case 0xc6: case 0xc9: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xde: case 0xe3:
    case 0xe4: case 0xe6:
        break;
    default: /* numbers past the table do nothing */
        break;
    }
}
