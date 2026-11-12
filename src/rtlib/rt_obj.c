/* Engine core, 0x10011f54-0x10014840 (load.cpp in the original): the object table.
 *
 * Every resource id has a 12-byte entry (data, a second value, a third), in tables of
 * 0x1528 entries. What the three fields hold depends on the resource type:
 *   1 bitmap: data, use count           0xf bitmap reference: header copy, 0, bitmap id
 *   6 MIDI, 0xc wave: sound object      0x10 group: id list, size
 *   0x14 sprite: record, sprite, has members    0x16 tile map: record, map
 *   4, 5, 7-10: the game's own types, created and freed through callbacks
 *   2, 0xe, 0x11-0x13: plain data, size
 * Loading a type loads what it refers to; releasing undoes it with use counts.
 * Rewritten from the original. */
#include "rtcore.h"
#include "crt.h"
#include "rt.h"
#include "rt_obj.h"

#define HSL R(uint8_t *, RT_HSL)
#define RES R(ResFile *, RT_RES)

#define RT_AUDIO 0x0c90
#define RT_CBC 0x0cbc
#define RT_COMPOSE_MIDI 0x0ccc
#define RT_COMPOSE_WAVE 0x0cd0
#define RT_OBJECTS 0x325b        /* plugin objects */
#define RT_TABLES 0x3267         /* 13 entry tables */
#define RT_329F 0x329f
#define RT_COPY_SOURCE 0x32a3    /* per copied resource, the id it was copied from */
#define RT_COPY_PROC 0x36d3      /* proc(rt, new id, id, type) */
#define RT_FREE_PROC 0x36d7      /* proc(rt, id, type, data) */
#define RT_CREATE_PROC 0x36db    /* proc(rt, data, size, id): the game's object */
#define RT_LOAD_HOOK 0x3c2f      /* proc(rt, id, type) after a load */
#define RT_RELEASE_HOOK 0x3c33   /* proc(rt, id, type) before a release */
#define RT_N_PLUGINS 0x3c4b
#define RT_3DF7 0x3df7
#define RT_FIRST_COPY 0x3dfb     /* ids from here on are copies made at run time */
#define RT_3E37 0x3e37
#define RT_N_OBJECTS 0x3e43
#define RT_3E57 0x3e57
#define RT_DRAG_SPRITE 0x3e73
#define RT_LOADING_ID 0x3f13
#define RT_CD_PLAYING 0x95d2
#define RT_SCRIPT_DEBUG 0x95d8
#define RT_COMPOSE_OWNS 0x95dc
#define RT_LOADING 0x95e8

#define g_releasing_all G(uint32_t, 0x1007c598)

/* audio sounds are kept as 32-bit values */
#define AUDIO_PTR(v) ((uint8_t *)(uintptr_t)(v))
#define AudioDestroyMidi(a, m) AudioDestroyMidi(AUDIO_PTR(a), AUDIO_PTR(m))
#define AudioDestroyWave(a, s) AudioDestroyWave(AUDIO_PTR(a), AUDIO_PTR(s))
/* movies (type 0xd) and text objects (0x15) are kept in the table as 32-bit values */
#define PTR(type, v) ((type *)(uintptr_t)(v))
#define VAL(p) ((uint32_t)(uintptr_t)(p))
#define type_d_create(p, size, id) VAL(movie_open(p, size, id))
#define type_d_free(m) movie_free(PTR(Movie, m))
#define type_15_create(p, id) VAL(text_create((const U *)(p), id))
#define type_15_free(t) text_free(PTR(uint8_t, t))
#define type_15_attach(t, s) text_attach(PTR(uint8_t, t), VAL(s))
#define type_15_detach(t, s) text_detach(PTR(uint8_t, t), s)

#define TYPE(id) ((uint32_t)ResGetType(RES, id))
#define DATA(e) ((uint8_t *)(uintptr_t)(e)[0])

/* Calls one of the game's callbacks (their calling convention is not fixed) */
static uint32_t call4(uint32_t off, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t n)
{
    uint32_t args[4] = {a, b, c, d};
    return orig_stdcall_unsafe(R(void *, off), n, args);
}

/* The original's debug assertion: report, and break when the report asks for it */
static void assert_failed(int line)
{
    crt_assert_failed("D:\\DEV\\RUNTIME\\SRC\\load.cpp", line, "0");
}

