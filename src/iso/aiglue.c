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
