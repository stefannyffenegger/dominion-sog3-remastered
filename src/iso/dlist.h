/* Game logic: the linked list library and the AI process exports (dlist.c) */
#ifndef DOMINION_ISO_DLIST_H
#define DOMINION_ISO_DLIST_H

#include <stdint.h>

void *__cdecl dlist_add(uint8_t *l, uintptr_t where, uint32_t size, const void *src, void *free_fn);
void __cdecl dlist_link(uint8_t *l, uint8_t *n, uintptr_t where);
void __cdecl dlist_clear(uint8_t *l);
void __cdecl dlist_remove(uint8_t *l, void *data);
void __cdecl dlist_unlink(uint8_t *l, uint8_t *n);
void __cdecl dlist_each(uint8_t *l, void *fn, void *arg);
int32_t __cdecl dlist_contains(uint8_t *l, void *data);
void *__cdecl dlist_first(uint8_t *l);
void *__cdecl dlist_last(uint8_t *l);
void __cdecl dlist_move(uint8_t *l, void *data, uintptr_t where);
void *__cdecl dlist_next(uint8_t *l, void *data);
void *__cdecl dlist_prev(uint8_t *l, void *data);
int32_t __cdecl dlist_count(uint8_t *l);
void *__cdecl dlist_new(void);
void __cdecl dlist_free(uint8_t *l);
int32_t __stdcall IsoInitAIProcessC(uint8_t *iso);
void __stdcall IsoStopAIProcessC(uint8_t *iso);
void __stdcall IsoAILoopC(uint8_t *iso);
void __stdcall IsoSetAIDelayC(uint8_t *iso, int32_t delay);

#endif