static int wrong_type(uint32_t id, uint32_t type)
{
    char msg[128];
    wsprintfA(msg, "Id: %d Type: %d", id, type);
    if (R(uint8_t, RT_SCRIPT_DEBUG))
        return rt_script_error(0x6c, VAL(msg));
    rt_fatal(0x3e9, 0);
    return 0;
}

/* RtGetLibraryState (0x10011f54): the state of the plugin with this name (8 letters) */
uint32_t __cdecl RtGetLibraryState(uint8_t *rt, LPCSTR name)
{
    char key[256];
    g_rt = rt;
    lstrcpynA(key, name, 9);
    for (int i = 0; i < R(int, RT_N_PLUGINS); i++) {
        uint8_t *p = R(uint8_t **, RT_OBJECTS)[i];
        if (!lstrcmpiA((LPCSTR)(p + 0x2c), key))
            return P(uint32_t, p, 4);
    }
    return 0;
}

/* 0x10013270 */
uint32_t *__stdcall obj_entry(uint32_t id)
{
    return (uint32_t *)(R(uint8_t *, RT_TABLES + (id / 0x1528) * 4) + (id % 0x1528) * 0xc);
}

/* 0x10012000: script error 0x6c (or fatal error 1001) unless id has this type */
int __stdcall obj_check_type(uint32_t id, uint32_t type)
{
    return TYPE(id) == type ? 1 : wrong_type(id, type);
}

/* 0x10014663: reads resource id from the file. For types outside 0xd-0x16 the load is
 * wrapped: sprite messages off, a progress callback every 0x2000 bytes. */
uint8_t *__stdcall obj_load(uint32_t id, uint32_t *size)
{
    uint32_t t = TYPE(id);
    if (t >= 0xd && t <= 0x16)
        return ResLoadResource(RES, id, size);
    int msgs = SpriteSetSendMsg(HSL, 0);
    load_progress();
    void *old = ResSetCallBack(RES, (void(__stdcall *)(void))(void *)load_progress, 0x2000);
    uint8_t loading = R(uint8_t, RT_LOADING);
    R(uint8_t, RT_LOADING) = 1;
    uint32_t loading_id = R(uint32_t, RT_LOADING_ID);
    R(uint32_t, RT_LOADING_ID) = id;
    uint8_t *p = ResLoadResource(RES, id, size);
    SpriteSetSendMsg(HSL, msgs);
    R(uint8_t, RT_LOADING) = loading;
    R(uint32_t, RT_LOADING_ID) = loading_id;
    ResSetCallBack(RES, old, 0x2000);
    return p;
}

/* 0x100147a6, 0x100147e8: makes sure the entry has the resource's data (and size) */
void __stdcall obj_ensure(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (!e[0])
        e[0] = (uint32_t)(uintptr_t)obj_load(id, NULL);
}

void __stdcall obj_ensure_sized(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (!e[0]) {
        uint32_t size;
        e[0] = (uint32_t)(uintptr_t)obj_load(id, &size);
        e[1] = size;
    }
}

/* 0x10014409: whether id (of this type) is loaded */
uint32_t __stdcall obj_loaded(uint32_t id, uint32_t type)
{
    if (TYPE(id) != type)
        return (uint32_t)wrong_type(id, type);
    uint32_t *e = obj_entry(id);
    switch (type) {
    case 1: case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 0xb: case 0xc: case 0xd: case 0xe:
    case 0xf: case 0x11: case 0x12: case 0x13: case 0x16:
        return e[0] != 0;
    case 0x14:
        return e[1] != 0;
    case 0x15:
        return e[2] != 0;
    }
    return 0;
}

/* ---- releasing ---- */

/* 0x100126bf: a bitmap nobody uses any more */
void __stdcall rel_bitmap(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (e[0] && !e[1]) {
        winfree(DATA(e));
        e[0] = 0;
    }
}

/* 0x10012717: a bitmap reference; lets go of its bitmap */
int __stdcall rel_bitmap_ref(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (!e[0] || e[1])
        return 1;
    if (e[2]) {
        obj_entry(e[2])[1]--;
        rel_bitmap(e[2]);
        e[2] = 0;
    }
    winfree(DATA(e));
    e[0] = 0;
    return 0;
}

