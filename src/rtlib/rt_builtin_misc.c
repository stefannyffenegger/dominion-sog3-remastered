/* Engine core, 0x1000f130-0x10010000: built-in script functions that take their
 * arguments from the script stack (last argument on top): points on curves and
 * lines, the date and time, collisions, list entries, and random and sequence picks
 * that remember their state per script. Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "math87.h"
#include "rt_builtin_misc.h"

#define RU(off) R(U, off)
#define RT_SP 0x3e4f
#define RT_PICKS 0x3263      /* remembered picks: {id, sequence, random}, 12 bytes each */
#define RT_PICK_COUNT 0x3e2f
#define RT_PICK_ROOM 0x3e33

#define g_stack ((int32_t *)ORIG_PTR(0x1007c408))

static int32_t pop(void)
{
    RU(RT_SP)--;
    return g_stack[RU(RT_SP)];
}
static int32_t *pop_ptr(void) { return (int32_t *)(uintptr_t)(U)pop(); }

static void put16(uint8_t *p, U v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, U v) { put16(p, v), put16(p + 2, v >> 16); }

/* 0x1000f130: (x, y, a..h): the point on a curve */
void __cdecl builtin_curve_point(void)
{
    int32_t v[8];
    for (int i = 7; i >= 0; i--)
        v[i] = pop();
    int32_t *py = pop_ptr(), *px = pop_ptr();
    int32_t out[2];
    SpriteCurvePoint((U)v[0], (U)v[1], (U)v[2], (U)v[3], (U)v[4], (U)v[5], (U)v[6], (U)v[7],
                     (U)(uintptr_t)out);
    *px = out[0];
    *py = out[1];
}

/* 0x1000f2bc: collision test (command 0x17) of two sprites; both 0 tests all */
void __cdecl builtin_collide(void)
{
    uint8_t c[0x20] = {0};
    put16(c, 0x17);
    U b = (U)pop(), a = (U)pop();
    put32(c + 6, b);
    put32(c + 2, a);
    if (!a && !b)
        c[0x16] = 1;
    cmd_collide(c);
}

/* 0x1000f346: (day of week, month, day, year); null pointers are skipped */
void __cdecl builtin_date(void)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    int32_t *year = pop_ptr(), *day = pop_ptr(), *month = pop_ptr(), *dow = pop_ptr();
    if (year)
        *year = t.wYear;
    if (day)
        *day = t.wDay;
    if (month)
        *month = t.wMonth;
    if (dow)
        *dow = t.wDayOfWeek;
}

/* 0x1000f43b: (hour, minute, second, hundredths); null pointers are skipped */
void __cdecl builtin_time(void)
{
    int32_t *hundredths = pop_ptr(), *sec = pop_ptr(), *min = pop_ptr(), *hour = pop_ptr();
    SYSTEMTIME t;
    GetLocalTime(&t);
    if (hour)
        *hour = t.wHour;
    if (min)
        *min = t.wMinute;
    if (sec)
        *sec = t.wSecond;
    if (hundredths)
        *hundredths = t.wMilliseconds / 10;
}

static int clamp(int v, int lo, int hi) { return v < lo ? lo : hi < v ? hi : v; }

/* 0x1000f538: (&x, &y, x, y, angle, x1, y1, x2, y2): where the ray from the point
 * (moved into the rectangle) leaves it */
void __cdecl builtin_ray_to_rect2(void)
{
    int y2 = pop(), x2 = pop(), y1 = pop(), x1 = pop();
    int angle = NormalizeAngle(pop());
    int y = pop(), x = pop();
    int32_t *py = pop_ptr(), *px = pop_ptr();
    int l = x1, r = x2, t = y1, b = y2;
    if (r < x1) {
        l = r;
        r = x1;
    }
    if (b < y1) {
        t = b;
        b = y1;
    }
    x = clamp(x, l, r);
    y = clamp(y, t, b);
    int out[2];
    ray_to_rect(angle, x, y, l, t, r, b, out);
    *px = clamp(out[0], l, r);
    *py = clamp(out[1], t, b);
}

/* 0x1000f798: (&x, &y, two lines): where the lines meet, if they do */
void __cdecl builtin_intersect2(void)
{
    int32_t v[8];
    for (int i = 7; i >= 0; i--)
        v[i] = pop();
    int32_t *py = pop_ptr(), *px = pop_ptr();
    int out[2];
    if (GetIntersect(out, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7])) {
        *px = out[0];
        *py = out[1];
    }
}

