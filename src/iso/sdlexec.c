/* The game logic, 0x10028603-0x1002c74e: carrying out one scenario description record
 * (from IsoLoadSDCFile's loader for the shipped .sdc files, and the text loader): the
 * main records (subtype 0: timers, hot spots, mission info) and the game settings,
 * units, objects, AI and events (by subtype). Ported a part at a time: subtypes not
 * rewritten yet run the original's code through a trampoline. */
#include "iso_core.h"
#include "scenario.h"
#include "sdlexec.h"

#define message_box ORIG(void(__cdecl *)(int, const char *, const char *, int), 0x1005c515)
#define ai_ready ORIG(int(__cdecl *)(void), 0x100e1e48)
#define ai_adding ORIG(void(__cdecl *)(int, int), 0x100e4a10)
#define IsoAddUnit ORIG(int(__stdcall *)(uint8_t *, int, uint32_t, uint32_t, int, uint32_t, uint32_t, int, uint32_t, uint32_t), 0x100aa140)
#define unit_health_changed ORIG(void(__cdecl *)(uint32_t), 0x1006cff3)
#define unit_selected ORIG(void(__cdecl *)(uint32_t, int), 0x1004ebe6)
#define IsoUnitCanEnter ORIG(int(__stdcall *)(uint8_t *, uint32_t), 0x100f0c6c)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)
#define unit_guard_at ORIG(void(__cdecl *)(uint32_t, int, int, int), 0x1007618a)
#define player_count_type ORIG(void(__cdecl *)(uint32_t, uint8_t), 0x100b1e2c)
#define unit_set_mode ORIG(void(__cdecl *)(uint32_t, uint8_t), 0x1009ce9f)
#define wall_link ORIG(void(__cdecl *)(uint32_t, int), 0x100b2953)
#define IsoAddObject ORIG(int(__stdcall *)(uint8_t *, int, int, int, uint32_t, int, int), 0x100aca6e)
#define ai_map_loaded ORIG(void(__cdecl *)(void), 0x100e1e80)
#define player_tech_changed ORIG(void(__cdecl *)(int), 0x1005a67b)
#define player_announce ORIG(void(__cdecl *)(uint32_t, int, int), 0x1005810f)
#define show_text ORIG(void(__cdecl *)(const char *, int), 0x1005878b)
#define game_over_cb ORIG_PTR(0x100562c2)
#define timer_set ORIG(void(__cdecl *)(int, int32_t, int32_t, void *, void *), 0x1002de13)

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
            timer_set(id, at, period, NULL, NULL);
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

