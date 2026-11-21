/* Game logic: the debug view's crosses on the map (debugmarks.c) */
#ifndef DOMINION_ISO_DEBUGMARKS_H
#define DOMINION_ISO_DEBUGMARKS_H

#include <stdint.h>

int32_t __cdecl debug_mark(int32_t x, int32_t y, int32_t white);
void __cdecl marks_clear(void);
void __cdecl marks_draw(void);

#endif
