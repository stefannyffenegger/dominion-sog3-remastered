/* Game logic: building an order record for the computer player (aiorder.c) */
#ifndef DOMINION_ISO_AIORDER_H
#define DOMINION_ISO_AIORDER_H

#include <stdint.h>

void __cdecl ai_order_begin(int32_t player);
int32_t __cdecl ai_order_add(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f);
uint8_t *__cdecl ai_order_send(void);

#endif
