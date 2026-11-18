/* The game logic, 0x10028603-0x1002c74e: carrying out one scenario description record
 * (from IsoLoadSDCFile's loader for the shipped .sdc files, and the text loader): the
 * main records (subtype 0: timers, hot spots, mission info) and the game settings,
 * units, objects, AI and events (by subtype). */
#include "iso_core.h"
#include "intfwave.h"
#include "healthbar.h"
#include "terrain.h"
#include "scenario.h"
#include "sdlexec.h"
#include "sdcget.h"
#include "events.h"

#define message_box ORIG(void(__cdecl *)(int, const char *, const char *, int), 0x1005c515)
#define ai_ready ORIG(int(__cdecl *)(void), 0x100e1e48)
#define ai_adding ORIG(void(__cdecl *)(int, int), 0x100e4a10)
#define IsoAddUnit ORIG(int(__stdcall *)(uint8_t *, int, uint32_t, uint32_t, int, uint32_t, uint32_t, int, uint32_t, uint32_t), 0x100aa140)
#define unit_health_changed ORIG(void(__cdecl *)(uint32_t), 0x1006cff3)
#define IsoUnitCanEnter ORIG(int(__stdcall *)(uint8_t *, uint32_t), 0x100f0c6c)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)
#define unit_guard_at ORIG(void(__cdecl *)(uint32_t, int, int, int), 0x1007618a)
#define player_count_type ORIG(void(__cdecl *)(uint32_t, uint8_t), 0x100b1e2c)
#define unit_set_mode ORIG(void(__cdecl *)(uint32_t, uint8_t), 0x1009ce9f)
#define wall_link ORIG(void(__cdecl *)(uint32_t, int), 0x100b2953)
#define IsoAddObject ORIG(int(__stdcall *)(uint8_t *, int, int, int, uint32_t, int, int), 0x100aca6e)
#define ai_map_loaded ORIG(void(__cdecl *)(void), 0x100e1e80)
#define player_tech_changed ORIG(void(__cdecl *)(int), 0x1005a67b)
#define show_text ORIG(void(__cdecl *)(const char *, int), 0x1005878b)
#define game_over_cb ORIG_PTR(0x100562c2)
#define IsoFindUnit ORIG(int32_t(__stdcall *)(uint8_t *, int32_t, int32_t), 0x100a9ec9)
#define ai_player_race ORIG(void(__cdecl *)(int, uint8_t), 0x100e2bf6)
#define ai_player_computer ORIG(void(__cdecl *)(uint32_t, uint8_t, int32_t, int), 0x100e2c82)
#define ai_player_reset ORIG(void(__cdecl *)(int, int), 0x100e2a64)
#define ai_init_record ORIG(void(__cdecl *)(int, ...), 0x100e1073)
#define unit_by_place ORIG(int32_t(__stdcall *)(uint8_t *, int32_t, int32_t), 0x10028549)
#define unit_convoy ORIG(void(__cdecl *)(int32_t, int32_t *, int32_t), 0x10076787)
#define unit_follow ORIG(void(__cdecl *)(int32_t, int32_t, int), 0x10075f93)
#define unit_patrol ORIG(void(__cdecl *)(int32_t, int32_t *, int32_t *, int32_t, int32_t), 0x100764b2)
#define unit_entry_point ORIG(void(__cdecl *)(int32_t, int16_t *, int16_t *, int16_t *), 0x100bf0c2)
#define unit_change_player ORIG(void(__cdecl *)(uint32_t, int32_t, int), 0x10058bf5)
#define uncover_fog ORIG(void(__cdecl *)(int32_t, int32_t, uint32_t, int32_t, int, uint32_t, int), 0x10090223)
#define sdl_8c755 ORIG(void(__cdecl *)(void), 0x1008c755)
#define IsoAddEnergyWell ORIG(void(__stdcall *)(uint8_t *, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int), 0x100ad1d9)
#define IsoEnableAlliances ORIG(void(__cdecl *)(uint8_t *, int, int), 0x10056eae)
#define IsoSetAlliance ORIG(void(__cdecl *)(uint8_t *, int32_t, int32_t, int, int), 0x10057b3b)

