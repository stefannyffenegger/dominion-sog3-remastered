/* Engine core, 0x10018ea0-0x10019a62: script command handlers (sprites, time and date,
 * conditions, keys, sound, random numbers). They get the command record with its
 * variable fields already resolved (rt_cmdargs.c). Rewritten from the original. */
#include "rtcore.h"
#include "crt.h"
#include "rt.h"
#include "rt_cmds.h"

#define HSL R(uint8_t *, RT_HSL)
#define F(c, off) P(uint32_t, c, off)
#define SPRITE(id) ((uint8_t *)(uintptr_t)obj_entry(id)[1])
#define VAR(c, off) (*script_var(F(c, off)))

#define RT_AUDIO 0x0c90
#define RES R(ResFile *, RT_RES)
#define RT_SCENE 0x0b88          /* the scene record being started, 0x104 bytes */
#define RT_SCENES 0x329b         /* scene records, 0x10c bytes each */
#define RT_KEYS 0x1947
#define RT_CURRENT_SCENE 0x3dcf
#define RT_3E5F 0x3e5f
#define RT_3E63 0x3e63
#define RT_DRAG_SPRITE 0x3e73
#define RT_WAVE_HANDLER 0x3f2f   /* event whenever a wave is played */
#define RT_SCENE_INDEX 0x3f37
#define RT_GLOBALS 0x3f4f
#define RT_CD_PLAYING 0x95d2
#define RT_COMPOSE_OWNS 0x95dc

#define AudioClearNotifications ORIG(void(__stdcall *)(uint8_t *), 0x1003fa9d)
#define AudioPlayMidi ORIG(void(__stdcall *)(uint8_t *, uint32_t, const void *), 0x1003f2dd)
#define AudioPlayWave ORIG(void(__stdcall *)(uint8_t *, uint32_t, int32_t *), 0x1003fc7b)
#define AudioPlayCDTrack ORIG(int(__stdcall *)(uint8_t *, uint32_t), 0x1004343e)

/* 0x10018ea0: lets a sprite's script loops finish (+2 sprite, +6 how) */
void __stdcall cmd_break_loops(const uint8_t *c)
{
    uint8_t *s = SPRITE(F(c, 2));
    if (s)
        SpriteBreakLoops(s, c[6]);
}

/* 0x10018ee3: draws a sprite (0: all) into the background */
void __stdcall cmd_comp_to_background(const uint8_t *c)
{
    uint8_t *s = NULL;
    if (F(c, 2) && !(s = SPRITE(F(c, 2))))
        return;
    SpriteCompToBackground(HSL, s, c[6]);
}

/* 0x10018f4b */
uint32_t __stdcall cmd_collide(uint8_t *c)
{
    collide_cmd(c);
    return 1;
}

/* 0x10018f6b: writes string +6 to the debug output */
void __stdcall cmd_debug_string(const uint8_t *c)
{
    const char *s = string_get(F(c, 6));
    if (s)
        DebugStr(s);
}

/* 0x10018fb1: starts a new scene (the record is 0x104 bytes; +0x102 forget the composed
 * sprite, +0x103 take the name from the scene table). Asking for the current scene by
 * table ends the game instead (WM_CLOSE). */
void __stdcall cmd_new_scene(uint8_t *c)
{
    if (c[0x103] && R(uint32_t, RT_SCENE_INDEX) == R(uint32_t, RT_CURRENT_SCENE)) {
        PostMessageA(R(HWND, RT_HWND), WM_CLOSE, 1, 0);
        return;
    }
    memcpy(g_rt + RT_SCENE, c, 0x104);
    if (c[0x103])
        memcpy(g_rt + RT_SCENE + 2, R(uint8_t *, RT_SCENES) + R(uint32_t, RT_SCENE_INDEX) * 0x10c, 0x100);
    if (c[0x102])
        compose_reset(R(uint8_t, RT_COMPOSE_OWNS));
    AudioClearNotifications(R(uint8_t *, RT_AUDIO));
    PostMessageA(R(HWND, RT_HWND), 0x4c9, 0, 0);
    memset(g_rt + RT_KEYS, 0, 0x1900);
    R(uint32_t, RT_3E63) = 0;
    R(uint32_t, RT_3E5F) = 0;
    R(uint32_t, RT_DRAG_SPRITE) = 0;
}

