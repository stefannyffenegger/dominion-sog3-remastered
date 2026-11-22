/* Game logic: a computer player's flags and its 9 lists (ailists.c) */
#ifndef DOMINION_ISO_AILISTS_H
#define DOMINION_ISO_AILISTS_H

#include <stdint.h>

int32_t __cdecl player_is_ai(int32_t k);
int32_t __cdecl ai_list_last(int32_t k, int32_t i);
int32_t __cdecl ai_list_set_last(int32_t k, int32_t i, int32_t v);
int32_t __cdecl ai_list_first(int32_t k, int32_t i);
int32_t __cdecl ai_list_set_first(int32_t k, int32_t i, int32_t v);

#endif
