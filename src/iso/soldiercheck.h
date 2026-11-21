/* Game logic: whether a soldier's patrol ends somewhere it can stand (soldiercheck.c) */
#ifndef DOMINION_ISO_SOLDIERCHECK_H
#define DOMINION_ISO_SOLDIERCHECK_H

#include <stdint.h>

int32_t __cdecl soldier_patrol_check(uint8_t *u);
int32_t __cdecl unit_kind_of(uint32_t id);
int32_t __cdecl unit_kind(uint8_t *u);

#endif
