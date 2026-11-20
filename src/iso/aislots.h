/* Game logic: the AI's global conditions and records (aislots.c) */
#ifndef DOMINION_ISO_AISLOTS_H
#define DOMINION_ISO_AISLOTS_H

#include <stdint.h>

int32_t __cdecl EDITPATH_GetEditModeC(void);
int32_t __cdecl ai_records_init(void);
uint8_t *__cdecl ai_record_add(const uint8_t *r);
uint8_t *__cdecl ai_global_condition(int32_t k);
uint8_t *__cdecl ai_record(int32_t k);

#endif
