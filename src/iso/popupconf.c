/* The game logic, 0x100d2d70-0x100d2e41: the popup menu's settings from dominion.ini. */
#include "iso_core.h"
#include "popupconf.h"

#define STR(a) ((const char *)ORIG_PTR(a))

/* 0x100d2d70: reads PopupEnabled (+0xdacc, default 1), PopupShowDisabling (+0xdac2, default
 * 1) and PopupClickLength (+0xdac8, default 400, kept within 200-800) */
void __cdecl popup_read_settings(uint8_t *iso)
{
    g_iso = iso;
    I(int32_t, 0xdacc) = GetPrivateProfileIntA(STR(0x10159150), STR(0x10159140), 1, STR(0x10159130));
    I(int32_t, 0xdac2) = GetPrivateProfileIntA(STR(0x10159180), STR(0x1015916c), 1, STR(0x1015915c));
    I(int32_t, 0xdac8) = GetPrivateProfileIntA(STR(0x101591b0), STR(0x1015919c), 0x190, STR(0x1015918c));
    if (I(int32_t, 0xdac8) > 0x320)
        I(int32_t, 0xdac8) = 0x320;
    else if (I(int32_t, 0xdac8) < 0xc8)
        I(int32_t, 0xdac8) = 0xc8;
}
