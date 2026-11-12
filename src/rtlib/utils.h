/* The engine's utility library (utils.c) */
#ifndef DOMINION_RTLIB_UTILS_H
#define DOMINION_RTLIB_UTILS_H

#include <stdint.h>
#include <windows.h>
#include <mmsystem.h>

void __stdcall CopyHugeBytes(void *dst, const void *src, uint32_t n);
void __stdcall CopyHugeStr(char *dst, const char *src);
int __stdcall HugeStrLen(const char *s);
void __stdcall SetHugeBytes(void *dst, uint8_t value, uint32_t n);
void __stdcall MapBytes(uint8_t *p, int n, const uint8_t *table);
uint32_t __stdcall BinarySearch(const char *key, const char *base, const int32_t *offsets, int n);
LPVOID __stdcall winmalloc(SIZE_T size, int zero);
LPVOID __stdcall winfree(LPCVOID p);
LPVOID __stdcall winptrcpy(LPCVOID p);
LPVOID __stdcall winrealloc(LPCVOID p, SIZE_T size, int zero);
LPVOID __stdcall CopyString(LPCSTR s);
uint32_t __stdcall mem_unused(uint32_t unused);
uint32_t __stdcall MemMgrPreAllocate(uint32_t unused);
uint32_t __cdecl MemMgrSize(void);
uint32_t __stdcall MemSetNotify(uint32_t unused);
int __stdcall NormalizeAngle(int a);
int __stdcall GetQuadrant(int a);
int __stdcall Sine(int a);
int __stdcall Cosine(int a);
int __stdcall Tangent(int a);
int __stdcall ReflectAngle(int a, int mirror);
void __cdecl utils_check_pointers(void);
void __cdecl utils_exit_nothing(void);
void __cdecl utils_reset_pointers(void);
uint32_t __cdecl utils_reset_memory(void);
void __stdcall DebugMessage(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);
void __cdecl DebugStr(LPCSTR fmt, ...);
void __cdecl SetDebugOutputHook(void(__stdcall *hook)(char *, uint32_t), uint32_t arg);
void __stdcall UtilsInitDebug(HWND wnd);
void __cdecl UtilsExitDebug(void);
HMMIO __stdcall FileExist(LPSTR path);
void __stdcall GetPathAndFile(LPCSTR path, char *dir, LPSTR file);
void __stdcall MakeFullPath(char *out, char *dir, char *file);
char *__stdcall SkipPath(char *path);
void __stdcall MakeModulePath(HMODULE module, LPSTR out, char *name);
uint8_t __cdecl GetProcessorType(void);
uint32_t __cdecl utils_detect_processor(void);
UINT __stdcall utils_ini_int(LPCSTR key, INT def);
uint32_t __cdecl UtilsInitLibrary(void);
void __cdecl UtilsExitLibrary(void);
uint32_t __stdcall LinearSearch(const char *key, const char *list, uint32_t n, uint32_t nocase);

#endif
