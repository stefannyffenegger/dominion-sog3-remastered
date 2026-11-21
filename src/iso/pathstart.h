/* Game logic: moving a unit on to its next waypoint (pathstart.c) */
#ifndef DOMINION_ISO_PATHSTART_H
#define DOMINION_ISO_PATHSTART_H

#include <stdint.h>

int32_t __cdecl path_start(uint32_t unit, int32_t keep, int32_t a3, int32_t forced);

#endif
