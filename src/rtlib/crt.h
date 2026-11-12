/* The engine's few C runtime functions (crt.c) */
#ifndef DOMINION_RTLIB_CRT_H
#define DOMINION_RTLIB_CRT_H

#include <stdint.h>

void crt_srand(unsigned seed);
int crt_rand(void);
int crt_atoi(const char *s);
char *crt_strstr(const char *hay, const char *needle);
char *crt_strrchr(const char *s, int c);
int crt_isalpha(int c);
int crt_scan_hex(const char *s, uint32_t *out, int n);
char *crt_getdcwd(int drive, char *buf, int n);
int crt_assert_failed(const char *file, int line, const char *expr);

#endif