/* 0x100127b3 */
void __stdcall rel_data(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (e[0]) {
        winfree(DATA(e));
        e[0] = 0;
    }
}

/* 0x10012f28: plain data with its size */
int __stdcall load_sized(uint32_t id, uint32_t type)
{
    (void)type;
    uint32_t *e = obj_entry(id);
    if (e[0])
        return 1;
    uint32_t size;
    uint8_t *p = obj_load(id, &size);
    if (!p)
        return 0;
    e[0] = (uint32_t)(uintptr_t)p;
    e[1] = size;
    return 1;
}

/* 0x100127f9: a group: releases every member. The list itself is not freed. */
int __stdcall rel_group(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (!e[0] && !load_sized(id, 0))
        return 0;
    const uint32_t *m = (const uint32_t *)DATA(e);
    uint32_t n = e[1] >> 2;
    for (uint32_t i = 0; i < n; i++)
        obj_release(m[i], TYPE(m[i]));
    e[0] = 0;
    return 1;
}

/* 0x100128a9, 0x10012e8c: sounds. While everything is being released, the sound of the
 * composed sprite is kept. */
void __stdcall rel_midi(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (g_releasing_all && e[0] == R(uint32_t, RT_COMPOSE_MIDI))
        return;
    if (e[0])
        AudioDestroyMidi(R(uint32_t, RT_AUDIO), e[0]);
    if (e[0] == R(uint32_t, RT_COMPOSE_MIDI))
        R(uint32_t, RT_COMPOSE_MIDI) = 0;
    e[0] = 0;
}

void __stdcall rel_wave(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (g_releasing_all && e[0] == R(uint32_t, RT_COMPOSE_WAVE))
        return;
    if (e[0])
        AudioDestroyWave(R(uint32_t, RT_AUDIO), e[0]);
    if (e[0] == R(uint32_t, RT_COMPOSE_WAVE))
        R(uint32_t, RT_COMPOSE_WAVE) = 0;
    e[0] = 0;
}

/* 0x10012945, 0x10012e22: types 0xd and 0x15 keep a helper object in the third field */
void __stdcall rel_type_d(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (e[0] && !e[1]) {
        winfree(DATA(e));
        e[0] = 0;
        e[2] = type_d_free(e[2]);
    }
}

void __stdcall rel_type_15(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (e[0] && !e[1]) {
        winfree(DATA(e));
        e[0] = 0;
        e[2] = type_15_free(e[2]);
    }
}

/* 0x100129af: the game's own types go back through its callback */
void __stdcall rel_custom(uint32_t id, uint32_t type)
{
    if (!R(uint32_t, RT_FREE_PROC))
        return;
    uint32_t *e = obj_entry(id);
    if (e[0]) {
        call4(RT_FREE_PROC, (uint32_t)(uintptr_t)g_rt, id, type, e[0], 4);
        e[0] = 0;
        e[1] = 0;
    }
}

/* 0x10012c1b */
void __stdcall rel_type_b(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (e[0] && P(uint32_t, DATA(e), 8)) {
        winfree(DATA(e));
        e[0] = 0;
    }
}

/* 0x10012d51: drops one use of each member and releases it */
void __stdcall members_release(const uint32_t *ids, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        uint32_t id = ids[i];
        if (!id)
            continue;
        uint32_t t = TYPE(id);
        uint32_t *m = obj_entry(id);
        if (m[1])
            m[1]--;
        obj_release(id, t);
    }
}

/* 0x10012c79: a tile map and its tiles */
int __stdcall rel_tile_map(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (!e[0])
        return 0;
    uint8_t *map = (uint8_t *)(uintptr_t)e[1];
    uint32_t n = map ? P(uint32_t, map, 0x40) : 0;
    if (n)
        members_release((const uint32_t *)(DATA(e) + 0x14), n);
    if (R(uint8_t *, RT_TILE_MAP) == map) {
        SpriteSetTileBackground(HSL, 0, NULL);
        R(uint32_t, RT_TILE_MAP) = 0;
    }
    if (map)
        winfree(map);
    e[0] = 0;
    e[1] = 0;
    return 0;
}

/* 0x10012deb */
int __stdcall sprite_forget(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    e[1] = 0;
    P(uint8_t, e, 8) = 0;
    return 1;
}

