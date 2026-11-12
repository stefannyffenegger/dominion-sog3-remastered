/* A test aid: DOMINION_ISO_STATELOG=<Windows path> writes one line per game tick, just
 * before the tick's game logic runs (the call at 0x1008d375 in IsoMovementLoop is
 * redirected here): the tick, the number of units and a hash of every unit's state (position,
 * facing, health, action, flags). Two runs with the same input give the same lines when
 * the game logic behaves the same; compare the ticks both runs logged. With
 * DOMINION_ISO_STATELOG_UNITS=1 each unit's fields are written too. With
 * DOMINION_ISO_STATELOG_ORDERS=1 the local player's units get move orders at fixed ticks
 * (Human mission 1: into the enemy at tick 40, on at tick 500), input that does not
 * depend on timing. */
#include "iso_core.h"
#include "statelog.h"
#include "loop.h"

/* the radar's move order: player, x, y, 2, 10, 0, flag */
#define order_move ORIG(void(__cdecl *)(int, int, uint32_t, int, uint32_t, int, int), 0x100b39aa)

static HANDLE g_log = INVALID_HANDLE_VALUE;
static int g_log_units, g_orders;
static uint32_t g_last_tick = 0xffffffff;

static void out(const char *text)
{
    DWORD n;
    WriteFile(g_log, text, (DWORD)lstrlenA(text), &n, NULL);
}

static uint32_t fnv(uint32_t h, const void *p, int n)
{
    for (int i = 0; i < n; i++)
        h = (h ^ ((const uint8_t *)p)[i]) * 16777619u;
    return h;
}

/* the fields hashed: offset, size. Left out, as the display changes them every frame:
 * 0x2d (selection display), 0x3e bit 0x40 (drawn), 0x10e bit 0x100 (flip pending). */
static const uint16_t fields[][2] = {
    {0x0c, 4}, {0x10, 4}, {0x14, 1}, {0x2e, 1}, {0x52, 2}, {0x54, 2}, {0x6c, 12},
    {0x78, 6}, {0xd6, 2}, {0x113, 2}, {0x119, 4}, {0x123, 2}, {0x145, 2},
    {0x147, 2}, {0x157, 2}, {0x15a, 1}, {0x19d, 2}, {0x1a1, 2},
};

static void log_tick(void)
{
    uint32_t tick = I(uint32_t, 0x17dd);
    if (tick == g_last_tick)
        return;
    g_last_tick = tick;
    uint32_t h = 2166136261u;
    int n = 0;
    char line[160];
    for (int i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
        int32_t id = I(int32_t *, ISO_UNIT_LIST)[i];
        uint8_t *u = id > 0 && id < 2000 ? iso_unit(id) : NULL;
        if (!u)
            continue;
        n++;
        h = fnv(h, &id, 4);
        for (unsigned f = 0; f < sizeof fields / sizeof *fields; f++)
            h = fnv(h, u + fields[f][0], fields[f][1]);
        uint32_t flags[2] = {P(uint32_t, u, 0x3e) & ~0x40u, P(uint32_t, u, 0x10e) & ~0x100u};
        h = fnv(h, flags, sizeof flags);
        if (g_log_units) {
            wsprintfA(line, "  %d t%d p%d (%d,%d) d%d hp%d a%d/%d f%08x/%08x c%d s%d\r\n", id,
                      P(int32_t, u, 0x0c), P(uint8_t, u, 0x14), P(int16_t, u, 0x52), P(int16_t, u, 0x54),
                      P(uint8_t, u, 0x79), P(int16_t, u, 0xd6), P(uint8_t, u, 0x147), P(uint8_t, u, 0x148),
                      P(uint32_t, u, 0x3e) & ~0x40u, P(uint32_t, u, 0x10e) & ~0x100u, P(uint16_t, u, 0x145), P(uint8_t, u, 0x157));
            out(line);
        }
    }
    wsprintfA(line, "%u %d %08x\r\n", tick, n, h);
    out(line);
}

#define TICK_CALL 0x1008d375 /* call <the tick's game logic> (stdcall, the game state) */
#define TICK_FUNC 0x10045c72

static void give_orders(void)
{
    static const struct { uint32_t tick; int x, y; } orders[] = {{40, -200, -250}, {500, -192, 0}};
    for (unsigned i = 0; i < sizeof orders / sizeof *orders; i++)
        if (I(uint32_t, 0x17dd) == orders[i].tick) {
            IsoSelectGroup(g_iso, 0xffffffff, 10, 0, 0);
            order_move(I(int, ISO_LOCAL_PLAYER), orders[i].x, (uint32_t)orders[i].y, 2, 10, 0, I(int, 0xd797) == 0);
        }
}

static void __stdcall tick_stub(uint8_t *iso)
{
    log_tick();
    if (g_orders)
        give_orders();
    ORIG(void(__stdcall *)(uint8_t *), TICK_FUNC)(iso);
}

void statelog_install(void)
{
    char path[MAX_PATH], units[8] = "";
    if (!GetEnvironmentVariableA("DOMINION_ISO_STATELOG", path, sizeof path))
        return;
    g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (g_log == INVALID_HANDLE_VALUE)
        return;
    GetEnvironmentVariableA("DOMINION_ISO_STATELOG_UNITS", units, sizeof units);
    g_log_units = units[0] == '1';
    units[0] = 0;
    GetEnvironmentVariableA("DOMINION_ISO_STATELOG_ORDERS", units, sizeof units);
    g_orders = units[0] == '1';
    uint8_t *p = ORIG(uint8_t *, TICK_CALL);
    if (p[0] != 0xE8)
        return;
    int32_t rel = (int32_t)((uint8_t *)tick_stub - (p + 5));
    DWORD old;
    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    memcpy(p + 1, &rel, 4);
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
}
