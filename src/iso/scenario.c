/* The game logic, 0x1001e25a-0x10020b81: loading a scenario (IsoLoadScenario): a .wxm
 * or .wxp file (all shipped ones are version 1.5): a 0x80-byte header, the map's cells,
 * its tiles (from the tile library, or embedded), the objects, and the AI's choke
 * points (versions x.3-x.5). */
#include "iso_core.h"
#include "tilelib.h"
#include "maptiles.h"
#include "mapedit.h"
#include "createmap.h"
#include "scenario.h"

/* not rewritten yet (names ours, provisional) */
#define IsoAddObject ORIG(int(__stdcall *)(uint8_t *, int, int, int, int, int, int), 0x100aca6e)
#define map_reset_view ORIG(void(__cdecl *)(void), 0x10050fc6)
#define ai_player_setup ORIG(void(__cdecl *)(int32_t), 0x100a783d)
#define choke_init ORIG(void(__cdecl *)(void), 0x10098542)
#define choke_link_fix ORIG(void(__cdecl *)(void *), 0x10098970)
#define choke_grid_done ORIG(void(__cdecl *)(void), 0x1008de00)
#define choke_point_done ORIG(void(__cdecl *)(void *), 0x10098dd4)
#define scenario_loaded ORIG(void(__cdecl *)(void), 0x10066fac)
#define units_after_load ORIG(void(__cdecl *)(void), 0x100dcda0)
#define message_box ORIG(void(__cdecl *)(int, const char *, const char *, int), 0x1005c515)
#define g_strcmp ORIG(int(__cdecl *)(const char *, const char *), 0x100f53a0)

#define HDR_OBJ_SIZE 0x4c
#define HDR_OBJ_AT 0x50
#define HDR_UNIT_SIZE 0x68
#define HDR_UNIT_AT 0x6c

/* the AI's choke points (I(0xe113)) */
typedef struct __attribute__((packed)) {
    int32_t count, cap;   /* points read, room for 20 more */
    uint8_t *points;      /* 0x46c bytes each */
    int32_t n_links;
    uint8_t *links;       /* 0x2a bytes each: a count and 20 point numbers */
    int32_t grid_size;
    int16_t grid_w, grid_h;
    uint8_t *grid;
} Choke;
#define POINT(c, i) ((c)->points + (i) * 0x46c)

/* the point's fields that the loader fixes: +0 active, +0x14/+0x16 position, +0x6a
 * count of cells (at +0x6c: cell, x, y; 8 bytes each) */
static void point_fix(uint8_t *p)
{
    if (!I(int32_t, ISO_HAVE_SPRITES)) {
        P(int32_t, p, 0) = 0;
    } else {
        P(int32_t, p, 0) = 1;
        if (!P(int16_t, p, 0x6a)) {
            int x = P(int16_t, p, 0x14), y = P(int16_t, p, 0x16), cx, cy;
            P(int32_t, p, 0x6c) = IsoVTilePos(g_iso, &x, &y, &cx, &cy);
            P(int16_t, p, 0x70) = (int16_t)(cx + 0x20);
            P(int16_t, p, 0x72) = (int16_t)(cy + 0x10);
            P(int16_t, p, 0x6a) = 1;
        }
    }
    /* the original also works out the position's cell into locals it never uses */
    int x = P(int16_t, p, 0x14), y = P(int16_t, p, 0x16), cx, cy;
    IsoVTilePos(g_iso, &x, &y, &cx, &cy);
}

static void points_clear_links(Choke *c, int from)
{
    for (int i = from; i < c->cap; i++)
        for (int k = 0; k < 0x14; k++) {
            P(int16_t, POINT(c, i), 0x1a + k * 4) = -1;
            P(int16_t, POINT(c, i), 0x1c + k * 4) = -1;
        }
}

static int fail(HMMIO f, int rc)
{
    map_free();
    mmioClose(f, 0);
    return rc;
}

