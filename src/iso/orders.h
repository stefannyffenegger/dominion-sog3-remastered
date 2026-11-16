/* Game logic: a selected unit's order for a clicked unit (orders.c) */
#ifndef DOMINION_ISO_ORDERS_H
#define DOMINION_ISO_ORDERS_H

#include <stdint.h>

int __cdecl order_board(uint32_t id, uint32_t into, int *tutorial);
int __cdecl order_enter(uint32_t id, uint32_t building, int *tutorial);
int __cdecl order_type21(uint32_t id, uint16_t target, int *tutorial);
int __cdecl order_engineer(uint32_t id, uint32_t target, int *tutorial, int *stagger);
int __cdecl order_attack(uint32_t id, uint32_t target, int *tutorial, int *stagger);

#endif
