/* The game logic: the main loop (0x100af179-0x100b0837), the engine's game callback, run as
 * often as the engine can. It keeps the game clock in 5 ms steps (at most 200 steps at a
 * time), counts the movement and weapon ticks due, runs the network, the movement, weapon
 * and AI loops, the display and page flips at their rates, the autosave, and the debug FPS
 * display. In a network game it also counts down the timeouts of players who stopped
 * answering. */
#include "iso_core.h"
#include "debugtime.h"
#include "view.h"
#include "mouse.h"
#include "loop.h"
#include "netflags.h"
#include "movement.h"
#include "weapons.h"
#include "mainloop.h"

#define CS_NET ((LPCRITICAL_SECTION)ORIG_PTR(0x1017bc28))
#define CS_TICK ((LPCRITICAL_SECTION)ORIG_PTR(0x1017c788))
#define RUNNING G(int32_t, 0x1017baf8)
#define MSG ((uint32_t *)ORIG_PTR(0x101572c0))
#define TIMEOUT(p) I(int16_t, (p) * ISO_PLAYER_SIZE + 0x2fc5)
#define NETFLAGS(p) I(uint16_t, (p) * ISO_PLAYER_SIZE + 0x2fc7)
#define net_lag_out ORIG(void(__cdecl *)(void), 0x1003150d)
#define net_player_dropped ORIG(void(__cdecl *)(uint32_t), 0x10041c26)
#define IsoNetWindow ORIG(void(__stdcall *)(uint8_t *, int, int, int), 0x10046d25)
#define net_send_ticks ORIG(void(__cdecl *)(void), 0x10033cc8)
#define net_ping ORIG(void(__cdecl *)(void), 0x10041fe4)
#define net_sync_a ORIG(void(__cdecl *)(void), 0x1004275c)
#define net_sync_b ORIG(void(__cdecl *)(void), 0x1003eed5)
#define IsoQuickSaveGame ORIG(void(__cdecl *)(uint8_t *, int), 0x10085bae)
#define IsoAILoop ORIG(void(__stdcall *)(uint8_t *), 0x100ca3a9)
#define BF_DrawTextShadow ORIG(int(__cdecl *)(void *, uint32_t, uint32_t, int, int, int, int, const char *, int, int), 0x101120c8)
#define orig_sprintf ORIG(int(__cdecl *)(char *, const char *, ...), 0x100f6f40)

/* time since `then`, as the original counts it across the wrap of the clock */
static uint32_t since(uint32_t now, uint32_t then) { return now < then ? 0xffffffff - then + now : now - then; }

/* adds `steps` to a counter, less any time owed (iso+0xd8c3). As in the original, the time
 * owed is cleared before it is subtracted, so all the steps count */
static void add_steps(uint32_t off, uint32_t steps)
{
    if (!I(uint32_t, 0xd8c3)) {
        I(uint32_t, off) += steps;
    } else if (I(uint32_t, 0xd8c3) < steps) {
        I(uint32_t, 0xd8c3) = 0;
        I(uint32_t, off) += steps - I(uint32_t, 0xd8c3);
    } else {
        I(uint32_t, 0xd8c3) -= steps;
    }
}

static void ticks_due(uint32_t base, uint32_t count, uint32_t rate)
{
    uint32_t d = I(uint32_t, 0xd877) - I(uint32_t, base);
    uint32_t per = I(uint32_t, rate) / 5;
    if (per && per < d) {
        uint32_t n = d / per;
        I(uint32_t, count) += n;
        I(uint32_t, base) += per * n;
    }
}

