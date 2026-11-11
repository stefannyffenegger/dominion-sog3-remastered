/* Engine core, 0x1001f550-0x10022e5c: scenes. A scene is one resource file with its
 * script; changing scenes saves the current one's variables, resets the sprite,
 * sound and object state, and loads and starts the next.
 * Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_scene.h"

#define C0(a) ORIG(U(__cdecl *)(void), a)()
#define C1(a, x) ORIG(U(__stdcall *)(U), a)((U)(x))
#define RU(off) R(U, off)
#define RB(off) R(uint8_t, off)
#define HSL R(uint8_t *, RT_HSL)
#define RES R(ResFile *, RT_RES)
#define AUDIO R(uint8_t *, RT_AUDIO)

#define RT_CLICK_RECTS 0x0b40
#define RT_HOVERS 0x0b54
#define RT_AUDIO 0x0c90
#define RT_TOPMOST 0x0cd4
#define RT_KEYS 0x0ce0
#define RT_EVENT_SLOTS 0x1947
#define RT_RES_BUF 0x3247
#define RT_LOAD_BUF 0x3263
#define RT_SCENES 0x329b       /* 0x10c bytes each: name (0x100), vars, ?, extra */
#define RT_SYS_PALETTE 0x32af
#define RT_PALETTE_COLORS 0x3e3b
#define RT_PAL_RESERVED 0x3df7
#define RT_DRAW_INFO 0x36cf
#define RT_SCENE 0x3dcf
#define RT_SCENE_COUNT 0x3e4b
#define RT_SCRIPT_START 0x3e7b
#define RT_SLOT_HANDLERS 0x3e7f
#define RT_PREV_SCENE 0x3f37
#define RT_VARS 0x3f4f
#define RT_CD_PLAYING 0x95d2
#define RT_COMPONENT 0x95d6
#define RT_SHOWN 0x95db
#define RT_SCENE_CHANGED 0x95dc
#define RT_NO_FADE 0x95ea
#define RT_PALETTE_DEVICE 0x95ed
#define RT_PLAYER 0x95f2
#define RT_LOADING 0x95f9
#define RT_DRAW_FLAGS 0x95c7
#define RT_SCENE_NAME 0x9954

#define SCENE_SIZE 0x10c
#define VARS_SIZE 0x4e24

#define flush_messages(m) C1(0x100260b4, m)
#define paint_black ORIG(void(__stdcall *)(HDC), 0x100268f0)
#define rt_script_error ORIG(U(__stdcall *)(U, U), 0x100260f6)
#define script_run(h) C1(0x10022ce9, h)
#define scene_open(name) C1(0x10020276, name)
#define scene_read(name) C1(0x10020564, name)
#define scene_player_setup() C0(0x100200ba)

static uint8_t *scene_entry(U i) { return R(uint8_t *, RT_SCENES) + i * SCENE_SIZE; }

/* 0x1001f550: sets a variable saved with the current scene */
U __stdcall scene_var_set(U n, U v)
{
    P(U *, scene_entry(RU(RT_SCENE)), 0x100)[n] = v;
    return 1;
}

/* 0x1001f599: the scene's palette: the system colors around the resource file's own,
 * then the reserved top colors from the sprite engine */
void __cdecl scene_palette_load(void)
{
    uint8_t *pal = g_rt + RT_SYS_PALETTE;
    GetSystemPaletteEntries(R(HDC, RT_DC), 0, 0x100, (PALETTEENTRY *)pal);
    RU(RT_PALETTE_COLORS) = ResLoadPalette(RES, pal + 10 * 4);
    U first = 0xf6 - RU(RT_PAL_RESERVED);
    SpriteGetPalette(HSL, pal + first * 4, first, RU(RT_PAL_RESERVED));
    SpriteSetPalette(HSL, pal);
    SpriteRealizePalette(HSL);
    plugins_palette();
}

/* 0x1001f671: forgets every scene but the current one, which becomes scene 0. (The
 * shrunk table from winrealloc is not stored back, as in the original; shrinking keeps
 * the block in place.) */
void __cdecl scenes_trim(void)
{
    uint8_t keep[SCENE_SIZE];
    memcpy(keep, scene_entry(RU(RT_SCENE)), SCENE_SIZE);
    for (U i = 0; i < RU(RT_SCENE_COUNT); i++) {
        if (i == RU(RT_SCENE))
            continue;
        if (P(U, scene_entry(i), 0x108))
            winfree(P(void *, scene_entry(i), 0x108));
        if (P(U, scene_entry(i), 0x100))
            winfree(P(void *, scene_entry(i), 0x100));
    }
    RU(RT_SCENE) = 0;
    RU(RT_SCENE_COUNT) = 1;
    void *p = winrealloc(R(void *, RT_SCENES), SCENE_SIZE, 0);
    if (p)
        memcpy(p, keep, SCENE_SIZE);
}

/* 0x1001fb4b: drops pending clicks and key presses */
void __cdecl input_flush(void)
{
    flush_messages(WM_LBUTTONDOWN);
    flush_messages(WM_KEYDOWN);
}

static void stop_cd(void)
{
    if (RB(RT_CD_PLAYING))
        RB(RT_CD_PLAYING) = AudioStopCD(AUDIO) != 0;
}