/* 0x10012b31: releases what a sprite's record lists (bitmaps lose a use) */
void __stdcall sprite_release_members(uint32_t sprite, uint32_t *e)
{
    uint8_t *d = DATA(e);
    P(uint8_t, e, 8) = 0;
    uint32_t n = P(uint32_t, d, 0xc);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t id = P(uint32_t, d, 0x26 + i * 4);
        uint32_t t = TYPE(id);
        if (t == 0x15)
            type_15_detach(obj_entry(id)[2], sprite);
        if (t == 1 || t == 0xd || t == 0x15 || t == 0xf) {
            uint32_t *m = obj_entry(id);
            if (m[1])
                m[1]--;
        }
        obj_release(id, t);
    }
}

/* 0x10012a2a: destroys a sprite. While everything is being released the composed sprite
 * survives (only its entry is cleared). */
int __stdcall rel_sprite(uint32_t id)
{
    if (R(uint32_t, RT_DRAG_SPRITE) == id)
        R(uint32_t, RT_DRAG_SPRITE) = 0;
    uint32_t *e = obj_entry(id);
    uint32_t s = e[1];
    if (g_releasing_all && s && R(uint32_t, RT_COMPOSE_SPRITE) == s)
        return sprite_forget(id);
    if (s)
        SpriteDestroy(HSL, (uint8_t *)(uintptr_t)s, g_releasing_all == 0, 0);
    e[1] = 0;
    if (P(uint8_t, e, 8))
        sprite_release_members(s, e);
    if (R(uint32_t, RT_COMPOSE_SPRITE) == s)
        R(uint32_t, RT_COMPOSE_SPRITE) = 0;
    return 1;
}

/* 0x10012546: releases id as a resource of this type */
int __cdecl obj_release(uint32_t id, uint32_t type)
{
    int r = 1;
    if (R(uint32_t, RT_RELEASE_HOOK))
        call4(RT_RELEASE_HOOK, (uint32_t)(uintptr_t)g_rt, id, type, 0, 3);
    switch (type) {
    case 1:
        rel_bitmap(id);
        break;
    case 2:
        rel_data(id);
        break;
    case 4: case 5: case 7: case 8: case 9: case 10:
        rel_custom(id, type);
        break;
    case 6:
        rel_midi(id);
        break;
    case 0xb:
        rel_type_b(id);
        break;
    case 0xc:
        rel_wave(id);
        break;
    case 0xd:
        rel_type_d(id);
        break;
    case 0xf:
        rel_bitmap_ref(id);
        break;
    case 0x10:
        r = rel_group(id);
        break;
    case 0x14:
        r = rel_sprite(id);
        break;
    case 0x15:
        rel_type_15(id);
        break;
    case 0x16:
        rel_tile_map(id);
        break;
    }
    return r;
}

/* 0x10012f96: releases everything and frees the tables */
void __cdecl obj_release_all(void)
{
    g_releasing_all = 1;
    for (uint32_t id = 0; id < R(uint32_t, RT_N_OBJECTS); id++) {
        obj_release(id, TYPE(id));
        if (R(uint32_t, RT_FIRST_COPY) <= id)
            ResDeleteResource(RES, id);
    }
    g_releasing_all = 0;
    R(uint32_t, RT_3E37) = 0;
    R(uint32_t, RT_3E57) = 0;
    R(uint32_t, RT_N_OBJECTS) = 0;
    array_free((RtArray *)(g_rt + 0xb54));
    array_free((RtArray *)(g_rt + 0xb40));
    for (uint32_t i = 0; i < 0xd; i++) {
        if (R(uint32_t, RT_TABLES + i * 4))
            winfree(R(void *, RT_TABLES + i * 4));
        R(uint32_t, RT_TABLES + i * 4) = 0;
    }
    if (R(uint32_t, RT_COPY_SOURCE))
        R(void *, RT_COPY_SOURCE) = winfree(R(void *, RT_COPY_SOURCE));
    if (R(uint32_t, RT_329F))
        R(void *, RT_329F) = winfree(R(void *, RT_329F));
    if (R(uint32_t, RT_CBC))
        R(void *, RT_CBC) = winfree(R(void *, RT_CBC));
}

