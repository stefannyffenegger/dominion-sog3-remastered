/* The game logic, 0x100e1073-0x100e12af: the commands of the mission script to the AI. */
#include <stdarg.h>
#include "iso_core.h"
#include "aibase.h"
#include "aicommand.h"
#include "squadlist.h"

#define debug_print ORIG(int32_t(__cdecl *)(int32_t, const char *, ...), 0x10095e98)
#define base_select ORIG(void(__cdecl *)(int32_t, int32_t), 0x100921f5)
#define fn_100e25ba ORIG(int32_t(__cdecl *)(void), 0x100e25ba)
#define fn_100e25d5 ORIG(int32_t(__cdecl *)(int32_t), 0x100e25d5)
#define fn_100e2727 ORIG(int32_t(__cdecl *)(int32_t), 0x100e2727)
#define fn_100e315a ORIG(void(__cdecl *)(void), 0x100e315a)
#define fn_100e4969 ORIG(void(__cdecl *)(int32_t), 0x100e4969)
#define fn_100f4430 ORIG(void(__cdecl *)(int32_t), 0x100f4430)
#define fn_100f44db ORIG(void(__cdecl *)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t), 0x100f44db)
#define fn_100f457c ORIG(void(__cdecl *)(void), 0x100f457c)

/* 0x100e1073: one AI command with its arguments; nothing in a network game (0). 2: select a
 * player's new base; 3: end it (always for player 0, see HIDDEN_AND_UNUSED); 4: a new squad;
 * 5-8: the rule records; 10, 13: load and save the databases; 14: 0x100e315a. An unknown
 * command returns 0, the others 1 or their result */
int32_t __cdecl ai_command(int32_t cmd, ...)
{
    if (I(int32_t, 0x175))
        return 0;
    int32_t result = 1, player = 0;
    va_list ap;
    va_start(ap, cmd);
    switch (cmd) {
    case 2: {
        player = va_arg(ap, int32_t);
        int32_t base = va_arg(ap, int32_t);
        base_select(player, ai_squad_new(player, (uint32_t)base, 0));
        break;
    }
    case 3:
        /* original: `player` is a local of this call, so this is always player 0 */
        base_select(player, -1);
        break;
    case 4:
        player = va_arg(ap, int32_t);
        squad_new(player, 2, 1, 0);
        break;
    case 5:
        fn_100e4969(fn_100e25ba());
        break;
    case 6:
        fn_100f4430(va_arg(ap, int32_t));
        break;
    case 7:
        fn_100f457c();
        break;
    case 8: {
        int32_t k = va_arg(ap, int32_t), b = va_arg(ap, int32_t), c = va_arg(ap, int32_t);
        int32_t d = va_arg(ap, int32_t), e = va_arg(ap, int32_t);
        fn_100f44db(G(int32_t, 0x101594d8 + k * 8), G(int32_t, 0x101594dc + k * 8), b, c, d, e);
        break;
    }
    case 10:
        result = fn_100e2727(va_arg(ap, int32_t));
        break;
    case 13:
        result = fn_100e25d5(va_arg(ap, int32_t));
        debug_print(0x1d, (const char *)ORIG_PTR(0x1015a6a4), result);
        break;
    case 14:
        fn_100e315a();
        break;
    default:
        result = 0;
        break;
    }
    va_end(ap);
    return result;
}
