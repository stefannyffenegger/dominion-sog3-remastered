/* Game logic: the linked lists of path nodes (pathlink.c) */
#ifndef DOMINION_ISO_PATHLINK_H
#define DOMINION_ISO_PATHLINK_H

#include <stdint.h>

void __cdecl vector_link(int32_t k, int32_t next, int32_t unused);
int32_t __cdecl path_stop(uint32_t id, int32_t after);
int32_t __cdecl unit_path_join(uint32_t id, int32_t at, int32_t node, int32_t before);

#endif
