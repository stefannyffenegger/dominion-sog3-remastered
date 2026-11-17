/* The game logic, 0x1002d589-0x1002e6f7: the scenario's events: the timers (32 slots,
 * kept sorted by time), hot spots (a unit entering a flagged sector), event counters
 * (type 0x1e records counting down), and what they run: the records that follow the
 * event's record, up to the next record of the same type and subtype. Also the map
 * information of a .sdc file for the menus. */
#include "iso_core.h"
#include "filelist.h"
#include "sdlexec.h"
#include "events.h"

#define g_qsort ORIG(void(__cdecl *)(void *, size_t, size_t, void *), 0x100f57d0)
#define g_strncmp ORIG(int(__cdecl *)(const char *, const char *, size_t), 0x100f6b20)
#define g_strcpy ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c70)
#define g_strcat ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c80)
#define ai_player_active ORIG(int(__cdecl *)(int), 0x100e2df0)
#define ai_player_hold ORIG(void(__cdecl *)(int, int), 0x100e48de)
#define ai_group_done ORIG(void(__cdecl *)(int), 0x100e4969)
#define unit_set_anim ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cc91)
#define structure_destroy ORIG(void(__cdecl *)(uint16_t), 0x100eb610)

#define S(addr) ((const char *)ORIG_PTR(addr))
#define NREC I(uint16_t, 0xc2e9)
#define REC(i) (I(uint8_t *, 0xc2e5) + (i) * 0x5a)
#define V(r, n) P(int32_t, r, 10 + (n) * 4)

/* a timer: 0x1a bytes */
typedef struct __attribute__((packed)) {
    int32_t time;
    uint16_t id;
    int32_t period;
    void(__cdecl *cb)(int32_t, int32_t, int32_t);
    int32_t args[3];
} Timer;
#define TIMERS I(Timer *, 0xcd9e)
#define TIMER_COUNT I(int32_t, 0x1ad5)

/* runs the records after record i that are not another (type, subtype) header of the
 * same kind: same type, other subtype */
static uint32_t run_after(uint32_t i)
{
    int32_t type = P(int32_t, REC(i), 0), sub = P(int32_t, REC(i), 4);
    uint32_t k;
    while ((k = i + 1, (int)k < (int)NREC) && P(int32_t, REC(k), 0) == type && P(int32_t, REC(k), 4) != sub) {
        sdl_exec(k);
        i = k;
    }
    return i;
}

/* 0x1002d589: the .sdc file's map information (terrain, players, height, width) */
int __cdecl sdc_file_info(char *path, uint32_t *info)
{
    uint8_t h[0x18];
    HMMIO f = mmioOpenA(path, NULL, 0);
    if (!f)
        return 0;
    if (mmioRead(f, (HPSTR)h, 0x18) == 0x18 && !g_strncmp((const char *)h, S(0x10136398), 3)) {
        mmioClose(f, 0);
        info[0] = h[6];
        info[1] = h[7];
        info[2] = P(uint32_t, h, 8) & 0xffff;
        info[3] = P(uint32_t, h, 8) >> 16;
        return 1;
    }
    mmioClose(f, 0);
    return 0;
}

/* 0x1002d655: the information of map `index` of the file list (in folder variable
 * `dir_var`, its name into `name_var`) into info[4]; a multiplayer client checks the
 * host's map. 1 (0: the client lacks the file) or a negative error. */