/* 0x1001910c: a sprite's bounds into four variables */
void __stdcall cmd_get_bounds(const uint8_t *c)
{
    if (!obj_check_type(F(c, 2), 0x14))
        return;
    uint8_t *s = SPRITE(F(c, 2));
    if (!s)
        return;
    RECT r;
    SpriteGetBounds(HSL, s, &r);
    VAR(c, 8) = r.left;
    VAR(c, 0xc) = r.top;
    VAR(c, 0x10) = r.right;
    VAR(c, 0x14) = r.bottom;
}

/* 0x100191de: the resource ids of a sprite's parts' bitmaps into variables. The first
 * variable is cleared as a global even when it is a local. */
void __stdcall cmd_get_bitmap_ids(const uint8_t *c)
{
    if (!obj_check_type(F(c, 2), 0x14))
        return;
    R(uint32_t, RT_GLOBALS + F(c, 8) * 4) = 0;
    if (!obj_loaded(F(c, 2), 0x14))
        return;
    uint32_t info[24];
    SpriteGetInfo(SPRITE(F(c, 2)), info);
    for (uint32_t i = 0; i < info[11]; i++)
        if (F(c, 8 + i * 4) && info[1 + i])
            VAR(c, 8 + i * 4) = P(int32_t, (uintptr_t)info[1 + i], 0x1c);
}

/* 0x100192c3: the date: +2 weekday, +6 month, +10 day, +0xe year (0: not wanted) */
void __stdcall cmd_get_date(const uint8_t *c)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    if (F(c, 0xe))
        VAR(c, 0xe) = t.wYear;
    if (F(c, 10))
        VAR(c, 10) = t.wDay;
    if (F(c, 6))
        VAR(c, 6) = t.wMonth;
    if (F(c, 2))
        VAR(c, 2) = t.wDayOfWeek;
}

/* 0x1001938e: a sprite's position into two variables (0, 0 when it has no sprite) */
void __stdcall cmd_get_position(const uint8_t *c)
{
    if (!obj_check_type(F(c, 2), 0x14))
        return;
    int32_t pos[2] = {0, 0};
    uint8_t *s = SPRITE(F(c, 2));
    if (s)
        SpriteGetPosition(s, pos);
    VAR(c, 8) = pos[0];
    VAR(c, 0xc) = pos[1];
}

/* 0x10019429: the time: +2 hours, +6 minutes, +10 seconds, +0xe hundredths */
void __stdcall cmd_get_time(const uint8_t *c)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    if (F(c, 2))
        VAR(c, 2) = t.wHour;
    if (F(c, 6))
        VAR(c, 6) = t.wMinute;
    if (F(c, 10))
        VAR(c, 10) = t.wSecond;
    if (F(c, 0xe))
        VAR(c, 0xe) = t.wMilliseconds / 10;
}

/* 0x100195c4: the test of a condition: op bits 1 ==, 2 >, 4 <, 8 |, 0x10 &, 0x20 %, none
 * a != 0; 0x80 negates */
uint32_t __stdcall cond_test(uint8_t op, uint32_t a, uint32_t b)
{
    uint32_t r;
    if (op & 1)
        r = b == a;
    else if (op & 2)
        r = (int32_t)b < (int32_t)a;
    else if (op & 4)
        r = (int32_t)a < (int32_t)b;
    else if (op & 8)
        r = b | a;
    else if (op & 0x10)
        r = b & a;
    else if (op & 0x20)
        r = (uint32_t)((int32_t)a % (int32_t)b);
    else
        r = a != 0;
    if (op & 0x80)
        r = r == 0;
    return r;
}

/* 0x100194fc: if (+0xe op +0x12) run script +2, else +6 (when +10 says there is an else).
 * next gets 0x1a, or 0 when there is no script to run. */