/* 0x10013176: forgets the composed sprite and its sounds; destroy: destroys them too */
void __stdcall compose_reset(int destroy)
{
    if (destroy && R(uint32_t, RT_COMPOSE_SPRITE))
        SpriteDestroy(HSL, R(uint8_t *, RT_COMPOSE_SPRITE), 1, 1);
    if (destroy && R(uint32_t, RT_COMPOSE_WAVE))
        AudioDestroyWave(R(uint32_t, RT_AUDIO), R(uint32_t, RT_COMPOSE_WAVE));
    if (destroy && R(uint32_t, RT_COMPOSE_MIDI))
        AudioDestroyMidi(R(uint32_t, RT_AUDIO), R(uint32_t, RT_COMPOSE_MIDI));
    R(uint32_t, RT_COMPOSE_SPRITE) = 0;
    R(uint32_t, RT_COMPOSE_WAVE) = 0;
    R(uint32_t, RT_COMPOSE_MIDI) = 0;
    R(uint8_t, RT_COMPOSE_OWNS) = 0;
}

/* ---- loading ---- */

/* 0x1001383d */
int __stdcall load_bitmap(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (e[0])
        return 1;
    uint8_t *p = obj_load(id, NULL);
    if (!p)
        return 0;
    e[0] = (uint32_t)(uintptr_t)p;
    e[1] = 0;
    return 1;
}

/* 0x100138aa: a bitmap reference: its own header (flip flags, offsets) on another
 * bitmap's pixels */
int __stdcall load_bitmap_ref(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (e[0])
        return 1;
    const uint32_t *ref = (const uint32_t *)obj_load(id, NULL);
    if (!ref)
        return 0;
    uint8_t *h = winmalloc(0x24, 0);
    if (!h)
        return 0;
    e[0] = (uint32_t)(uintptr_t)h;
    e[1] = 0;
    if (!load_bitmap(ref[0]))
        return rel_bitmap_ref(id);
    e[2] = ref[0];
    uint32_t *b = obj_entry(ref[0]);
    b[1]++;
    memcpy(h, DATA(b), 0x24);
    P(uint32_t, h, 0x10) = (P(uint32_t, h, 0x10) & 0xffffffcf) | (ref[1] & 0x30);
    P(uint32_t, h, 0x14) = ref[2];
    P(uint32_t, h, 0x18) = ref[3];
    P(uint32_t, h, 0x1c) = id;
    return 1;
}

/* 0x100139c5: a group: the id list, then every member */
int __stdcall load_group_members(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (e[0])
        return 1;
    uint32_t size;
    const uint32_t *m = (const uint32_t *)obj_load(id, &size);
    if (!m)
        return 0;
    e[0] = (uint32_t)(uintptr_t)m;
    e[1] = size;
    uint32_t i = 0;
    for (; i < size >> 2; i++)
        if (!obj_is(m[i], TYPE(m[i])))
            break;
    return i == size >> 2;
}

/* 0x10013561: collects into arr the bitmaps that loading id will need and that aren't
 * loaded yet (through references, groups and sprites) */
int __stdcall collect_bitmaps(void *arr, uint32_t id, uint32_t type)
{
    RtArray *a = arr;
    if (type != 1 && type != 0xf && type != 0x10 && type != 0x14)
        return 1;
    if (obj_loaded(id, type))
        return 1;
    if (type == 1 || type == 0xf) {
        if (type == 0xf) {
            const uint32_t *ref = (const uint32_t *)obj_load(id, NULL);
            if (!ref)
                return 0;
            id = ref[0];
            if (obj_loaded(id, 1))
                return 1;
        }
        const uint32_t *have = (const uint32_t *)a->data;
        for (uint32_t i = 0; i < a->count; i++)
            if (have[i] == id)
                return 1;
        uint32_t *slot = array_add(a);
        if (!slot)
            return 0;
        *slot = id;
        return 1;
    }
    uint32_t size;
    const uint8_t *m = obj_load(id, &size);
    if (!m)
        return 0;
    if (type == 0x14) {
        m += 0x26;
        size -= 0x26;
    }
    uint32_t n = size >> 2;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t member = P(uint32_t, m, i * 4);
        if (!collect_bitmaps(a, member, TYPE(member)))
            return 0;
    }
    return 1;
}