int __stdcall IsoGetSDCInfo(uint8_t *iso, int index, uint32_t dir_var, uint32_t name_var, uint32_t *info)
{
    char path[0x104];
    uint8_t h[0x18];
    int rc;
    g_iso = iso;
    char *name = RtGetVariableString(I(void *, ISO_APP), name_var);
    char *dir = RtGetVariableString(I(void *, ISO_APP), dir_var);
    if (!I(void *, 0x18f0))
        return -1;
    g_strcpy(path, dir);
    if (!I(int32_t, 0x148) && I(int, 0x175) && *(I(char *, 0x18f0) + index * 0x104)) {
        g_strcpy(name, I(char *, 0x18f0));
        g_strcat(path, name);
        HMMIO f = mmioOpenA(path, NULL, 0);
        if (!f) {
            rc = 0;
            DebugStr(S(0x1013639c), path);
        } else {
            LONG got = mmioRead(f, (HPSTR)h, 0x18);
            if (got != 0x18 || g_strncmp((const char *)h, S(0x101363b0), 3)) {
                mmioClose(f, 0);
                return -2;
            }
            mmioClose(f, 0);
            rc = 1;
        }
    } else {
        if (index < 0 || index > I(int32_t, 0x179f))
            return -4;
        g_strcpy(name, I(char *, 0x18f0) + index * 0x104);
        g_strcat(path, name);
        memset(h, 0, 0x18);
        HMMIO f = mmioOpenA(path, NULL, 0);
        if (!f) {
            DebugStr(S(0x101363b4), path);
            return -5;
        }
        LONG got = mmioRead(f, (HPSTR)h, 0x18);
        if (got != 0x18 || g_strncmp((const char *)h, S(0x101363cc), 3)) {
            mmioClose(f, 0);
            return -6;
        }
        mmioClose(f, 0);
        rc = 1;
    }
    /* when the client lacks the file, h is whatever the stack held, as in the original */
    DebugStr(S(0x101363d0));
    DebugStr(S(0x101363e0), h[6]);
    info[0] = h[6];
    DebugStr(S(0x101363f4), h[7]);
    info[1] = h[7];
    DebugStr(S(0x10136408), P(uint32_t, h, 8) & 0xffff);
    info[2] = P(uint32_t, h, 8) & 0xffff;
    DebugStr(S(0x1013641c), P(uint32_t, h, 8) >> 16);
    info[3] = P(uint32_t, h, 8) >> 16;
    return rc;
}

/* 0x1002d9c8: unit `id` entered sector `s`: the first hot spot record for its player
 * (and objective) that lists the sector runs (and clears its sectors when it fires once) */
void __cdecl hotspot_entered(int s, uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    for (uint32_t i = 0; (int)i < (int)NREC; i++) {
        uint8_t *r = REC(i);
        if (P(int32_t, r, 0) != 0x14 || P(int32_t, r, 4) != 0)
            continue;
        if (P(uint8_t, u, U_PLAYER) != (uint32_t)V(r, 1) || (V(r, 0) && P(uint8_t, u, 0x2b) != (uint32_t)V(r, 0)))
            continue;
        for (int k = 0; k < P(int16_t, r, 8) - 3; k++) {
            if (V(r, 3 + k) != s)
                continue;
            if (V(r, 2)) { /* once: the sectors are cleared */
                for (int m = 0; m < P(int16_t, r, 8) - 3; m++) {
                    int32_t t = V(r, 3 + m);
                    if (t >= 0 && t < I(int32_t, 0xc2f1)) {
                        P(uint8_t, I(uint8_t *, 0xc2eb) + t * 0x1e, 0x1a) &= (uint8_t)~(1 << (P(uint8_t, u, U_PLAYER) & 0x1f));
                        DebugStr(S(0x10136430), t, P(uint8_t, u, U_PLAYER));
                    }
                }
            }
            run_after(i);
            return;
        }
    }
}

/* 0x1002dc7f: event `id` happened: the first counter for it counts down, and runs at 0 */
void __cdecl event_count(int id)
{
    for (uint32_t i = 0; (int)i < (int)NREC; i++) {
        uint8_t *r = REC(i);
        if (P(int32_t, r, 0) == 0x1e && !P(int32_t, r, 4) && V(r, 0) == id && (uint32_t)V(r, 1) < 0x80000000u) {
            V(r, 1)--;
            if (V(r, 1) < 1) {
                run_after(i);
                return;
            }
        }
    }
}

/* 0x1002e002: sorts the timers by time (empty ones last) and counts them */
void __cdecl timers_sort(void)
{
    int n;
    g_qsort(TIMERS, 0x20, 0x1a, (void *)timer_compare);
    for (n = 0; n < 0x20 && (TIMERS[n].id || TIMERS[n].time); n++)
        ;
    TIMER_COUNT = n;
}

/* 0x1002df4e: the timers' order */
int __cdecl timer_compare(const void *pa, const void *pb)
{
    const Timer *a = pa, *b = pb;
    int32_t ta = a->time, tb = b->time;
    if (!ta && !*(const int16_t *)&a->id)
        ta = 0x7fffffff;
    if (!tb && !*(const int16_t *)&b->id)
        tb = 0x7fffffff;
    return ta < tb ? -1 : ta == tb ? 0 : 1;
}

/* 0x1002de13: a new timer `id` at tick `at` (repeating every `period`), calling `cb` with
 * args[3] (or running the timer records) */
