/* Engine core: the script interpreter's command loop (rt_interp.c) */
#ifndef DOMINION_RTLIB_RT_INTERP_H
#define DOMINION_RTLIB_RT_INTERP_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
U __stdcall script_cmd_size(const uint16_t *c);
U __stdcall handler_count(U id);
U __stdcall handler_record_run(const uint8_t *rec);
void __stdcall handler_post_run(const uint8_t *p);
void __stdcall script_message(const uint16_t *msg, U arg);
U __stdcall cmd_exec(const uint8_t *c, int *len);
U __stdcall handler_exec(U id);
U __stdcall script_run(U id);

#endif