static void net_timeouts(uint32_t steps)
{
    if (I(int32_t, 0x1b17) <= 1)
        return;
    if (!I(int32_t, 0x1af1) && I(uint8_t, 0xd90f))
        IsoNetWindow(g_iso, 0, I(uint8_t, 0xd916), 0x2d);
    I(uint32_t, 0xd918) += steps * 5;
    if (I(uint32_t, 0xd918) < 1000)
        return;
    for (int32_t p = 0; p < I(int32_t, 0x1b1f); p++)
        if ((NETFLAGS(p) >> 9 & 1) && I(uint8_t, 0xd916) != (uint32_t)p && ((NETFLAGS(p) & 1) || (NETFLAGS(p) >> 2 & 1)) &&
            !(NETFLAGS(p) >> 3 & 1)) {
            if (TIMEOUT(p))
                TIMEOUT(p)--;
            if (!TIMEOUT(p)) {
                iso_debug_print();
                net_player_dropped((uint32_t)p + 1);
            }
        }
    if (I(int32_t, 0x1b09))
        I(int32_t, 0x1b09)--;
    if (TIMEOUT(I(uint8_t, 0xd916)))
        TIMEOUT(I(uint8_t, 0xd916))--;
    if (!I(int32_t, 0x1b09)) {
        iso_debug_print();
        net_player_dropped(I(uint8_t, 0xd916) + 1u);
    }
    I(uint32_t, 0x1b05) = I(uint8_t, 0xd916);
    iso_debug_print();
    I(uint32_t, 0xd918) -= 1000;
}

