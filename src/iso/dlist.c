/* The game logic, 0x100c9cf0-0x100ca41e: a doubly linked list library (the terrain analysis
 * and the AI use it) and the AI process exports. A list is {count, first, last}; a node is
 * {next, prev, free function} followed by its data, and the functions hand out pointers to
 * the data (node + 0xc). Memory comes from GlobalAlloc(GPTR). */
#include "iso_core.h"
#include "dlist.h"

#define NODE(data) ((uint8_t *)(data) - 0xc)
#define NEXT(n) P(uint8_t *, n, 0)
#define PREV(n) P(uint8_t *, n, 4)
#define FIRST(l) P(uint8_t *, l, 4)
#define LAST(l) P(uint8_t *, l, 8)
#define COUNT(l) P(int32_t, l, 0)
#define orig_memcpy ORIG(void *(__cdecl *)(void *, const void *, size_t), 0x100f5530)

/* The original calls the free and visit functions without cleaning the stack after them,
 * so they may be cdecl or stdcall: these call them in a frame of their own that restores
 * the stack either way. */
int __cdecl dlist_call1(void *fn, void *a);
int __cdecl dlist_call3(void *fn, void *a, void *b, void *c);
__asm__(".text\n"
        "_dlist_call1:\n"
        "\tpushl %ebp\n\tmovl %esp, %ebp\n"
        "\tpushl 12(%ebp)\n\tcall *8(%ebp)\n"
        "\tleave\n\tret\n"
        "_dlist_call3:\n"
        "\tpushl %ebp\n\tmovl %esp, %ebp\n"
        "\tpushl 20(%ebp)\n\tpushl 16(%ebp)\n\tpushl 12(%ebp)\n\tcall *8(%ebp)\n"
        "\tleave\n\tret\n");

/* 0x100c9d89: links node `n` into list `l`: `where` 0 at the front, 1 at the back,
 * otherwise after the node of data `where` */
void __cdecl dlist_link(uint8_t *l, uint8_t *n, uintptr_t where)
{
    if (!FIRST(l)) {
        PREV(n) = 0;
        FIRST(l) = n;
        LAST(l) = n;
    } else if (where == 0) {
        NEXT(n) = FIRST(l);
        PREV(n) = 0;
        PREV(FIRST(l)) = n;
        FIRST(l) = n;
    } else if (where == 1) {
        NEXT(n) = 0;
        PREV(n) = LAST(l);
        NEXT(LAST(l)) = n;
        LAST(l) = n;
    } else {
        uint8_t *after = NODE(where);
        NEXT(n) = NEXT(after);
        PREV(n) = after;
        NEXT(after) = n;
        if (NEXT(n))
            PREV(NEXT(n)) = n;
        else
            LAST(l) = n;
    }
}

/* 0x100c9cf0: a new node of `size` bytes in list `l` (where: see dlist_link), filled from
 * `src` or with zeros, and with free function `free_fn`; its data, or 0 */
void *__cdecl dlist_add(uint8_t *l, uintptr_t where, uint32_t size, const void *src, void *free_fn)
{
    uint8_t *n = GlobalAlloc(GPTR, size + 0xc);
    if (!n)
        return 0;
    COUNT(l)++;
    dlist_link(l, n, where);
    P(void *, n, 8) = free_fn;
    uint8_t *data = n + 0xc;
    if (src)
        orig_memcpy(data, src, size);
    else
        memset(data, 0, size);
    return data;
}

/* 0x100c9e84: empties list `l`, calling each node's free function */
void __cdecl dlist_clear(uint8_t *l)
{
    uint8_t *n;
    while ((n = FIRST(l))) {
        if (P(void *, n, 8))
            dlist_call1(P(void *, n, 8), n + 0xc);
        FIRST(l) = NEXT(n);
        if (FIRST(l))
            PREV(FIRST(l)) = 0;
        GlobalFree(n);
        COUNT(l)--;
    }
}

/* 0x100c9f52: takes node `n` out of list `l` */
void __cdecl dlist_unlink(uint8_t *l, uint8_t *n)
{
    if (FIRST(l) == n) {
        if (NEXT(n))
            PREV(NEXT(n)) = 0;
        else
            LAST(l) = 0;
        FIRST(l) = NEXT(n);
    } else if (LAST(l) == n) {
        NEXT(PREV(n)) = 0;
        LAST(l) = PREV(n);
    } else {
        PREV(NEXT(n)) = PREV(n);
        NEXT(PREV(n)) = NEXT(n);
    }
}

