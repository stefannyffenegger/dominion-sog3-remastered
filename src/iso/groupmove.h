/* Game logic: the group move (groupmove.c) */
#ifndef DOMINION_ISO_GROUPMOVE_H
#define DOMINION_ISO_GROUPMOVE_H

#include <stdint.h>

void __cdecl group_move(int32_t player, int32_t x, uint32_t y, uint32_t a4, uint32_t action, int32_t no_leader,
                        uint32_t a7);

#endif
