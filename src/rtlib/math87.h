/* The utility library's floating-point math, bit-exact with the original (math87.c) */
#ifndef DOMINION_RTLIB_MATH87_H
#define DOMINION_RTLIB_MATH87_H

#include <stdint.h>

#ifndef __stdcall
#define __stdcall __attribute__((stdcall))
#endif

int __stdcall GetAngle(int dx, int dy);
int __stdcall SquareRoot(int v);
int __stdcall Distance(int x1, int y1, int x2, int y2);
void __stdcall Collide(int x1, int y1, int x2, int y2, int *speed1, int *angle1, int *speed2, int *angle2);
int __stdcall GetIntersect(int *out, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);

/* integer trigonometry (utils.c) */
int __stdcall NormalizeAngle(int a);
int __stdcall Sine(int a);
int __stdcall Cosine(int a);

#endif
