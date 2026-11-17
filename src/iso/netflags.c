/* The game logic, from the network code (0x100313e0-0x100443xx, not ported yet: it is
 * only reached in network games): the few parts that run in every game. */
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
