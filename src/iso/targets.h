/* Game logic: AI target record fields (targets.c) */
#ifndef DOMINION_ISO_TARGETS_H
#define DOMINION_ISO_TARGETS_H

#include <stdint.h>

int32_t __cdecl ai_head(void);
int32_t __cdecl ai_set_head(int32_t v);
int32_t __cdecl target_next_by_player(int32_t player, int32_t t);
int32_t __cdecl target_prev_by_player(int32_t player, int32_t t);
int32_t __cdecl target_next(int32_t unused, int32_t t);
int32_t __cdecl target_field70(int32_t player, int32_t t);
int32_t __cdecl target_fieldb0(int32_t player, int32_t t);
int32_t __cdecl target_fieldd0(int32_t player, int32_t t);
int32_t __cdecl target_prev(int32_t unused, int32_t t);
int32_t __cdecl target_fieldf0(int32_t player, int32_t t);
int32_t __cdecl target_set_next_by_player(int32_t player, int32_t t, int32_t v);
int32_t __cdecl target_set_prev_by_player(int32_t player, int32_t t, int32_t v);
int32_t __cdecl target_set_next(int32_t unused, int32_t t, int32_t v);
int32_t __cdecl target_set_field70(int32_t player, int32_t t, int32_t v);
int32_t __cdecl target_set_fieldb0(int32_t player, int32_t t, int32_t v);
int32_t __cdecl target_set_fieldd0(int32_t player, int32_t t, int32_t v);
int32_t __cdecl target_set_prev(int32_t unused, int32_t t, int32_t v);
int32_t __cdecl target_set_fieldf0(int32_t player, int32_t t, int32_t v);

int32_t __cdecl target_new(int32_t unit);
int32_t __cdecl target_new_transport(uint32_t unit);

#endif
