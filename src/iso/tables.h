/* Game logic: neighbour offsets, external tables, cel offsets (tables.c) */
#ifndef DOMINION_ISO_TABLES_H
#define DOMINION_ISO_TABLES_H

#include <stdint.h>

void __cdecl map_neighbour_offsets(void);
int32_t __cdecl IsoFileExists(uint8_t *iso, uint32_t name);
uint32_t __cdecl IsoGetShortUnitTypeString(uint8_t *iso, int32_t race, int32_t type, uint32_t str);
int32_t __stdcall IsoLoadExternalTable(uint8_t *iso, uint32_t res, uint32_t file);
void __stdcall IsoSetCelOffset(uint8_t *iso, uint32_t res, int32_t to, int32_t dx, int32_t dy, int32_t from,
                               int32_t dx2, int32_t dy2);
int __cdecl file_name_compare(const char *a, const char *b);

#endif
