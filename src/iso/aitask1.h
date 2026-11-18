/* Game logic: the first AI task handlers (aitask1.c) */
#ifndef DOMINION_ISO_AITASK1_H
#define DOMINION_ISO_AITASK1_H

#include <stdint.h>

int32_t __cdecl task_new_recon(int32_t player, uint32_t wanted, int32_t steps);
int32_t __cdecl task_recon(int32_t t);
int32_t __cdecl task_new_scan(int32_t player, int32_t steps);
int32_t __cdecl task_scan(int32_t t);
int32_t __cdecl task_new_16(int32_t player, int32_t steps);
int32_t __cdecl task_16(int32_t t);
int32_t __cdecl task_new_build(int32_t player, int32_t steps);
int32_t __cdecl task_build(int32_t t);
int32_t __cdecl task_new_base(int32_t player, int32_t steps);
int32_t __cdecl task_base(int32_t t);
int32_t __cdecl task_new_reserves(int32_t player, int32_t steps);
int32_t __cdecl task_reserves(int32_t t);

#endif
