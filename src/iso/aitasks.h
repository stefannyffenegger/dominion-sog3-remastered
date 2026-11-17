/* Game logic: the AI's task pool (aitasks.c) */
#ifndef DOMINION_ISO_AITASKS_H
#define DOMINION_ISO_AITASKS_H

#include <stdint.h>

int32_t __cdecl task_free_head(void);
int32_t __cdecl task_set_free_head(int32_t t);
int32_t __cdecl task_next(int32_t t);
int32_t __cdecl task_set_next(int32_t t, int32_t next);
int32_t __cdecl task_prev(int32_t t);
int32_t __cdecl task_set_prev(int32_t t, int32_t prev);
int32_t __cdecl task_link(int32_t player, int32_t t);
int32_t __cdecl task_new(int32_t player, int32_t type, int32_t list, int32_t steps);
int32_t __cdecl task_set_steps(int32_t t, int32_t steps);
int32_t __cdecl task_run(int32_t t);
int32_t __cdecl task_step(int32_t t);
void __cdecl ai_run_tasks(int32_t player);

#endif
