/* Game logic: float vectors, score counters (vecscore.c) */
#ifndef DOMINION_ISO_VECSCORE_H
#define DOMINION_ISO_VECSCORE_H

#include <stdint.h>

long double __cdecl vec_sqrt(float v);
long double __cdecl vec_sqrt2(float v);
void __cdecl vec_sub(const float *a, const float *b, float *c);
void __cdecl vec_normalize(float *v);
void __cdecl vec_direction(const float *a, const float *b, float *c);
void __cdecl score_unit_killed(uint32_t type, uint8_t owner, uint8_t killer);
void __cdecl score_unit_built(int32_t player, uint32_t type);
void __cdecl score_add_men(int32_t player, int32_t n);
void __cdecl score_add_materials(int32_t player, int32_t n);

#endif
