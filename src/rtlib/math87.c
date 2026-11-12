/* The utility library's floating-point math (0x1003dad4, 0x1003dcd3, 0x1003deb1,
 * 0x1003df0b, 0x1003e660), rewritten so that every result matches the original bit
 * for bit: the same x87 operations in the same order, `long double` for values the
 * original keeps on the FPU stack, `volatile double` where it stores them, and the
 * C library's conversion to integer (truncation through a 64-bit store). Like the
 * original, the arithmetic runs at whatever precision the calling thread has set;
 * only atan2 switches to full precision, as the C library did.
 * Checked against the original by tests/math_compare.c.
 *
 * Angles are in tenths of a degree. The engine converts radians with pi = 3.1415. */
#include "math87.h"

#define RAD_TO_ANGLE 0x1.1e7cc2c2add15p+9 /* 1800 / 3.1415, as the original's constant */
#define PI_ENGINE 3.1415

/* 32-bit subtraction that wraps */
static int sub(int a, int b) { return (int)((uint32_t)a - (uint32_t)b); }

/* The C library's __ftol: truncation toward zero, the low 32 bits of a 64-bit store
 * (0 for values out of range) */
static int32_t ftol(long double v)
{
    uint16_t cw, chop;
    int64_t r;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    chop = cw | 0x0c00;
    __asm__ volatile("fldcw %1\n\tfistpll %0\n\tfldcw %2" : "=m"(r) : "m"(chop), "m"(cw), "t"(v) : "st");
    return (int32_t)r;
}

/* atan2(y, x) as the C library computed it: fpatan at 64-bit precision, the result left
 * at full precision */
static long double crt_atan2(long double y, long double x)
{
    uint16_t cw, full = 0x133f;
    long double r;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    __asm__ volatile("fldcw %3\n\tfpatan\n\tfldcw %4" : "=t"(r) : "0"(x), "u"(y), "m"(full), "m"(cw) : "st(1)");
    return r;
}

/* sqrt as the C library computed it, at 64-bit precision */
static long double crt_sqrt(long double v)
{
    uint16_t cw, full = 0x133f;
    long double r;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    __asm__ volatile("fldcw %2\n\tfsqrt\n\tfldcw %3" : "=t"(r) : "0"(v), "m"(full), "m"(cw));
    return r;
}

/* GetAngle (0x1003dad4): the angle of the vector (dx, dy), 0-3599 */
int __stdcall GetAngle(int dx, int dy)
{
    int a = ftol(crt_atan2((long double)dx, (long double)dy) * (long double)RAD_TO_ANGLE);
    return a < 0 ? a + 3600 : a;
}

/* SquareRoot (0x1003df0b) */
int __stdcall SquareRoot(int v) { return ftol(crt_sqrt((long double)(volatile double)(double)v)); }

/* Distance (0x1003deb1): between two points; the squares are 32-bit and may wrap */
int __stdcall Distance(int x1, int y1, int x2, int y2)
{
    uint32_t dx = (uint32_t)(x2 - x1), dy = (uint32_t)(y2 - y1);
    return SquareRoot((int)(dy * dy + dx * dx));
}

/* Collide (0x1003dcd3): two bodies bounce. Speeds and angles are updated in place; the
 * collision line is the angle between the points, or x1 itself when y1 is 30000. */
void __stdcall Collide(int x1, int y1, int x2, int y2, int *speed1, int *angle1, int *speed2, int *angle2)
{
    if (!*speed1 && !*speed2)
        return;
    int base = y1 == 30000 ? x1 : NormalizeAngle(GetAngle(sub(x2, x1), sub(y2, y1)));
    *angle1 = sub(*angle1, base);
    *angle2 = sub(*angle2, base);
    int c1 = (int)((uint32_t)Cosine(*angle1) * (uint32_t)*speed1) / 10000;
    int s1 = (int)((uint32_t)Sine(*angle1) * (uint32_t)*speed1) / 10000;
    int c2 = (int)((uint32_t)Cosine(*angle2) * (uint32_t)*speed2) / 10000;
    int s2 = (int)((uint32_t)Sine(*angle2) * (uint32_t)*speed2) / 10000;
    *speed1 = SquareRoot((int)((uint32_t)c2 * (uint32_t)c2 + (uint32_t)s1 * (uint32_t)s1));
    *speed2 = SquareRoot((int)((uint32_t)s2 * (uint32_t)s2 + (uint32_t)c1 * (uint32_t)c1));
    *angle1 = NormalizeAngle(
        (int)((uint32_t)base + (uint32_t)ftol(crt_atan2((long double)s1, (long double)c2) * (long double)1800.0 / (long double)PI_ENGINE)));
    *angle2 = NormalizeAngle(
        (int)((uint32_t)base + (uint32_t)ftol(crt_atan2((long double)s2, (long double)c1) * (long double)1800.0 / (long double)PI_ENGINE)));
}

/* the original's direction of a line: -1 vertical, 1 horizontal, 0 sloped */
static int line_kind(int x1, int y1, int x2, int y2) { return x1 == x2 ? -1 : y1 == y2 ? 1 : 0; }

/* GetIntersect (0x1003e660): where the lines (x1, y1)-(x2, y2) and (x3, y3)-(x4, y4)
 * cross; 0 when they are parallel */
int __stdcall GetIntersect(int *out, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4)
{
    int k1 = line_kind(x1, y1, x2, y2), k2 = line_kind(x3, y3, x4, y4);
    if (k1 == k2 && k1)
        return 0;
    volatile double m1 = 0, m2 = 0, b1, b2;
    if (!k1)
        m1 = (long double)sub(y1, y2) / (long double)sub(x1, x2);
    if (!k2)
        m2 = (long double)sub(y3, y4) / (long double)sub(x3, x4);
    if (k1 == 1 && k2 == -1)
        out[0] = x3, out[1] = y1;
    if (k1 == -1 && k2 == 1)
        out[0] = x1, out[1] = y3;
    if (m2 == m1)
        return 0;
    b1 = (long double)y1 - (long double)x1 * (long double)m1;
    b2 = (long double)y3 - (long double)x3 * (long double)m2;
    int x, y;
    if (!k1) {
        if (!k2) {
            x = ftol(((long double)b2 - (long double)b1) / ((long double)m1 - (long double)m2));
            y = ftol((long double)x * (long double)m1 + (long double)b1);
        } else if (k2 == -1) {
            x = x3;
            y = ftol((long double)x * (long double)m1 + (long double)b1);
        } else {
            y = y3;
            x = ftol(((long double)y - (long double)b1) / (long double)m1);
        }
    } else if (k1 == -1) {
        x = x1;
        y = k2 ? y3 : ftol((long double)x * (long double)m2 + (long double)b2);
    } else {
        y = y1;
        x = k2 ? x3 : ftol(((long double)y - (long double)b2) / (long double)m2);
    }
    out[0] = x;
    out[1] = y;
    return 1;
}
