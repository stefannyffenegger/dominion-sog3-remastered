/* The game logic: the AI's current record (0x100e25ba) and setting up one computer player
 * (0x100e2832). */
#include "iso_core.h"
#include "aiplayer.h"

#define AI G(uint8_t *, 0x1018b604)

/* 0x100e25ba: the AI block's +0xa28 */
int32_t __cdecl ai_current_value(void)
{
    return P(int32_t, AI, 0xa28);
}

/* 0x100e2832: resets player `k`'s record (0x143 bytes each from +0xc of the AI block). Always 1
 * once the block exists. The field +0xfe is written twice, 500 and then 20, as in the original */
int32_t __cdecl ai_player_init(int32_t k)
{
    if (!AI)
        return 0;
    uint8_t *p = AI + k * 0x143 + 0xc;
    if (!p)
        return 1;
    P(uint8_t, p, 0) = (uint8_t)k;
    P(uint8_t, p, 1) = 0;
    P(int32_t, p, 2) = 0;
    P(int32_t, p, 6) = 0;
    P(int32_t, p, 0xa) = 0;
    P(int32_t, p, 0xe) = 0;
    P(int32_t, p, 0x12) = 8;
    P(int32_t, p, 0xf2) = -1;
    P(int32_t, p, 0x5e) = 1;
    P(int32_t, p, 0x106) = 0;
    for (int32_t i = 0; i < 9; i++) {
        P(int32_t, p, 0x16 + i * 4) = -1;
        P(int32_t, p, 0x3a + i * 4) = -1;
    }
    P(uint16_t, p, 0x115) = 0xffff;
    P(uint16_t, p, 0x117) = 0xffff;
    P(uint16_t, p, 0x119) = 0xffff;
    P(int32_t, p, 0x11d) = -1;
    P(int32_t, p, 0x12f) = -1;
    P(int32_t, p, 0x133) = -1;
    P(int32_t, p, 0x137) = -1;
    P(int32_t, p, 0x13b) = -1;
    P(int32_t, p, 0xf6) = 0;
    for (int32_t i = 0; i < 0x12; i++)
        P(uint16_t, p, 0xa0 + i * 2) = 0;
    P(uint16_t, p, 0x62) = 0;
    P(uint16_t, p, 0x64) = 0;
    P(uint16_t, p, 0x68) = 0;
    P(uint16_t, p, 0x94) = 0;
    P(uint16_t, p, 0x96) = 0;
    P(uint16_t, p, 0x98) = 0;
    P(int32_t, p, 0xfa) = 0;
    P(uint16_t, p, 0x9e) = 0;
    P(uint8_t, p, 0x10a) = 0;
    P(uint16_t, p, 0x9a) = 0;
    P(uint16_t, p, 0x9c) = 0;
    P(int32_t, p, 0x12b) = 0;
    P(int32_t, p, 0xfe) = 0x1f4;
    P(int32_t, p, 0xfe) = 0x14;
    return 1;
}
