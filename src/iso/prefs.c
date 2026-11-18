/* The game logic, 0x1005c5b7-0x1005d140 (the parts every game runs): races, text entry,
 * tab sets, graphics quality, the copy protection, fabs. */
#include "iso_core.h"
#include "prefs.h"

#define orig_rand ORIG(int(__cdecl *)(void), 0x100f4d80)
#define cd_check ORIG(int32_t(__cdecl *)(void), 0x100c6d20) /* returns 1 */

/* 0x1005c5b7: every unit of the player gets type = class + offset (another race's
 * units) */
void __cdecl IsoResetPlayerRace(uint8_t *iso, uint32_t player, int32_t offset)
{
    g_iso = iso;
    for (int i = 0; i < I(int32_t, 0x1b43); i++) {
        uint8_t *u = iso_unit(I(int32_t *, 0xd67a)[i]);
        if (u && P(uint8_t, u, U_PLAYER) == player)
            P(int32_t, u, U_ID_TYPE) = P(int32_t, u, 0x10) + offset;
    }
}

/* 0x1005c971: text entry on (the status line is then left alone) */
void __cdecl IsoSetTextEntryFlag(uint8_t *iso, int32_t on)
{
    g_iso = iso;
    I(int32_t, 0x16c) = on;
}

/* the tab set records: two banks (0x8c apart) of 0x1c-byte records at 0x8ea: a value
 * and five parameters */
#define TAB(bank, set, off) I(int32_t, (bank) * 0x8c + (set) * 0x1c + (off))

/* 0x1005c998: the set's value and parameters (six, from the second bank when the first
 * is unset) */
int32_t __cdecl IsoGetTabsetParms(uint8_t *iso, int32_t set, int32_t *out)
{
    g_iso = iso;
    int bank = TAB(0, set, 0x8ea) == -1;
    for (int i = 0; i < 6; i++)
        out[i] = TAB(bank, set, 0x8ee + i * 4);
    return TAB(bank, set, 0x8ea);
}

/* 0x1005ca73: the set's value and five parameters into a bank (-1: the first unless it
 * is set); value -0x29a clears both banks */
void __cdecl IsoSetTabsetParms(uint8_t *iso, int32_t set, int32_t value, int32_t bank, int32_t *params)
{
    g_iso = iso;
    if (value == -0x29a) {
        for (int b = 0; b < 2; b++) {
            TAB(b, set, 0x8ea) = -1;
            for (int i = 0; i < 5; i++)
                TAB(b, set, 0x8ee + i * 4) = -1;
        }
        return;
    }
    if (bank == -1)
        bank = TAB(0, set, 0x8ea) == -1 ? 0 : 1;
    TAB(bank, set, 0x8ea) = value;
    for (int i = 0; i < 5; i++)
        TAB(bank, set, 0x8ee + i * 4) = params[i];
    TAB(bank, set, 0x902) = bank;
}

/* 0x1005ced1: 1-3, the engine's two quality switches */
void __cdecl IsoSetGraphicsQuality(uint8_t *iso, uint32_t q)
{
    g_iso = iso;
    if (!q)
        q = 1;
    if (q > 3)
        q = 3;
    int32_t a = 1, b = 1;
    if (q == 2) {
        a = 1;
        b = 0;
    } else if (q == 3) {
        a = 0;
        b = 0;
    }
    SpriteSetGraphicsQuality(I(void *, 0x181e), &a, &b, 0, 0);
    I(uint8_t, 0x17f9) = (uint8_t)q;
}

/* 0x1005cfc7: the copy protection, switched off: check 100 asks a function that returns
 * 1, checks 200 and 300 pass after a rand() call (kept: it moves the random numbers) */
int32_t __cdecl IsoCopyProtection(uint8_t *iso, int32_t check)
{
    g_iso = iso;
    if (I(int32_t, 0x170))
        return 1;
    int32_t rc = 0;
    if (check == 100) {
        rc = cd_check();
    } else if (check == 200 || check == 300) {
        orig_rand();
        rc = 1;
    }
    return rc;
}

/* 0x1005d110 and 0x1005d0f0: fabs((double)v), returned on the x87 stack (the second
 * one is a void wrapper that leaves it there) */
long double __cdecl fabs_x87(float v)
{
    union { double d; uint64_t u; } x = {(double)v};
    x.u &= ~(1ull << 63);
    return (long double)x.d;
}
long double __cdecl fabs_x87_wrap(float v)
{
    return fabs_x87(v);
}
