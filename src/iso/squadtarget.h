/* Game logic: a squad's own units and its totals (squadtarget.c) */
#ifndef DOMINION_ISO_SQUADTARGET_H
#define DOMINION_ISO_SQUADTARGET_H

#include <stdint.h>

void __cdecl squad_target_changed(int32_t s, int32_t t, int32_t add);
int32_t __cdecl squad_health_total(int32_t s);
int32_t __cdecl squad_add_target(int32_t s, int32_t t);
void __cdecl squad_remove_target(int32_t t);

#endif
