/* Engine core: script and controller timers (rt_timer.c) */
#ifndef DOMINION_RTLIB_RT_TIMER_H
#define DOMINION_RTLIB_RT_TIMER_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
int __stdcall ctimer_delete(int i);
void __stdcall ctimer_remove(U key);
void __stdcall timer_delete(int i);
void __stdcall timer_remove(U id, U all);
void __stdcall timer_set(U id, U handler, U interval, uint8_t flag);
void __stdcall ctimer_set(U key, U interval, U callback, uint8_t flag);

#endif
