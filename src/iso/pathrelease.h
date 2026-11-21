/* Game logic: freeing the nodes of a unit's second path (pathrelease.c) */
#ifndef DOMINION_ISO_PATHRELEASE_H
#define DOMINION_ISO_PATHRELEASE_H

#include <stdint.h>

int32_t __cdecl path_release(uint32_t id, int32_t at, int32_t all, int32_t from_list);

#endif