#define S(addr) ((const char *)ORIG_PTR(addr))
#define TEXT_BUF ((char *)(g_iso + 0xdea))
#define REC(i) (I(uint8_t *, 0xc2e5) + (i) * 0x5a)
#define V(r, n) P(int32_t, r, 10 + (n) * 4) /* the record's n-th value */
#define TITLE S(0x101351fc)                 /* "IsoLoadSDLFile() Error" */

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
            timer_set((uint16_t)id, at, period, NULL, NULL);
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
        unit_health_bar((uint32_t)id, 1);
        break;
    case 3000: /* a quarter */
        P(int16_t, iso_unit(id), 0xd6) = (int16_t)(P(int16_t, iso_unit(id), 0xd6) / 4);
        unit_health_changed((uint32_t)id);
        unit_health_bar((uint32_t)id, 1);
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

#define PL(p, off) I(int32_t, (off) + (p) * ISO_PLAYER_SIZE)
#define PLB(p, off) I(uint8_t, (off) + (p) * ISO_PLAYER_SIZE)

/* subtype 120: SET_OBJECTIVE type, x, y (the unit there), a, b */
static int set_objective(uint8_t *r)
{
    DebugStr(S(0x1013577c));
    int32_t type = V(r, 1), a = V(r, 4), b = V(r, 5);
    int32_t id = IsoFindUnit(g_iso, V(r, 2), V(r, 3));
    if (id < 1 || P(int32_t, iso_unit(id), U_ID_TYPE) != type) {
        wsprintfA(TEXT_BUF, S(0x101357a4), id, designer(0x10135790));
        return bad();
    }
    P(uint8_t, iso_unit(id), 0x2b) = (uint8_t)a;
    P(uint8_t, iso_unit(id), 0x2c) = (uint8_t)b;
    return 1;
}

/* subtype 140: SET_COMPUTER_PLAYER player [, posture] */
static int set_computer_player(uint8_t *r)
{
    DebugStr(S(0x101357f0));
    int32_t p = V(r, 0);
    int32_t posture = P(int16_t, r, 8) < 2 ? 0 : V(r, 1);
    if (!I(int, 0x175)) {
        if (p < 0 || p > 7) {
            wsprintfA(TEXT_BUF, S(0x10135820), p, designer(0x1013580c));
            return bad();
        }
        PL(p, 0x1b69) = 2;
        if (I(int32_t, ISO_HAVE_SPRITES))
            ai_player_computer((uint32_t)p, PLB(p, 0x1b9f), posture, -1);
    }
    if (!I(int32_t, ISO_HAVE_SPRITES))
        *I(uint8_t *, 0x2dfd + p * ISO_PLAYER_SIZE) = (uint8_t)posture;
    return 1;
}

/* subtype 150: SET_PLAYER_RACE player, race (0 Human, 1 Darken, 2 Scorp, 3 Merc): also
 * the player's name ("Human (AI)" for a computer player) */
static int set_player_race(uint8_t *r)
{
    static const uint32_t ai_names[5] = {0x101358a0, 0x101358ac, 0x101358b8, 0x101358c4, 0x101358d0};
    static const uint32_t names[5] = {0x10135878, 0x10135880, 0x10135888, 0x10135890, 0x10135898};
    DebugStr(S(0x10135860));
    int32_t p = V(r, 0), race = V(r, 1);
    if (p < 0 || p >= 8)
        return 1;
    int k = race >= 0 && race <= 3 ? race : 4;
    char *name = (char *)(g_iso + 0x1b6d + p * ISO_PLAYER_SIZE);
    if (PL(p, 0x1b69) == 2) {
        lstrcpyA(name, S(ai_names[k]));
    } else {
        PL(p, 0x1b69) = 1;
        if (!I(int, 0x175))
            lstrcpyA(name, S(names[k]));
    }
    PLB(p, 0x1ba0) = 0;
    PLB(p, 0x1ba0) |= (uint8_t)(1 << (p & 0x1f)); /* allied with itself */
    PLB(p, 0x1b9f) = (uint8_t)(race >= 0 && race < 4 ? race : 0);
    if (I(int32_t, ISO_HAVE_SPRITES))
        ai_player_race(p, PLB(p, 0x1b9f));
    return 1;
}

