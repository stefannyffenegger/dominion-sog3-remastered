/* The game logic, 0x10024060-0x100241a0: small functions the AI's tables call. */
#include "iso_core.h"
#include "aiglue.h"

#define g_ai_players G(uint8_t *, 0x1018b604) /* 0x143 bytes a player */
#define ai_unit_order ORIG(void(__cdecl *)(int, uint32_t), 0x100562e2)

/* an AI callback: +4 the number of arguments after the first, +8 the function */
struct Callback { int32_t unused, argc; void *fn; };

/* 0x10024060: calls `cb` with `a` and its argc more arguments */
void __cdecl ai_callback(uint32_t a, Callback *cb, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    switch (cb->argc) {
    case 0: ((void(__cdecl *)(uint32_t))cb->fn)(a); break;
    case 1: ((void(__cdecl *)(uint32_t, uint32_t))cb->fn)(a, b); break;
    case 2: ((void(__cdecl *)(uint32_t, uint32_t, uint32_t))cb->fn)(a, b, c); break;
    case 3: ((void(__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t))cb->fn)(a, b, c, d); break;
    case 4: ((void(__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))cb->fn)(a, b, c, d, e); break;
    }
}

/* 0x10024130: only a line in the (silent) debug log */
void __cdecl ai_log(uint32_t a)
{
    (void)a;
}

/* 0x10024153: AI player `p`'s value at 0x6a */
void __cdecl ai_set_6a(int p, uint32_t v)
{
    P(uint32_t, g_ai_players + p * 0x143, 0x6a) = v;
}

/* 0x10024180 */
void __cdecl ai_unit_order_cb(uint32_t unused, int a, uint32_t b)
{
    (void)unused;
    ai_unit_order(a, b);
}

/* AI script commands, 0x100241a0-0x10024acc (names by address: what they stand for
 * is not worked out yet; the debug log lines between are silent in the shipped game) */
#define g_ai_tasks G(uint8_t *, 0x1018b600) /* 0x74 bytes a task */
#define TASK(t, off) P(int32_t, g_ai_tasks + (t) * 0x74, off)
#define ai_group_of ORIG(int(__cdecl *)(uint32_t, int), 0x10092060)
#define ai_name ORIG(void(__cdecl *)(int, char *), 0x1009ddd0)
#define ai_group_take ORIG(void(__cdecl *)(uint32_t, int), 0x100e487c)
#define ai_order_group ORIG(void(__cdecl *)(uint32_t, int, uint32_t, int, uint32_t, int), 0x1009f112)
#define ai_group_done ORIG(void(__cdecl *)(uint32_t), 0x100e4969)
#define ai_group_free ORIG(void(__cdecl *)(uint32_t, int), 0x100921f5)
#define ai_group_free_r ORIG(int(__cdecl *)(uint32_t, int), 0x100921f5)
#define ai_task_first ORIG(int(__cdecl *)(uint32_t, int), 0x100e2ee5)
#define ai_task_next ORIG(int(__cdecl *)(int), 0x1004bb2c)
#define ai_task_add ORIG(int(__cdecl *)(uint32_t, int, int, int, int, int, int), 0x1005d140)
#define ai_62630 ORIG(int(__cdecl *)(int, int), 0x10062630)
#define ai_unit_record ORIG(uint8_t *(__cdecl *)(int), 0x1009c887)
#define ai_61a7e ORIG(int(__cdecl *)(int, int), 0x10061a7e)
#define ai_61ae1 ORIG(int(__cdecl *)(int, int, int), 0x10061ae1)
#define ai_4d8a6 ORIG(int(__cdecl *)(uint32_t, int, int), 0x1004d8a6)
#define ai_638c3 ORIG(int(__cdecl *)(uint32_t, int, uint32_t, int), 0x100638c3)
#define ai_4dd72 ORIG(int(__cdecl *)(uint32_t, uint32_t), 0x1004dd72)

/* the first of AI player p's tasks of kind 7 whose type (0x30) is `type` */
static int task_of_type(uint32_t p, int32_t type)
{
    int t;
    for (t = ai_task_first(p, 7); t != -1 && TASK(t, 0x30) != type; t = ai_task_next(t))
        ;
    return t;
}

/* 0x100241a0 */
void __cdecl ai_script_241a0(uint32_t p, int g, int target, uint32_t a, int b)
{
    char name[0x30];
    int group = ai_group_of(p, g);
    ai_name(target, name);
    if (group == -1)
        return;
    ai_group_take(p, group);
    ai_order_group(p, target, P(uint8_t, g_ai_players + p * 0x143, 0xd), (int)a, (uint32_t)b, 1);
    ai_group_done(p);
}

