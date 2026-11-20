/* The game logic, 0x10095e98-0x100962f0: the debug output, switched off before release (each
 * function returns 0 at once; their bodies, which printed by level through the flags at
 * iso+0xe09b and wrote debug files, are still in the original but never run), and the AI's
 * step timing. */
#include "iso_core.h"
#include "debugtime.h"

/* 0x10095e98: the debug print (level, format, ...): switched off */
int32_t __cdecl iso_debug_print(void) { return 0; }

/* 0x10095f64: the debug log (level, text): switched off */
int32_t __cdecl iso_debug_log(void) { return 0; }

/* IsoDebugFile: switched off */
int32_t __cdecl IsoDebugFile(void) { return 0; }

/* 0x10096250: notes the time an AI step starts (0x1017c9e4) */
void __cdecl ai_timing_start(void) { G(uint32_t, 0x1017c9e4) = timeGetTime(); }

/* 0x1009626b: an AI step ends: its time (from `since`, or from the last start), printed
 * when timing output is on (0x1017c9e0), and the slowest kept (0x10152b58) */
void __cdecl ai_timing_log(int level, const char *what, uint32_t since)
{
    uint32_t took = since ? timeGetTime() - since : timeGetTime() - G(uint32_t, 0x1017c9e4);
    if (G(int32_t, 0x1017c9e0) && took)
        iso_debug_print();
    (void)level;
    (void)what;
    if (G(uint32_t, 0x10152b58) < took)
        G(uint32_t, 0x10152b58) = took;
}