/* subtype 170: SET_MAX_PLAYERS n */
static int set_max_players(uint8_t *r)
{
    DebugStr(S(0x101358f8));
    if (I(int32_t, ISO_HAVE_SPRITES) && !I(int, 0x175)) {
        I(int32_t, 0x1b1f) = V(r, 0);
        I(int32_t, 0x1b1b) = I(int32_t, 0x1b1f);
        for (int p = 0; p < I(int32_t, 0x1b1b); p++)
            ai_player_reset(p, 0);
        DebugStr(S(0x10135910), I(int32_t, 0x1b1b));
    }
    return 1;
}

/* subtype 180: SET_MEN_MATERIAL player, how (0 set, 1 none, 2 add), men, material
 * [, men and material at difficulty 1, at difficulty 2] */
static int set_men_material(uint8_t *r)
{
    DebugStr(S(0x10135954));
    int32_t p = V(r, 0), how = V(r, 1), men = V(r, 2), mat = V(r, 3);
    int16_t n = P(int16_t, r, 8);
    if (!I(int32_t, ISO_HAVE_SPRITES)) {
        I(int32_t, 0xd9e6 + p * 4) = men;
        I(int32_t, 0xda46 + p * 4) = mat;
        I(int32_t, 0xda06 + p * 4) = n == 8 ? V(r, 4) : men;
        I(int32_t, 0xda66 + p * 4) = n == 8 ? V(r, 5) : mat;
        I(int32_t, 0xda26 + p * 4) = n == 8 ? V(r, 6) : men;
        I(int32_t, 0xda86 + p * 4) = n == 8 ? V(r, 7) : mat;
    }
    if ((how == 0 || how == 2) && n > 4) {
        if (n < 8) {
            wsprintfA(TEXT_BUF, S(0x10135980), designer(0x1013596c));
            return bad();
        }
        if (I(uint8_t, 0x106a) == 1) {
            men = V(r, 4);
            mat = V(r, 5);
        } else if (I(uint8_t, 0x106a) == 2) {
            men = V(r, 6);
            mat = V(r, 7);
        }
    }
    if (how == 0) {
        PL(p, 0x1b97) = men;
        PL(p, 0x1b9b) = mat;
    } else if (how == 1) {
        PL(p, 0x1b97) = 0;
        PL(p, 0x1b9b) = 0;
    } else if (how == 2) {
        PL(p, 0x1b97) += men;
        PL(p, 0x1b9b) += mat;
    }
    if (I(int32_t, 0x17f5) == 4)
        I(uint8_t, 0xd696) = 1;
    return 1;
}

/* subtype 200: SET_DEBUG_LEVEL bits... */
static int set_debug_level(uint8_t *r)
{
    DebugStr(S(0x10135a94));
    I(uint32_t, 0xeea) = 0;
    for (int k = 0; k < P(int16_t, r, 8); k++)
        I(uint32_t, 0xeea) |= (uint32_t)V(r, k);
    DebugStr(S(0x10135aac), I(uint32_t, 0xeea));
    return 1;
}

#define g_group_player G(int32_t, 0x1017bb58)  /* the player of the group or base being defined */
#define g_group_number G(int32_t, 0x1017bb5c)  /* its group number */
#define g_base_number G(int32_t, 0x1017bb30)   /* its base number */
#define g_last_player G(int32_t, 0x1017bb60)

/* the AI's records need the AI started first */
static int ai_needed(uint32_t text, uint32_t fallback)
{
    if (I(int32_t, ISO_HAVE_SPRITES) && !ai_ready()) {
        wsprintfA(TEXT_BUF, S(text), designer(fallback));
        return bad();
    }
    return 1;
}

/* subtype 210: DEFRULE [player] */
static int def_rule(uint8_t *r)
{
    g_group_player = P(int16_t, r, 8) == 1 ? V(r, 0) : 0;
    if (I(int, 0x175))
        return 1;
    if (!ai_needed(0x10135ad8, 0x10135ac4))
        return 0;
    if (I(int32_t, ISO_HAVE_SPRITES))
        ai_init_record(6, V(r, 0));
    return 1;
}

/* subtype 300: DEFGROUP [player]: the player's next group number */
static int def_group(uint8_t *r)
{
    g_group_player = P(int16_t, r, 8) == 1 ? V(r, 0) : 0;
    G(int32_t, 0x1017bb38 + g_group_player * 4)++;
    g_group_number = G(int32_t, 0x1017bb38 + g_group_player * 4);
    g_last_player = g_group_player;
    if (I(int, 0x175)) {
        g_last_player = g_group_player;
        return 1;
    }
    if (!ai_needed(0x10135b68, 0x10135b54))
        return 0;
    if (I(int32_t, ISO_HAVE_SPRITES))
        ai_init_record(4, V(r, 0));
    return 1;
}

