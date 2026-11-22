/* Game logic: a unit moving on to the next node of its path (pathadvance.c) */
#ifndef DOMINION_ISO_PATHADVANCE_H
#define DOMINION_ISO_PATHADVANCE_H

#include <stdint.h>

int32_t __cdecl unit_next_node(uint32_t id, int32_t full);

#endif
