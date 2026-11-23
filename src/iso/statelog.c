/* A test aid: DOMINION_ISO_STATELOG=<Windows path> writes one line per game tick, just
 * before the tick's game logic runs (the call at 0x1008d375 in IsoMovementLoop is
 * redirected here): the tick, the number of units and a hash of every unit's state (position,
 * facing, health, action, flags). Two runs with the same input give the same lines when
 * the game logic behaves the same; compare the ticks both runs logged. With
 * DOMINION_ISO_STATELOG_UNITS=1 each unit's fields are written too (=from-to: only for those
 * ticks, which disturbs the timing less). With
 * DOMINION_ISO_STATELOG_ORDERS=1 the local player's units get move orders at fixed ticks
 * (Human mission 1: into the enemy at tick 40, on at tick 500) and stances (kneel at 60,
 * crawl at 120, stand at 200), input that does not depend on timing. With =2 the base test
 * on Darken mission 12 runs instead: start a Power Plant at tick 100, place one at tick 300,
 * then damage, engineer repair, repair button, upgrade and a sale by tick 560;
 * DOMINION_ISO_STATELOG_SAVE=<file> also saves the game at tick 600. DOMINION_ISO_STATELOG_CLOCK=<ms> gives the game logic (the original
 * ISO and ours) a virtual timeGetTime that advances <ms> per displayed frame, so that the
 * game does not depend on how fast the computer is either. */
#include "iso_core.h"
#include "statelog.h"
#include "loop.h"

/* the radar's move order: player, x, y, 2, 10, 0, flag */
#define order_move ORIG(void(__cdecl *)(int, int, uint32_t, int, uint32_t, int, int), 0x100b39aa)

