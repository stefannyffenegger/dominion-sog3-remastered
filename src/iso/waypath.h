/* Game logic: new waypoints in a unit's path, dropping a saved path (waypath.c) */
#ifndef DOMINION_ISO_WAYPATH_H
#define DOMINION_ISO_WAYPATH_H

#include <stdint.h>

int32_t __cdecl path_insert(uint32_t unit, int32_t x, int32_t y, int32_t after, int32_t to, uint32_t how,
                            uint32_t spread);
void __cdecl saved_path_drop(uint32_t unit, int32_t all);

#endif