int __stdcall cmd_if(const uint8_t *c, uint32_t *next)
{
    uint32_t script;
    int yes = cond_test(c[0x16], F(c, 0xe), F(c, 0x12)) != 0;
    if (next)
        *next = 0x1a;
    if (yes)
        script = F(c, 2);
    else {
        if (!c[10])
            return 1;
        script = F(c, 6);
    }
    if (!script) {
        if (next)
            *next = 0;
        return 1;
    }
    return script_run(script);
}

/* 0x10019706: the size of the command to go on with: 0x12 (the next one) when the
 * condition holds, else the jump at +2 */
uint32_t __stdcall cmd_if_goto(const uint8_t *c)
{
    return cond_test(c[0xe], F(c, 6), F(c, 10)) ? 0x12 : F(c, 2);
}

/* 0x1001975e: the same with an expression at +10: jump +6 if true, +2 if false */
uint32_t __stdcall cmd_if_expr(const uint8_t *c)
{
    return script_eval(c + 10) ? F(c, 6) : F(c, 2);
}

/* 0x1001979d: whether key +6 is down, into variable +2 */
void __stdcall cmd_key_state(const uint8_t *c)
{
    uint32_t key = F(c, 6);
    if (key >= 0x100)
        return;
    int32_t *v = script_var(F(c, 2));
    *v = GetKeyState(key) < 0;
}

/* 0x1001980b, 0x10019869 */
void __stdcall cmd_set_level(const uint8_t *c)
{
    if (obj_loaded(F(c, 2), 0x14))
        SpriteSetLevel(HSL, SPRITE(F(c, 2)), P(int32_t, c, 6));
}

void __stdcall cmd_offset(const uint8_t *c)
{
    if (obj_loaded(F(c, 2), 0x14))
        SpriteOffset(HSL, SPRITE(F(c, 2)), P(int32_t, c, 6), P(int32_t, c, 10));
}

/* 0x100198c5: plays a CD audio track */
void __stdcall cmd_cd_track(uint8_t track)
{
    R(uint8_t, RT_CD_PLAYING) = (uint8_t)AudioPlayCDTrack(R(uint8_t *, RT_AUDIO), track);
}

/* 0x100198f8: plays MIDI resource +2 with the parameters at +8 */
void __stdcall cmd_play_midi(uint8_t *c)
{
    if (obj_is(F(c, 2), 6))
        AudioPlayMidi(R(uint8_t *, RT_AUDIO), obj_entry(F(c, 2))[0], c + 8);
}

/* 0x1001994c: plays wave resource id and tells the wave handler */
uint32_t __stdcall cmd_play_wave(uint32_t id, int32_t *params)
{
    if (!obj_is(id, 0xc))
        return 0;
    AudioPlayWave(R(uint8_t *, RT_AUDIO), obj_entry(id)[0], params);
    if (R(uint32_t, RT_WAVE_HANDLER))
        event_send(R(uint32_t, RT_WAVE_HANDLER), id, 0);
    return 1;
}

/* 0x100199c7: a random number from +6 to +10 into variable +2, never the same draw
 * twice in a row */
void __stdcall cmd_random(const uint8_t *c)
{
    int32_t lo = P(int32_t, c, 6), hi = P(int32_t, c, 10);
    if (hi < lo) {
        int32_t t = lo;
        lo = hi, hi = t;
    }
    uint32_t n = (uint32_t)(hi - lo) + 1;
    uint32_t r = (uint32_t)crt_rand() % n;
    if (r == G(uint32_t, 0x1007cad8))
        r = (r + 1) % n;
    G(uint32_t, 0x1007cad8) = r;
    *script_var(F(c, 2)) = (int32_t)(r + lo);
}

/* ---- second batch: 0x10019a62-0x1001aa0b ---- */

