/* Engine core, 0x100248f0-0x10024db2: timers. Scripts have up to 20 timers that run a
 * handler after an interval; the controller layer has up to 8 that call back in C.
 * Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_timer.h"

#define RU(off) R(U, off)
#define RB(off) R(uint8_t, off)

/* script timers: 0x12 bytes each: due time, interval, id, handler, flag */
#define RT_TIMERS 0x3c5b
#define RT_TIMER_COUNT 0x3e57
#define TIMER_SIZE 0x12
#define MAX_TIMERS 20
/* controller timers: 0x11 bytes each: due time, interval, key, callback, flag */
#define RT_CTIMERS 0x18bf
#define RT_CTIMER_COUNT 0x3e27
#define CTIMER_SIZE 0x11
#define MAX_CTIMERS 8
#define RT_SCRIPT_DEBUG 0x95d8

#define g_timer_iter G(int, 0x100871a8) /* the timer being serviced */
#define rt_fatal ORIG(void(__stdcall *)(U, U), 0x10025b0f)
#define rt_script_error ORIG(U(__stdcall *)(U, U), 0x100260f6)

static uint8_t *timer(U i) { return g_rt + RT_TIMERS + i * TIMER_SIZE; }
static uint8_t *ctimer(U i) { return g_rt + RT_CTIMERS + i * CTIMER_SIZE; }

/* 0x1002496f: removes controller timer i; returns the index to go on from */
int __stdcall ctimer_delete(int i)
{
    RU(RT_CTIMER_COUNT)--;
    int after = (int)RU(RT_CTIMER_COUNT) - i;
    if (after)
        memcpy(ctimer((U)i), ctimer((U)i + 1), (U)after * CTIMER_SIZE);
    if (i < (int)RU(RT_CTIMER_COUNT))
        i--;
    return i;
}

/* 0x100248f0: removes the controller timer of `key` */
void __stdcall ctimer_remove(U key)
{
    U i = 0;
    while (i < RU(RT_CTIMER_COUNT) && P(U, ctimer(i), 8) != key)
        i++;
    if (i != RU(RT_CTIMER_COUNT))
        ctimer_delete((int)i);
}

/* 0x10024a9f: removes script timer i */
void __stdcall timer_delete(int i)
{
    RU(RT_TIMER_COUNT)--;
    int after = (int)RU(RT_TIMER_COUNT) - i;
    if (after)
        memcpy(timer((U)i), timer((U)i + 1), (U)after * TIMER_SIZE);
    if (i <= g_timer_iter)
        g_timer_iter--;
}

/* 0x10024a04: removes the script timer `id`, or all of them */
void __stdcall timer_remove(U id, U all)
{
    if (all) {
        RU(RT_TIMER_COUNT) = 0;
        return;
    }
    U i = 0;
    while (i < RU(RT_TIMER_COUNT) && P(U, timer(i), 8) != id)
        i++;
    if (i != RU(RT_TIMER_COUNT))
        timer_delete((int)i);
}

/* 0x10024b2c: sets script timer `id`. With all 20 in use the original reports the
 * error and then writes past the table anyway; kept as is. */
void __stdcall timer_set(U id, U handler, U interval, uint8_t flag)
{
    U i = 0;
    while (i < RU(RT_TIMER_COUNT) && P(U, timer(i), 8) != id)
        i++;
    if (i >= MAX_TIMERS) {
        if (!RB(RT_SCRIPT_DEBUG)) {
            rt_fatal(0x3eb, 0);
            return;
        }
        rt_script_error(0x74, 0);
    }
    if (RU(RT_TIMER_COUNT) == i)
        RU(RT_TIMER_COUNT)++;
    uint8_t *t = timer(i);
    P(U, t, 8) = id;
    P(U, t, 4) = interval;
    P(U, t, 0) = interval + timeGetTime();
    P(U, t, 0xc) = handler;
    t[0x10] = flag;
}

/* 0x10024c69: sets the controller timer of `key` (8 at most; same overrun) */
void __stdcall ctimer_set(U key, U interval, U callback, uint8_t flag)
{
    U i = 0;
    while (i < RU(RT_CTIMER_COUNT) && P(U, ctimer(i), 8) != key)
        i++;
    if (i >= MAX_CTIMERS) {
        if (!RB(RT_SCRIPT_DEBUG)) {
            rt_fatal(0x3eb, 0);
            return;
        }
        rt_script_error(0x74, 0);
    }
    if (RU(RT_CTIMER_COUNT) == i)
        RU(RT_CTIMER_COUNT)++;
    uint8_t *t = ctimer(i);
    P(U, t, 8) = key;
    P(U, t, 4) = interval;
    P(U, t, 0) = interval + timeGetTime();
    P(U, t, 0xc) = callback;
    t[0x10] = flag;
}
