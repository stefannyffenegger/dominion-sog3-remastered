/* The game logic, 0x10028603-0x1002c74e: carrying out one scenario description record
 * (from IsoLoadSDCFile's loader for the shipped .sdc files, and the text loader): the
 * main records (subtype 0: timers, hot spots, mission info) and the game settings,
 * units, objects, AI and events (by subtype). Ported a part at a time: subtypes not
 * rewritten yet run the original's code through a trampoline. */
#include "iso_core.h"
#include "sdlexec.h"

#define message_box ORIG(void(__cdecl *)(int, const char *, const char *, int), 0x1005c515)
#define timer_set ORIG(void(__cdecl *)(int, int32_t, int32_t, int, void *), 0x1002de13)

#define S(addr) ((const char *)ORIG_PTR(addr))
#define TEXT_BUF ((char *)(g_iso + 0xdea))
#define REC(i) (I(uint8_t *, 0xc2e5) + (i) * 0x5a)
#define V(r, n) P(int32_t, r, 10 + (n) * 4) /* the record's n-th value */
#define TITLE S(0x101351fc)                 /* "IsoLoadSDLFile() Error" */

/* the original's code after its 9-byte prologue (the hook overwrote the first 5) */
void *g_sdl_exec_body;
__attribute__((naked)) static int __cdecl sdl_exec_orig(uint32_t rec)
{
    (void)rec;
    __asm__ volatile("pushl %ebp\n\t"
                     "movl %esp, %ebp\n\t"
                     "subl $0x22c, %esp\n\t"
                     "jmp *_g_sdl_exec_body");
}

static const char *designer(uint32_t fallback)
{
    return I(char *, 0x1918) ? I(char *, 0x1918) : S(fallback);
}

/* a message box about a bad record; the record fails */
static int bad(void)
{
    message_box(0, TEXT_BUF, TITLE, 0);
    return 0;
}

/* subtype 0: the main records, by type */
static int main_record(uint32_t i, uint8_t *r, int32_t type)
{
    switch (type) {
    case 10: { /* TIMER_EVENT id, time, on, relative, repeat */
        int32_t id = V(r, 0), t = V(r, 1);
        uint8_t on = P(uint8_t, r, 0x12), relative = P(uint8_t, r, 0x16), repeat = P(uint8_t, r, 0x1a);
        int32_t at, period;
        DebugStr(S(0x10135214), id, t, on, relative, repeat);
        if (id < 1 || id > 0x20) {
            wsprintfA(TEXT_BUF, S(0x10135248), id, designer(0x10135234));
            return bad();
        }
        if (on) {
            if (relative) {
                at = I(int32_t, 0x17dd) + t;
                period = repeat ? t : 0;
            } else {
                at = t;
                period = 0;
            }
            timer_set(id, at, period, 0, NULL);
        }
        break;
    }
    case 0x14: { /* HOTSPOT_EVENT id, player, kind, sectors... */
        int32_t id = V(r, 0), player = V(r, 1), kind = V(r, 2);
        for (int k = 0; k < P(int16_t, r, 8) - 3; k++) {
            int32_t s = V(r, 3 + k);
            if (s < 0 || s >= I(int32_t, 0xc2f1) || player < 0 || player >= 8) {
                wsprintfA(TEXT_BUF, S(0x101352ac), id, player, kind, s, designer(0x10135298));
                return bad();
            }
            P(uint8_t, I(uint8_t *, 0xc2eb) + s * 0x1e, 0x1a) |= (uint8_t)(1 << (player & 0x1f));
            DebugStr(S(0x1013527c), s, player);
        }
        break;
    }
    case 0x28:
        DebugStr(S(0x10135300));
        break;
    case 0x32:
        DebugStr(S(0x1013531c));
        G(uint8_t, 0x101347c8) = P(uint8_t, r, 10);
        DebugStr(S(0x10135338), G(uint8_t, 0x101347c8));
        break;
    case 0x3c:
        DebugStr(S(0x10135350));
        break;
    case 0x46: /* the three cinematics */
        I(uint8_t *, 0x1908) = r + 10;
        I(uint8_t *, 0x190c) = r + 0x1e;
        I(uint8_t *, 0x1910) = r + 0x32;
        break;
    case 0x50: /* the briefing's wave */
        DebugStr(S(0x10135370));
        DebugStr(S(0x10135388), r + 10);
        I(uint8_t *, 0x18d8) = r + 10;
        break;
    case 0x5a: /* designer, version, date */
        I(uint8_t *, 0x1918) = r + 10;
        I(uint8_t *, 0x191c) = r + 0x1e;
        I(uint8_t *, 0x1914) = r + 0x32;
        break;
    default:
        wsprintfA(TEXT_BUF, S(0x101353a8), i & 0xffff, type, 0, designer(0x10135394));
        return bad();
    }
    return 1;
}

/* 0x10028603: record `i`; 0 when it is bad (a message box has said why) */
int __cdecl sdl_exec(uint32_t i)
{
    uint8_t *r = REC(i & 0xffff);
    int32_t type = P(int32_t, r, 0), sub = P(int32_t, r, 4);
    if (sub == 0)
        return main_record(i, r, type);
    if (!g_sdl_exec_body)
        g_sdl_exec_body = ORIG_PTR(0x1002860c);
    return sdl_exec_orig(i);
}