void __cdecl timer_set(uint16_t id, int32_t at, int32_t period, void *cb, int32_t *args)
{
    int32_t n = TIMER_COUNT;
    if (n < 0 || n >= 0x20)
        return;
    Timer *t = &TIMERS[n];
    t->id = id;
    t->time = at;
    t->period = period;
    t->cb = (void(__cdecl *)(int32_t, int32_t, int32_t))cb;
    if (cb) {
        t->args[0] = args[0];
        t->args[1] = args[1];
        t->args[2] = args[2];
    }
    timers_sort();
}

/* 0x1002e0ad: removes timer `id` */
void __cdecl timer_disable(uint32_t id)
{
    for (int k = 0; k < 0x20; k++)
        if (TIMERS[k].id == id) {
            memset(&TIMERS[k], 0, 0x1a);
            timers_sort();
            return;
        }
}

/* 0x1002e14f: sets timer `id` from its TIMER_EVENT record (when not on at the start or
 * repeating); 0 when there is none */
int __cdecl timer_enable(int id)
{
    int32_t t = 0, on = 0, relative = 0, repeat = 0, tid = 0;
    int found = 0;
    for (int i = 0; i < (int)NREC; i++) {
        uint8_t *r = REC(i);
        if (P(int32_t, r, 0) == 10 && !P(int32_t, r, 4)) {
            DebugStr(S(0x1013647c));
            tid = V(r, 0);
            t = V(r, 1);
            on = V(r, 2);
            relative = V(r, 3);
            repeat = V(r, 4);
            if (tid == id) {
                found = 1;
                break;
            }
        }
    }
    if (!found || (on && !repeat))
        return 0;
    if (relative)
        timer_set((uint16_t)tid, I(int32_t, 0x17dd) + t, repeat ? t : 0, NULL, NULL);
    else
        timer_set((uint16_t)tid, t, 0, NULL, NULL);
    return 1;
}

/* 0x1002e2e2: timer slot n is due: its callback or its records run (the AI players hold
 * meanwhile); it is removed, and set again when it repeats */
void __cdecl timer_fire(int n)
{
    Timer *t = &TIMERS[n];
    uint16_t id = t->id;
    int32_t period = 0, again = 0;
    void *cb = NULL;
    int32_t *args = NULL;
    int fired = 0;
    if (!id)
        return;
    for (int p = 0; p < 8; p++)
        if (ai_player_active(p))
            ai_player_hold(p, 1);
    if (!t->cb) {
        for (uint32_t i = 0; (int)i < (int)NREC; i++) {
            uint8_t *r = REC(i);
            if (P(int32_t, r, 0) != 10 || P(int32_t, r, 4))
                continue;
            period = V(r, 1);
            again = V(r, 4);
            if ((uint32_t)V(r, 0) != id)
                continue;
            run_after(i);
            fired = 1;
            break;
        }
    } else {
        t->cb(t->args[0], t->args[1], t->args[2]);
        period = t->period;
        again = t->period;
        cb = (void *)t->cb;
        args = (int32_t *)((uint8_t *)t + 0xe); /* read again after the timers were sorted, as in the original */
        fired = 1;
    }
    for (int p = 0; p < 8; p++)
        if (ai_player_active(p))
            ai_group_done(p);
    if (fired) {
        timer_disable(id);
        if (again)
            timer_set(id, I(int32_t, 0x17dd) + period, period, cb, args);
    }
}

/* 0x1002e606: destroys the units of objective `obj` (`effects`: with explosions) */
void __cdecl destroy_units(uint32_t obj, int effects)
{
    for (int k = 0; k < I(int32_t, ISO_UNIT_COUNT); k++) {
        uint32_t v = I(uint32_t *, ISO_UNIT_LIST)[k];
        uint8_t *u = I(uint8_t **, ISO_UNITS)[v];
        if (!u || P(uint8_t, u, 0x2b) != obj)
            continue;
        if (P(int32_t, u, U_ID_TYPE) < 0x40 || P(int32_t, u, U_ID_TYPE) > 0x9f) {
            if (effects)
                unit_set_anim(v, 0x20, 1, 0);
        } else {
            if (effects)
                structure_destroy((uint16_t)v);
            P(int32_t, u, 0x1a7) = 10;
            unit_set_anim(v, 0x4c, 1, 0);
        }
    }
}
