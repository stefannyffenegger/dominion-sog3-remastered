/* Engine core, 0x10018ea0-0x10019a62: script command handlers (sprites, time and date,
 * conditions, keys, sound, random numbers). They get the command record with its
 * variable fields already resolved (rt_cmdargs.c). Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_cmds.h"

#define HSL R(uint8_t *, RT_HSL)
#define F(c, off) P(uint32_t, c, off)
#define SPRITE(id) ((uint8_t *)(uintptr_t)obj_entry(id)[1])
#define VAR(c, off) (*script_var(F(c, off)))

#define RT_AUDIO 0x0c90
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

#define script_run ORIG(int(__stdcall *)(uint32_t), 0x10022ce9)
#define event_send ORIG(int(__stdcall *)(uint32_t, uint32_t, uint32_t), 0x10025e97)
#define string_get ORIG(const char *(__stdcall *)(uint32_t), 0x100259a6)
#define crt_rand ORIG(int(__cdecl *)(void), 0x10028850)
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
