/* Engine core: the script expression evaluator and function calls (rt_script.c) */
#ifndef DOMINION_RTLIB_RT_SCRIPT_H
#define DOMINION_RTLIB_RT_SCRIPT_H

#include <stdint.h>

int32_t __stdcall script_eval(const uint8_t *code);
void __stdcall script_call(const int32_t *args);
void __stdcall script_extern_call(uint32_t fn, int argc);
int __stdcall script_user_call(const int32_t *args);
int32_t *__cdecl script_var(uint32_t n);
uint32_t __stdcall ExternalCall(void *fn, uint32_t ctx, const uint32_t *args, uint32_t argc, uint32_t unused);

#endif