/* subtype 10: ADD_UNIT player, type, dir, a, mode, damage, x, y [, count, difficulty, b] */
static int add_unit(uint8_t *r)
{
    if (!I(void *, ISO_MAP)) {
        wsprintfA(TEXT_BUF, S(0x10135414), designer(0x10135400));
        return bad();
    }
    if (I(int, 0x175))
        return 1; /* multiplayer: the units come from the host */
    if (I(int32_t, ISO_HAVE_SPRITES) && !ai_ready()) {
        wsprintfA(TEXT_BUF, S(0x10135478), designer(0x10135464));
        return bad();
    }
    int32_t player = V(r, 0);
    uint32_t type = (uint32_t)V(r, 1), dir = (uint32_t)V(r, 2);
    int32_t a = V(r, 3), mode = V(r, 4), damage = V(r, 5), x = V(r, 6);
    uint32_t y = (uint32_t)V(r, 7);
    uint8_t count, difficulty = 0; /* the original's is left unset when absent */
    int32_t b;
    if (P(int16_t, r, 8) < 8) {
        count = 1;
    } else {
        count = P(uint8_t, r, 0x2a);
        if (count < 2)
            count = 1;
    }
    if (P(int16_t, r, 8) >= 9) {
        difficulty = P(uint8_t, r, 0x2e);
        if (difficulty > 2) { /* 3-5: only at difficulty 0-2 */
            if (I(int32_t, ISO_HAVE_SPRITES) && difficulty - 3 != I(uint8_t, 0x106a))
                return 1;
        } else if (I(int32_t, ISO_HAVE_SPRITES) && I(uint8_t, 0x106a) < difficulty) {
            return 1; /* 0-2: from that difficulty on */
        }
    }
    b = P(int16_t, r, 8) >= 10 ? V(r, 10) : 0;
    if (I(int32_t, ISO_HAVE_SPRITES))
        ai_adding(player, 1);
    int32_t id = IsoAddUnit(g_iso, player, type, I(uint8_t, 0x1b9f + player * ISO_PLAYER_SIZE), x, y, dir, 0, count,
                            mode == 0x23 ? 2 : 0);
    if (I(int32_t, ISO_HAVE_SPRITES))
        ai_adding(player, 0);
    if (id <= 0)
        return 1;
    DebugStr(S(0x101354b0), id);
    uint8_t *u = iso_unit(id);
    P(uint8_t, u, 0x2b) = (uint8_t)a;
    P(uint8_t, iso_unit(id), 0x2c) = (uint8_t)b;
    P(uint32_t, iso_unit(id), U_FLAGS) &= 0xfffbffff;
    switch (damage) {
    case 1000:
        P(uint32_t, iso_unit(id), U_FLAGS) |= 0x800;
        P(uint32_t, iso_unit(id), U_FLAGS) |= 0x8000000;
        break;
    case 2000: /* half health */
        P(int16_t, iso_unit(id), 0xd6) = (int16_t)(P(int16_t, iso_unit(id), 0xd6) / 2);
        unit_health_changed((uint32_t)id);
        unit_selected((uint32_t)id, 1);
        break;
    case 3000: /* a quarter */
        P(int16_t, iso_unit(id), 0xd6) = (int16_t)(P(int16_t, iso_unit(id), 0xd6) / 4);
        unit_health_changed((uint32_t)id);
        unit_selected((uint32_t)id, 1);
        break;
    }
    if (P(int32_t, iso_unit(id), U_CLASS) == 0x38 && P(uint8_t, iso_unit(id), 0x79) == 3) {
        if (IsoUnitCanEnter(g_iso, (uint32_t)id))
            unit_set_action((uint32_t)id, 0x11, 1, 0);
        else
            DebugStr(S(0x101354c8));
    }
    if (I(int32_t, ISO_HAVE_SPRITES) && mode > 0) {
        if (mode == 0x1f) {
            unit_guard_at((uint32_t)id, P(int16_t, iso_unit(id), U_X), P(int16_t, iso_unit(id), U_Y), 1);
        } else if (mode == 0x23) {
            uint32_t owner = P(uint8_t, iso_unit(id), U_PLAYER);
            I(int32_t, 0x1b8b + owner * ISO_PLAYER_SIZE)--;
            player_count_type(P(uint8_t, iso_unit(id), U_PLAYER), (uint8_t)P(int32_t, iso_unit(id), U_ID_TYPE));
            owner = P(uint8_t, iso_unit(id), U_PLAYER);
            I(int32_t, 0x1b8f + owner * ISO_PLAYER_SIZE)++;
            P(uint32_t, iso_unit(id), U_FLAGS) |= 0x10;
            P(int16_t, iso_unit(id), 0x42) = 1;
            unit_set_mode((uint32_t)id, (uint8_t)mode);
            unit_set_action((uint32_t)id, (uint32_t)mode, 1, 0);
        } else {
            unit_set_mode((uint32_t)id, (uint8_t)mode);
            unit_set_action((uint32_t)id, (uint32_t)mode, 1, 0);
        }
    }
    if (!I(int32_t, ISO_HAVE_SPRITES) && P(uint8_t *, iso_unit(id), 0x3a1))
        P(uint8_t, P(uint8_t *, iso_unit(id), 0x3a1), 5) = mode == 0x1f ? 2 : mode == 0x23 ? 1 : 0;
    /* walls: a new piece links to the last one when both are of one kind */
    int32_t kind = (int32_t)(type & 0xfffffffc), last = I(int32_t, 0xd9de);
    if (kind >= 0x88 && kind <= 0x9f && id > 1 && last > 0 && iso_unit(last)) {
        int32_t lk = P(int32_t, iso_unit(last), U_CLASS);
        if (((kind >= 0x88 && kind <= 0x90 && lk >= 0x88 && lk <= 0x90) ||
             (kind >= 0x94 && kind <= 0x9c && lk >= 0x94 && lk <= 0x9c)) &&
            (kind == 0x88 || kind == 0x94 || lk == 0x88 || lk == 0x94)) {
            P(int16_t, iso_unit(id), 0xf8) = (int16_t)I(int32_t, 0xd9de);
            P(int16_t, iso_unit(id), 0xf6) = 0;
            P(int16_t, iso_unit(I(int32_t, 0xd9de)), 0xf6) = (int16_t)id;
        }
    }
    if (kind >= 0x88 && kind <= 0x9c) {
        I(int32_t, 0xd9de) = id;
        if (P(uint16_t, iso_unit(id), 0xf8) && kind != 0x88 && kind != 0x94)
            wall_link((uint32_t)id, 0);
    }
    if (!I(int32_t, ISO_HAVE_SPRITES)) {
        *P(uint8_t *, iso_unit(id), 0x3a1) = difficulty;
        P(uint8_t, iso_unit(id), 0x15) =
            (uint8_t)(P(int32_t, iso_unit(id), U_CLASS) < 0x40 ? G(int32_t, 0x1017bb5c) : G(int32_t, 0x1017bb30));
        G(int32_t, 0x1017bb60) = player;
    }
    return 1;
}

