/* Game logic: small AI callbacks (aiglue.c) */
#ifndef DOMINION_ISO_AIGLUE_H
#define DOMINION_ISO_AIGLUE_H

#include <stdint.h>

typedef struct Callback Callback;
void __cdecl ai_callback(uint32_t a, Callback *cb, uint32_t b, uint32_t c, uint32_t d, uint32_t e);
void __cdecl ai_log(uint32_t a);
void __cdecl ai_set_6a(int p, uint32_t v);
void __cdecl ai_unit_order_cb(uint32_t unused, int a, uint32_t b);

#endif