static HANDLE g_log = INVALID_HANDLE_VALUE;
static int g_log_units, g_orders;
static uint32_t g_units_from, g_units_to = 0xffffffff;
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
            if (fields[f][0] != 0x2e || !I(int, 0x175)) /* in a match, selection is each player's own */
                h = fnv(h, u + fields[f][0], fields[f][1]);
        uint32_t flags[2] = {P(uint32_t, u, 0x3e) & ~0x40u, P(uint32_t, u, 0x10e) & ~0x100u};
        h = fnv(h, flags, sizeof flags);
        if (g_log_units && tick >= g_units_from && tick <= g_units_to) {
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

/* the base test: the interface's own calls, through the original's addresses, so that an
 * original run uses the original. Each step writes a "#" line, which log comparisons skip. */
static uint8_t *base_unit(int i, int32_t *id)
{
    *id = I(int32_t *, ISO_UNIT_LIST)[i];
    return *id > 0 && *id < 2000 ? iso_unit(*id) : NULL;
}

static int32_t base_find_plant(uint32_t player, int type, int x, int y)
{
    for (int i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
        int32_t id;
        uint8_t *u = base_unit(i, &id);
        if (u && P(int32_t, u, 0x0c) == type && P(uint8_t, u, 0x14) == player && P(int16_t, u, 0x52) == x &&
            P(int16_t, u, 0x54) == y)
            return id;
    }
    return 0;
}

/* the first of the player's units of a type range */
static int32_t base_find(uint32_t player, int lo, int hi)
{
    for (int i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
        int32_t id;
        uint8_t *u = base_unit(i, &id);
        if (u && P(uint8_t, u, 0x14) == player && P(int32_t, u, 0x0c) >= lo && P(int32_t, u, 0x0c) <= hi)
            return id;
    }
    return 0;
}

static void base_note(const char *what, int32_t a, int32_t b)
{
    char line[80];
    wsprintfA(line, "# %u %s %d %d\r\n", I(uint32_t, 0x17dd), what, a, b);
    out(line);
}

static void give_base_orders(void)
{
    static char save_name[MAX_PATH];
    enum { POWER_PLANT = 0x20, X = 512, Y = 128 };
    uint32_t tick = I(uint32_t, 0x17dd), player = (uint32_t)I(int, ISO_LOCAL_PLAYER);
    int side = G(uint8_t *, 0x1018b604)[player * 0x143 + 0xd], voice = 0, stagger = 0;
    int type = POWER_PLANT * 4 + side;
    int32_t plant = tick >= 320 ? base_find_plant(player, type, X, Y) : 0, id;
    switch (tick) {
    case 100:
        ORIG(void(__cdecl *)(uint32_t, int, int), 0x1009f224)(player, POWER_PLANT, side);
        break;
    case 300:
        ORIG(void(__cdecl *)(uint32_t, int, int, int, uint32_t, int), 0x1009f112)(player, POWER_PLANT, side, X, Y, 1);
        break;
    case 320: /* damage it, and the mission's own plant */
        id = base_find(player, type, type);
        if (plant)
            P(int16_t, iso_unit(plant), 0xd6) -= 300;
        if (id && id != plant)
            P(int16_t, iso_unit(id), 0xd6) -= 200;
        base_note("damage", plant, id);
        break;
    case 330: /* an engineer repairs the new one */
        id = base_find(player, 0x10, 0x13);
        if (id && plant)
            ORIG(int(__cdecl *)(uint32_t, uint32_t, int *, int *), 0x10019535)((uint32_t)id, (uint32_t)plant, &voice, &stagger);
        base_note("engineer", id, plant);
        break;
    case 340: /* the repair button on the other */
        id = base_find(player, type, type);
        if (id == plant)
            id = 0;
        if (id)
            ORIG(void(__cdecl *)(uint32_t, int), 0x1005b007)((uint32_t)id, 1);
        base_note("repair", id, 0);
        break;
    case 400: /* the upgrade button */
        if (plant)
            ORIG(int(__cdecl *)(uint32_t, int), 0x1005b314)((uint32_t)plant, 1);
        base_note("upgrade", plant, 0);
        break;
    case 600: /* with DOMINION_ISO_STATELOG_SAVE=<file>: what the save screen's button calls */
        if (GetEnvironmentVariableA("DOMINION_ISO_STATELOG_SAVE", save_name, sizeof save_name)) {
            int ok = ORIG(int(__cdecl *)(char *, int), 0x10084ed7)(save_name, 0);
            ORIG(int(__cdecl *)(void), 0x1007fa22)();
            base_note("save", ok, 0);
        }
        break;
    case 560:
        if (plant)
            ORIG(uint32_t(__stdcall *)(uint8_t *, uint32_t), 0x10043010)(g_iso, (uint32_t)plant);
        base_note("sell", plant, 0);
        break;
    }
}

static void give_orders(void)
{
    if (g_orders == 2) {
        give_base_orders();
        return;
    }
    static const struct { uint32_t tick; int x, y; } orders[] = {{40, -200, -250}, {500, -192, 0}};
    /* stances for the selected units: kneel, crawl, stand, given with the interface's
     * buttons (actions 9, 8, 10; through the original's address, so original runs use
     * the original) */
    static const struct { uint32_t tick, state; } stances[] = {{60, 1}, {120, 2}, {200, 0}};
    for (unsigned i = 0; i < sizeof orders / sizeof *orders; i++)
        if (I(uint32_t, 0x17dd) == orders[i].tick) {
            IsoSelectGroup(g_iso, 0xffffffff, 10, 0, 0);
            order_move(I(int, ISO_LOCAL_PLAYER), orders[i].x, (uint32_t)orders[i].y, 2, 10, 0, I(int, 0xd797) == 0);
        }
    for (unsigned i = 0; i < sizeof stances / sizeof *stances; i++)
        if (I(uint32_t, 0x17dd) == stances[i].tick)
            ORIG(void(__stdcall *)(uint8_t *, int), 0x10017124)(g_iso, 10 - (int)stances[i].state);
}

static void __stdcall tick_stub(uint8_t *iso)
{
    log_tick();
    if (g_orders)
        give_orders();
    ORIG(void(__stdcall *)(uint8_t *), TICK_FUNC)(iso);
}

static void clock_install(void);

/* the tick's game logic as our IsoMovementLoop calls it: through the log when it is on */
void(__stdcall *statelog_tick)(uint8_t *iso);

void statelog_install(void)
{
    char path[MAX_PATH], units[32] = "";
    clock_install();
    if (!GetEnvironmentVariableA("DOMINION_ISO_STATELOG", path, sizeof path))
        return;
    g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (g_log == INVALID_HANDLE_VALUE)
        return;
    GetEnvironmentVariableA("DOMINION_ISO_STATELOG_UNITS", units, sizeof units);
    g_log_units = units[0] != 0;
    if (units[0] && units[0] != '1') { /* a tick range "from-to" */
        char *p = units;
        g_units_from = 0;
        while (*p >= '0' && *p <= '9')
            g_units_from = g_units_from * 10 + (uint32_t)(*p++ - '0');
        if (*p == '-') {
            g_units_to = 0;
            for (p++; *p >= '0' && *p <= '9'; p++)
                g_units_to = g_units_to * 10 + (uint32_t)(*p - '0');
        }
    }
    units[0] = 0;
    GetEnvironmentVariableA("DOMINION_ISO_STATELOG_ORDERS", units, sizeof units);
    g_orders = units[0] == '1' ? 1 : units[0] == '2' ? 2 : 0;
    statelog_tick = tick_stub;
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

/* ---- the virtual clock ---- */

static uint32_t g_clock, g_clock_step;
static int g_clock_on; /* from the first displayed frame of a mission; real time before */
static DWORD(WINAPI *real_time)(void); /* winmm's (our own import of it is redirected too) */

static DWORD g_frame_real; /* the real time of the last displayed frame */

/* with no frame for half a second (loading a saved game waits on the clock with nothing
 * displayed) the clock follows real time until the frames come back */
static DWORD WINAPI virtual_time(void)
{
    if (!g_clock_on)
        return real_time();
    DWORD now = real_time();
    if (now - g_frame_real > 500) {
        g_clock += now - g_frame_real;
        g_frame_real = now;
    }
    return g_clock;
}

void statelog_frame(void)
{
    if (!g_clock_step)
        return;
    if (!g_clock_on) {
        g_clock = real_time();
        g_clock_on = 1;
    }
    g_clock += g_clock_step;
    g_frame_real = real_time();
}

/* points the module's import of winmm's timeGetTime at the virtual clock */
static void patch_import(HMODULE mod)
{
    uint8_t *base = (uint8_t *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress)
        return;
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir.VirtualAddress); d->Name; d++) {
        if (lstrcmpiA((char *)(base + d->Name), "winmm.dll"))
            continue;
        IMAGE_THUNK_DATA *names = (IMAGE_THUNK_DATA *)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        IMAGE_THUNK_DATA *iat = (IMAGE_THUNK_DATA *)(base + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, iat++) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                continue;
            IMAGE_IMPORT_BY_NAME *n = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
            if (lstrcmpA((char *)n->Name, "timeGetTime"))
                continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, sizeof iat->u1.Function, PAGE_READWRITE, &old);
            iat->u1.Function = (uintptr_t)virtual_time;
            VirtualProtect(&iat->u1.Function, sizeof iat->u1.Function, old, &old);
        }
    }
}

static void clock_install(void)
{
    char v[16] = "";
    if (!GetEnvironmentVariableA("DOMINION_ISO_STATELOG_CLOCK", v, sizeof v))
        return;
    for (char *p = v; *p >= '0' && *p <= '9'; p++)
        g_clock_step = g_clock_step * 10 + (uint32_t)(*p - '0');
    if (!g_clock_step)
        return;
    real_time = (DWORD(WINAPI *)(void))(void *)GetProcAddress(GetModuleHandleA("winmm.dll"), "timeGetTime");
    if (!real_time) {
        g_clock_step = 0;
        return;
    }
    HMODULE self;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (const char *)clock_install, &self);
    patch_import(GetModuleHandleA("ISO_orig.dll"));
    patch_import(self);
}
