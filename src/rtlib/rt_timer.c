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

#define RT_DELAYED_EVENT 0x3f3f
#define RT_DELAYED_DUE 0x0b80
#define RT_PAUSE_TIME 0x0b78
#define RT_TIMERS_PAUSED 0x95f0
#define event_send ORIG(U(__stdcall *)(U, U, U), 0x10025e97)

/* 0x10024db2: sends the delayed event when its time has come */
void __cdecl delayed_event_service(void)
{
    if (RU(RT_DELAYED_EVENT) && RU(RT_DELAYED_DUE) <= timeGetTime()) {
        event_send(RU(RT_DELAYED_EVENT), 0, 0);
        RU(RT_DELAYED_EVENT) = 0;
    }
}

/* 0x10024e10: runs the handlers of due script timers; one-shot timers go away first */
void __cdecl timers_service(void)
{
    if (RB(RT_TIMERS_PAUSED))
        return;
    for (g_timer_iter = 0; g_timer_iter < (int)RU(RT_TIMER_COUNT); g_timer_iter++) {
        uint8_t *t = timer((U)g_timer_iter);
        if (P(U, t, 0) > timeGetTime())
            continue;
        U handler = P(U, t, 0xc);
        if (!t[0x10])
            timer_delete(g_timer_iter);
        else
            P(U, timer((U)g_timer_iter), 0) = P(U, timer((U)g_timer_iter), 4) + timeGetTime();
        script_run(handler);
    }
}

/* 0x10024f1c: calls back due controller timers */
void __cdecl ctimers_service(void)
{
    if (RB(RT_TIMERS_PAUSED))
        return;
    for (int i = 0; i < (int)RU(RT_CTIMER_COUNT); i++) {
        uint8_t *t = ctimer((U)i);
        if (P(U, t, 0) > timeGetTime())
            continue;
        void *cb = P(void *, t, 0xc);
        U key = P(U, t, 8);
        if (!t[0x10])
            i = ctimer_delete(i);
        else
            P(U, ctimer((U)i), 0) = P(U, ctimer((U)i), 4) + timeGetTime();
        orig_stdcall_unsafe(cb, 1, &key);
    }
}

/* 0x10025037: pauses all timers; unpausing moves them on by the time paused */
void __stdcall timers_pause(uint8_t paused)
{
    U now = timeGetTime();
    RB(RT_TIMERS_PAUSED) = paused;
    if (paused) {
        RU(RT_PAUSE_TIME) = now;
        return;
    }
    U d = now - RU(RT_PAUSE_TIME);
    for (U i = 0; i < RU(RT_TIMER_COUNT); i++)
        P(U, timer(i), 0) += d;
    for (U i = 0; i < RU(RT_CTIMER_COUNT); i++)
        P(U, ctimer(i), 0) += d;
}
