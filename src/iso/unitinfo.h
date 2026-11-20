/* Game logic: unit type lookups, rank names (unitinfo.c) */
#ifndef DOMINION_ISO_UNITINFO_H
#define DOMINION_ISO_UNITINFO_H

#include <stdint.h>

int32_t __cdecl unit_type_value0(uint32_t unit);
int32_t __cdecl unit_type_field(uint32_t unit, uint8_t which);
int32_t __cdecl unit_kind_quarter(uint32_t unit);
char *__cdecl unit_type_name(int32_t k, char *out);
char *__cdecl ai_task_type_name(uint32_t v, char *out);
char *__cdecl ai_task_state_name(uint32_t v, char *out);
char *__cdecl ai_attitude_name(uint32_t v, char *out);
char *__cdecl ai_squad_kind_name(uint32_t v, char *out);
uint32_t __cdecl unit_valid_target(uint32_t unit);
int32_t __cdecl unit_being_built(uint32_t unit);
int32_t __cdecl unit_takes_orders_from(uint32_t unit, uint8_t p);
void __cdecl unit_set_1b1(uint32_t unit, uint16_t v);
void __cdecl unit_stop(uint32_t unit);

#endif
