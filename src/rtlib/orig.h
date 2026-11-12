/* Access to the original engine (Rtlib32_orig.dll) while we rewrite it.
 *
 * Addresses are the original's own (image base 0x10000000), as in the decompiled code.
 * The original may be loaded elsewhere, so every address is rebased at run time. Its
 * globals are no longer its own: addresses in its data section resolve to our copy.
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

extern uint8_t *g_orig;  /* where Rtlib32_orig.dll is loaded */
extern uint8_t *g_odata; /* the original's data section: our own copy (see hooks.c) */

#define ORIG_DATA 0x10075000u     /* the original's .data, initialized and not */
#define ORIG_DATA_SIZE 0x1bb48u
#define ORIG_PTR(addr)                                                                                            \
    ((void *)((uint32_t)(addr) - ORIG_DATA < ORIG_DATA_SIZE ? g_odata + ((uint32_t)(addr) - ORIG_DATA)            \
                                                              : g_orig + ((uint32_t)(addr) - ORIG_BASE)))
#define ORIG(type, addr) ((type)ORIG_PTR(addr))
#define G(type, addr) (*(type *)ORIG_PTR(addr))

void rt_debug(const char *fmt, ...);

/* Calls a 2-argument stdcall function of the original that may change EBX, ESI or EDI
 * (hand-written assembly); our compiled code expects them kept. */
uint32_t __cdecl orig_call2_unsafe(void *fn, uint32_t a, uint32_t b);
/* Calls a stdcall (or cdecl) function of the original that may change EBX, ESI or EDI;
 * the stack pointer is restored afterwards whatever the callee removed. */
uint32_t __cdecl orig_stdcall_unsafe(void *fn, uint32_t n, const uint32_t *args);

#endif