/* 0x10024267 */
void __cdecl ai_script_24267(uint32_t p, int g, int target, int32_t a, int32_t b)
{
    char name[0x30];
    int group = ai_group_of(p, g);
    ai_name(target, name);
    if (group == -1)
        return;
    ai_group_free(p, group);
    int t = task_of_type(p, 0xb);
    int n = ai_task_add(p, 4, target, 7, t, 0, 4);
    if (n == -1)
        return;
    TASK(n, 0x38) = group;
    TASK(n, 0x54) = a;
    TASK(n, 0x58) = b;
}

/* 0x100243d6 */
void __cdecl ai_script_243d6(uint32_t p, int g, int target)
{
    int group = ai_group_of(p, g);
    if (group == -1)
        return;
    int t = task_of_type(p, 0xb);
    ai_group_free(p, group);
    int n = ai_task_add(p, 4, target, 7, t, 0, 4);
    if (n != -1)
        TASK(n, 0x38) = P(int16_t, g_ai_players + p * 0x143, 0x121);
}

/* 0x100244f6 */
void __cdecl ai_script_244f6(uint32_t p, int target)
{
    char name[0x80];
    int kind = 5;
    ai_name(target, name);
    int t = task_of_type(p, 0xe);
    if (target < 6)
        kind = 6;
    int n = ai_task_add(p, kind, target, 5, t, 0, 10);
    if (n != -1)
        TASK(n, 0x38) = P(int16_t, g_ai_players + p * 0x143, 0x135);
}

/* 0x10024611 */
void __cdecl ai_script_24611(int p)
{
    ai_62630(p, 9);
}

/* 0x1002466e */
void __cdecl ai_script_2466e(uint32_t unused, int n)
{
    (void)unused;
    P(int32_t, ai_unit_record(n - 1), 0xc) = 0;
}

/* 0x100246b2 */
void __cdecl ai_script_246b2(int p, int g)
{
    if (g == 0)
        g = 1;
    int group = ai_group_of((uint32_t)p, g);
    if (g == -10)
        ai_61a7e(p, 10);
    else
        ai_61ae1(p, group, 10);
}

/* 0x1002478b */
void __cdecl ai_script_2478b(uint32_t p, int n)
{
    for (int t = ai_task_first(p, 7); t != -1; t = ai_task_next(t))
        if (!TASK(t, 0x4c))
            n--;
    if (n < 1)
        return;
    while (n) {
        int u = ai_4d8a6(p, 1, 0);
        if (u == -1)
            return;
        ai_638c3(p, u, 0, 4);
        n--;
    }
}

/* 0x100248c3 */
void __cdecl ai_script_248c3(uint32_t p, uint32_t what)
{
    char name[0x80];
    ai_name((int)what, name);
    for (int t = ai_task_first(p, 7); t != -1; t = ai_task_next(t))
        if (TASK(t, 0x30) == 8 && (uint32_t)TASK(t, 0x4c) == what)
            return;
    int u = ai_4dd72(p, what);
    if (u != -1)
        ai_638c3(p, u, what, 4);
}

/* 0x100249e0 */
void __cdecl ai_script_249e0(uint32_t p, int what)
{
    char name[0x80];
    ai_name(what, name);
    int t = ai_task_first(p, 7);
    while (t >= 0) {
        int next = ai_task_next(t);
        if (TASK(t, 0x30) == 8 && TASK(t, 0x4c) == what) {
            TASK(t, 0x40) = -1;
            TASK(t, 0x2c) = 1;
        }
        t = next;
    }
}

#define g_ai_squads G(uint8_t *, 0x1018b608) /* 0x130 bytes a squad */
#define ai_60d40 ORIG(int(__cdecl *)(int, uint32_t, int), 0x10060d40)
#define ai_64919 ORIG(void(__cdecl *)(int, int), 0x10064919)
#define ai_62b12 ORIG(void(__cdecl *)(int, int, int), 0x10062b12)
#define ai_631c5 ORIG(int(__cdecl *)(int, int), 0x100631c5)
#define ai_squad_order ORIG(void(__cdecl *)(int, int, int, int), 0x100645a1)
#define ai_squad_next ORIG(int(__cdecl *)(int), 0x100c038c)
#define ai_squad_mode ORIG(void(__cdecl *)(int, int), 0x10092dfc)

/* 0x10024acc */
void __cdecl ai_script_24acc(uint32_t unused, int n)
{
    (void)unused;
    P(int32_t, ai_unit_record(n - 1), 0xc) = 1;
}