/* 0x1001f805: leaves the current scene and loads the one in `path` */
void __stdcall scene_change(char *path)
{
    memcpy(scene_entry(RU(RT_SCENE)), g_rt + RT_SCENE_NAME, 0x100);
    memcpy(P(void *, scene_entry(RU(RT_SCENE)), 0x100), g_rt + RT_VARS, VARS_SIZE);
    RU(RT_PREV_SCENE) = RU(RT_SCENE);
    stop_cd();
    char *name = SkipPath(path);
    SpriteSetTopMost(HSL, R(uint8_t *, RT_TOPMOST));
    SpriteSetClipBox(HSL, NULL);
    SpriteSetScrollBox(HSL, NULL);
    SpriteSetSendMsg(HSL, 0);
    if (!RB(RT_PALETTE_DEVICE))
        SpriteSetBkgTransparent(HSL, 1);
    scene_hide(100);
    plugins_notify_20(2, (U)(uintptr_t)name);
    flush_messages(0x52c);
    flush_messages(0x52d);
    flush_messages(0x4c9);
    obj_release_all();
    SpriteSetBackground(HSL, NULL, NULL);
    SpriteResetLibrary(HSL);
    AudioResetLibrary(AUDIO);
    SpriteSetSendMsg(HSL, 1);
    if (RU(RT_RES_BUF))
        RU(RT_RES_BUF) = (U)(uintptr_t)winfree(R(void *, RT_RES_BUF));
    stop_cd();
    if (RU(RT_RES))
        ResCloseFile(RES);
    RU(RT_RES) = 0;
    plugins_unload();
    RB(RT_SCENE_CHANGED) = 1;
    paint_black(R(HDC, RT_DC));
    if (scene_load(name, 1, 0, 0)) {
        if (RU(RT_TOPMOST))
            SpriteBreakLoops(R(uint8_t *, RT_TOPMOST), 1);
        if (RB(RT_SHOWN))
            scene_show(RB(RT_NO_FADE) ? 1 : 100);
    }
    input_flush();
    if (RB(RT_SHOWN) && !RB(RT_PALETTE_DEVICE))
        SpriteSetBkgTransparent(HSL, 0);
}

/* 0x1001fb6f: opens and reads the scene `name`, sets its palette and runs its start
 * handler. `cb(arg, 0)` and `cb(arg, 1)` are called around the start handler. */
U __stdcall scene_load(char *name, U progress, U cb, U arg)
{
    static const uint16_t zero32[] = {0x3f33, 0x3f2f, 0x3f2b, 0x3f17, 0x3f07, 0x3e6f, 0x3e6b,
                                      0x3e67, 0x3e63, 0x3e5f, 0x3e57, 0x3e53, 0x3e47, 0x3e3f,
                                      0x3e37, 0x3e33, 0x3e2f, 0x3e27, 0x3e1b};
    static const uint16_t zero8[] = {0x95fa, 0x95f8, 0x95f1, 0x95f0, 0x95ee,
                                     0x95e5, 0x95e1, 0x95e0, 0x95df};
    HCURSOR old = GetCursor();
    SetCursor(LoadCursorA(NULL, IDC_WAIT));
    memset(g_rt + RT_EVENT_SLOTS, 0, 0x1900);
    for (unsigned i = 0; i < sizeof zero32 / sizeof *zero32; i++)
        RU(zero32[i]) = 0;
    for (unsigned i = 0; i < sizeof zero8 / sizeof *zero8; i++)
        RB(zero8[i]) = 0;
    RU(0x3f3f) = 0;
    RU(0x3dd7) = 0;
    RU(RT_DRAW_INFO) = (U)(uintptr_t)(g_rt + RT_DRAW_FLAGS);
    array_init((RtArray *)(g_rt + RT_HOVERS), 0x18, 0, 10);
    array_init((RtArray *)(g_rt + RT_CLICK_RECTS), 0x19, 0, 1);
    memset(g_rt + RT_SLOT_HANDLERS, 0, 0x88);
    memset(g_rt + RT_KEYS, 0, 0xbd7);
    if (!scene_open(name)) {
        if (RU(RT_RES))
            ResCloseFile(RES);
        RU(RT_RES) = 0;
        RU(0x3e43) = 0;
        SetCursor(old);
        return 0;
    }
    if (!scene_read(name)) {
        SetCursor(old);
        return rt_script_error(100, 0);
    }
    if (RB(RT_PLAYER) || RB(RT_COMPONENT))
        scene_player_setup();
    if (progress)
        ResSetCallBack(RES, (void(__stdcall *)(void))ORIG_PTR(0x1002635d), 0x2000);
    scene_palette_load();
    RB(RT_LOADING) = 1;
    RB(RT_SHOWN) = 1;
    palette_set_painting(HSL, 0, 0);
    if (cb) {
        U a[2] = {arg, 0};
        orig_stdcall_unsafe((void *)(uintptr_t)cb, 2, a);
    }
    script_run(RU(RT_SCRIPT_START));
    if (cb) {
        U a[2] = {arg, 1};
        orig_stdcall_unsafe((void *)(uintptr_t)cb, 2, a);
    }
    if (RU(0x3e33) && RU(RT_LOAD_BUF)) {
        RU(0x3e33) = 0;
        RU(0x3e2f) = 0;
        winfree(R(void *, RT_LOAD_BUF));
    }
    SetCursor(old);
    RB(RT_LOADING) = 0;
    ResSetCallBack(RES, NULL, 0);
    return 1;
}
