/* Game logic: path smoothing (pathsmooth.c) */
#ifndef DOMINION_ISO_PATHSMOOTH_H
#define DOMINION_ISO_PATHSMOOTH_H

#include <stdint.h>

int32_t __cdecl path_smooth(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int32_t flags, int32_t *path);
long double __cdecl vec_turn_angle(const float *a, const float *b);
int32_t __cdecl segment_cross(float *out1, float *out2, const float *p3, const float *p4, const float *p5,
                              const float *p6);
int32_t __cdecl unit_path_build(int16_t unit, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t last,
                                uint16_t b6, uint16_t b8);

#endif
