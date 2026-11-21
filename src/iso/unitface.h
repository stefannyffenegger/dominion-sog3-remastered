/* Game logic: turning a unit to face a direction (unitface.c) */
#ifndef DOMINION_ISO_UNITFACE_H
#define DOMINION_ISO_UNITFACE_H

#include <stdint.h>

int32_t __cdecl unit_face(uint32_t unit, uint32_t dir, int32_t any, int32_t forced);

#endif
