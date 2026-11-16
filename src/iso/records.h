/* Game logic: the record table and unit categories (records.c) */
#ifndef DOMINION_ISO_RECORDS_H
#define DOMINION_ISO_RECORDS_H

#include <stdint.h>
#include <windows.h>

int __cdecl records_init(void);
int __cdecl records_load(HMMIO f);
int __cdecl records_save(HMMIO f);
void __cdecl records_free(void);
uint32_t __cdecl records_size(void);
uint8_t *__cdecl record_add(const uint32_t *from);
void __cdecl record_remove(uint32_t *r);
uint8_t *__cdecl record_get(int i);
uint8_t *__cdecl record_find_first(int(__cdecl *test)(uint8_t *));
uint8_t *__cdecl record_find_next(void);
uint8_t __cdecl unit_category(uint32_t id);

#endif
