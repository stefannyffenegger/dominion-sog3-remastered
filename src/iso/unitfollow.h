/* Game logic: a unit setting off along a new path (unitfollow.c) */
#ifndef DOMINION_ISO_UNITFOLLOW_H
#define DOMINION_ISO_UNITFOLLOW_H

#include <stdint.h>

void __cdecl unit_path_follow(uint32_t id, int32_t node, uint32_t flag);

#endif