/* 0x1001247e: a resource read by the batch loader arrives */
int __stdcall obj_batch_loaded(uint32_t *data, uint32_t size, uint32_t type, uint32_t id)
{
    uint32_t *e;
    if (type == 1) {
        e = obj_entry(id);
        e[0] = (uint32_t)(uintptr_t)data;
        e[1] = 0;
    } else if (type == 2) {
        e = obj_entry(id);
        e[0] = (uint32_t)(uintptr_t)data;
        e[1] = size;
    } else if (type == 0xb) {
        e = obj_entry(id);
        e[0] = (uint32_t)(uintptr_t)data;
        e[1] = size;
        data[0] = (uint32_t)(uintptr_t)(data + 0xf);
        data[3] = id;
    }
    return 1;
}

/* Reads the bitmaps id needs in one pass over the file, when there are several */
static void preload_bitmaps(uint32_t id, uint32_t type)
{
    static RtArray ids; /* 0x1007c5a0, 0x1007c5b8 */
    memset(&ids, 0, sizeof ids);
    array_init(&ids, 4, 0, 0x20);
    if (collect_bitmaps(&ids, id, type) && ids.count > 1)
        res_load_batch(RES, obj_batch_loaded, (uint32_t *)ids.data, ids.count);
    array_free(&ids);
}

/* 0x100134d4 */
int __stdcall load_group(uint32_t id)
{
    preload_bitmaps(id, 0x10);
    return load_group_members(id);
}

/* 0x100137b0 */
int __stdcall load_sprite(uint32_t id)
{
    preload_bitmaps(id, 0x14);
    return sprite_create(id);
}

/* 0x10013aa9, 0x10014370: sounds; a playing CD track is stopped first */
static void stop_cd(void)
{
    if (R(uint8_t, RT_CD_PLAYING))
        R(uint8_t, RT_CD_PLAYING) = (uint8_t)AudioStopCD(R(uint8_t *, RT_AUDIO));
}

int __stdcall load_midi(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (!e[0]) {
        stop_cd();
        e[0] = (uint32_t)(uintptr_t)ResCreateMidiEvent(RES, R(uint32_t, RT_AUDIO), id);
    }
    return 1;
}

int __stdcall load_wave(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (!e[0]) {
        stop_cd();
        e[0] = ResCreateWaveEvent(RES, R(uint32_t, RT_AUDIO), id,
                                  (uint32_t(__stdcall *)(uint32_t, void *, void *))(void *)AudioCreateWave);
    }
    return 1;
}

/* 0x10013b3d, 0x100142ed. (Type 0xd's data pointer is not stored, so its release never
 * finds anything to free.) */
int __stdcall load_type_d(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (e[0])
        return 1;
    uint32_t size;
    uint8_t *p = obj_load(id, &size);
    if (!p)
        return 0;
    e[2] = type_d_create(p, size, id);
    return e[2] != 0;
}

int __stdcall load_type_15(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (e[2])
        return 1;
    uint8_t *p = obj_load(id, NULL);
    if (!p)
        return 0;
    e[2] = type_15_create(p, id);
    return e[2] != 0;
}

/* 0x10013bc5: the game's own types: its callback turns the data into an object */
int __stdcall load_custom(uint32_t id, uint32_t type)
{
    (void)type;
    if (!R(uint32_t, RT_CREATE_PROC))
        return 0;
    uint32_t *e = obj_entry(id);
    if (e[0])
        return 1;
    uint32_t size;
    uint8_t *p = obj_load(id, &size);
    if (!p)
        return 0;
    e[1] = size;
    e[0] = call4(RT_CREATE_PROC, (uint32_t)(uintptr_t)g_rt, (uint32_t)(uintptr_t)p, size, id, 4);
    return e[0] != 0;
}

/* 0x10013c79 */
int __stdcall load_type_b(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (e[0])
        return 1;
    uint32_t size;
    uint8_t *p = obj_load(id, &size);
    if (!p)
        return 0;
    P(uint32_t, p, 0) = (uint32_t)(uintptr_t)(p + 0x3c);
    P(uint32_t, p, 0xc) = id;
    e[0] = (uint32_t)(uintptr_t)p;
    e[1] = size;
    return 1;
}

