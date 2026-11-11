/* Engine core: cursors, strings, errors, event queue, message flushing (rt_msg.c) */
#ifndef DOMINION_RTLIB_RT_MSG_H
#define DOMINION_RTLIB_RT_MSG_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
char *__stdcall string_var(U n);
char *__stdcall string_get(U n);
void __stdcall cursor_show(int on);
U __cdecl cursor_update(void);
void __stdcall cursor_set(U mode);
void __cdecl load_progress(void);
U __stdcall res_extract_to_file(LPSTR path, U id);
void __stdcall flush_messages(UINT msg);
void __cdecl flush_engine_messages(void);
void __stdcall rt_fatal(U code, U unused);
U __stdcall rt_script_error(UINT id, U arg);
void __stdcall rt_message_box(HWND owner, UINT id, U arg);
U __stdcall event_send(U handler, U a, U b);
U __stdcall event_post(const uint8_t *rec);

#endif
