/* Game logic: the AI's terrain analysis, the parts every game runs (terrain.c) */
#ifndef DOMINION_ISO_TERRAIN_H
#define DOMINION_ISO_TERRAIN_H

#include <stdint.h>

void __cdecl ta_clear_table(void);
int32_t __cdecl aoa_free(void **aoa);
void __cdecl ta_free(void);
void __cdecl ta_init(void);
int32_t *__cdecl aoa_new(void);
uint32_t *__cdecl aoa_add_sector(int32_t *aoa, uint32_t sector);
int32_t *__cdecl kt_find(uint32_t sector, int32_t type);
int32_t *__cdecl kt_add(int32_t type, uint32_t sector, int32_t value);
int32_t __cdecl sdl_aoa(uint8_t *rec);
int32_t __cdecl sdl_kt(uint8_t *rec);

#endif
