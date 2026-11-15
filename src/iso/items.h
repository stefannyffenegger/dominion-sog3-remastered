/* Game logic: unit and object values for scripts (items.c) */
#ifndef DOMINION_ISO_ITEMS_H
#define DOMINION_ISO_ITEMS_H

#include <stdint.h>

int __stdcall IsoSetUnitItem(uint8_t *iso, uint32_t value, uint32_t id, int item);
uint32_t __stdcall IsoGetUnitItem(uint8_t *iso, int id, int item);
int __stdcall IsoSetObjectItem(uint8_t *iso, uint32_t value, int i, int item);
uint32_t __stdcall IsoGetObjectItem(uint8_t *iso, int i, int item);

#endif
