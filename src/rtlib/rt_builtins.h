/* Engine core: the script's built-in functions (rt_builtins.c) */
#ifndef DOMINION_RTLIB_RT_BUILTINS_H
#define DOMINION_RTLIB_RT_BUILTINS_H

#include <stdint.h>

typedef uint32_t U;
void __stdcall builtin_call(U fn, U argc);

#endif
