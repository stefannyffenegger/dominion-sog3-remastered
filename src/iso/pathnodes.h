/* Game logic: the free list of path nodes (pathnodes.c) */
#ifndef DOMINION_ISO_PATHNODES_H
#define DOMINION_ISO_PATHNODES_H

#include <stdint.h>

int32_t __cdecl vector_new(uint32_t owner, uint32_t action);
int32_t __cdecl vector_free(uint32_t k);

#endif
