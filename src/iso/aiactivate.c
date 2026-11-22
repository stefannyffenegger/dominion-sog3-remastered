/* The game logic, 0x100e2a64-0x100e2df0: turning a player over to the computer. */
#include "iso_core.h"
#include "aiactivate.h"

#define AI G(uint8_t *, 0x1018b604)
#define PLAYER(k) (AI + (k) * 0x143 + 0xc)
#define STR(a) ((const char *)ORIG_PTR(a))
#define debug_print ORIG(int32_t(__cdecl *)(int32_t, const char *, ...), 0x10095e98)
#define squad_create ORIG(int32_t(__cdecl *)(int32_t, int32_t, int32_t, int32_t), 0x100c0462)
#define ai_squad_create ORIG(int32_t(__cdecl *)(int32_t, int32_t, int32_t), 0x10092102)
#define attitude_name ORIG(const char *(__cdecl *)(int32_t, char *), 0x1009e26a)
#define task_scan ORIG(void(__cdecl *)(int32_t, int32_t), 0x10061285)
#define task_recon ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x10060d40)
#define task_build ORIG(void(__cdecl *)(int32_t, int32_t), 0x10061415)
#define task_base ORIG(void(__cdecl *)(int32_t, int32_t), 0x100614d7)
#define task_reserves ORIG(void(__cdecl *)(int32_t, int32_t), 0x10062630)
#define task_16 ORIG(void(__cdecl *)(int32_t, int32_t), 0x1006134c)
#define script_24b57 ORIG(void(__cdecl *)(int32_t, int32_t, int32_t), 0x10024b57)
#define script_248c3 ORIG(void(__cdecl *)(int32_t, int32_t), 0x100248c3)
#define script_2478b ORIG(void(__cdecl *)(int32_t, int32_t), 0x1002478b)

/* 0x100e2a64: marks player `k` as set up, of race `race`, with its standing squads (+0x129,
 * +0x121, +0x125, +0x11d) and standing tasks */
int32_t __cdecl ai_player_reset(int32_t k, uint32_t race)
{
    if (!AI)
        return 0;
    uint8_t *p = PLAYER(k);
    debug_print(0x10, STR(0x1015ab64), k, race);
    P(int32_t, p, 2) = 1;
    P(int32_t, p, 6) = 0;
    P(uint8_t, p, 1) = (uint8_t)race;
    P(uint16_t, p, 0x129) = (uint16_t)squad_create(k, 2, 0, 0);
    P(int32_t, p, 0x121) = squad_create(k, 2, 4, 0);
    P(int32_t, p, 0x125) = squad_create(k, 2, 5, 0);
    P(int32_t, p, 0x11d) = squad_create(k, 2, 3, 0);
    ai_squad_create(k, 1, 1);
    P(uint16_t, p, 0x119) = P(uint16_t, p, 0x129);
    task_scan(k, 2);
    task_recon(k, 0x19, 0xa);
    if (!I(uint8_t, 0x106a))
        script_24b57(k, 0, 0);
    task_build(k, 3);
    task_base(k, 4);
    task_reserves(k, 5);
    task_16(k, 0x1e);
    return 1;
}

/* 0x100e2bf6: sets player `k`'s race */
int32_t __cdecl ai_player_race(int32_t k, uint32_t race)
{
    if (!AI)
        return 0;
    P(uint8_t, PLAYER(k), 1) = (uint8_t)race;
    return 1;
}

/* 0x100e2c82: makes player `k` a computer player of race `race` and difficulty `level`
 * (+0x5e); levels 0, 2 and up, and 3 and up set further script values */
int32_t __cdecl ai_player_computer(int32_t k, uint32_t race, int32_t level, int32_t value)
{
    if (!AI)
        return 0;
    uint8_t *p = PLAYER(k);
    char name[16];
    if (!P(int32_t, p, 2))
        ai_player_reset(k, race);
    debug_print(0x10, STR(0x1015ab90), k, race, attitude_name(level, name));
    P(int32_t, p, 6) = 1;
    P(uint8_t, p, 1) = (uint8_t)race;
    P(int32_t, p, 0x5e) = level;
    P(int32_t, p, 0xf2) = value;
    if (!P(int32_t, p, 0x5e))
        script_24b57(k, 0, 0x64);
    if (P(int32_t, p, 0x5e) >= 2) {
        script_24b57(k, 2, 0x23);
        script_248c3(k, 0x17);
        script_248c3(k, 0x21);
    }
    if (P(int32_t, p, 0x5e) >= 3)
        script_2478b(k, 3);
    return 1;
}

/* 0x100e2db2: whether player `k` has been set up (+2) */
int32_t __cdecl ai_player_active(int32_t k)
{
    return AI ? P(int32_t, PLAYER(k), 2) : 0;
}
