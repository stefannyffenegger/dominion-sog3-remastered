/* The game logic, from the network code (0x100313e0 to about 0x10048000, mostly not
 * ported yet: only network games reach it): the parts that run in every game. */
#include "iso_core.h"
#include "netflags.h"

#define NET_LOCK ((LPCRITICAL_SECTION)ORIG_PTR(0x1017bc28))

/* 0x1003f390: the network delay in ticks (two counters, read under the lock while the
 * network thread runs) */
int32_t __cdecl net_delay(void)
{
    if (I(int32_t, 0xd813))
        EnterCriticalSection(NET_LOCK);
    int32_t n = I(int32_t, 0xd857) + I(int32_t, 0xd86b);
    if (I(int32_t, 0xd813))
        LeaveCriticalSection(NET_LOCK);
    return n;
}

/* 0x10042b05 */
void __stdcall IsoSetServerFlag(uint8_t *iso, int32_t server)
{
    g_iso = iso;
    I(int32_t, 0x148) = server;
}

/* 0x10042b2e: whether this is the host of a network game */
int32_t __stdcall IsoGetServerFlag(uint8_t *iso)
{
    g_iso = iso;
    return I(int32_t, 0x175) && I(int32_t, 0x148) ? 1 : 0;
}

/* 0x10042b78 */
void __stdcall IsoSetNetworkPlay(uint8_t *iso, int32_t on, int32_t value)
{
    g_iso = iso;
    I(int32_t, 0x175) = on;
    I(int32_t, 0x1ae1) = -1;
    I(int32_t, 0x1ae5) = -1;
    I(int32_t, 0xd8d4) = value;
}

/* 0x10042bce */
int32_t __stdcall IsoGetNetworkPlay(uint8_t *iso)
{
    g_iso = iso;
    return I(int32_t, 0x175);
}

#define MSG_LOCK ((LPCRITICAL_SECTION)ORIG_PTR(0x1017bb68))
#define message_box ORIG(void(__cdecl *)(int, const char *, const char *, int), 0x1005c515)
/* the network code's message handler, not ported yet */
#define net_handle_msg ORIG(uint32_t(__stdcall *)(uint8_t *, int32_t, uint32_t, uint32_t *, int32_t *), 0x100443d3)
#define orig_memcpy ORIG(void *(__cdecl *)(void *, const void *, uint32_t), 0x100f5530)
#define orig_memset ORIG(void *(__cdecl *)(void *, int, uint32_t), 0x100f4c10)
#define debug_printf ORIG(void(__cdecl *)(int, const char *, ...), 0x10095e98)
#define calc_crc32 ORIG(uint32_t(__cdecl *)(const void *, int32_t), 0x10112044) /* WAVEPLAY ordinal 48 */

/* 0x10045c72: hands the received network messages to the game, up to 256 a call; when
 * starting or joining failed, says so and leaves the game. 1, or RtRunMsg's result */
int32_t __stdcall net_pump(uint8_t *iso)
{
    g_iso = iso;
    if (I(int32_t, 0xd904)) {
        I(int32_t, 0xd904) = 0;
        if (!I(int32_t, 0x148))
            message_box(0, (const char *)ORIG_PTR(0x10139c90), (const char *)ORIG_PTR(0x10139c88), 0);
        else
            message_box(0, (const char *)ORIG_PTR(0x10139c70), (const char *)ORIG_PTR(0x10139c68), 0);
        I(int32_t, 0x168) = 1;
        uint8_t msg[0x22];
        orig_memset(msg, 0, sizeof msg);
        *(uint32_t *)(msg + 2) = I(uint32_t, 0x1aba);
        msg[9] = 3;
        *(uint32_t *)(msg + 0xa) = 2;
        *(uint32_t *)(msg + 0xe) = 0;
        *(uint32_t *)(msg + 0x12) = 1;
        return RtRunMsg(I(void *, 0x180e), msg);
    }
    if (I(int32_t, 0xd803) || !I(int32_t, 0x1ac7) || I(int32_t, 0x1ae9) == I(int32_t, 0x1aed))
        return 1;
    for (uint32_t n = 0;; n++) {
        if (I(int32_t, 0xe020)) /* IsoKaboom: a crash on purpose */
            orig_memcpy(NULL, NULL, 1);
        if (n >= 0x100 || !I(int32_t, 0x1ac7))
            break;
        if (I(int32_t, 0xd813))
            EnterCriticalSection(MSG_LOCK);
        if (I(int32_t, 0x1ae9) == I(int32_t, 0x1aed)) {
            if (I(int32_t, 0xd813))
                LeaveCriticalSection(MSG_LOCK);
            break;
        }
        if (I(int32_t, 0xd813))
            LeaveCriticalSection(MSG_LOCK);
        uint8_t *m = I(uint8_t *, 0x1ac7) + I(int32_t, 0x1ae9);
        int32_t ok;
        net_handle_msg(iso, *(int32_t *)m, *(uint32_t *)(I(uint8_t *, 0x1ac7) + I(int32_t, 0x1ae9) + 6),
                       (uint32_t *)(I(uint8_t *, 0x1ac7) + I(int32_t, 0x1ae9) + 10), &ok);
        if (!ok || !I(int32_t, 0x1ac7))
            break;
        if (I(int32_t, 0xd813))
            EnterCriticalSection(MSG_LOCK);
        I(int32_t, 0x1ae9) += *(int16_t *)(m + 4);
        if ((uint32_t)I(int32_t, 0x1ae9) + 0x200 > 0x20000)
            I(int32_t, 0x1ae9) = 0;
        if (I(int32_t, 0xd813))
            LeaveCriticalSection(MSG_LOCK);
    }
    return 1;
}

