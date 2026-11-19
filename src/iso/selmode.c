/* The game logic, 0x10073acc-0x10073f47: the selection modes. A mode installs a handler
 * for the next map click (0xc303, with its arguments 0xc307/0xc30b), which the window
 * handler calls on a left click. The handlers are called with message 2000 when installed
 * and 3000 when replaced. */
#include "iso_core.h"
#include "selmode.h"

typedef void(__cdecl *click_hook)(uint32_t, int32_t, int32_t, uint32_t, uint32_t, int);

#define HOOK I(click_hook, 0xc303)

/* IsoActivateSelectionMode: 1000 picking, 1010 (ends the previous mode first), 1020-1100
 * the other modes (their handlers 0x10073f47-0x100758b9); anything else switches the modes
 * off and gives the map back its normal clicks */
void __cdecl IsoActivateSelectionMode(uint8_t *iso, int mode, uint32_t a, uint32_t b)
{
    static const uint32_t handlers[] = { 0x10073f47, 0x10073f90, 0x10074692, 0x100749fb, 0x10074b32, 0x10075606,
                                         0x10074fa6, 0x1007564f, 0x100758b9, 0x100751ed, 0x100751dd };
    g_iso = iso;
    if (!mode && P(int, iso, 0x134) && P(click_hook, iso, 0xc303))
        P(click_hook, iso, 0xc303)(0, 0, 0, 0, 0, 3000);
    I(int, 0x134) = 1;
    I(uint32_t, 0xc30f) = 0;
    I(uint32_t, 0xc307) = a;
    I(uint32_t, 0xc30b) = b;
    I(uint32_t, 0xc313) = 0;
    I(uint32_t, 0xc317) = 0;
    I(int, 0xd7a3) = 0;
    int k = (mode - 1000) / 10;
    if (mode < 1000 || mode > 1100 || (mode - 1000) % 10) {
        I(int, 0xd7a3) = 1;
        I(int, 0x134) = 0;
        I(uint32_t, 0xc30f) = 0;
        I(uint32_t, 0xc307) = 0;
        I(uint32_t, 0xc30b) = 0;
        I(uint32_t, 0xc313) = 0;
        I(uint32_t, 0xc317) = 0;
        HOOK = 0;
        return;
    }
    if (mode == 1010 && HOOK)
        HOOK(0, 0, 0, 0, 0, 3000);
    HOOK = (click_hook)ORIG_PTR(handlers[k]);
    if (mode != 1000 && mode != 1050)
        HOOK(0, 0, 0, a, b, 2000);
}
