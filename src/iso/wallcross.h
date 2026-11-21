/* Game logic: walls and the patrols that cross them (wallcross.c) */
#ifndef DOMINION_ISO_WALLCROSS_H
#define DOMINION_ISO_WALLCROSS_H

#include <stdint.h>

void __cdecl walls_check_patrols(void);
int32_t __cdecl wall_cuts_patrols(uint8_t *pa, uint8_t *pb);
int32_t __cdecl sectors_near(uint8_t *u, uint8_t *o);

#endif
