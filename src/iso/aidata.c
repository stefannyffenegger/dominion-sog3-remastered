/* The game logic, 0x100e1e48-0x100e2095: starting the computer player's data for a map. */
#include "iso_core.h"
#include "aidata.h"

#define AI G(uint8_t *, 0x1018b604)
#define AI_TIMING G(int32_t, 0x1017c9e0)
#define STR(a) ((char *)ORIG_PTR(a))
#define orig_malloc ORIG(uint8_t *(__stdcall *)(uint32_t, int32_t), 0x100f4976)
#define orig_atoi ORIG(int(__cdecl *)(const char *), 0x100f6260)
#define ai_tables_init ORIG(void(__cdecl *)(void), 0x100e2095)
#define ai_player_init ORIG(void(__cdecl *)(int32_t), 0x100e2832)
#define debug_print ORIG(int32_t(__cdecl *)(int32_t, const char *, ...), 0x10095e98)

/* 0x100e1e48: whether the AI's block exists */
int32_t __cdecl ai_ready(void)
{
    return AI != 0;
}

/* 0x100e1e80: reads the optional `AITiming` setting from Dominion.ini and AISTRUCT.BLD
 * (up to 0xa200 bytes, into 0x10181400), allocates the AI's block (0xa4c bytes), notes the
 * map's sector table in it, sets up its tables and the 8 players. 0 on failure */
int32_t __cdecl ai_map_loaded(void)
{
    char buf[12];
    AI_TIMING = (int32_t)GetPrivateProfileStringA(STR(0x1015aa20), STR(0x1015aa14), STR(0x1015aa10), buf, 10,
                                                  STR(0x1015aa00));
    AI_TIMING = buf[0] ? orig_atoi(buf) : 0;
    HMMIO h = mmioOpenA(STR(0x1015aa2c), NULL, 0);
    if (!h)
        return 0;
    if (mmioRead(h, (HPSTR)ORIG_PTR(0x10181400), 0xa200) <= 0) {
        mmioClose(h, 0);
        return 0;
    }
    mmioClose(h, 0);
    AI = orig_malloc(0xa4c, 1);
    if (!AI)
        return 0;
    P(uint32_t, AI, 0xa30) = I(uint32_t, 0xc2eb);
    P(uint32_t, AI, 0xa34) = I(uint16_t, 0xc2fd);
    P(uint32_t, AI, 0xa38) = I(uint32_t, 0xc2f9) / ((uint32_t)I(uint16_t, 0xc2fd) * 30);
    P(uint32_t, AI, 0xa3c) = P(uint32_t, AI, 0xa34) * P(uint32_t, AI, 0xa38);
    P(uint32_t, AI, 0xa40) = 0;
    P(uint32_t, AI, 0xa44) = 0;
    P(uint32_t, AI, 0xa48) = 0;
    ai_tables_init();
    P(uint16_t, AI, 0xa26) = 0;
    P(uint32_t, AI, 0xa2c) = 1;
    for (int32_t i = 0; i < 8; i++)
        ai_player_init(i);
    debug_print(0x10, STR(0x1015aa58), g_iso + 0xbfd9, STR(0x1015aa3c));
    return 1;
}
