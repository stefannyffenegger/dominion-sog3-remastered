/* The game logic, 0x100ce796-0x100ce8a6: how many cells a straight leg crosses. */
#include "iso_core.h"
#include "patrolcells.h"

#define crt_abs ORIG(int(__cdecl *)(int), 0x100f4f80)
#define aim_round ORIG(int32_t(__cdecl *)(float), 0x1008cc80)

/* 0x100ce796: the number of cells between points `a` and `b` (each two shorts packed in a
 * dword) along direction `dir`: the x distance over 64 east and west, over 32 on the
 * diagonals, the y distance over 32 north and south (rounded by 0x1008cc80); 0 for no
 * direction */
int32_t __cdecl cells_between(uint32_t a, uint32_t b, uint32_t dir)
{
    int16_t ax = (int16_t)a, ay = (int16_t)(a >> 16), bx = (int16_t)b, by = (int16_t)(b >> 16);
    switch ((uint8_t)dir) {
    case 0:
    case 4:
        return aim_round((float)((long double)crt_abs(ax - bx) / G(float, 0x101132bc)));
    case 1:
    case 3:
    case 5:
    case 7:
        return aim_round((float)((long double)crt_abs(ax - bx) / G(float, 0x101132c0)));
    case 2:
    case 6:
        return aim_round((float)((long double)crt_abs(ay - by) / G(float, 0x101132c0)));
    default:
        return 0;
    }
}
