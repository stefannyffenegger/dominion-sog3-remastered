/* Game logic: debug output (switched off) and AI timing (debugtime.c) */
#ifndef DOMINION_ISO_DEBUGTIME_H
#define DOMINION_ISO_DEBUGTIME_H

#include <stdint.h>

int32_t __cdecl iso_debug_print(void);
int32_t __cdecl iso_debug_log(void);
int32_t __cdecl IsoDebugFile(void);
void __cdecl ai_timing_start(void);
void __cdecl ai_timing_log(int level, const char *what, uint32_t since);

#endif
