/* Access to the original engine (Rtlib32_orig.dll) while we rewrite it.
 *
 * Addresses are the original's own (image base 0x10000000), as in the decompiled code.
 * The original may be loaded elsewhere, so every address is rebased at run time.
 *
 *   G(int, 0x1007cee8)              a global of the original
 *   ORIG(int(__cdecl *)(int), 0x10066992)(x)   call a function we haven't rewritten yet
 *
 * When that function is rewritten later, the call reaches the rewrite through its hook. */
#ifndef DOMINION_RTLIB_ORIG_H
#define DOMINION_RTLIB_ORIG_H

#include <stdint.h>
#include <string.h>
#include <windows.h>

#define ORIG_BASE 0x10000000u

extern uint8_t *g_orig; /* where Rtlib32_orig.dll is loaded */

#define ORIG_PTR(addr) ((void *)(g_orig + ((uint32_t)(addr) - ORIG_BASE)))
#define ORIG(type, addr) ((type)ORIG_PTR(addr))
#define G(type, addr) (*(type *)ORIG_PTR(addr))

void rt_debug(const char *fmt, ...);

#endif
