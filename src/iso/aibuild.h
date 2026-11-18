/* Game logic: the AI's building tasks (aibuild.c) */
#ifndef DOMINION_ISO_AIBUILD_H
#define DOMINION_ISO_AIBUILD_H

#include <stdint.h>

int32_t __cdecl ai_build_turn(int32_t player);
int32_t __cdecl ai_can_afford(int32_t player);

#endif
