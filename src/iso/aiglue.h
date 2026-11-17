/* Game logic: small AI callbacks (aiglue.c) */
#ifndef DOMINION_ISO_AIGLUE_H
#define DOMINION_ISO_AIGLUE_H

#include <stdint.h>

typedef struct Callback Callback;
void __cdecl ai_callback(uint32_t a, Callback *cb, uint32_t b, uint32_t c, uint32_t d, uint32_t e);
void __cdecl ai_log(uint32_t a);
void __cdecl ai_set_6a(int p, uint32_t v);
void __cdecl ai_unit_order_cb(uint32_t unused, int a, uint32_t b);

void __cdecl ai_script_241a0(uint32_t p, int g, int target, uint32_t a, int b);
void __cdecl ai_script_24267(uint32_t p, int g, int target, int32_t a, int32_t b);
void __cdecl ai_script_243d6(uint32_t p, int g, int target);
void __cdecl ai_script_244f6(uint32_t p, int target);
void __cdecl ai_script_24611(int p);
void __cdecl ai_script_2466e(uint32_t unused, int n);
void __cdecl ai_script_246b2(int p, int g);
void __cdecl ai_script_2478b(uint32_t p, int n);
void __cdecl ai_script_248c3(uint32_t p, uint32_t what);
void __cdecl ai_script_249e0(uint32_t p, int what);
void __cdecl ai_script_24acc(uint32_t unused, int n);
void __cdecl ai_script_24b10(void);
void __cdecl ai_script_24b57(int p, int32_t a, uint32_t b);
void __cdecl ai_script_24c5c(int p);
void __cdecl ai_script_24c8d(int p, int what);
void __cdecl ai_script_24cd5(int p, int n);
void __cdecl ai_script_24d38(int p, int n);
void __cdecl ai_script_24dbb(int p, int type, int v);
void __cdecl ai_script_24f1a(int p, int type, int v);
void __cdecl ai_script_24f74(int p, int g, int32_t how);

#endif
