/* Engine core: settings, the display mode switch and small commands (rt_ini.c) */
#ifndef DOMINION_RTLIB_RT_INI_H
#define DOMINION_RTLIB_RT_INI_H

#include <stdint.h>
#include <windows.h>
#include <mmsystem.h>

void __stdcall cmd_zoom(const uint8_t *c);
void __cdecl cd_stop(void);
void __stdcall cmd_stop_wave(uint32_t id, int32_t *params);
void __stdcall ini_delete_key(LPCSTR file, LPCSTR section, LPCSTR key);
void __stdcall ini_delete_section(LPCSTR file, LPCSTR section);
void __stdcall cmd_ini_delete_section(const uint8_t *c);
void __stdcall cmd_ini(const uint8_t *c);
void __stdcall ini_backup(uint32_t unused);
int __stdcall file_copy_mmio(HMMIO dst, HMMIO src);
void __stdcall backup_name(char *out, char *dir);
void __stdcall ini_get_string(LPCSTR file, LPCSTR section, LPCSTR key, LPSTR out, int crypt);
void __stdcall ini_unescape(char *dst, const char *src);
void __stdcall ini_unscramble(char *s, char key);
void __stdcall ini_get_int(LPCSTR file, LPCSTR section, LPCSTR key, int32_t *out, int crypt);
void __stdcall reg_get_string(HKEY root, LPCSTR path, LPCSTR key, LPCSTR name, LPBYTE out, int crypt);
void __stdcall reg_get_int(HKEY root, LPCSTR path, LPCSTR key, LPCSTR name, int32_t *out, int crypt);
uint32_t __stdcall reg_int_unscramble(uint32_t v);
uint32_t __stdcall bits_unscramble(uint8_t b);
void __stdcall ini_set_string(LPCSTR file, LPCSTR section, LPCSTR key, LPCSTR value, int crypt);
void __stdcall ini_escape(char *dst, const char *src);
void __stdcall ini_scramble(char *s, char key);
void __stdcall ini_set_int(LPCSTR file, LPCSTR section, LPCSTR key, int32_t value, int crypt);
void __stdcall reg_set_string(HKEY root, LPCSTR path, LPCSTR key, LPCSTR name, const BYTE *value, int crypt);
void __stdcall reg_set_int(HKEY root, LPCSTR path, LPCSTR key, LPCSTR name, uint32_t value, int del, int crypt);
uint32_t __stdcall reg_int_scramble(uint32_t v);
uint32_t __stdcall bits_scramble(uint8_t b);
DWORD __stdcall display_change_thread(void *mode);
DWORD __cdecl display_change(void *mode);
uint32_t __stdcall display_mode_set(DWORD w, DWORD h, uint32_t bpp);
int __cdecl windows_has_display_modes(void);
int __cdecl screen_is(int w, int h);
void __cdecl RtCaptureFrame(uint8_t *rt, uint32_t id, uint32_t arg);

#endif
