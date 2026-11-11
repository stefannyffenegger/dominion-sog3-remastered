/* Engine core: the script commands' argument resolvers (rt_cmdargs.c) */
#ifndef DOMINION_RTLIB_RT_CMDARGS_H
#define DOMINION_RTLIB_RT_CMDARGS_H

#include <stdint.h>

uint32_t __stdcall script_value(uint32_t v, uint32_t is_var);
void __stdcall cmdw_inplace(uint8_t *c);
void __stdcall cmdw_set_3e63(const uint8_t *c);
uint32_t __stdcall cmdw_100168c0(const uint8_t *c);
uint32_t __stdcall cmdw_1001692d(const uint8_t *c);
uint32_t __stdcall cmdw_10016969(const uint8_t *c);
void __stdcall cmd_rect(uint32_t *r, uint32_t flags);
uint32_t __stdcall cmdw_10016ac7(const uint8_t *c);
void __stdcall cmdw_clip_box(const uint8_t *c);
uint32_t __stdcall cmdw_10016b8d(const uint8_t *c);
uint32_t __stdcall cmdw_10016bc9(const uint8_t *c);
void __stdcall cmdw_set_cursor_pos(const uint8_t *c);
uint32_t __stdcall cmdw_10016cca(const uint8_t *c);
uint32_t __stdcall cmdw_10016d0e(const uint8_t *c);
uint32_t __stdcall cmdw_10016d80(const uint8_t *c);
uint32_t __stdcall cmdw_10016ddb(const uint8_t *c);
uint32_t __stdcall cmdw_10016e16(const uint8_t *c);
uint32_t __stdcall cmdw_10016e51(const uint8_t *c);
uint32_t __stdcall cmdw_10016efe(const uint8_t *c);
uint32_t __stdcall cmdw_10016f48(const uint8_t *c, uint32_t arg);
uint32_t __stdcall cmdw_10016fb9(const uint8_t *c);
uint32_t __stdcall cmdw_if_loaded(const uint8_t *c);
uint32_t __stdcall cmdw_100170ac(const uint8_t *c);
uint32_t __stdcall cmdw_10017107(const uint8_t *c);
uint32_t __stdcall cmdw_10017142(const uint8_t *c);
uint32_t __stdcall cmdw_1001719c(const uint8_t *c);
uint32_t __stdcall cmdw_100171f0(const uint8_t *c);
uint32_t __stdcall cmdw_10017234(const uint8_t *c);
void __stdcall cmdw_set_slot(const uint8_t *c);
void __stdcall cmdw_100172ce(const uint8_t *c);
uint32_t __stdcall cmdw_100172f5(const uint8_t *c);
void __stdcall cmdw_set_3f0f(const uint8_t *c);
void __stdcall cmdw_load_release(const uint8_t *c);
void __stdcall cmdw_set_mouse_handler(const uint8_t *c);
uint32_t __stdcall cmdw_1001744d(const uint8_t *c);
void __stdcall cmdw_set_pause_handler(const uint8_t *c);
uint32_t __stdcall cmdw_100174f0(const uint8_t *c);
void __stdcall cmdw_1001754a(const uint8_t *c);
void __stdcall cmdw_event(const uint8_t *c);
uint32_t __stdcall cmdw_100175ee(const uint8_t *c);
uint32_t __stdcall cmdw_10017648(uint8_t *c);
void __stdcall cmdw_handler_call(const uint8_t *c);
uint32_t __stdcall cmdw_100177a0(const uint8_t *c);
uint32_t __stdcall cmdw_100177ed(uint8_t *c);
uint32_t __stdcall cmdw_10017899(const uint8_t *c);
void __stdcall cmdw_10017902(const uint8_t *c);
uint32_t __stdcall cmdw_1001794a(const uint8_t *c);
uint32_t __stdcall cmdw_100179ac(const uint8_t *c);
uint32_t __stdcall cmdw_100179e8(const uint8_t *c);
uint32_t __stdcall cmdw_10017a2c(const uint8_t *c);
uint32_t __stdcall cmdw_10017a9e(const uint8_t *c);
uint32_t __stdcall cmdw_10017b32(const uint8_t *c);
uint32_t __stdcall cmdw_10017ba4(const uint8_t *c);
void __stdcall cmdw_10017bf8(const uint8_t *c);
uint32_t __stdcall cmdw_10017c6e(const uint8_t *c);
void __stdcall cmdw_10017cb2(const uint8_t *c);
void __stdcall cmdw_10017cdd(const uint8_t *c);
uint32_t __stdcall cmdw_10017d0f(const uint8_t *c);
void __stdcall cmdw_10017da2(const uint8_t *c);
uint32_t __stdcall cmdw_10017dfe(const uint8_t *c);
uint32_t __stdcall cmdw_10017e9b(const uint8_t *c);
uint32_t __stdcall cmdw_10017edf(const uint8_t *c);

#endif