#define list_count handler_count
#define list_run handler_run_at
#define wave_save res_extract_to_file
#define file_saved wallpaper_set
#define AudioStopMidi ORIG(void(__stdcall *)(uint8_t *, uint32_t), 0x1003f57c)
#define AudioGetWaveVolume ORIG(uint32_t(__stdcall *)(uint8_t *), 0x1004137a)
#define AudioSetWaveVolume ORIG(void(__stdcall *)(uint8_t *, uint32_t), 0x10041446)
#define AudioGetCDVolume ORIG(uint32_t(__stdcall *)(uint8_t *), 0x10042fe6)
#define AudioSetCDVolume ORIG(void(__stdcall *)(uint8_t *, uint32_t), 0x1004359c)
#define AudioGetMidiVolume ORIG(uint32_t(__stdcall *)(uint8_t *), 0x10043aca)
#define AudioSetMidiVolume ORIG(void(__stdcall *)(uint8_t *, uint32_t), 0x10043b7a)
#define AUDIO R(uint8_t *, RT_AUDIO)

/* A random index below n, not the one at *last unless n is 1 */
static uint32_t pick_other(uint32_t n, uint8_t *last)
{
    uint32_t r = (uint32_t)crt_rand() % n;
    if (*last == r && ++r >= n)
        r = 0;
    *last = (uint8_t)r;
    return r;
}

/* 0x10019a62: runs one of several scripts at random: +2 count and scripts at +6, or
 * (+4 set) the entries of list +6. +3 keeps the last pick. */
int __stdcall cmd_random_script(uint8_t *c)
{
    if (!c[4])
        return script_run(F(c, 6 + pick_other(c[2], c + 3) * 4));
    uint32_t list = F(c, 6), n = list_count(list);
    if (!n)
        return 0;
    return list_run(list, pick_other(n, c + 3));
}

/* 0x10019b54: saves resource +2 (a wave, or a bitmap as .bmp) as the file named by
 * string +6 in the Windows directory */
void __stdcall cmd_save_resource(const uint8_t *c)
{
    char *name = (char *)string_get(F(c, 6));
    if (!name)
        return;
    char dir[256], path[256];
    GetWindowsDirectoryA(dir, sizeof dir);
    MakeFullPath(path, dir, name);
    uint32_t id = F(c, 2);
    uint16_t t = ResGetType(RES, id);
    int ok = 0;
    if (t == 3)
        ok = wave_save(path, id);
    else if (t == 1 && obj_is(F(c, 2), 1)) {
        ok = dib_save_bmp((uint8_t *)(uintptr_t)obj_entry(id)[0], path);
        obj_release(F(c, 2), 1);
    }
    if (ok)
        file_saved(path, c[10]);
}

/* 0x10019c89 */
uint32_t __stdcall cmd_set_position(const uint8_t *c)
{
    if (!obj_is(F(c, 2), 0x14))
        return 0;
    uint8_t *s = SPRITE(F(c, 2));
    if (s)
        SpriteSetPosition(HSL, s, P(int32_t, c, 6), P(int32_t, c, 10), 0);
    return 1;
}

/* 0x10019ecd: the number of % conversions in a format; -1 for a % at the end */
int __stdcall format_arg_count(LPCSTR fmt)
{
    int n = 0;
    uint32_t len = (uint32_t)lstrlenA(fmt);
    for (uint32_t i = 0; i < len; i++) {
        if (fmt[i] != '%')
            continue;
        if (i == len - 1)
            return -1;
        if (fmt[i + 1] == '%')
            i++;
        else
            n++;
    }
    return n;
}

/* 0x10019d07: string +2 = format string +6 with +0xc arguments (6 bytes each at +0x10:
 * value, then 1 for a number or 0 for a string id). +9 bit 7: check the count. */
void __stdcall cmd_format(const uint8_t *c)
{
    if (c[5] & 0x80)
        return;
    char *dst = (char *)string_get(F(c, 2));
    const char *fmt = string_get(F(c, 6));
    if (!dst || !fmt)
        return;
    uint32_t n = F(c, 0xc);
    if (!n) {
        lstrcpynA(dst, fmt, 0x200);
        return;
    }
    char *buf = winmalloc(0x400, 0);
    if (!buf)
        return;
    if (c[9] & 0x80) {
        int k = format_arg_count(fmt);
        if ((int)n != k || k == -1) {
            if (!R(uint8_t, 0x95d8)) {
                rt_fatal(0x3f0, 0);
                return; /* (the buffer is not freed) */
            }
            rt_script_error(0x73, 0);
        }
    }
    uint32_t args[12];
    for (uint32_t i = 0; i < n && i < 12; i++) {
        const uint8_t *a = c + 0x10 + i * 6;
        if (a[4])
            args[i] = F(a, 0);
        else {
            const char *s = string_get(F(a, 0));
            args[i] = (uint32_t)(uintptr_t)(s ? s : "<NULL>");
        }
    }
    wvsprintfA(buf, fmt, (va_list)args);
    lstrcpynA(dst, buf, 0x200);
    winfree(buf);
}

