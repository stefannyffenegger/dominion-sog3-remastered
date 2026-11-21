/* The game logic, 0x100b6e66-0x100b72e6: turning a unit to face a direction (a one-point
 * vector with the direction, put in front of its path). */
#include "iso_core.h"
#include "sector.h"
#include "unitface.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
/* the original reads the unit record anew each time */
#define U (I(uint8_t **, ISO_UNITS)[unit & 0xffff])
#define vector_new ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t), 0x100e98de)
#define vector_link ORIG(void(__cdecl *)(int32_t, int32_t, int), 0x100d4450)
#define net_delay ORIG(int(__cdecl *)(void), 0x1003f390)
#define unit_path_follow ORIG(void(__cdecl *)(uint32_t, int32_t, uint32_t), 0x100e9260)
#define unit_path_join ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t, int), 0x100d4bfa)
#define net_send_face ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t), 0x100331ab)

/* the player byte, with the record pointer's upper half above it as the original passes it */
static uint32_t owner_arg(const uint8_t *u)
{
    return ((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint8_t, u, 0x14);
}

/* 0x100b6e66: turns unit `unit` to face direction `dir` (the facing command); a type that
 * cannot turn on its own (+0xb4) only when `any`. In a network game only for the local
 * player's units (or when `forced`), and the others are told. 0 when nothing was done */
int32_t __cdecl unit_face(uint32_t unit, uint32_t dir, int32_t any, int32_t forced)
{
    if (I(int32_t, 0x175) && !unit_is_local(unit) && !forced)
        return 1;
    if (P(uint32_t, U, 0x10e) >> 12 & 1)
        return 0;
    if (P(int32_t, P(uint8_t *, U, 0x32), 0xb4) && !any)
        return 0;
    if (P(int16_t, U, 0x113) > 0)
        return 0;
    if (P(uint8_t, U, 0x79) == (uint8_t)dir)
        return 0;
    int16_t nv = (int16_t)vector_new(owner_arg(U), P(uint32_t, U, 0xc));
    if (nv != -1) {
        P(int16_t, VEC(nv), 4) = P(int16_t, U, 0x52);
        P(int16_t, VEC(nv), 6) = P(int16_t, U, 0x54);
        P(uint8_t, VEC(nv), 9) = (uint8_t)dir;
        P(uint8_t, U, 0x195) |= 1;
        if (I(int32_t, 0x175))
            P(uint32_t, VEC(nv), 0) = net_delay() + I(uint32_t, 0x17dd);
        unit_path_follow(unit, nv, 0);
        vector_link(nv, -3, 1);
        /* and a waypoint where it stands, joined to its path */
        nv = (int16_t)vector_new(owner_arg(U), P(uint32_t, U, 0xc));
        if (nv != -1) {
            P(uint8_t, VEC(nv), 8)++;
            P(int16_t, VEC(nv), 4) = P(int16_t, U, 0x52);
            P(int16_t, VEC(nv), 6) = P(int16_t, U, 0x54);
            unit_path_join(unit, P(int16_t, U, 0x11b), nv, 1);
            if (P(int16_t, U, 0x11b) <= 0)
                P(int16_t, U, 0x11b) = P(int16_t, U, 0x119);
        }
    }
    if (I(int32_t, 0x175)) {
        if (!P(int32_t, U, 0x149))
            P(uint32_t, U, 0x149) = net_delay() + I(uint32_t, 0x17dd);
        net_send_face(unit & 0xffff, dir, any);
    }
    return 1;
}
