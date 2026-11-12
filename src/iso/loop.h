/* Game logic: the display loop, map editor loop, debug sprites, next unit, groups (loop.c) */
#ifndef DOMINION_ISO_LOOP_H
#define DOMINION_ISO_LOOP_H

#include <stdint.h>

int __stdcall IsoDisplayLoop(uint8_t *iso);
int __stdcall IsoMapEditDisplayLoop(uint8_t *iso, int sync);
int __stdcall IsoSetDebugSprite(uint8_t *iso, uint32_t res1, uint32_t v1, uint32_t res2, uint32_t v2);
int __stdcall IsoFindNextUnit(uint8_t *iso, uint32_t start, uint32_t player, int kind);
int __stdcall IsoSelectGroup(uint8_t *iso, uint32_t player, uint32_t kind, int make, int add);

#endif
