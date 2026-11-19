/* Game logic: AI script condition helpers (aiconds.c) */
#ifndef DOMINION_ISO_AICONDS_H
#define DOMINION_ISO_AICONDS_H

#include <stdint.h>

int32_t __cdecl ai_compare(int32_t a, int32_t b, uint32_t op);
int32_t __cdecl ai_condition_value(int32_t player, uint8_t *c, uint32_t a, uint32_t b, uint32_t d, uint32_t e);
void __cdecl ai_condition_update(int32_t player, uint8_t *c);
int32_t __cdecl ai_condition_true(void);
int32_t __cdecl ai_squad_attacked(int32_t player, int32_t kind);
int32_t __cdecl ai_condition_difficulty(int32_t player, uint32_t op, int32_t value);

#endif