/* 0x10024b10: only a line in the (silent) debug log */
void __cdecl ai_script_24b10(void)
{
}

/* 0x10024b57 */
void __cdecl ai_script_24b57(int p, int32_t a, uint32_t b)
{
    int t;
    for (t = ai_task_first((uint32_t)p, 7); t != -1; t = ai_task_next(t))
        if (TASK(t, 0x30) == 1) {
            TASK(t, 0x50) = a;
            TASK(t, 0x4c) = (int32_t)b;
            return;
        }
    t = ai_60d40(p, b, 10);
    TASK(t, 0x50) = a;
}

/* 0x10024c5c */
void __cdecl ai_script_24c5c(int p)
{
    ai_64919(p, 10);
}

/* 0x10024c8d */
void __cdecl ai_script_24c8d(int p, int what)
{
    char name[0x80];
    ai_name(what, name);
    ai_64919(p, 10);
}

/* 0x10024cd5 */
void __cdecl ai_script_24cd5(int p, int n)
{
    char name[0x84];
    ai_name(n, name);
    if (!n)
        n = 0x14;
    ai_62b12(p, n, 3);
}

/* 0x10024d38 */
void __cdecl ai_script_24d38(int p, int n)
{
    char name[0x84];
    ai_name(n, name);
    if (!n)
        n = 0x14;
    int t = ai_631c5(p, 10);
    if (t != -1)
        TASK(t, 0x50) = n;
}

/* 0x10024dbb: an order with value -v to AI player p's squads: -30 the main squad
 * (0x123), -20 the first, -10 all, otherwise those of type `type` */
void __cdecl ai_script_24dbb(int p, int type, int v)
{
    for (int s = ai_task_first((uint32_t)p, 3); s != -1;) {
        if (type == -30) {
            if (P(int16_t, g_ai_players + p * 0x143, 0x123) == s)
                ai_squad_order(p, s, -v, 1);
            return;
        }
        if (type == -20) {
            ai_squad_order(p, s, -v, 1);
            return;
        }
        if (type == -10)
            ai_squad_order(p, s, -v, 1);
        else if (P(int32_t, g_ai_squads + s * 0x130, 0x18) == type)
            ai_squad_order(p, s, -v, 1);
        s = ai_squad_next(s);
    }
}

/* 0x10024f1a */
void __cdecl ai_script_24f1a(int p, int type, int v)
{
    char name[0x80];
    ai_name(v, name);
    ai_script_24dbb(p, type, -v);
}

/* 0x10024f74: squad mode 1-4 (as 3, 4, 5, 2) for group `g`, or all squads (-10) */
void __cdecl ai_script_24f74(int p, int g, int32_t how)
{
    int group = ai_group_of((uint32_t)p, g);
    int mode;
    switch (how) {
    case 1: mode = 3; break;
    case 2: mode = 4; break;
    case 3: mode = 5; break;
    case 4: mode = 2; break;
    default: mode = 3; break;
    }
    if (g == -10) {
        for (int s = ai_task_first((uint32_t)p, 3); s != -1; s = ai_squad_next(s)) {
            P(int32_t, g_ai_squads + s * 0x130, 300) = mode != 2;
            ai_squad_mode(s, mode);
        }
    } else {
        P(int32_t, g_ai_squads + group * 0x130, 300) = mode != 2;
        ai_squad_mode(group, mode);
    }
}

#define g_ai_units G(uint8_t *, 0x1018b610) /* 0x120 bytes an AI unit entry */
#define ai_squad_unit_next ORIG(int(__cdecl *)(uint32_t, int), 0x1007223b)
#define ai_a03cf ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100a03cf)
#define ai_a0339 ORIG(int(__cdecl *)(int, int, uint32_t), 0x100a0339)
#define unit_busy ORIG(int(__cdecl *)(uint32_t), 0x1009eeb4)
#define ai_616d4 ORIG(void(__cdecl *)(int, int, int, int), 0x100616d4)
#define AIUNIT(u, off) P(int32_t, g_ai_units + (u) * 0x120, off)

/* 0x1002511b */
void __cdecl ai_script_2511b(uint32_t unused, int32_t v)
{
    (void)unused;
    P(int32_t, g_ai_players, 0xa2c) = v;
}