/* subtype 320: SET_START_CONVOY x, y (the unit there), then way points */
static int start_convoy(uint8_t *r)
{
    int32_t pts[64];
    if (!I(int32_t, ISO_HAVE_SPRITES))
        return 1;
    DebugStr(S(0x10135c3c));
    int32_t x = V(r, 0), y = V(r, 1);
    int32_t id = unit_by_place(g_iso, x, y);
    if (id < 1) {
        DebugStr(S(0x10135c54), x, y, id);
        return 1;
    }
    int n = 0;
    for (int k = 0; k < P(int16_t, r, 8) - 2; k++, n++) {
        pts[n * 2] = V(r, 2 + k);
        k++;
        pts[n * 2 + 1] = V(r, 2 + k);
    }
    unit_convoy(id, pts, n);
    return 1;
}

/* subtype 340: SET_PATROL_PATH x, y (the unit there), mode, then (x, y, time) points */
static int patrol_path(uint8_t *r)
{
    int32_t pts[64], times[32];
    if (!I(int32_t, ISO_HAVE_SPRITES))
        return 1;
    DebugStr(S(0x10135cf0));
    if (!I(int32_t, ISO_HAVE_SPRITES))
        return 1;
    int32_t mode = V(r, 2);
    int32_t id = IsoFindUnit(g_iso, V(r, 0), V(r, 1));
    if (id <= 0)
        return 1;
    int n = 0;
    for (int k = 0; k < P(int16_t, r, 8) - 3; k++, n++) {
        pts[n * 2] = V(r, 3 + k);
        k++;
        pts[n * 2 + 1] = V(r, 3 + k);
        k++;
        times[n] = V(r, 3 + k);
    }
    unit_patrol(id, pts, times, n, mode);
    return 1;
}

/* subtype 350: DEFBASE [player]: the player's next base number */
static int def_base(uint8_t *r)
{
    g_group_player = P(int16_t, r, 8) == 1 ? V(r, 0) : 0;
    G(int32_t, 0x1017bb10 + g_group_player * 4)++;
    g_base_number = G(int32_t, 0x1017bb10 + g_group_player * 4);
    g_last_player = g_group_player;
    if (I(int, 0x175)) {
        g_last_player = g_group_player;
        return 1;
    }
    if (!ai_needed(0x10135b20, 0x10135b0c))
        return 0;
    if (I(int32_t, ISO_HAVE_SPRITES))
        ai_init_record(2, g_last_player, g_base_number);
    return 1;
}

/* subtype 370: SET_DEFAULT_OBJ n (0-163), a, b */
static int default_obj(uint8_t *r)
{
    if (!I(int32_t, ISO_HAVE_SPRITES))
        return 1;
    DebugStr(S(0x10135d20));
    int32_t n = V(r, 0);
    uint32_t a = (uint32_t)V(r, 1), b = (uint32_t)V(r, 2);
    if (n < 0 || n > 0xa3 || (b | a) > 0xff) {
        wsprintfA(TEXT_BUF, S(0x10135d4c), n, a, b, designer(0x10135d38));
        return bad();
    }
    I(uint8_t, 0xf1e + n * 2) = (uint8_t)a;
    I(uint8_t, 0xf1f + n * 2) = (uint8_t)b;
    return 1;
}