/* the points of a version 3 file: 0x36c bytes each, the cells as 6-byte entries */
static void read_points_v3(HMMIO f, Choke *c)
{
    uint8_t r[0x36c];
    for (int i = 0; i < c->count; i++) {
        mmioRead(f, (HPSTR)r, 0x36c);
        uint8_t *p = POINT(c, i);
        memcpy(p, r, 0x18);
        P(int16_t, p, 0x18) = P(int16_t, r, 0x18);
        for (int k = 0; k < P(int16_t, p, 0x18); k++) {
            P(int16_t, p, 0x1a + k * 4) = P(int16_t, r, 0x1a + k * 4);
            P(int16_t, p, 0x1c + k * 4) = P(int16_t, r, 0x1c + k * 4);
        }
        P(int16_t, p, 0x6a) = P(int16_t, r, 0x6a);
        for (int k = 0; k < P(int16_t, p, 0x6a); k++) {
            P(int32_t, p, 0x6c + k * 8) = P(int16_t, r, 0x6c + k * 6);
            P(int16_t, p, 0x70 + k * 8) = P(int16_t, r, 0x6e + k * 6);
            P(int16_t, p, 0x72 + k * 8) = P(int16_t, r, 0x70 + k * 6);
            if (c->grid_size >= 0x8000) { /* the previous map's grid size */
                int x = P(int16_t, p, 0x70 + k * 8), y = P(int16_t, p, 0x72 + k * 8), cx, cy;
                P(int32_t, p, 0x6c + k * 8) = IsoVTilePos(g_iso, &x, &y, &cx, &cy);
            }
        }
        point_fix(p);
    }
}

/* versions 3-5: the choke points, their links and their grid; 0 or -0x50 */
static int read_choke(HMMIO f, int version)
{
    if (!I(void *, 0xe113))
        choke_init();
    Choke *c = I(Choke *, 0xe113);
    mmioRead(f, (HPSTR)&c->count, 4);
    c->cap = c->count + 0x14;
    c->points = winmalloc((SIZE_T)(c->cap * 0x46c), 0);
    if (!c->points)
        return -0x50;
    memset(c->points, 0, (size_t)(c->cap * 0x46c));
    points_clear_links(c, 0);
    if (version == 3) {
        read_points_v3(f, c);
    } else {
        for (int i = 0; i < c->count; i++) {
            mmioRead(f, (HPSTR)POINT(c, i), 0x46c);
            point_fix(POINT(c, i));
        }
    }
    points_clear_links(c, c->count);
    mmioRead(f, (HPSTR)&c->n_links, 4);
    c->links = winmalloc((SIZE_T)(c->n_links * 0x2a), 0);
    if (!c->links)
        return -0x50;
    memset(c->links, 0, (size_t)(c->n_links * 0x2a));
    for (int i = 0; i < c->n_links; i++)
        for (int k = 0; k < 0x14; k++)
            P(int16_t, c->links + i * 0x2a, 2 + k * 2) = -1;
    for (int i = 0; i < c->n_links; i++) {
        uint8_t *l = c->links + i * 0x2a;
        if (version == 5) {
            mmioRead(f, (HPSTR)l, 0x2a);
        } else { /* a count and 10 points */
            int16_t r[11];
            mmioRead(f, (HPSTR)r, 0x16);
            P(int16_t, l, 0) = r[0];
            for (int k = 0; k < 10; k++)
                P(int16_t, l, 2 + k * 2) = r[1 + k];
        }
    }
    for (int i = 0; i < c->n_links; i++) {
        int16_t *l = (int16_t *)(c->links + i * 0x2a);
        for (int k = l[0]; k < 0x14; k++)
            l[k + 1] = -1;
        choke_link_fix(l);
    }
    mmioRead(f, (HPSTR)&c->grid_w, 2);
    mmioRead(f, (HPSTR)&c->grid_h, 2);
    c->grid_size = c->grid_w * c->grid_h;
    c->grid = winmalloc((SIZE_T)c->grid_size, 0);
    if (!c->grid)
        return -0x50;
    memset(c->grid, 0, (size_t)c->grid_size);
    mmioRead(f, (HPSTR)c->grid, c->grid_size);
    choke_grid_done();
    for (int i = 0; i < c->count; i++)
        choke_point_done(POINT(c, i));
    return 0;
}

