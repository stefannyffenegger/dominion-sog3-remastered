/* Game logic: free ids, the unit and object lists (unitlists.c) */
#ifndef DOMINION_ISO_UNITLISTS_H
#define DOMINION_ISO_UNITLISTS_H

#include <stdint.h>

uint32_t __cdecl unit_id_free(void);
uint32_t __cdecl object_slot_free(void);
int32_t __cdecl unit_list_add(uint32_t id);
int32_t __cdecl unit_list_remove(uint32_t id);
int32_t __cdecl object_list_add(uint32_t id);
int32_t __cdecl object_list_remove(uint32_t id);
int32_t __cdecl view_to_player_start(int32_t player);

#endif