/* subtype 380: CHANGE_PLAYER_ID objective (0: the unit at x, y), x, y, player */
static int change_player(uint8_t *r)
{
    DebugStr(S(0x10135d8c));
    uint32_t obj = (uint32_t)V(r, 0), p = (uint32_t)V(r, 3);
    int32_t x = V(r, 1), y = V(r, 2);
    if (p > 7) {
        wsprintfA(TEXT_BUF, S(0x10135e88), obj, x, y, p, designer(0x10135e74));
        return bad();
    }
    if (!obj) {
        int32_t id = IsoFindUnit(g_iso, x, y);
        if (!id || !iso_unit(id)) {
            wsprintfA(TEXT_BUF, S(0x10135db8), obj, x, y, p, designer(0x10135da4));
            return bad();
        }
        unit_change_player((uint32_t)id, (int32_t)p, 1);
        return 1;
    }
    int n = 0;
    for (int k = 0; k < I(int32_t, ISO_UNIT_COUNT); k++) {
        uint32_t v = I(uint32_t *, ISO_UNIT_LIST)[k];
        uint8_t *u = I(uint8_t **, ISO_UNITS)[v];
        if (u && (P(uint8_t, u, 0x2b) == obj || P(uint8_t, u, 0x2c) == obj)) {
            unit_change_player(v, (int32_t)p, 1);
            n++;
        }
    }
    if (!n) {
        wsprintfA(TEXT_BUF, S(0x10135e1c), obj, x, y, p, designer(0x10135e08));
        return bad();
    }
    return 1;
}

/* subtype 390: UNCOVER_FOG objective (0: at x, y), x, y, player, small */
static int uncover(uint8_t *r)
{
    if (!I(int32_t, ISO_HAVE_SPRITES))
        return 1;
    DebugStr(S(0x10135f68));
    uint32_t obj = (uint32_t)V(r, 0), p = (uint32_t)V(r, 3);
    int32_t x = V(r, 1), y = V(r, 2), small = V(r, 4);
    if (p >= 8) {
        wsprintfA(TEXT_BUF, S(0x10135ffc), obj, x, y, p, small, designer(0x10135fe8));
        return bad();
    }
    int radius = small ? 8 : 6;
    if (!obj) {
        uncover_fog(x, y, p, radius, -1, 0, 1);
        return 1;
    }
    int n = 0;
    for (int k = 0; k < I(int32_t, ISO_UNIT_COUNT); k++) {
        uint32_t v = I(uint32_t *, ISO_UNIT_LIST)[k];
        uint8_t *u = I(uint8_t **, ISO_UNITS)[v];
        if (u && (P(uint8_t, u, 0x2b) == obj || P(uint8_t, u, 0x2c) == obj)) {
            /* the high half of the player argument is the unit's address, as in the original */
            uncover_fog(P(int16_t, u, U_X), P(int16_t, u, U_Y), ((uint32_t)(uintptr_t)u & 0xffff0000u) | P(uint8_t, u, U_PLAYER),
                        radius, -1, v, 1);
            n++;
        }
    }
    if (!n) {
        wsprintfA(TEXT_BUF, S(0x10135f90), obj, x, y, p, small, designer(0x10135f7c));
        return bad();
    }
    return 1;
}

/* subtype 410: CHANGE_OBJECTIVE player, type, a, b, new a, new b */
static int change_objective(uint8_t *r)
{
    if (!I(int32_t, ISO_HAVE_SPRITES))
        return 1;
    DebugStr(S(0x10135ee0));
    uint32_t p = (uint32_t)V(r, 0), a = (uint32_t)V(r, 2), b = (uint32_t)V(r, 3);
    int32_t type = V(r, 1), na = V(r, 4), nb = V(r, 5);
    int n = 0;
    for (int k = 0; k < I(int32_t, ISO_UNIT_COUNT); k++) {
        uint8_t *u = I(uint8_t **, ISO_UNITS)[I(int32_t *, ISO_UNIT_LIST)[k]];
        if (u && P(uint8_t, u, U_PLAYER) == p && P(int32_t, u, U_ID_TYPE) == type && P(uint8_t, u, 0x2b) == a &&
            P(uint8_t, u, 0x2c) == b) {
            P(uint8_t, u, 0x2b) = (uint8_t)na;
            P(uint8_t, u, 0x2c) = (uint8_t)nb;
            n++;
        }
    }
    if (!n) {
        wsprintfA(TEXT_BUF, S(0x10135f0c), p, type, a, b, na, nb, designer(0x10135ef8));
        return bad();
    }
    return 1;
}