/* the objects; record layouts by version */
static void add_objects(const uint8_t *buf, uint32_t size, int version)
{
    int32_t n;
    switch (version) {
    case 0: /* 6 ints: used, x, y, type, flags */
        n = (int32_t)(size / 0x18);
        for (int i = 0; i < n; i++) {
            const int32_t *r = (const int32_t *)(buf + i * 0x18);
            if (r[0] <= 0)
                continue;
            int32_t type = r[3];
            int z = type >= 0x15e && type <= 0x185 ? -30000 : -1;
            if (type != 0x118)
                IsoAddObject(g_iso, type, r[1], r[2], r[4], -1, z);
        }
        break;
    case 1: /* 12 bytes, but the original steps 48 bytes a record */
        n = (int32_t)(size / 0xc);
        for (int i = 0; i < n; i++) {
            const uint8_t *r = buf + i * 0x30;
            if (P(int32_t, r, 0) <= 0)
                continue;
            uint16_t type = P(uint16_t, r, 8);
            int z = type >= 0x15e && type <= 0x185 ? -30000 : -1;
            if (type != 0x118)
                IsoAddObject(g_iso, type, P(int16_t, r, 4) + 0x40, P(int16_t, r, 6) + 0x20, P(uint16_t, r, 0xa), -1, z);
        }
        break;
    default: /* 2-5: 12 bytes; types 0xb4-0x107 are skipped */
        n = (int32_t)(size / 0xc);
        for (int i = 0; i < n; i++) {
            const uint8_t *r = buf + i * 0xc;
            uint16_t type = P(uint16_t, r, 8);
            if (P(int32_t, r, 0) <= 0 || (type >= 0xb4 && type <= 0x103) || (type >= 0x104 && type <= 0x107))
                continue;
            int z = type >= 0x15e && type <= 0x185 ? -30000 : -1;
            if (type != 0x118)
                IsoAddObject(g_iso, type, P(int16_t, r, 4) - 0x20, P(int16_t, r, 6) - 0x10, P(uint16_t, r, 0xa), -1, z);
        }
        break;
    }
}

static void forget_scenario(void)
{
    if (I(void *, 0x1974)) {
        winfree(I(void *, 0x1974));
        I(void *, 0x1974) = NULL;
    }
    memset(g_iso + 0x1994, 0, 0x104);
    map_free();
}

/* 0x1001e2eb: loads scenario `name` (in the scenario folder 0xc1e1); `no_embedded`
 * uses the tile library even when the file has its tiles; `ai` sets up the local
 * player's AI. 0 or a negative error. */