/* 0x10025151: group g's first unit of type 0x14 or 0x19 */
void __cdecl ai_script_25151(uint32_t p, int g)
{
    int group = ai_group_of(p, g);
    if (group == -1)
        return;
    int u = P(int32_t, g_ai_squads + group * 0x130, 0x24);
    while (u != -1 && AIUNIT(u, 0x24) != 0x14 && AIUNIT(u, 0x24) != 0x19)
        u = ai_squad_unit_next(p, u);
    if (u != -1 && AIUNIT(u, 0x20)) {
        ai_a03cf(p, (uint32_t)AIUNIT(u, 0x20));
        P(int16_t, g_ai_players + p * 0x143, 0x123) = (int16_t)ai_group_free_r(p, group);
    }
}

/* 0x1002528c: group g's first idle unit of type `type` */
void __cdecl ai_script_2528c(int p, int g, int type)
{
    char name[0x30];
    ai_name(type, name);
    if (!P(int16_t, g_ai_players + p * 0x143, 0xac + type * 2) ||
        !ai_a0339(p, type, P(uint8_t, g_ai_players + p * 0x143, 0xd)))
        return;
    int group = ai_group_of((uint32_t)p, g);
    if (group == -1)
        return;
    for (int u = P(int32_t, g_ai_squads + group * 0x130, 0x24);; u = ai_squad_unit_next((uint32_t)p, u)) {
        if (u == -1 || !AIUNIT(u, 0x20))
            return;
        if (AIUNIT(u, 0x24) == type && !unit_busy((uint16_t)AIUNIT(u, 0x20))) {
            ai_616d4(p, u, -1, 10);
            return;
        }
    }
}

#define ai_6559f ORIG(int(__cdecl *)(int, int, int), 0x1006559f)
#define ai_6671b ORIG(int(__cdecl *)(int, int, int), 0x1006671b)
#define ai_90223 ORIG(void(__cdecl *)(int, uint32_t, uint32_t, uint32_t, int, int, int), 0x10090223)
#define g_fopen ORIG(void *(__cdecl *)(const char *, const char *), 0x100f5e30)
#define g_fclose ORIG(int(__cdecl *)(void *), 0x100f5e60)
#define g_strcmpi ORIG(int(__cdecl *)(const char *, const char *), 0x100f5680)
#define g_qsort ORIG(void(__cdecl *)(void *, size_t, size_t, void *), 0x100f57d0)

/* 0x1002540b */
void __cdecl ai_script_2540b(int p, int what)
{
    char name[0x80];
    ai_name(what, name);
    ai_6559f(p, what, 0x14);
}

/* 0x1002547b */
void __cdecl ai_script_2547b(int p, int what)
{
    char name[0x80];
    ai_name(what, name);
    ai_6671b(p, what, 0x14);
}

/* 0x100254eb */
void __cdecl ai_script_254eb(uint32_t unused, uint32_t a, int b, uint32_t c, uint32_t d)
{
    (void)unused;
    ai_90223(b, c, a, d, -1, 0, 1);
}

/* 0x10025540: the map editor's AI temp file n (0-8): "_AITemp<n>.txt", new (mode 0),
 * appended (5, 10) or read */
void *__cdecl ai_temp_file(int n, int mode)
{
    char name[20];
    if (n < 0 || n > 8)
        return NULL;
    wsprintfA(name, (const char *)ORIG_PTR(0x101347cc), n);
    if (!mode)
        return g_fopen(name, (const char *)ORIG_PTR(0x101347dc));
    if (mode == 5 || mode == 10)
        return g_fopen(name, (const char *)ORIG_PTR(0x101347e0));
    return g_fopen(name, (const char *)ORIG_PTR(0x101347e4));
}

/* 0x10025600: the map editor's AI defaults per player, and empty temp files */
int __cdecl IsoInitMapEditAITempFiles(void)
{
    for (int i = 0; i < 8; i++) {
        I(int32_t, 0xd9e6 + i * 4) = 1000;
        I(int32_t, 0xda06 + i * 4) = 1000;
        I(int32_t, 0xda26 + i * 4) = 1000;
        I(int32_t, 0xda46 + i * 4) = 10000;
        I(int32_t, 0xda66 + i * 4) = 10000;
        I(int32_t, 0xda86 + i * 4) = 10000;
        I(uint8_t, 0xd9e6 + 0xc0 + i) = 0xd;
    }
    for (int i = 0; i < 9; i++) {
        void *f = ai_temp_file(i, 0);
        if (f)
            g_fclose(f);
    }
    return 1;
}

/* 0x100256ef */
int __cdecl ai_name_compare(const char *a, const char *b)
{
    return g_strcmpi(a, b);
}

/* 0x1002571e: sorts the table of 0xc9 names (0x28 bytes each) at 0x10132860 */
void __cdecl ai_names_sort(void)
{
    g_qsort(ORIG_PTR(0x10132860), 0xc9, 0x28, (void *)ai_name_compare);
}