/* 0x10019fe1: one step of an arithmetic command: +0 value, +4 operation (0 set, 1 +,
 * 2 -, 3 *, 4 / rounded, 5 |, 6 &, 7 %), +6 negate the value */
uint32_t __stdcall math_step(const uint8_t *e, uint32_t acc)
{
    uint32_t v = F(e, 0);
    if (e[6])
        v = 0u - v;
    switch (e[4]) {
    case 0:
        return v;
    case 1:
        return v + acc;
    case 2:
        return acc - v;
    case 3:
        return v * acc;
    case 4: {
        if (!v)
            return 0;
        int32_t half = (int32_t)v / 2;
        if ((int32_t)v < 0)
            half = -half;
        return (uint32_t)(((int32_t)acc + half) / (int32_t)v);
    }
    case 5:
        return v | acc;
    case 6:
        return v & acc;
    case 7:
        return (uint32_t)((int32_t)acc % (int32_t)v);
    }
    return 0;
}

/* 0x10019f62: variable +2 = variable +2 combined with +6 steps of 8 bytes at +10 */
void __stdcall cmd_math(const uint8_t *c)
{
    uint32_t acc = (uint32_t)*script_var(F(c, 2));
    for (uint32_t i = 0; i < F(c, 6); i++)
        acc = math_step(c + 10 + i * 8, acc);
    *script_var(F(c, 2)) = (int32_t)acc;
}

/* 0x1001a32a: a volume (two 16-bit channels) one step of 31 up or down */
uint32_t __stdcall volume_step(uint32_t vol, int up)
{
    uint32_t l = (vol & 0xffff) >> 11, r = (uint32_t)((int32_t)(vol >> 16) >> 11);
    if (up && l <= 0x1e && r <= 0x1e)
        l++, r++;
    else if (!up && l && r)
        l--, r--;
    return (r * 0xffff) / 0x1f << 16 | ((l * 0xffff) / 0x1f & 0xffff);
}

/* 0x1001a132: volume command: +2 0 up, 1 down, 2 set to +6 (0-127, flag +4); +3 which
 * (0 all, 1 CD, 2 MIDI, 3 wave) */
void __stdcall cmd_volume(const uint8_t *c)
{
    uint32_t set = 0;
    if (c[2] == 2) {
        set = script_value(F(c, 6), c[4]);
        if (set > 0x7f)
            set = 0x7f;
        set = set << 25 | (set & 0x7f) << 9;
    }
    int up = c[2] == 0;
    if (c[3] == 0 || c[3] == 1)
        AudioSetCDVolume(AUDIO, c[2] == 2 ? set : volume_step(AudioGetCDVolume(AUDIO), up));
    if (c[3] == 0 || c[3] == 2)
        AudioSetMidiVolume(AUDIO, c[2] == 2 ? set : volume_step(AudioGetMidiVolume(AUDIO), up));
    if (c[3] == 0 || c[3] == 3)
        AudioSetWaveVolume(AUDIO, c[2] == 2 ? set : volume_step(AudioGetWaveVolume(AUDIO), up));
}

/* 0x1001a3f0: runs the scripts in turn: +2 count at +6, or (+4 set) list +6; +3 the
 * position. A script that is a still unfinished 0xa7 command keeps the position. */