/* subtype 600: ADD_ENERGYWELL x, y, a, b, c */
static int add_energy_well(uint8_t *r)
{
    if (!I(void *, ISO_MAP)) {
        wsprintfA(TEXT_BUF, S(0x10135648), designer(0x10135634));
        return bad();
    }
    int32_t x = V(r, 0), y = V(r, 1), a = V(r, 2), b = V(r, 3), c = V(r, 4);
    if (!a || b < 2) {
        wsprintfA(TEXT_BUF, S(0x101356b0), x, y, a, b, designer(0x1013569c));
        message_box(0, TEXT_BUF, TITLE, 0); /* only a warning */
    } else {
        IsoAddEnergyWell(g_iso, x - 0x20, y - 0x10, a, b, c, I(uint16_t, 0xc340), 0);
    }
    return 1;
}

/* subtype 610: SET_ENTRY_POINT x, y (the unit there), then two or three points */
static int entry_point(uint8_t *r)
{
    int16_t pt[6];
    if (!I(int32_t, ISO_HAVE_SPRITES))
        return 1;
    DebugStr(S(0x10135d08));
    if (!I(int32_t, ISO_HAVE_SPRITES))
        return 1;
    int32_t id = unit_by_place(g_iso, V(r, 0), V(r, 1));
    if (id <= 0)
        return 1;
    pt[0] = (int16_t)V(r, 2);
    pt[1] = (int16_t)V(r, 3);
    pt[2] = (int16_t)V(r, 4);
    pt[3] = (int16_t)V(r, 5);
    int16_t *third = &pt[2];
    if (P(int16_t, r, 8) == 8) {
        pt[4] = (int16_t)V(r, 6);
        pt[5] = (int16_t)V(r, 7);
        third = &pt[4];
    }
    unit_entry_point(id, &pt[0], &pt[2], third);
    return 1;
}

/* subtype 650: show the countdown for n seconds (sprite resource 0xcd8e, its image +1) */
static int countdown_show(uint8_t *r)
{
    RtLoad(I(void *, ISO_APP), I(uint32_t, 0xcd8e));
    I(void *, 0xcd96) = RtGetResource(I(void *, ISO_APP), I(uint32_t, 0xcd8e))[1];
    if (!I(void *, 0xcd96)) {
        I(int32_t, 0xcd8a) = 0;
        return 1;
    }
    SpriteSetLevel(I(void *, ISO_HSL), I(void *, 0xcd96), 0x6144);
    I(void *, 0xcd92) = *RtGetResource(I(void *, ISO_APP), I(uint32_t, 0xcd8e) + 1);
    if (!I(void *, 0xcd92)) {
        I(int32_t, 0xcd8a) = 0;
        return 1;
    }
    int32_t x = (int32_t)((P(uint32_t, I(uint8_t *, 0xcd92), 0xc) >> 1) - (uint32_t)(I(int32_t, 0xd722) >> 1)) + 2;
    SpriteSetPosition(I(void *, ISO_HSL), I(void *, 0xcd96), 0, x, 0);
    I(int32_t, 0xcd9a) = V(r, 0) * 1000 + I(int32_t, 0x1093);
    I(int32_t, 0xcd8a) = 1;
    return 1;
}

