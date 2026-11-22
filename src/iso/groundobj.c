/* The game logic, 0x100f3c43-0x100f3e10: the ground object under a structure. */
#include "iso_core.h"
#include "groundobj.h"
#include "objects.h"

#define res_get ORIG(uint32_t *(__cdecl *)(uint32_t, uint32_t), 0x100f48a4)
#define res_type ORIG(int32_t(__cdecl *)(uint32_t, uint32_t), 0x100f4964)
#define sprite_flip ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t, int32_t), 0x100f497c)
#define delete_object ORIG(void(__stdcall *)(uint8_t *, int32_t), 0x100adbde)

/* 0x100f3c43: adds the ground object of the structure's type (+0xc0) as +0x18, replacing
 * one it has only if `replace`. Mirrored wall pieces (classes 0x94-0x9c) mirror a sprite
 * object too. 1 if an object was added */
int32_t __cdecl structure_ground_object(uint8_t *u, int32_t replace)
{
    int32_t obj = P(int32_t, P(uint8_t *, u, 0x32), 0xc0);
    if (obj <= 0) {
        P(int32_t, u, 0x18) = -1;
        return 0;
    }
    if (P(int32_t, u, 0x18) != -1 && !replace)
        return 0;
    uint8_t *table = I(uint8_t *, 0x1930);
    uint32_t flags = P(uint32_t, P(uint8_t *, table, 0), P(int32_t, table, 0x2c) * obj * 4 + 0xc);
    if (replace && P(int32_t, u, 0x18) != -1)
        delete_object(g_iso, P(int32_t, u, 0x18));
    P(int32_t, u, 0x18) = IsoAddObjectC(g_iso, obj, P(int16_t, u, 0x52), P(int16_t, u, 0x54), flags, -1, -1);
    if (P(int32_t, u, 0x18) > 0) {
        uint32_t key = P(uint32_t, I(uint8_t *, 0x192c), P(int32_t, u, 0x18) * 0x12);
        uint32_t s = res_get(I(uint32_t, 0x180e), key)[1];
        if (res_type(I(uint32_t, 0x180e), key) == 0x14 && P(int32_t, u, 0x10) >= 0x94 && P(int32_t, u, 0x10) <= 0x9c)
            sprite_flip(I(uint32_t, 0x181e), s, 1, 0);
    }
    return 1;
}