int __stdcall cmd_sequence(uint8_t *c)
{
    if (!c[4]) {
        if (c[2] <= c[3])
            c[3] = 0;
        uint32_t id = F(c, 6 + c[3] * 4);
        if (!obj_is(id, 0x13))
            return 0;
        const uint8_t *s = (const uint8_t *)(uintptr_t)obj_entry(id)[0];
        if (P(int16_t, s, 0) != 0xa7 || s[2] == s[3])
            c[3]++;
        return script_run(id);
    }
    uint32_t list = F(c, 6), n = list_count(list);
    if (!n)
        return 0;
    uint8_t i = c[3]++;
    if (n <= c[3])
        c[3] = 0;
    return list_run(list, i);
}

/* 0x1001a50a: gives sprite object id the script queue `queue` (0: empties it); hide:
 * hide it first */
uint32_t __stdcall cmd_set_queue(uint32_t id, uint32_t queue, int hide)
{
    if (!id)
        return 0;
    if (!queue && !obj_loaded(id, 0x14))
        return 1;
    if (!obj_is(id, 0x14))
        return 0;
    uint8_t *s = SPRITE(id);
    if (s && hide)
        SpriteHide(HSL, s, 1);
    if (!queue) {
        SpriteEmptyQueue(HSL, s);
        return 1;
    }
    if (!obj_is(queue, 0x12))
        return 0;
    uint32_t *q = obj_entry(queue);
    SpriteSetQueue(HSL, s, (const void *)(uintptr_t)q[0], q[1]);
    return 1;
}

/* 0x1001a621: stops MIDI resource id (0: all) */
void __stdcall cmd_stop_midi(uint32_t id)
{
    AudioStopMidi(AUDIO, id ? obj_entry(id)[0] : 0);
}

/* 0x1001a66f: compares strings +6 and +10 into variable +2; command 0xd6 ignores case */
void __stdcall cmd_strcmp(const uint8_t *c)
{
    const char *a = string_get(F(c, 6)), *b = string_get(F(c, 10));
    if (!a || !b)
        return;
    int32_t *v = script_var(F(c, 2));
    *v = P(int16_t, c, 0) == 0xd6 ? lstrcmpiA(a, b) : lstrcmpA(a, b);
}

/* 0x1001a70c: runs entry +10 of list +2; past the end, script +6 if +0xe; +0xf: step
 * the index on (wrapping) */
uint32_t __stdcall cmd_list_entry(uint8_t *c)
{
    uint32_t list = F(c, 2), n = list_count(list);
    if (!n)
        return 0;
    uint32_t i = F(c, 10), r;
    if (i < n)
        r = list_run(list, i);
    else
        r = !c[0xe] || script_run(F(c, 6));
    if (c[0xf])
        F(c, 10) = (i + 1) % n;
    return r;
}

/* The case table of a switch: +6 its offset, +0xe count, entries of 12 bytes (+2 value,
 * +6 jump, +10 value is a variable); +10 the default jump */
static uint32_t switch_jump(const uint8_t *c, uint32_t v)
{
    uint32_t n = P(uint16_t, c, 0xe);
    const uint8_t *t = c + F(c, 6);
    for (uint32_t i = 0; i < n; i++)
        if (script_value(F(t, 2 + i * 12), t[10 + i * 12]) == v)
            return F(t, 6 + i * 12);
    return F(c, 10);
}

/* 0x1001a7d2: switch on value +2 (variable flag +0x11) */
uint32_t __stdcall cmd_switch(const uint8_t *c)
{
    return switch_jump(c, script_value(F(c, 2), c[0x11]));
}

/* 0x1001a88c: switch on the expression at +0x10 */
uint32_t __stdcall cmd_switch_expr(const uint8_t *c)
{
    return switch_jump(c, (uint32_t)script_eval(c + 0x10));
}

/* 0x1001a93f: whether sprite object +6 is shown, into variable +2 */
void __stdcall cmd_is_visible(const uint8_t *c)
{
    uint8_t *s = obj_loaded(F(c, 6), 0x14) ? SPRITE(F(c, 6)) : NULL;
    int32_t *v = script_var(F(c, 2));
    *v = s && SpriteIsVisible(s);
}

/* 0x1001a9cc: like cmd_if_expr */
uint32_t __stdcall cmd_if_expr2(const uint8_t *c)
{
    return script_eval(c + 10) ? F(c, 6) : F(c, 2);
}
