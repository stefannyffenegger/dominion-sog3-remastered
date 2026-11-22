/* Game logic: a target within weapon range, facing the target (unitfacing.c) */
#ifndef DOMINION_ISO_UNITFACING_H
#define DOMINION_ISO_UNITFACING_H

#include <stdint.h>

int32_t __cdecl unit_weapon_reach(uint32_t id, uint32_t tid);
int32_t __cdecl unit_faces_unit(uint32_t id, uint32_t tid, int32_t *dir);
int32_t __cdecl unit_faces_target(uint32_t id, int32_t *dir);

#endif
