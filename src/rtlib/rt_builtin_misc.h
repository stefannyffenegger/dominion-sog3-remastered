/* Engine core: built-ins on the script stack: curves, date and time, picks (rt_builtin_misc.c) */
#ifndef DOMINION_RTLIB_RT_BUILTIN_MISC_H
#define DOMINION_RTLIB_RT_BUILTIN_MISC_H

#include <stdint.h>

typedef uint32_t U;
void __cdecl builtin_curve_point(void);
void __cdecl builtin_collide(void);
void __cdecl builtin_date(void);
void __cdecl builtin_time(void);
void __cdecl builtin_ray_to_rect2(void);
void __cdecl builtin_intersect2(void);
void __cdecl builtin_line_point(void);
void __cdecl builtin_list_entry(void);
U __stdcall pick_find(U id);
U __stdcall pick_get(U id, U which);
void __stdcall pick_set(U id, U value, U which);
void __cdecl builtin_random_script(void);
void __cdecl builtin_sequence(void);
void __cdecl builtin_collide_ex(void);

#endif