/* 0x10046342: the resources for the scenario's "show text" */
void __stdcall IsoInitSDLShowText(uint8_t *iso, uint32_t res1, int32_t a, uint32_t res2, int32_t b, int32_t c)
{
    g_iso = iso;
    I(uint32_t, 0xd9b2) = res1;
    I(void *, 0xd9b6) = RtGetResource(I(void *, 0x180e), res1)[1];
    I(int32_t, 0xd9ba) = a;
    I(uint32_t, 0xd9be) = res2;
    I(void *, 0xd9c2) = RtGetResource(I(void *, 0x180e), res2)[1];
    I(int32_t, 0xd9c6) = b;
    I(int32_t, 0xd9a6) = c;
}

/* 0x10046953 */
uint8_t __stdcall IsoGetWXPInvalid(uint8_t *iso)
{
    g_iso = iso;
    return I(uint8_t, 0xbfc0);
}

/* the CRC of a tile or mask library (two or three dimensions) */
static uint32_t lib_crc(uint8_t *lib, int three_first)
{
    int32_t n;
    if (P(int32_t, lib, 0x10) == 2)
        n = P(int32_t, lib, 0x2c) * P(int32_t, lib, 0x28);
    else
        n = three_first ? P(int32_t, lib, 0x30) * P(int32_t, lib, 0x28) * P(int32_t, lib, 0x2c)
                        : P(int32_t, lib, 0x2c) * P(int32_t, lib, 0x30) * P(int32_t, lib, 0x28);
    return calc_crc32(*(void **)lib, n);
}

/* 0x1004697c: the CRCs the players compare before a network game: 1-5 the libraries
 * at 0x197c, 0x1990, 0x18ec, 0x1968, 0x18e4 */
int32_t __stdcall IsoInitCRCCheck(uint8_t *iso, int32_t which)
{
    g_iso = iso;
    switch (which) {
    case 1: I(uint32_t, 0xd8f0) = lib_crc(I(uint8_t *, 0x197c), 0); break;
    case 2: I(uint32_t, 0xd8f4) = lib_crc(I(uint8_t *, 0x1990), 0); break;
    case 3: I(uint32_t, 0xd8f8) = lib_crc(I(uint8_t *, 0x18ec), 1); break;
    case 4: I(uint32_t, 0xd8fc) = lib_crc(I(uint8_t *, 0x1968), 1); break;
    case 5: I(uint32_t, 0xd900) = lib_crc(I(uint8_t *, 0x18e4), 0); break;
    }
    return 1;
}

/* 0x10046cde */
void __stdcall IsoDebugScriptMessageProc(uint8_t *iso, int32_t starting)
{
    g_iso = iso;
    debug_printf(3, (const char *)ORIG_PTR(starting ? 0x10139cec : 0x10139d0c));
}

/* 0x100472cc */
void __stdcall IsoInitNetworkSprites(uint8_t *iso, int32_t a, int32_t b)
{
    g_iso = iso;
    I(int32_t, 0xbe75) = a;
    I(int32_t, 0xbe7d) = b;
    I(int32_t, 0xbe71) = 0;
    I(int32_t, 0xbec1) = 0;
}