/* 0x10013e06: loads a tile map's tiles (bitmaps or references) and stores their data */
int __stdcall tile_map_members(uint32_t *out, const uint32_t *ids, uint32_t n)
{
    uint32_t i = 0;
    for (; i < n; i++) {
        uint32_t id = ids[i];
        if (id) {
            uint32_t t = TYPE(id);
            if ((t != 1 && t != 0xf) || !obj_is(id, t))
                break;
            uint32_t *m = obj_entry(id);
            m[1]++;
            out[i] = m[0];
        }
    }
    return n <= i;
}

/* 0x10013d01: a tile map: a 20-byte head and a list of tile ids become the sprite
 * engine's map record (0x7c bytes plus one pointer per tile) */
int __stdcall load_tile_map(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (e[0])
        return 1;
    uint32_t size;
    uint8_t *p = obj_load(id, &size);
    if (!p)
        return 0;
    e[0] = (uint32_t)(uintptr_t)p;
    uint32_t n = (size - 0x14) >> 2;
    uint32_t *map = winmalloc(n * 4 + 0x7c, 1);
    if (!map)
        return rel_tile_map(id);
    e[1] = (uint32_t)(uintptr_t)map;
    memcpy(map + 7, p, 0x14);
    map[0xf] = id;
    map[0x10] = n;
    map[0] = (uint32_t)(uintptr_t)(map + 0x1f);
    if (!tile_map_members(map + 0x1f, (const uint32_t *)(p + 0x14), n))
        return rel_tile_map(id);
    return 1;
}

/* 0x10014025: a sprite that carries its own palette takes over the composed slot */
int __stdcall compose_palette(uint8_t *d)
{
    uint32_t n = P(uint32_t, d, 0xc);
    compose_reset(R(uint8_t, RT_COMPOSE_OWNS));
    SpriteChangePalette(HSL, 0xf6 - R(uint32_t, RT_3DF7), P(uint32_t, d, 0x18), (const uint32_t *)(d + 0x46 + n * 4), 1, 0);
    return 1;
}

/* 0x1001409c: loads a sprite's members: bitmaps go to the sprite (50 at a time), a wave
 * or MIDI becomes the composed sprite's sound */
int __stdcall sprite_members(uint8_t *s, const uint32_t *ids, uint32_t n)
{
    uint32_t bitmaps[50], count = 0, wave = 0, midi = 0, i = 0;
    for (; i < n; i++) {
        uint32_t id = ids[i], t = TYPE(id);
        if (!obj_is(id, t))
            break;
        uint32_t *m = obj_entry(id);
        if (t == 1 || t == 0x15 || t == 0xf) {
            bitmaps[count++] = m[0];
            m[1]++;
            if (count > 0x31) {
                if (!SpriteAppendBitmaps(HSL, s, bitmaps, count))
                    break;
                count = 0;
            }
            if (t == 0x15 && !type_15_attach(m[2], s))
                break;
        } else if (t == 0xd)
            m[1]++;
        else if (t == 0xc)
            wave = m[0];
        else if (t == 6)
            midi = m[0];
    }
    if (i < n)
        return 0;
    if (count && !SpriteAppendBitmaps(HSL, s, bitmaps, count))
        return 0;
    if (R(uint8_t *, RT_COMPOSE_SPRITE) == s) {
        R(uint32_t, RT_COMPOSE_WAVE) = wave;
        R(uint32_t, RT_COMPOSE_MIDI) = midi;
    }
    return 1;
}

/* 0x10013edf: creates the sprite of a sprite record (+8 member count, +0x1c level,
 * +0x21 and +0x24 pick mode, +0x23 has a palette, +0x25 scrolls, +0x26 member ids) */
int __stdcall sprite_create(uint32_t id)
{
    uint32_t *e = obj_entry(id);
    if (e[1])
        return 1;
    obj_ensure(id);
    uint8_t *d = DATA(e);
    P(uint32_t, d, 0xc) = P(uint32_t, d, 8);
    uint8_t *s = SpriteCreate(HSL, P(int32_t, d, 0x1c), 0);
    if (!s)
        return 0;
    SpriteSetId(s, id);
    SpriteSetPickMode(s, d[0x21], d[0x24]);
    SpriteSetScrollable(HSL, s, d[0x25]);
    if (d[0x23]) {
        compose_palette(d);
        R(uint8_t *, RT_COMPOSE_SPRITE) = s;
    }
    if (!sprite_members(s, (const uint32_t *)(d + 0x26), P(uint32_t, d, 0xc))) {
        rel_sprite(id);
        return 0;
    }
    e[1] = (uint32_t)(uintptr_t)s;
    P(uint8_t, e, 8) = 1;
    SpriteEnable(s, 1);
    return 1;
}

