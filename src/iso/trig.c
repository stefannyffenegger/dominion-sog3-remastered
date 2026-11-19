/* The game logic, 0x1006fd69-0x10070c3b: angles in tenths of a degree (0-3599): the
 * direction from a vector, sine and cosine from a quarter-wave table, a few bit fields, and
 * loading the player colour map. */
#include "iso_core.h"
#include "trig.h"

#define crt_atan2 ORIG(long double(__cdecl *)(double, double), 0x100f74a8)
#define SINE ((uint16_t *)ORIG_PTR(0x10140760)) /* sin * 10000 for 0-90 degrees in tenths */

/* 0x1007042c: the direction of (dx, dy) as the game's sprites face: atan2 in tenths of a
 * degree, turned by 90 degrees and mirrored, then bent so that the eight main directions
 * fall on the isometric view's (steps of 1.5 and 0.75) */
int32_t __stdcall iso_angle(uint8_t *iso, int32_t dx, int32_t dy)
{
    g_iso = iso;
    int32_t a = (int32_t)(int64_t)(crt_atan2((double)dx, (double)dy) * (long double)G(double, 0x101130c0));
    if (a < 0)
        a += 0xe10;
    a -= 900;
    if (a < 0)
        a += 0xe10;
    a -= 0xe10;
    if (a < 0)
        a = -a;
    if (a == 0xe10)
        a = 0;
    if (a < 300)
        return (int32_t)((long double)a * 1.5L);
    if (a < 900)
        return (int32_t)((long double)(a - 300) * 0.75L) + 450;
    if (a < 1500)
        return (int32_t)((long double)(a - 900) * 0.75L) + 900;
    if (a < 1800)
        return (int32_t)((long double)(a - 1500) * 1.5L) + 1350;
    if (a < 2100)
        return (int32_t)((long double)(a - 1800) * 1.5L) + 1800;
    if (a < 2700)
        return (int32_t)((long double)(a - 2100) * 0.75L) + 2250;
    if (a < 3300)
        return (int32_t)((long double)(a - 2700) * 0.75L) + 2700;
    return (int32_t)((long double)(a - 3300) * 1.5L) + 3150;
}

/* 0x10070380: cosine * 10000 */
int32_t __stdcall iso_cos(uint8_t *iso, int32_t a)
{
    g_iso = iso;
    a = NormalizeAngle(a - 900);
    int32_t k = a % 900;
    int upper = a > 0x707;
    if (upper)
        a -= 0x708;
    if (a > 899)
        k = 900 - k;
    int32_t v = SINE[k];
    return upper ? v : -v;
}

/* 0x10070757: sine * 10000 */
int32_t __stdcall iso_sin(uint8_t *iso, int32_t a)
{
    g_iso = iso;
    a = NormalizeAngle(a);
    int upper = a > 0x707;
    if (upper)
        a -= 0x708;
    int32_t k = a % 900;
    if (a > 899)
        k = 900 - k;
    int32_t v = SINE[k];
    return upper ? -v : v;
}

/* 0x100708b6: clears bit 3 */
void __cdecl flags_clear_bit3(uint32_t *f) { *f &= 0xfffffff7; }

/* 0x10070a36: sets bits 16-27 */
void __cdecl flags_set_field16(uint32_t *f, int16_t v) { *f = ((uint32_t)(int32_t)v & 0xfff) << 16 | (*f & 0xf000ffff); }

/* 0x10070a64: clears bits 16-27 */
void __cdecl flags_clear_field16(uint32_t *f) { *f &= 0xf000ffff; }

/* 0x10070b60: loads COLORMAP.DAT (0xc6000 bytes, the player colour tables) and reads
 * [Dominion] NetColorMode. 0 if it cannot */
int32_t __cdecl colormap_load(void)
{
    G(char *, 0x10140e70) = winmalloc(0xc6000, 0);
    if (!G(char *, 0x10140e70))
        return 0;
    HMMIO h = mmioOpenA((char *)ORIG_PTR(0x101464f8), 0, 0);
    if (!h)
        return 0;
    mmioRead(h, G(char *, 0x10140e70), 0xc6000);
    mmioClose(h, 0);
    I(char, 0xe098) = (char)GetPrivateProfileIntA((const char *)ORIG_PTR(0x10146528),
                                                  (const char *)ORIG_PTR(0x10146518), 0,
                                                  (const char *)ORIG_PTR(0x10146508));
    return 1;
}
