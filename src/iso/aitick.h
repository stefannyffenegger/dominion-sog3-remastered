/* Game logic: the computer players' main tick (aitick.c) */
#ifndef DOMINION_ISO_AITICK_H
#define DOMINION_ISO_AITICK_H

#include <stdint.h>

void __cdecl ai_player_rules(int32_t k);
int32_t __cdecl ai_tick(void);

#endif