void __stdcall main_loop(uint8_t *iso)
{
    g_iso = iso;
    if (!iso)
        return;
    RtDominionService(I(void *, ISO_APP));
    if (I(uint32_t, 0xd936)) {
        if (!I(int32_t, 0xd813)) {
            /* a script message at an interval (iso+0xd93a) */
            uint32_t now = timeGetTime();
            uint32_t t = since(now, I(uint32_t, 0xd93e));
            uint32_t left = I(uint32_t, 0xd93a) - I(uint32_t, 0xd942);
            if (t <= left)
                left = t;
            I(uint32_t, 0xd942) += left;
            if (I(uint32_t, 0xd93a) <= I(uint32_t, 0xd942)) {
                I(uint32_t, 0xd942) = 0;
                memset(ORIG_PTR(0x1017df60), 0, 0x22);
                G(uint32_t, 0x1017df62) = I(uint32_t, 0xd936);
                G(uint8_t, 0x1017df69) = 0;
                RtRunMsg(I(void *, ISO_APP), ORIG_PTR(0x1017df60));
            }
            I(uint32_t, 0xd93e) = now;
        } else {
            I(uint32_t, 0xd936) = 0;
            I(uint32_t, 0xd93a) = 0;
        }
    }
    I(uint32_t, 0xd911) = 0;
    if (I(uint32_t, 0xd863)) {
        uint32_t t = since(timeGetTime(), I(uint32_t, 0xd7fb));
        if (t >= 5) {
            uint32_t q = t / 5;
            uint32_t steps = q < 200 ? q : 200;
            if (I(int32_t, 0x175) && I(int32_t, 0xd813) && !I(uint8_t, 0xd90f)) {
                I(uint32_t, 0xd918) += q * 5;
                if (I(uint32_t, 0xd918) >= 1000) {
                    if (!TIMEOUT(I(int32_t, 0x1b65)))
                        net_lag_out();
                    else
                        TIMEOUT(I(int32_t, 0x1b65))--;
                    iso_debug_print();
                    I(uint32_t, 0xd918) = 0;
                }
            }
            if (I(int32_t, 0x175) && I(uint8_t, 0xd949)) {
                if (I(int32_t, 0xd813))
                    EnterCriticalSection(CS_NET);
                for (int32_t p = 0; p < I(int32_t, 0x1b1f); p++)
                    if (NETFLAGS(p) >> 5 & 1) {
                        iso_debug_print();
                        net_player_dropped((uint32_t)p + 1);
                        NETFLAGS(p) &= 0xffdf;
                    }
                I(uint8_t, 0xd949) = 0;
                if (I(int32_t, 0xd813))
                    LeaveCriticalSection(CS_NET);
            }
            if (!I(uint8_t, 0x180)) {
                if (I(int32_t, 0x1af1))
                    IsoNetWindow(g_iso, 1, -1, -1);
                if (I(int32_t, 0xd813))
                    EnterCriticalSection(CS_TICK);
                if ((!I(int32_t, 0x175) || I(uint8_t, 0xd90f)) &&
                    (I(uint8_t, 0x180) || (I(uint8_t, 0x17e) && I(int32_t, 0x175)))) {
                    add_steps(0xd8bf, steps);
                } else if (!I(uint8_t, 0x17e) && !I(uint8_t, 0x180) && I(int32_t, 0x17f5) == 4 &&
                           (!I(int32_t, 0x175) || I(uint8_t, 0xd90f))) {
                    /* the game runs: its clock, and the movement and weapon ticks due */
                    add_steps(0xd877, steps);
                    if (I(int32_t, 0xd8d8) && I(uint32_t, 0xd8e0) <= I(uint32_t, 0xd877)) {
                        I(uint32_t, 0xd863) = I(uint32_t, 0xd8e4);
                        I(uint32_t, 0xd897) = I(uint32_t, 0xd8e8);
                        I(uint32_t, 0xd8d8) = 0;
                        I(uint32_t, 0xd8e0) = 0;
                    }
                    ticks_due(0xd87b, 0x17e5, 0xd863);
                    ticks_due(0xd87f, 0x17f1, 0xd897);
                }
                if (I(int32_t, 0xd813))
                    LeaveCriticalSection(CS_TICK);
            } else {
                if (I(int32_t, 0xd813))
                    EnterCriticalSection(CS_NET);
                net_timeouts(steps);
                if (I(int32_t, 0xd813))
                    LeaveCriticalSection(CS_NET);
            }
            I(uint32_t, 0xd7fb) += q * 5;
            if (!I(uint32_t, 0xd7fb))
                I(uint32_t, 0xd7fb) = 1;
            if (I(int32_t, 0x175) && I(int32_t, 0xd813)) {
                EnterCriticalSection(CS_NET);
                uint32_t e = since(timeGetTime(), I(uint32_t, 0xd873));
                uint32_t every = I(int32_t, 0x148) ? I(uint32_t, 0xd89b) : I(uint32_t, 0xd857) * 2 + I(uint32_t, 0xd89b);
                if (every <= e)
                    net_send_ticks();
                if (I(uint8_t, 0xd90f) && I(uint32_t, 0xd922) <= since(timeGetTime(), I(uint32_t, 0xd91e)))
                    net_ping();
                if (I(uint8_t, 0xd90f)) {
                    if (!I(uint32_t, 0xd92e)) {
                        net_sync_a();
                        net_sync_b();
                    } else {
                        uint32_t m = I(uint32_t, 0xd92e) < steps ? I(uint32_t, 0xd92e) : steps;
                        I(uint32_t, 0xd92e) -= m;
                    }
                }
                LeaveCriticalSection(CS_NET);
            }
        }
    }
    if (I(int32_t, 0x175) && !I(int32_t, 0x17dd)) {
        uint32_t t0 = timeGetTime();
        net_pump(iso);
        uint32_t took = timeGetTime() - t0;
        I(uint32_t, 0xbee2) = (I(uint32_t, 0xbee2) + took) >> 1;
    }
    if (I(uint32_t, 0xd863) && RUNNING && I(uint32_t, 0x17e1) < I(uint32_t, 0x17e5)) {
        uint32_t t0 = timeGetTime();
        IsoMovementLoop(iso);
        uint32_t took = timeGetTime() - t0;
        I(uint32_t, 0xbede) = (I(uint32_t, 0xbede) + took) >> 1;
    }
    if (I(uint32_t, 0xd897) && RUNNING && I(uint32_t, 0x17ed) < I(uint32_t, 0x17f1)) {
        uint32_t t0 = timeGetTime();
        IsoWeaponMovementLoop(iso, 1);
        uint32_t took = timeGetTime() - t0;
        I(uint32_t, 0xbeea) = (I(uint32_t, 0xbeea) + took) >> 1;
    }
    if (I(uint32_t, 0xd84f) && I(int32_t, ISO_HAVE_SPRITES) && RUNNING) {
        if (since(timeGetTime(), I(uint32_t, 0xd883)) > 10) {
            view_scroll(0, 0, 0);
            I(uint32_t, 0xd883) = timeGetTime();
            if (I(int32_t, 0xd79f)) {
                IsoDisplayLoop(iso);
                cursor_follow();
                SpriteAnimate(I(void *, 0x181e));
                SpriteAnimate(I(void *, 0x181e));
            }
        }
    } else if (I(int32_t, 0xd79f)) {
        cursor_follow();
    }
    if (I(uint32_t, 0xd84f) && RUNNING) {
        uint32_t t0 = timeGetTime();
        uint32_t e = since(t0, I(uint32_t, 0xd853));
        uint32_t rate = I(int32_t, 0xd79f) ? I(uint32_t, 0xd83f) : I(uint32_t, 0xd84f);
        if (rate < e) {
            I(uint32_t, 0xd853) = timeGetTime();
            if (!I(int32_t, 0xd79f)) {
                IsoDisplayLoop(iso);
                I(int32_t, 0x17c9)++;
            } else {
                uint32_t f = RtFlipPages(I(void *, ISO_APP));
                if (!f) {
                    cursor_follow();
                } else {
                    I(int32_t, 0x17d1)++;
                    I(int32_t, 0x17c9)++;
                    I(uint32_t, 0xbed6) = (I(uint32_t, 0xbed6) + f) >> 1;
                }
            }
            I(uint32_t, 0x17d9) = timeGetTime() - I(uint32_t, 0x17d5);
            if (I(uint32_t, 0x17d9) > 1000) {
                I(int32_t, 0xbeda) = (I(int32_t, 0x17c9) + I(int32_t, 0xbeda)) / 2;
                I(int32_t, 0x17c9) = 0;
                I(uint32_t, 0x17d5) = timeGetTime();
            }
            uint32_t took = timeGetTime() - t0;
            if (!I(int32_t, 0xd79f))
                I(uint32_t, 0xbed6) = (I(uint32_t, 0xbed6) + took) >> 1;
        } else if (I(int32_t, 0xd79f)) {
            cursor_follow();
        }
    } else if (I(int32_t, 0xd79f) && I(uint32_t, 0xd83f)) {
        uint32_t e = since(timeGetTime(), I(uint32_t, 0xd843));
        cursor_follow();
        if (I(uint32_t, 0xd83f) < e) {
            SpriteAnimate(I(void *, 0x181e));
            SpriteAnimate(I(void *, 0x181e));
            RtFlipPages(I(void *, ISO_APP));
            I(uint32_t, 0xd843) = timeGetTime();
        }
    }
    if (I(uint32_t, 0xd837) && RUNNING) {
        uint32_t t0 = timeGetTime();
        if (I(uint32_t, 0xd837) < since(t0, I(uint32_t, 0xd83b))) {
            I(uint32_t, 0xd83b) = timeGetTime();
            IsoAILoop(iso);
            uint32_t took = timeGetTime() - t0;
            I(uint32_t, 0xbece) = (I(uint32_t, 0xbece) + took) >> 1;
        }
    }
    uint32_t now = timeGetTime();
    if (I(uint32_t, 0x1083) && !I(int32_t, 0x175) && I(uint32_t, 0xd863) && RUNNING && !I(int32_t, 0x168) &&
        I(uint32_t, 0x1083) < now - I(uint32_t, 0x107f)) {
        IsoQuickSaveGame(g_iso, 1);
        I(uint32_t, 0x107f) = now;
    }
    if (I(uint8_t, 0xeeb) & 0x20) {
        /* the debug FPS display */
        uint32_t t = timeGetTime();
        if (t - I(uint32_t, 0xbee6) > 1000) {
            if (I(int32_t, 0x175))
                orig_sprintf((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x10157e50), I(uint32_t, 0xbeda),
                             I(uint32_t, 0xd863) * I(uint32_t, 0xd857), I(uint32_t, 0xbee2) + I(uint32_t, 0xbede),
                             I(uint32_t, 0xbed2), I(uint32_t, 0xbed6), I(uint32_t, 0xbeea));
            else
                orig_sprintf((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x10157e98), I(uint32_t, 0xbeda),
                             I(uint32_t, 0xbede), I(uint32_t, 0xbed2), I(uint32_t, 0xbed6), I(uint32_t, 0xbece),
                             I(uint32_t, 0xbeea));
            BF_DrawTextShadow(I(void *, 0x1812), I(uint32_t, 0xf0e), I(uint32_t, 0x17c5), -1, 0, 0, 0x22,
                              (const char *)(g_iso + 0xdea), 0, 0);
            MSG[1] = 0;
            MSG[2] = 0;
            MSG[3] = 0;
            SpriteSendMessage(I(void *, 0x181e), I(void *, 0xeee), 4, MSG);
            I(uint32_t, 0xbee6) = t;
        }
    }
}
