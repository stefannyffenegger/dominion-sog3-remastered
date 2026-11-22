/* The game logic, 0x100ed2ba-0x100ed6fc: the animation of a structure's top object. */
#include "iso_core.h"
#include "structshow.h"

#define HSL I(uint32_t, 0x181e)
#define MSG(off) G(int32_t, 0x101572c0 + (off))
#define MSG_PTR ORIG_PTR(0x101572c0)
#define res_get ORIG(uint32_t *(__cdecl *)(uint32_t, uint32_t), 0x100f48a4)
#define sprite_empty ORIG(void(__stdcall *)(uint32_t, uint32_t), 0x100f48c2)
#define sprite_send ORIG(void(__stdcall *)(uint32_t, uint32_t, int32_t, void *), 0x100f48bc)
#define sprite_post ORIG(void(__stdcall *)(uint32_t, uint32_t, int32_t, void *), 0x100f48ce)
#define sprite_process ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t), 0x100f48da)
#define health_share ORIG(int32_t(__cdecl *)(uint8_t *, int32_t), 0x100a24d0)
#define top_sound ORIG(void(__cdecl *)(uint32_t, int32_t), 0x100ed6fc)
#define OBJ_INFO(k, off)                                                                     \
    P(int32_t, P(uint8_t *, I(uint8_t *, 0x1930), 0), P(int32_t, I(uint8_t *, 0x1930), 0x2c) * (k) * 4 + (off))

/* whether the unit's health is above the share where its top stops */
static int working(uint8_t *u)
{
    return (int16_t)health_share(u, 0) < P(int16_t, u, 0xd6);
}

/* mode 0: the top starts turning from its first frame */
static void top_start(uint32_t id, uint8_t *u, uint32_t s, uint32_t kind)
{
    if (!working(u))
        return;
    sprite_process(HSL, s, 1);
    if (P(int32_t, u, 0x10) == 0x5c &&
        (P(int32_t, u, 0xfa) < 0 || I(uint8_t, 0x10ab + P(int32_t, u, 0xfa) * 43) != 2))
        return;
    sprite_send(HSL, s, 1, ORIG_PTR(0x10157334));
    MSG(4) = 0;
    MSG(8) = OBJ_INFO(kind, 4) - 1;
    MSG(0xc) = OBJ_INFO(kind, 8);
    sprite_post(HSL, s, 4, MSG_PTR);
    sprite_post(HSL, s, 4, ORIG_PTR(0x10157338));
    top_sound(id, 1);
}

/* mode 1: the top winds down from where it is, slower each round, and stops at its last frame */
static void top_stop(uint32_t id, uint8_t *u, uint32_t s, uint32_t kind)
{
    if (!working(u))
        return;
    sprite_process(HSL, s, 1);
    uint32_t last = (uint32_t)(OBJ_INFO(kind, 4) - 1), cur = P(uint32_t, (uint8_t *)(uintptr_t)s, 0x74);
    uint32_t next = cur + 1 > last ? 0 : cur + 1;
    MSG(4) = (int32_t)cur;
    MSG(8) = (int32_t)cur;
    MSG(0xc) = 0;
    sprite_send(HSL, s, 4, MSG_PTR);
    MSG(4) = (int32_t)next;
    MSG(8) = (int32_t)last;
    MSG(0xc) = OBJ_INFO(kind, 8);
    MSG(0xc) += MSG(0xc) / 2;
    sprite_post(HSL, s, 4, MSG_PTR);
    MSG(4) = 0;
    MSG(8) = (int32_t)last;
    MSG(0xc) += MSG(0xc) / 2;
    sprite_post(HSL, s, 4, MSG_PTR);
    MSG(4) = (int32_t)last;
    MSG(8) = (int32_t)last;
    MSG(0xc) = 0;
    sprite_post(HSL, s, 4, MSG_PTR);
    top_sound(id, 0);
}

/* 0x100ed2ba: sets structure `id`'s top object (+0x1c) going (mode 0), winding down (1) or
 * stopped (2). A refinery's top only turns while its slot is in state 2. 0 without
 * animation (+0x11c), else 1 */
int32_t __cdecl structure_show(uint32_t id, int32_t mode)
{
    if (!I(int32_t, 0x11c))
        return 0;
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    int32_t obj = P(int32_t, u, 0x1c);
    if (obj == -1)
        return 1;
    uint8_t *rec = I(uint8_t *, 0x192c) + obj * 0x12;
    uint32_t kind = P(uint16_t, rec, 8);
    uint32_t s = res_get(I(uint32_t, 0x180e), P(uint32_t, rec, 0))[1];
    if (!s)
        return 1;
    sprite_empty(HSL, s);
    if (mode == 0) {
        top_start(id, u, s, kind);
    } else if (mode == 1) {
        top_stop(id, u, s, kind);
    } else if (mode == 2) {
        sprite_process(HSL, s, 0);
        sprite_send(HSL, s, 1, ORIG_PTR(0x10157324));
    }
    return 1;
}