/* 0x1000f931: (&x, &y, a..f): the point on a line */
void __cdecl builtin_line_point(void)
{
    int32_t v[6];
    for (int i = 5; i >= 0; i--)
        v[i] = pop();
    int32_t *py = pop_ptr(), *px = pop_ptr();
    int32_t out[2];
    SpriteLinePoint((U)v[0], (U)v[1], (U)v[2], (U)v[3], (U)v[4], (U)v[5], (U)(uintptr_t)out);
    *px = out[0];
    *py = out[1];
}

/* 0x1000fa75: a list entry (command 0xd9); the result replaces the slot below the
 * arguments */
void __cdecl builtin_list_entry(void)
{
    uint8_t c[0x12] = {0};
    put16(c, 0xd9);
    int32_t a = pop();
    put32(c + 6, (U)a);
    c[0xe] = a != 0;
    c[0xf] = pop() != 0;
    put32(c + 0xa, (U)pop());
    put32(c + 2, (U)pop());
    cmd_list_entry(c);
    g_stack[RU(RT_SP) - 1] = P(int32_t, c, 0xa);
}

/* 0x1000fc8a: the remembered pick of `id`, or -1 */
U __stdcall pick_find(U id)
{
    if (!RU(RT_PICK_COUNT))
        return 0xffffffffu;
    for (U i = 0; i < RU(RT_PICK_COUNT); i++)
        if (R(U *, RT_PICKS)[i * 3] == id)
            return i;
    return 0xffffffffu;
}

/* 0x1000fbe0: the remembered random (`which` 0) or sequence (1) state of `id` */
U __stdcall pick_get(U id, U which)
{
    if (!RU(RT_PICK_COUNT) || !RU(RT_PICK_ROOM) || !RU(RT_PICKS))
        return 0;
    U i = pick_find(id);
    if (i == 0xffffffffu)
        return 0;
    return R(U *, RT_PICKS)[i * 3 + (which ? 1 : 2)];
}

/* 0x1000fd15: remembers the state of `id` (the table grows by 100) */
void __stdcall pick_set(U id, U value, U which)
{
    if (!RU(RT_PICK_ROOM)) {
        RU(RT_PICK_COUNT) = 0;
        RU(RT_PICK_ROOM) = 100;
        R(U *, RT_PICKS) = winmalloc(RU(RT_PICK_ROOM) * 12, 1);
    }
    U i = pick_find(id);
    if (i == 0xffffffffu) {
        if (RU(RT_PICK_COUNT) == RU(RT_PICK_ROOM)) {
            RU(RT_PICK_ROOM) += 100;
            R(U *, RT_PICKS) = winrealloc(R(U *, RT_PICKS), RU(RT_PICK_ROOM) * 12, 1);
        }
        R(U *, RT_PICKS)[RU(RT_PICK_COUNT) * 3] = id;
        i = RU(RT_PICK_COUNT)++;
    }
    R(U *, RT_PICKS)[i * 3 + (which ? 1 : 2)] = value;
}

/* runs a pick command (0x9a random, 0xa7 sequence) with the state remembered */
static void pick_run(uint16_t op, U which, int(__stdcall *cmd)(uint8_t *))
{
    uint8_t c[0x1e] = {0};
    put16(c, op);
    c[4] = 1;
    U id = (U)pop();
    put32(c + 6, id);
    c[3] = (uint8_t)pick_get(id, which);
    cmd(c);
    pick_set(id, c[3], which);
}

/* 0x1000fb65: runs a random script of a list */
void __cdecl builtin_random_script(void) { pick_run(0x9a, 0, cmd_random_script); }

/* 0x1000fe6a: runs the next script of a list */
void __cdecl builtin_sequence(void) { pick_run(0xa7, 1, cmd_sequence); }

/* 0x1000fee5: a collision test (command 0x17) with all its settings */
void __cdecl builtin_collide_ex(void)
{
    uint8_t c[0x20] = {0};
    put16(c, 0x17);
    c[0x19] = (uint8_t)pop();
    c[0x1a] = (uint8_t)pop();
    put32(c + 0x12, (U)pop());
    put32(c + 0xe, (U)pop());
    put32(c + 0xa, (U)pop());
    c[0x18] = (uint8_t)pop();
    c[0x17] = (uint8_t)pop();
    put32(c + 6, (U)pop());
    put32(c + 2, (U)pop());
    cmd_collide(c);
}
