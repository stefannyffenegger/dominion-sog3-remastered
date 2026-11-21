/* The game logic, 0x100a78fe-0x100a7f17: the integer log2, and the sprite setup exports. */
#include "iso_core.h"
#include "debugtime.h"
#include "spriteinit.h"

#define IsoSetCommandPoint ORIG(void(__stdcall *)(uint8_t *, uint32_t, uint32_t, int32_t, int32_t), 0x100a061d)

/* 0x100a78fe: the integer log2 of n (0 for n < 2) */
int32_t __stdcall int_log2(int32_t n)
{
    int32_t r = 0;
    for (; n > 1; n >>= 1)
        r++;
    return r;
}

/* IsoInitSprites: the cursor (iso+0xc33b), the main sprite (iso+0, its object at +4, its
 * first five frames' data at +8) and, with sprites shown, four interface values
 * (iso+0xbeee-0xbefa) and two sprite resources loaded (iso+0x1b5d, iso+0xd6ed) */
void __stdcall IsoInitSpritesC(uint8_t *iso, uint32_t cursor, uint32_t sprite, uint32_t a, uint32_t b,
                               uint32_t c, uint32_t d, uint32_t res1, uint32_t res2, uint32_t cd8e)
{
    g_iso = iso;
    I(uint32_t, 0xc33b) = cursor;
    I(uint32_t, 0) = sprite;
    RtLoad(I(void *, ISO_APP), I(uint32_t, 0));
    I(void *, 4) = ((void **)RtGetResource(I(void *, ISO_APP), sprite))[1];
    I(uint32_t, 0xcd8a) = 0;
    I(uint32_t, 0xcd8e) = cd8e;
    I(uint32_t, 0xcd96) = 0;
    I(uint32_t, 0xcd92) = 0;
    I(uint32_t, 0xcd9a) = 0;
    uint32_t *f = P(uint32_t *, I(uint8_t *, 4), 0x24);
    for (uint8_t k = 0; k < 5; k++)
        I(uint32_t, 8 + k * 4) = *f++;
    if (I(int32_t, ISO_HAVE_SPRITES)) {
        I(uint32_t, 0xbeee) = a;
        I(uint32_t, 0xbef2) = b;
        I(uint32_t, 0xbef6) = c;
        I(uint32_t, 0xbefa) = d;
        I(uint32_t, 0x1b5d) = res1;
        RtLoad(I(void *, ISO_APP), I(uint32_t, 0x1b5d));
        I(uint32_t, 0xd6ed) = res2;
        RtLoad(I(void *, ISO_APP), I(uint32_t, 0xd6ed));
    }
}

/* IsoInitRendezvousSprites: the two rendezvous (command point) sprites and eight colour
 * bytes; the local player's command points already set are placed again */
void __stdcall IsoInitRendezvousSpritesC(uint8_t *iso, uint32_t s1, uint32_t s2, uint32_t c1, uint32_t c2,
                                         uint32_t c3, uint32_t c4, uint32_t c5, uint32_t c6, uint32_t c7,
                                         uint32_t c8)
{
    g_iso = iso;
    iso_debug_print();
    RtLoad(I(void *, ISO_APP), s1);
    RtLoad(I(void *, ISO_APP), s2);
    I(uint8_t *, 0x1806) = ((uint8_t **)RtGetResource(I(void *, ISO_APP), s1))[1];
    I(uint32_t, 0xde2) = s1;
    P(uint32_t, I(uint8_t *, 0x1806), 0xf0) = 1;
    I(uint8_t *, 0x180a) = ((uint8_t **)RtGetResource(I(void *, ISO_APP), s2))[1];
    I(uint32_t, 0xde6) = s2;
    P(uint32_t, I(uint8_t *, 0x180a), 0xf0) = 1;
    I(uint8_t, 0xbf24) = (uint8_t)c1;
    I(uint8_t, 0xbf25) = (uint8_t)c2;
    I(uint8_t, 0xbf26) = (uint8_t)c3;
    I(uint8_t, 0xbf27) = (uint8_t)c4;
    I(uint8_t, 0xbf28) = (uint8_t)c5;
    I(uint8_t, 0xbf29) = (uint8_t)c6;
    I(uint8_t, 0xbf2a) = (uint8_t)c7;
    I(uint8_t, 0xbf2b) = (uint8_t)c8;
    for (uint8_t k = 0; k < 2; k++) {
        uint32_t p = I(uint32_t, 0x1b65);
        if (I(int32_t, p * ISO_PLAYER_SIZE + k * 0xc + 0x2db3))
            IsoSetCommandPoint(g_iso, I(uint32_t, 0x1b65), k,
                               I(int32_t, I(int32_t, 0x1b65) * ISO_PLAYER_SIZE + k * 0xc + 0x2dab) - I(int32_t, 0xcd82),
                               I(int32_t, I(int32_t, 0x1b65) * ISO_PLAYER_SIZE + k * 0xc + 0x2daf) - I(int32_t, 0xcd86));
    }
}
