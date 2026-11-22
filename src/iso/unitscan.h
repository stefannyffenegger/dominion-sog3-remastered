/* Game logic: a unit looking for a target around it (unitscan.c) */
#ifndef DOMINION_ISO_UNITSCAN_H
#define DOMINION_ISO_UNITSCAN_H

#include <stdint.h>

int32_t __cdecl unit_scan_targets(uint32_t id, uint32_t sector, uint32_t count, int32_t flags);

#endif
