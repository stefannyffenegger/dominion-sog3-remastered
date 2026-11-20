/* Game logic: path smoothing (pathsmooth.c) */
#ifndef DOMINION_ISO_PATHSMOOTH_H
#define DOMINION_ISO_PATHSMOOTH_H

#include <stdint.h>

int32_t __cdecl path_smooth(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int32_t flags, int32_t *path);

#endif
