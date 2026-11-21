/* Game logic: suspending a unit's waypoint path (waypoints.c) */
#ifndef DOMINION_ISO_WAYPOINTS_H
#define DOMINION_ISO_WAYPOINTS_H

#include <stdint.h>

int32_t __cdecl unit_path_suspend(uint32_t unit, int32_t suspend, int32_t forced);

#endif