/* subtype 20: ADD_APC_UNIT, a unit with fewer settings */
static int add_apc_unit(uint8_t *r)
{
    if (!I(void *, ISO_MAP)) {
        wsprintfA(TEXT_BUF, S(0x101354f8), designer(0x101354e4));
        return bad();
    }
    if (I(int, 0x175))
        return 1;
    if (I(int32_t, ISO_HAVE_SPRITES) && !ai_ready()) {
        wsprintfA(TEXT_BUF, S(0x10135560), designer(0x1013554c));
        return bad();
    }
    int32_t player = V(r, 0), b = V(r, 2), a = V(r, 3), mode = V(r, 4), x = V(r, 6);
    uint32_t type = (uint32_t)V(r, 1), y = (uint32_t)V(r, 7);
    uint8_t count;
    if (P(int16_t, r, 8) < 8) {
        count = 1;
    } else {
        count = P(uint8_t, r, 0x2a);
        if (count < 2)
            count = 1;
    }
    int32_t id = IsoAddUnit(g_iso, player, type, I(uint8_t, 0x1b9f + player * ISO_PLAYER_SIZE), x, y, 0, 0, count, 0);
    if (id <= 0)
        return 1;
    DebugStr(S(0x1013559c), id);
    P(uint8_t, iso_unit(id), 0x2b) = (uint8_t)a;
    P(uint8_t, iso_unit(id), 0x2c) = (uint8_t)b;
    P(uint32_t, iso_unit(id), U_FLAGS) &= 0xfffbffff;
    if (mode > 0) {
        if (mode == 0x1f) {
            unit_guard_at((uint32_t)id, P(int16_t, iso_unit(id), U_X), P(int16_t, iso_unit(id), U_Y), 0);
        } else {
            unit_set_mode((uint32_t)id, (uint8_t)mode);
            unit_set_action((uint32_t)id, (uint32_t)mode, 1, 0);
        }
    }
    return 1;
}