/* 0x100132ad: makes sure id is loaded as a resource of this type */
int __stdcall obj_is(uint32_t id, uint32_t type)
{
    if (TYPE(id) != type)
        return wrong_type(id, type);
    int r = 0;
    switch (type) {
    case 1:
        r = load_bitmap(id);
        break;
    case 2: case 0xe: case 0x11: case 0x12: case 0x13:
        r = load_sized(id, type);
        break;
    case 4: case 5: case 7: case 8: case 9: case 10:
        r = load_custom(id, type);
        break;
    case 6:
        r = load_midi(id);
        break;
    case 0xb:
        r = load_type_b(id);
        break;
    case 0xc:
        r = load_wave(id);
        break;
    case 0xd:
        r = load_type_d(id);
        break;
    case 0xf:
        r = load_bitmap_ref(id);
        break;
    case 0x10:
        r = load_group(id);
        break;
    case 0x14:
        r = load_sprite(id);
        break;
    case 0x15:
        r = load_type_15(id);
        break;
    case 0x16:
        r = load_tile_map(id);
        break;
    }
    if (R(uint32_t, RT_LOAD_HOOK))
        call4(RT_LOAD_HOOK, (uint32_t)(uintptr_t)g_rt, id, type, 0, 3);
    return r;
}

/* ---- copies ---- */

/* 0x100121fb: a run-time copy of a resource (sprites and the game's types 7-0xb);
 * returns the new id */
uint32_t __stdcall obj_copy(uint32_t id)
{
    uint32_t t = TYPE(id);
    int custom = t == 7 || t == 9 || t == 8 || t == 10;
    if (!custom && t != 0x14 && t != 0xb) {
        assert_failed(0x35a);
        return 0;
    }
    if (t == 0x14 && !obj_is(id, 0x14)) {
        assert_failed(0x35f);
        return 0;
    }
    uint32_t copy = ResCopyResource(RES, id);
    if (R(uint32_t, RT_COPY_SOURCE))
        R(uint32_t *, RT_COPY_SOURCE)[copy - R(uint32_t, RT_FIRST_COPY)] = id;
    if (!copy) {
        assert_failed(0x370);
        return 0;
    }
    if (custom && R(uint32_t, RT_COPY_PROC))
        call4(RT_COPY_PROC, (uint32_t)(uintptr_t)g_rt, copy, id, t, 4);
    return copy;
}

/* 0x100123b5: releases a resource and removes it from the file's table */
int __stdcall obj_delete(uint32_t id)
{
    uint32_t t = TYPE(id);
    if (t != 0x14 && t != 0xb && t != 8 && t != 10 && t != 7 && t != 9)
        return 0;
    if (!obj_release(id, t) || !ResDeleteResource(RES, id))
        return 0;
    obj_entry(id)[0] = 0;
    return 1;
}

/* 0x10012092: draws bitmap src (type 1 or 0x15) into bitmap dst at x, y, both centered
 * on their hot spots; mask: clear dst where src is solid instead */
int __stdcall bitmap_merge_objects(uint32_t dst, uint32_t src, int x, int y, int mask)
{
    if (!obj_is(dst, 1))
        return 0;
    uint32_t t = TYPE(src);
    if ((t != 1 && t != 0x15) || !obj_is(src, t))
        return 0;
    uint32_t d = obj_entry(dst)[0], s = obj_entry(src)[0];
    RECT db, sb, area;
    SpriteDibGetBox((uint8_t *)(uintptr_t)d, (int32_t *)&db);
    SpriteDibGetBox((uint8_t *)(uintptr_t)s, (int32_t *)&sb);
    OffsetRect(&sb, x, y);
    if (!IntersectRect(&area, &db, &sb))
        return 0;
    if (!mask)
        SpriteDibMerge(d, s, area.right - area.left, area.bottom - area.top, area.left - db.left, area.top - db.top,
                       area.left - sb.left, area.top - sb.top);
    else
        sprite_dib_mask(d, s, area.right - area.left, area.bottom - area.top, area.left - db.left, area.top - db.top,
                        area.left - sb.left, area.top - sb.top);
    return 1;
}