/* 0x100c9efd: removes the node of data `data` from list `l` and frees it */
void __cdecl dlist_remove(uint8_t *l, void *data)
{
    uint8_t *n = NODE(data);
    dlist_unlink(l, n);
    if (P(void *, n, 8))
        dlist_call1(P(void *, n, 8), n + 0xc);
    GlobalFree(n);
    COUNT(l)--;
}

/* 0x100c9fef: calls `fn(l, data, arg)` for each node until it returns 0 */
void __cdecl dlist_each(uint8_t *l, void *fn, void *arg)
{
    if (!fn)
        return;
    for (uint8_t *n = FIRST(l), *next; n; n = next) {
        next = NEXT(n);
        if (!dlist_call3(fn, l, n + 0xc, arg))
            return;
    }
}

/* 0x100ca056: whether `data` is in list `l` */
int32_t __cdecl dlist_contains(uint8_t *l, void *data)
{
    for (uint8_t *n = FIRST(l); n; n = NEXT(n))
        if (n + 0xc == data)
            return 1;
    return 0;
}

/* 0x100ca0a9 */
void *__cdecl dlist_first(uint8_t *l)
{
    return FIRST(l) ? FIRST(l) + 0xc : 0;
}

/* 0x100ca0e4 */
void *__cdecl dlist_last(uint8_t *l)
{
    return LAST(l) ? LAST(l) + 0xc : 0;
}

/* 0x100ca11f: moves the node of data `data` (where: see dlist_link; nothing when it is
 * there already) */
void __cdecl dlist_move(uint8_t *l, void *data, uintptr_t where)
{
    uint8_t *n = NODE(data);
    if (where == 0) {
        if (FIRST(l) == n)
            return;
    } else if (where == 1) {
        if (LAST(l) == n)
            return;
    } else if (PREV(n) == NODE(where) || where == (uintptr_t)data) {
        return;
    }
    dlist_unlink(l, n);
    dlist_link(l, n, where);
}

/* 0x100ca1c8: the data after `data` (0: the first), or 0 */
void *__cdecl dlist_next(uint8_t *l, void *data)
{
    if (!data)
        return dlist_first(l);
    uint8_t *n = NODE(data);
    return NEXT(n) ? NEXT(n) + 0xc : 0;
}

/* 0x100ca225: the data before `data` (0: the last), or 0 */
void *__cdecl dlist_prev(uint8_t *l, void *data)
{
    if (!data)
        return dlist_last(l);
    uint8_t *n = NODE(data);
    return PREV(n) ? PREV(n) + 0xc : 0;
}

/* 0x100ca284 */
int32_t __cdecl dlist_count(uint8_t *l)
{
    return COUNT(l);
}

/* (0x100ca299 and 0x100ca2a9 are empty functions, left original) */

/* 0x100ca2b9: a new empty list, or 0 */
void *__cdecl dlist_new(void)
{
    uint8_t *l = GlobalAlloc(GPTR, 0x10);
    if (!l)
        return 0;
    COUNT(l) = 0;
    FIRST(l) = 0;
    LAST(l) = 0;
    return l;
}

/* 0x100ca30a */
void __cdecl dlist_free(uint8_t *l)
{
    if (!l)
        return;
    dlist_clear(l);
    GlobalFree(l);
}

/* ---- the AI process exports ---- */

#define ai_tick ORIG(void(__cdecl *)(void), 0x100e3077)

/* 0x100ca340: the AI runs every 30 ticks from now on */
int32_t __stdcall IsoInitAIProcessC(uint8_t *iso)
{
    g_iso = iso;
    I(int32_t, 0xd837) = 0x1e;
    I(uint32_t, 0xd83b) = timeGetTime();
    return 1;
}

/* 0x100ca380 */
void __stdcall IsoStopAIProcessC(uint8_t *iso)
{
    g_iso = iso;
    I(int32_t, 0xd837) = 0;
}

/* 0x100ca3a9: one AI turn, unless the game is paused (+0x17e) or minimised (+0x180) */
void __stdcall IsoAILoopC(uint8_t *iso)
{
    g_iso = iso;
    if (!I(uint8_t, 0x17e) && !I(uint8_t, 0x180))
        ai_tick();
}

/* 0x100ca3f7 */
void __stdcall IsoSetAIDelayC(uint8_t *iso, int32_t delay)
{
    g_iso = iso;
    I(int32_t, 0xd837) = delay;
}
