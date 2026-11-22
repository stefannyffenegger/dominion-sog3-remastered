/* Game logic: spreading a remaining distance over a walk (unitstepmove.c) */
#ifndef DOMINION_ISO_UNITSTEPMOVE_H
#define DOMINION_ISO_UNITSTEPMOVE_H

#include <stdint.h>

int32_t __cdecl path_step(uint32_t id, int32_t frames, int16_t *first, const int16_t *rem, int16_t *count,
                          int16_t *step, int16_t *period);
int32_t __cdecl path_move(int16_t *rem, int16_t *count, int16_t *step, const int16_t *period, int16_t *x, int16_t *y,
                          int32_t *tx, int32_t *ty);

#endif
