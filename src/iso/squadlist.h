/* Game logic: the AI's squad list (squadlist.c) */
#ifndef DOMINION_ISO_SQUADLIST_H
#define DOMINION_ISO_SQUADLIST_H

#include <stdint.h>

int32_t __cdecl squad_first(void);
int32_t __cdecl squad_set_first(int32_t k);
int32_t __cdecl squad_next(int32_t k);
int32_t __cdecl squad_set_next(int32_t k, int32_t next);
int32_t __cdecl squad_prev(int32_t k);
int32_t __cdecl squad_set_prev(int32_t k, int32_t prev);
int32_t __cdecl squad_new(int32_t player, int32_t kind, int32_t task, const int32_t *types);

#endif
