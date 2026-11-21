/* Game logic: unit events for the AI, sorting by distance (unitai.c) */
#ifndef DOMINION_ISO_UNITAI_H
#define DOMINION_ISO_UNITAI_H

#include <stdint.h>

int32_t __cdecl unit_ai_event(uint32_t unit, int32_t event, uint32_t arg);
int32_t __cdecl units_by_distance(const uint16_t *a, const uint16_t *b);

#endif
