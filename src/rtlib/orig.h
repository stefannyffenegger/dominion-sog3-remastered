/* The original engine's addresses (Rtlib32_orig.dll, mapped as data by main.c).
 *
 * Our code names the engine's globals by the original's addresses (image base
 * 0x10000000), as in the decompiled code:
 *
 *   G(int, 0x1007cee8)    a global; its storage is our own copy of the original's data
 *                         section, which starts as the original file has it
 *   ORIG_PTR(0x1007b7e8)  the address of a global or table
 *
 * Addresses outside the data section (strings and constants in .rdata) point into the
 * mapped original. None of its code runs. */
#ifndef DOMINION_RTLIB_ORIG_H
#define DOMINION_RTLIB_ORIG_H

#include <stdint.h>
#include <string.h>
#include <windows.h>

#define ORIG_BASE 0x10000000u

extern uint8_t *g_orig;  /* where Rtlib32_orig.dll is mapped (as data) */
extern uint8_t *g_odata; /* the original's data section: our own copy (main.c) */

#define ORIG_DATA 0x10075000u     /* the original's .data, initialized and not */
#define ORIG_DATA_SIZE 0x1bb48u
#define ORIG_PTR(addr)                                                                                            \
    ((void *)((uint32_t)(addr) - ORIG_DATA < ORIG_DATA_SIZE ? g_odata + ((uint32_t)(addr) - ORIG_DATA)            \
                                                              : g_orig + ((uint32_t)(addr) - ORIG_BASE)))
#define G(type, addr) (*(type *)ORIG_PTR(addr))

void rt_debug(const char *fmt, ...);

/* Calls a 2-argument stdcall callback that may change EBX, ESI or EDI; our compiled
 * code expects them kept. */
uint32_t __cdecl orig_call2_unsafe(void *fn, uint32_t a, uint32_t b);
/* Calls a stdcall (or cdecl) callback that may change EBX, ESI or EDI; the stack
 * pointer is restored afterwards whatever the callee removed. */
uint32_t __cdecl orig_stdcall_unsafe(void *fn, uint32_t n, const uint32_t *args);

#endif
