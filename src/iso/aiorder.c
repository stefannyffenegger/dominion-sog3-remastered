/* The game logic, 0x100f4430-0x100f4599: building an order record for the computer player. */
#include "iso_core.h"
#include "aiorder.h"
#include "ailists.h"
#include "aislots.h"

#define COUNT G(int32_t, 0x1018b7a8)
#define KIND G(int32_t, 0x1018b624)
#define PLAYER G(int32_t, 0x1018b61c)
#define ENTRY(k, off) G(int32_t, 0x1018b628 + (k) * 24 + (off))

/* 0x100f4430: starts a record for `player`, with its 16 entries empty. For -1, resets it
 * once for each computer player, so the last of them stays */
void __cdecl ai_order_begin(int32_t player)
{
    COUNT = 0;
    if (player != -1) {
        KIND = 1;
        PLAYER = player;
        for (int k = 0; k < 0x10; k++)
            ENTRY(k, 0) = -1;
        return;
    }
    for (int32_t p = 0; p < 8; p++)
        if (player_is_ai(p))
            ai_order_begin(p);
}

/* 0x100f44db: adds an entry of six values. 0 when all 16 are taken */
int32_t __cdecl ai_order_add(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    if (COUNT >= 0x10)
        return 0;
    ENTRY(COUNT, 0) = a;
    ENTRY(COUNT, 4) = b;
    ENTRY(COUNT, 8) = c;
    ENTRY(COUNT, 0xc) = d;
    ENTRY(COUNT, 0x10) = e;
    ENTRY(COUNT, 0x14) = f;
    COUNT++;
    return 1;
}

/* 0x100f457c: hands the finished record to the AI's list */
uint8_t *__cdecl ai_order_send(void)
{
    return ai_record_add((const uint8_t *)ORIG_PTR(0x1018b618));
}
