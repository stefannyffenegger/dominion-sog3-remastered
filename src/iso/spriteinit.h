/* Game logic: log2 and the sprite setup exports (spriteinit.c) */
#ifndef DOMINION_ISO_SPRITEINIT_H
#define DOMINION_ISO_SPRITEINIT_H

#include <stdint.h>

int32_t __stdcall int_log2(int32_t n);
void __stdcall IsoInitSpritesC(uint8_t *iso, uint32_t cursor, uint32_t sprite, uint32_t a, uint32_t b,
                               uint32_t c, uint32_t d, uint32_t res1, uint32_t res2, uint32_t cd8e);
void __stdcall IsoInitRendezvousSpritesC(uint8_t *iso, uint32_t s1, uint32_t s2, uint32_t c1, uint32_t c2,
                                         uint32_t c3, uint32_t c4, uint32_t c5, uint32_t c6, uint32_t c7,
                                         uint32_t c8);

#endif
