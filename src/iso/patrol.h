/* Game logic: a unit's patrol order (patrol.c) */
#ifndef DOMINION_ISO_PATROL_H
#define DOMINION_ISO_PATROL_H

#include <stdint.h>

/* an assembly entry stub (it keeps two stack slots the original reads without setting) */
int32_t __cdecl patrol_order(uint32_t unit, uint32_t way, int32_t start, int32_t check, int32_t join, int32_t keep,
                             int32_t forced, uint32_t flag);

#endif