/* subtype 30: ADD_OBJECT type, x, y, flags (three values or-ed) */
static int add_object(uint8_t *r)
{
    if (!I(void *, ISO_MAP)) {
        wsprintfA(TEXT_BUF, S(0x101355cc), designer(0x101355b8));
        return bad();
    }
    int32_t id = IsoAddObject(g_iso, V(r, 0), V(r, 1), V(r, 2), (uint32_t)(V(r, 3) | V(r, 4) | V(r, 5)), -1, -1);
    if (id >= 0)
        DebugStr(S(0x1013561c), id);
    return 1;
}

/* subtypes 40, 50: the game is won or lost, 0x30 ticks later */
static int game_over(int won)
{
    int32_t data[2] = {won, 0};
    DebugStr(won ? S(0x101356fc) : S(0x1013570c));
    timer_set(won ? 0x309 : 0x29a, I(int32_t, 0x17dd) + 0x30, 0, game_over_cb, data);
    return 1;
}

/* subtype 70: LOAD_MAP */
static int load_map(uint8_t *r)
{
    DebugStr(S(0x101359c4));
    DebugStr(S(0x101359d4), r + 10);
    int rc = scenario_load(g_iso, (const char *)r + 10, !I(int32_t, ISO_HAVE_SPRITES), 1);
    DebugStr(S(0x101359e8), rc);
    if (rc < 0) {
        wsprintfA(TEXT_BUF, S(0x10135a08), g_iso + 0xc1e1, r + 10, designer(0x101359f4));
        return bad();
    }
    if (I(int32_t, ISO_HAVE_SPRITES))
        ai_map_loaded();
    return 1;
}

/* subtype 90: SET_TECH_LEVEL player, level (21 and up: 13) */
static int set_tech_level(uint8_t *r)
{
    int32_t player = V(r, 0), level = V(r, 1);
    DebugStr(S(0x10135734));
    I(uint8_t, 0x1baa + player * ISO_PLAYER_SIZE) = (uint8_t)(level < 0x15 ? level : 0xd);
    if (!I(int32_t, ISO_HAVE_SPRITES))
        I(uint8_t, 0xdaa6 + player) = I(uint8_t, 0x1baa + player * ISO_PLAYER_SIZE);
    else
        player_tech_changed(player);
    return 1;
}

/* subtype 110: PLAYWAVE n (1-99: the local player's announcement n) */
static int play_wave(uint8_t *r)
{
    DebugStr(S(0x10135764));
    DebugStr(S(0x10135774), V(r, 0));
    int32_t n = V(r, 0);
    if (n > 0 && n < 100)
        player_announce(I(uint32_t, ISO_LOCAL_PLAYER), n, 1);
    return 1;
}

/* 0x10028603: record `i`; 0 when it is bad (a message box has said why) */
int __cdecl sdl_exec(uint32_t i)
{
    uint8_t *r = REC(i & 0xffff);
    int32_t type = P(int32_t, r, 0), sub = P(int32_t, r, 4);
    if (sub == 0)
        return main_record(i, r, type);
    if (sub == 5)
        return 1;
    switch (sub) {
    case 10:
        return add_unit(r);
    case 20:
        return add_apc_unit(r);
    case 30:
        return add_object(r);
    case 40:
        return game_over(1);
    case 50:
        return game_over(0);
    case 60:
        DebugStr(S(0x1013571c)); /* TRANSPORT_LINK: nothing */
        return 1;
    case 70:
        return load_map(r);
    case 80:
        DebugStr(S(0x10135a44)); /* LOAD_SDL: nothing */
        DebugStr(S(0x10135a54), r + 10);
        return 1;
    case 90:
        return set_tech_level(r);
    case 100:
        DebugStr(S(0x1013574c)); /* SET_MAX_GAME_TIME: nothing */
        return 1;
    case 110:
        return play_wave(r);
    case 130:
        DebugStr(S(0x10135928), I(int32_t, 0x17dd) + 0x40);
        show_text((const char *)r + 10, 0);
        return 1;
    }
    if (!g_sdl_exec_body)
        g_sdl_exec_body = ORIG_PTR(0x1002860c);
    return sdl_exec_orig(i);
}
