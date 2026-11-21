/* Game logic: IsoDeleteUnit (unitdelete.c) */
#ifndef DOMINION_ISO_UNITDELETE_H
#define DOMINION_ISO_UNITDELETE_H

#include <stdint.h>

int32_t __stdcall IsoDeleteUnitC(uint8_t *iso, uint32_t id, int32_t mark);
int32_t __stdcall IsoDeleteAllUnitsC(uint8_t *iso);

#endif
