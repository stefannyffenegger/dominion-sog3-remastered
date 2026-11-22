/* Game logic: the two tests of a ring entry (ringtest.c) */
#ifndef DOMINION_ISO_RINGTEST_H
#define DOMINION_ISO_RINGTEST_H

#include <stdint.h>

int32_t __cdecl ring_point_free(int32_t x, int32_t y);
int32_t __cdecl ring_cell_taken(int32_t c);

#endif
