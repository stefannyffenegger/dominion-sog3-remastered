/* Access to the original game logic (ISO_orig.dll) while we rewrite it.
 *
 * Addresses are the original's own (image base 0x10000000), as in work/decomp/ISO.c.
 * The original may be loaded elsewhere, so every address is rebased at run time.
 *
 *   G(int, 0x1017c584)                        a global of the original
 *   ORIG(int(__cdecl *)(int), 0x10001000)(x)  call a function we haven't rewritten yet
 *
 * When that function is rewritten later, the call reaches the rewrite through its hook. */
#ifndef DOMINION_ISO_ORIG_H
#define DOMINION_ISO_ORIG_H

#include <stdint.h>
#include <string.h>
#include <windows.h>

#define ORIG_BASE 0x10000000u

extern uint8_t *g_orig; /* where ISO_orig.dll is loaded */

#define ORIG_PTR(addr) ((void *)(g_orig + ((uint32_t)(addr) - ORIG_BASE)))
#define ORIG(type, addr) ((type)ORIG_PTR(addr))
#define G(type, addr) (*(type *)ORIG_PTR(addr))

void rt_debug(const char *fmt, ...);

/* Calls a stdcall (or cdecl) function of the original that may change EBX, ESI or EDI;
 * the stack pointer is restored afterwards whatever the callee removed. */
uint32_t __cdecl orig_stdcall_unsafe(void *fn, uint32_t n, const uint32_t *args);
uint32_t __cdecl orig_call2_unsafe(void *fn, uint32_t a, uint32_t b);

#endif
