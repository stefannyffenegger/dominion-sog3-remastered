/* Game logic: a unit being given a target (unittarget.c) */
#ifndef DOMINION_ISO_UNITTARGET_H
#define DOMINION_ISO_UNITTARGET_H

#include <stdint.h>

void __cdecl unit_attack_target(uint32_t id, uint32_t tid, int32_t delay, uint32_t flag);

#endif