int __stdcall scenario_load(uint8_t *iso, const char *name, int no_embedded, int ai)
{
    char path[260];
    g_iso = iso;
    uint8_t *h = g_iso + 0xbf40; /* the header */
    memset(h, 0, 0x80);
    lstrcpyA((char *)(g_iso + 0x1994), name);
    wsprintfA(path, (const char *)ORIG_PTR(0x10131318), g_iso + 0xc1e1, g_iso + 0x1994);
    HMMIO f = mmioOpenA(path, NULL, 0);
    if (!f)
        return -0x32;
    LONG got = mmioRead(f, (HPSTR)h, 0x80);
    g_strcmp((const char *)h, (const char *)ORIG_PTR(0x10131360)); /* a .wxm (unused) */
    if (got != 0x80 || (g_strcmp((const char *)h, (const char *)ORIG_PTR(0x10131364)) &&
                        g_strcmp((const char *)h, (const char *)ORIG_PTR(0x10131368)))) {
        mmioClose(f, 0);
        return -0x3c;
    }
    DebugStr((const char *)ORIG_PTR(0x101313ac), h[4], h[5]);
    SpriteSetScrollPos(I(void *, ISO_HSL), 0, 0);
    I(int32_t, 0xcd82) = 0;
    I(int32_t, 0xcd86) = 0;
    I(int32_t, 0xbfc1) = 0;
    I(int32_t, 0xbfc5) = 0;
    I(int32_t, 0xd6bb) = I(int32_t, 0xd78b) + I(int32_t, 0xcd86) - 0x80;
    I(int32_t, 0xd6c3) = I(int32_t, 0xd793) + I(int32_t, 0xcd86) + 0x80;
    I(int32_t, 0xd6bf) = I(int32_t, 0xd78f) + I(int32_t, 0xcd82) + 0x80;
    I(int32_t, 0xd6b7) = I(int32_t, 0xd787) + I(int32_t, 0xcd82) - 0x80;
    uint32_t w = P(uint16_t, h, 0x3a), ht = P(uint16_t, h, 0x38);
    int rc = IsoCreateMap(iso, w, ht, h[0x2e], 0, 0, 0, 0, 0);
    if (rc < 0) {
        mmioClose(f, 0);
        return rc;
    }
    mmioSeek(f, P(LONG, h, 0x42), SEEK_SET);
    got = mmioRead(f, I(HPSTR, ISO_MAP), (LONG)(ht * w * 4));
    if (ht * w * 4 != (uint32_t)got) {
        memset(g_iso + 0x1994, 0, 0x104);
        map_free();
        mmioClose(f, 0);
        return -0x46;
    }
    DebugStr((const char *)ORIG_PTR(0x101313c0), got);
    DebugStr((const char *)ORIG_PTR(0x101313d0), 4);
    I(uint16_t, 0xc340) = h[0x2e];
    char *lib = (char *)(g_iso + h[0x2e] * 0x104 + 0xc352);
    /* the high halves of two arguments are the header's address, as in the original */
    uint32_t hi = (uint32_t)(uintptr_t)h & 0xffff0000u;
    if (!P(uint32_t, h, 0x5a) || no_embedded) {
        rc = tilelib_load(lib, 0, 0, 0, 1);
        DebugStr((const char *)ORIG_PTR(0x101313e4), rc);
    } else {
        I(void *, 0x1974) = winmalloc(P(uint32_t, h, 0x5a), 1);
        if (I(void *, 0x1974)) {
            mmioSeek(f, P(LONG, h, 0x5e), SEEK_SET);
            got = mmioRead(f, I(HPSTR, 0x1974), P(LONG, h, 0x5a));
        }
        if (!I(void *, 0x1974) || P(LONG, h, 0x5a) != got ||
            !tilelib_load(lib, I(void *, 0x1974), P(int32_t, h, 0x76), hi | P(uint16_t, h, 0x74), 0)) {
            forget_scenario();
            mmioClose(f, 0);
            return -0x5a;
        }
        if (I(void *, 0x1974)) {
            winfree(I(void *, 0x1974));
            I(void *, 0x1974) = NULL;
        }
    }
    IsoRescanMap(iso);
    rc = tilelib_background(iso, hi | P(uint16_t, h, 0x3a), hi | P(uint16_t, h, 0x38), I(int32_t, 0xcda2));
    if (!rc) {
        tilelib_free();
        forget_scenario();
        mmioClose(f, 0);
        return -0x104;
    }
    if (P(uint32_t, h, HDR_OBJ_SIZE)) {
        uint8_t *buf = winmalloc(P(uint32_t, h, HDR_OBJ_SIZE), 1);
        if (buf) {
            mmioSeek(f, P(LONG, h, HDR_OBJ_AT), SEEK_SET);
            got = mmioRead(f, (HPSTR)buf, P(LONG, h, HDR_OBJ_SIZE));
        }
        if (!buf || P(LONG, h, HDR_OBJ_SIZE) != got) {
            memset(g_iso + 0x1994, 0, 0x104);
            if (buf)
                winfree(buf);
            map_free();
            mmioClose(f, 0);
            return -0x50;
        }
        if (h[5] <= 5)
            add_objects(buf, P(uint32_t, h, HDR_OBJ_SIZE), h[5]);
        winfree(buf);
    }
    if (ai && !I(int, 0x175))
        ai_player_setup(I(int32_t, ISO_LOCAL_PLAYER));
    DebugStr((const char *)ORIG_PTR(0x101313ec), P(int32_t, h, HDR_UNIT_SIZE));
    if (P(uint32_t, h, HDR_UNIT_SIZE)) {
        mmioSeek(f, P(LONG, h, HDR_UNIT_AT), SEEK_SET);
        if (h[5] >= 3 && h[5] <= 5 && read_choke(f, h[5]) < 0)
            return fail(f, -0x50);
    }
    scenario_loaded();
    mmioClose(f, 0);
    int32_t cells = I(int32_t, ISO_MAP_H) * I(int32_t, ISO_MAP_W);
    for (int i = 0; i < cells; i++)
        I(uint32_t *, ISO_MAP)[i] &= 0xff00ffff;
    map_reset_view();
    if (I(int32_t, ISO_HAVE_SPRITES))
        units_after_load();
    if (!I(int32_t, ISO_HAVE_SPRITES)) {
        int fixed = 0;
        for (int row = 0; row < I(int32_t, ISO_MAP_H); row++) {
            int c = row << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f);
            for (int col = 0; col < I(int32_t, ISO_MAP_W); col++, c++)
                if ((I(uint32_t *, ISO_MAP)[c] & 0x7fff) >= 22000) {
                    fixed = 1;
                    I(uint32_t *, ISO_MAP)[c] &= 0xffff8000;
                }
        }
        if (fixed)
            message_box(0, (const char *)ORIG_PTR(0x10131414), (const char *)ORIG_PTR(0x10131404), 0);
    }
    if (I(int32_t, ISO_HAVE_SPRITES) && !I(void *, 0xe113))
        message_box(0, (const char *)ORIG_PTR(0x10131438), (const char *)ORIG_PTR(0x10131430), 0);
    return 0;
}

/* 0x1001e25a: loads the scenario named by string `name_id` */
int __stdcall IsoLoadScenario(uint8_t *iso, uint32_t name_id, int no_embedded, int ai)
{
    g_iso = iso;
    RtGetString(I(void *, ISO_APP), name_id); /* for the original's (silent) debug log */
    return scenario_load(iso, RtGetString(I(void *, ISO_APP), name_id), no_embedded, ai);
}