/* subtype 660: hide the countdown */
static int countdown_hide(void)
{
    I(int32_t, 0xcd8a) = 0;
    I(int32_t, 0xcd9a) = 0;
    SpriteHide(I(void *, ISO_HSL), I(void *, 0xcd96), 1);
    I(void *, 0xcd96) = NULL;
    I(void *, 0xcd92) = NULL;
    RtUnload(I(void *, ISO_APP), I(uint32_t, 0xcd8e));
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
    case 120:
        return set_objective(r);
    case 140:
        return set_computer_player(r);
    case 150:
        return set_player_race(r);
    case 160: { /* SET_PLAYER_COLOR player, colour (0-8) */
        DebugStr(S(0x101358e0));
        int32_t c = V(r, 1);
        I(int32_t, 0x1ba2 + V(r, 0) * ISO_PLAYER_SIZE) = c < 0 || c > 8 ? 0 : c;
        return 1;
    }
    case 170:
        return set_max_players(r);
    case 180:
        return set_men_material(r);
    case 190: /* SET_NEXT_SCENARIO name */
        DebugStr(S(0x10135a68));
        DebugStr(S(0x10135a80), r + 10);
        I(uint8_t *, 0x1928) = r + 10;
        return 1;
    case 200:
        return set_debug_level(r);
    case 210:
        return def_rule(r);
    case 220:
        if (I(int32_t, ISO_HAVE_SPRITES))
            ai_init_record(8, V(r, 0), V(r, 1), V(r, 2), V(r, 3), V(r, 4));
        return 1;
    case 230:
        if (I(int32_t, ISO_HAVE_SPRITES))
            ai_init_record(7);
        return 1;
    case 240: /* ENERGYWALL_LINK */
        DebugStr(S(0x10135b9c));
        energywall_link(V(r, 0), V(r, 1), V(r, 2), V(r, 3));
        return 1;
    case 250: /* ENABLE_TIMER id */
        if (I(int32_t, ISO_HAVE_SPRITES)) {
            DebugStr(S(0x10135bb4));
            if (!timer_enable(V(r, 0))) {
                wsprintfA(TEXT_BUF, S(0x10135bdc), V(r, 0), designer(0x10135bc8));
                return bad();
            }
        }
        return 1;
    case 260: /* DISABLE_TIMER id */
        if (I(int32_t, ISO_HAVE_SPRITES)) {
            DebugStr(S(0x10135c14));
            timer_disable((uint32_t)V(r, 0));
        }
        return 1;
    case 270: /* DESTROY_UNIT */
        if (I(int32_t, ISO_HAVE_SPRITES)) {
            DebugStr(S(0x10135c28));
            destroy_units((uint32_t)V(r, 0), V(r, 1));
        }
        return 1;
    case 280:
        if (I(int32_t, ISO_HAVE_SPRITES))
            sdl_aoa(r); /* an area of approach */
        return 1;
    case 290:
        if (I(int32_t, ISO_HAVE_SPRITES))
            sdl_kt(r); /* key terrain */
        return 1;
    case 300:
        return def_group(r);
    case 310: /* the end of a group */
        if (I(int32_t, ISO_HAVE_SPRITES))
            ai_init_record(5);
        g_group_number = 1;
        return 1;
    case 320:
        return start_convoy(r);
    case 330: { /* SET_FOLLOW_UNIT: the unit at x2, y2 follows the one at x1, y1 */
        if (!I(int32_t, ISO_HAVE_SPRITES))
            return 1;
        DebugStr(S(0x10135c80));
        int32_t lead = IsoFindUnit(g_iso, V(r, 0), V(r, 1)), id = IsoFindUnit(g_iso, V(r, 2), V(r, 3));
        if ((id | lead) < 1) {
            wsprintfA(TEXT_BUF, S(0x10135cac), V(r, 0), V(r, 1), V(r, 2), V(r, 3), designer(0x10135c98));
            return bad();
        }
        unit_follow(id, lead, 1);
        return 1;
    }
    case 340:
        return patrol_path(r);
    case 350:
        return def_base(r);
    case 360: /* the end of a base */
        if (I(int32_t, ISO_HAVE_SPRITES))
            ai_init_record(3);
        g_base_number = 1;
        return 1;
    case 370:
        return default_obj(r);
    case 380:
        return change_player(r);
    case 390:
        return uncover(r);
    case 400:
        return 1;
    case 410:
        return change_objective(r);
    case 520:
        sdl_8c755();
        return 1;
    case 600:
        return add_energy_well(r);
    case 605: /* SET_ALLIANCE a, b */
        if (I(int32_t, ISO_HAVE_SPRITES)) {
            int32_t a = V(r, 0), b = V(r, 1);
            if (a >= 0 && a < 8 && b >= 0 && b < 8) {
                IsoEnableAlliances(g_iso, 1, 0);
                IsoSetAlliance(g_iso, a, b, 1, 0);
                IsoSetAlliance(g_iso, b, a, 1, 0);
            }
        }
        return 1;
    case 610:
        return entry_point(r);
    case 620: /* the game's end condition */
        I(int32_t, 0x158) = 0;
        I(int32_t, 0x150) = 0;
        if (V(r, 0) == 1)
            I(int32_t, 0x158) = 1;
        else if (V(r, 0) == 2)
            I(int32_t, 0x150) = 1;
        return 1;
    case 650:
        return countdown_show(r);
    case 660:
        return countdown_hide();
    case 130:
        DebugStr(S(0x10135928), I(int32_t, 0x17dd) + 0x40);
        show_text((const char *)r + 10, 0);
        return 1;
    }
    wsprintfA(TEXT_BUF, S(0x10136068), i & 0xffff, type, sub, designer(0x10136054));
    return bad();
}
