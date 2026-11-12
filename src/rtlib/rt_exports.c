/* Engine core, 0x1001c176-0x1001f550: the Rt* exports the game calls. Each sets the
 * current state block first, as the original does. Most are getters and setters or
 * pass the call on; the callee's result is returned where the original leaves it in
 * EAX. Calls go to the original's addresses, which reach our code where it is ported.
 * Rewritten from the original. */
#include "rtcore.h"
#include "crt.h"
#include "rt.h"
#include "rt_exports.h"

typedef uint32_t U;
#define SET(rt) (g_rt = (uint8_t *)(rt))
#define C2(a, x, y) ORIG(U(__stdcall *)(U, U), a)((U)(x), (U)(y))
#define RB(off) ((U)R(uint8_t, off))
#define RU(off) R(U, off)
#define HSLU RU(RT_HSL)
#define HSL R(uint8_t *, RT_HSL)

#define RT_AUDIO 0x0c90
#define RT_HRL 0x0cc4
#define RT_CURSOR_SPRITE 0x0cd8
#define RT_GAME_INFO 0x3253
#define RT_SCENES 0x329b
#define RT_SCRIPT_TEXT 0x329f
#define RT_SCRIPT_OFFSETS 0x0cbc
#define RT_CALL_HOOK 0x3c3b
#define RT_CALL_HOOK_ARG 0x3c3f
#define RT_SCRIPT_NAME 0x3f43
#define RT_SCRIPT_INDEX 0x3f47
#define RT_RESULT 0x3f3b
#define RT_GLOBALS 0x3f4f
#define RT_FIRST_COPY 0x3dfb
#define RT_COMPONENT 0x95d6
#define RT_DIRECTDRAW 0x95d9
#define RT_FADING 0x95da
#define RT_NO_FADE 0x95ea
#define RT_PALETTE_REALIZED 0x95ee
#define RT_PAUSED 0x95f1


U __cdecl RtCopy(uint8_t *rt, U id) { SET(rt); return obj_copy(id); }
U __cdecl RtCursor(uint8_t *rt, uint8_t id) { SET(rt); cursor_set(id); return 0; }
U __cdecl RtDeviceBitmapToDibData(uint8_t *rt, U bm, int8_t a, uint8_t b, uint8_t c)
{
    SET(rt);
    return (U)(uintptr_t)dib_from_bitmap((HBITMAP)(uintptr_t)bm, (uint8_t)a, b, c);
}

/* deletes a resource: run-time copies are removed, the others only released */
U __cdecl RtDelete(uint8_t *rt, U id)
{
    SET(rt);
    if (id < RU(RT_FIRST_COPY))
        return (U)obj_release(id, ResGetType(R(ResFile *, RT_RES), id));
    return (U)obj_delete(id);
}

U __cdecl RtDirectDrawRect(uint8_t *rt, U a, U b, U c, U d)
{
    SET(rt);
    return R(uint8_t, RT_DIRECTDRAW) ? (U)display_set_border(HSL, (int)a, (int)b, (int)c, (int)d) : 0;
}

/* a new resource of `type` with data at its entry */
U __cdecl RtDynamicResource(uint8_t *rt, int8_t type, U data)
{
    SET(rt);
    U id = ResDynamicResource(R(ResFile *, RT_RES), (uint8_t)type, 0);
    if (!id)
        return 0;
    obj_entry(id)[0] = data;
    return id;
}

U __cdecl RtFlipPages(uint8_t *rt) { SET(rt); return R(uint8_t, RT_DIRECTDRAW) ? (U)SpriteFlipPages(HSL) : 0; }
U __cdecl RtGetAllocatedMemSize(uint8_t *rt) { SET(rt); return MemMgrSize(); }
U __cdecl RtGetBkgColor(uint8_t *rt) { SET(rt); return RU(RT_BKG_COLOR); }
U __cdecl RtGetComponent(uint8_t *rt) { SET(rt); return RB(RT_COMPONENT); }

/* the name of the script running and its text */
U __cdecl RtGetCurrentScript(uint8_t *rt, LPSTR name, char *text)
{
    U r = 0;
    SET(rt);
    if (name)
        *name = 0;
    if (text)
        *text = 0;
    if (RU(RT_SCRIPT_TEXT) && RU(RT_SCRIPT_NAME)) {
        if (name)
            r = (U)(uintptr_t)lstrcpyA(name, string_get(RU(RT_SCRIPT_NAME)));
        if (text) {
            CopyHugeStr(text, (const char *)(uintptr_t)(R(uint32_t *, RT_SCRIPT_OFFSETS)[RU(RT_SCRIPT_INDEX)] + RU(RT_SCRIPT_TEXT)));
            r = 0; /* the original returned what its copy left in EAX */
        }
    }
    return r;
}

U __cdecl RtGetDC(uint8_t *rt) { SET(rt); return RU(RT_DC); }
U __cdecl RtGetDirectDrawMode(uint8_t *rt) { SET(rt); return RB(RT_DIRECTDRAW); }
U __cdecl RtGetDirectDrawStats(uint8_t *rt, U a, U b, U c) { SET(rt); return (U)display_get_memory(HSL, (uint32_t *)(uintptr_t)a, (uint32_t *)(uintptr_t)b, (uint32_t *)(uintptr_t)c); }

U __cdecl RtGetDisplayMode(uint8_t *rt, int *w, int *h, int *bpp, U *dd)
{
    SET(rt);
    *w = GetDeviceCaps(R(HDC, RT_DC), HORZRES);
    *h = GetDeviceCaps(R(HDC, RT_DC), VERTRES);
    *bpp = GetDeviceCaps(R(HDC, RT_DC), BITSPIXEL);
    *dd = RB(RT_DIRECTDRAW);
    return *dd;
}

U __cdecl RtGetFade(uint8_t *rt) { SET(rt); return RB(RT_FADING); }
U __cdecl RtGetGameInfo(uint8_t *rt) { SET(rt); return (U)(uintptr_t)(rt + RT_GAME_INFO); }
U __cdecl RtGetHAL(uint8_t *rt) { SET(rt); return RU(RT_AUDIO); }
U __cdecl RtGetHResfile(uint8_t *rt) { SET(rt); return RU(RT_RES); }
U __cdecl RtGetHRL(uint8_t *rt) { SET(rt); return RU(RT_HRL); }
U __cdecl RtGetHSL(uint8_t *rt) { SET(rt); return HSLU; }
U __cdecl RtGetHiliteTable(uint8_t *rt) { SET(rt); return (U)(uintptr_t)ORIG_PTR(0x1007bbe8); }

/* the directory the engine DLL was loaded from */
U __cdecl RtGetModulePath(uint8_t *rt, LPSTR out, int size)
{
    char file[256], dir[256], name[256];
    SET(rt);
    if (!out)
        return 0;
    GetModuleFileNameA(G(HMODULE, 0x1007cef0), file, sizeof file);
    GetPathAndFile(file, dir, name);
    lstrcpynA(out, dir, size);
    return (U)lstrlenA(out);
}

U __cdecl RtGetNumSprites(uint8_t *rt) { SET(rt); return SpriteGetNumSprites(HSL); }
U __cdecl RtGetNoFade(uint8_t *rt) { SET(rt); return RB(RT_NO_FADE); }
U __cdecl RtGetPaletteRealized(uint8_t *rt) { SET(rt); return RB(RT_PALETTE_REALIZED); }

U __cdecl RtGetResource(uint8_t *rt, U id)
{
    SET(rt);
    if (!id)
        crt_assert_failed("D:\\DEV\\RUNTIME\\SRC\\rtlib.cpp", 0x17f, "Id > 0");
    return (U)(uintptr_t)obj_entry(id);
}

U __cdecl RtGetSceneName(uint8_t *rt) { SET(rt); return RU(RT_SCENES); }
U __cdecl RtGetShadowTable(uint8_t *rt) { SET(rt); return (U)(uintptr_t)ORIG_PTR(0x1007b7e8); }
U __cdecl RtGetString(uint8_t *rt, U id) { SET(rt); return (U)(uintptr_t)string_get(id); }
U __cdecl RtGetVar(uint8_t *rt, int n) { SET(rt); return RU(RT_GLOBALS + n * 4); }
U __cdecl RtGetVarPtr(uint8_t *rt) { SET(rt); return (U)(uintptr_t)(rt + RT_GLOBALS); }

U __cdecl RtGetVars(uint8_t *rt, int first, int n, void *out)
{
    SET(rt);
    memcpy(out, rt + RT_GLOBALS + first * 4, n << 2);
    return (U)(uintptr_t)out;
}

U __cdecl RtGetVariableString(uint8_t *rt, U id) { SET(rt); return (U)(uintptr_t)string_var(id); }
/* (the one export that doesn't set the state block) */
U __cdecl RtGetWaveDeviceFormat(uint8_t *rt, U out) { (void)rt; return C2(0x10041299, RU(RT_AUDIO), out); }
U __cdecl RtGetWindow(uint8_t *rt) { SET(rt); return RU(RT_HWND); }
U __cdecl RtGetWindowBox(uint8_t *rt) { SET(rt); return RB(RT_BKG_SCROLLS); }
U __cdecl RtIsDirectDraw(uint8_t *rt) { SET(rt); return RB(RT_DIRECTDRAW); }
U __cdecl RtIsIncrementalRepaint(uint8_t *rt) { SET(rt); return display_get_b5c(HSL); }

U __cdecl RtLevel(uint8_t *rt, U id, int level)
{
    SET(rt);
    if (!obj_loaded(id, 0x14))
        return 0;
    SpriteSetLevel(R(uint8_t *, RT_HSL), (uint8_t *)(uintptr_t)obj_entry(id)[1], level);
    return 1;
}

U __cdecl RtLoad(uint8_t *rt, U id) { SET(rt); return (U)obj_is(id, ResGetType(R(ResFile *, RT_RES), id)); }

U __cdecl RtMapPoint(uint8_t *rt, POINT *pt)
{
    SET(rt);
    mouse_to_view(pt);
    return 0;
}

U __cdecl RtOffsetAuxillaryScrollPos(uint8_t *rt, int dx, int dy)
{
    SET(rt);
    U r = 0;
    SpriteOffsetAuxillaryScrollPos(HSL, dx, dy);
    if (RU(RT_CALL_HOOK)) {
        U a[2] = {0xbe, RU(RT_CALL_HOOK_ARG)};
        r = orig_stdcall_unsafe(R(void *, RT_CALL_HOOK), 2, a);
    }
    return r;
}

U __cdecl RtPause(uint8_t *rt, uint8_t on) { SET(rt); R(uint8_t, RT_PAUSED) = on; return on; }
U __cdecl RtPauseMovies(uint8_t *rt, uint8_t on) { SET(rt); movies_pause(on); return 0; }
U __cdecl RtPauseTimers(uint8_t *rt, uint8_t on) { SET(rt); timers_pause(on); return 0; }
U __cdecl RtPost(uint8_t *rt, U handler, U id, U arg) { SET(rt); return event_send(handler, id, arg); }
U __cdecl RtPostMsg(uint8_t *rt, U msg) { SET(rt); return event_post((const uint8_t *)(uintptr_t)msg); }

U __cdecl RtRegisterCursorSprite(uint8_t *rt, U s)
{
    SET(rt);
    RU(RT_CURSOR_SPRITE) = s;
    set_b60(HSL, s);
    return 0;
}

U __cdecl RtResLoaded(uint8_t *rt, U id) { SET(rt); return obj_loaded(id, ResGetType(R(ResFile *, RT_RES), id)); }

/* runs a script handler with two arguments */
U __cdecl RtRun(uint8_t *rt, U handler, U a, U b)
{
    uint8_t rec[0x22];
    SET(rt);
    memset(rec, 0, sizeof rec);
    P(U, rec, 2) = handler;
    P(U, rec, 10) = a;
    P(U, rec, 14) = b;
    rec[9] = 2;
    rec[8] = 0x20;
    return handler_record_run(rec);
}

/* runs a prepared handler record; returns the handler's result (1 if it sets none) */
U __cdecl RtRunMsg(uint8_t *rt, uint8_t *rec)
{
    SET(rt);
    RU(RT_RESULT) = 1;
    rec[8] = 0x20;
    handler_record_run(rec);
    return RU(RT_RESULT);
}

U __cdecl RtSetBkgColor(uint8_t *rt, U color) { SET(rt); RU(RT_BKG_COLOR) = color; return color; }
U __cdecl RtSetBlitLookupTable(uint8_t *rt, int which, U table) { SET(rt); set_fx_table(HSL, which, table); return 0; }
U __cdecl RtSetBriefingScreenStatus(uint8_t *rt, int on) { SET(rt); sprite_set_interface(HSL, on); return 0; }
U __cdecl RtSetComponent(uint8_t *rt, uint8_t v) { SET(rt); R(uint8_t, RT_COMPONENT) = v; return v; }

U __cdecl RtSetComposeCallback(uint8_t *rt, U a, U b, U c, int d, int e, int f)
{
    SET(rt);
    set_screen_mode(HSL, a, b, c, d, e, f);
    return 0;
}

/* ---- second batch: RtSetDisplayMode to RtGetPalette ---- */

#define RT_FULLSCREEN_OK 0x95cf
#define RT_SMALL_SCREEN 0x95cc
#define RT_95DE 0x95de
#define RT_95EC 0x95ec
#define RT_SWITCHING 0x95f4
#define RT_SAVED_W 0x3e13
#define RT_SAVED_H 0x3e17
#define RT_8D7B 0x8d7b
#define RT_8D7F 0x8d7f
#define RT_8DA7 0x8da7
#define RT_VIEW_ORG 0x36c7
#define RT_MEM_FREE 0x0b70
#define RT_PALETTE_SIZE 0x3e3b

/* Switches the display: w x h at depth bits. full: DirectDraw full screen; windowed:
 * keep the window's mode (0: change the desktop, w = h = 0 back to the saved size).
 * Phase 1a's "palette" patch is built in: the display is always treated as a 256-color
 * palette device (the original asks GetDeviceCaps(RASTERCAPS) & RC_PALETTE, which is
 * false on today's desktops, and its 8-bit screen then stays black). */
U __cdecl RtSetDisplayMode(uint8_t *rt, U w, U h, int depth, int full, int windowed, int keep_dd, U flags)
{
    SET(rt);
    U was_dd = RB(RT_DIRECTDRAW);
    int dd = R(uint8_t, RT_FULLSCREEN_OK) && ((!keep_dd && full) || (keep_dd && was_dd));
    int change = windowed == 0;
    if (!dd)
        flags = 0;
    if (change) {
        if (!w || !h) {
            w = RU(RT_SAVED_W);
            h = RU(RT_SAVED_H);
        } else {
            RU(RT_SAVED_W) = RU(RT_SCREEN_W);
            RU(RT_SAVED_H) = RU(RT_SCREEN_H);
        }
        R(uint8_t, RT_SWITCHING) = 1;
    }
    if (was_dd && !dd)
        R(uint8_t, RT_DIRECTDRAW) = 0;
    if (!SpriteSetDisplayMode(R(uint8_t *, RT_HSL), w, h, depth, was_dd, dd, change, flags))
        return 0;
    if (change) {
        R(uint8_t, RT_SWITCHING) = 0;
        RU(RT_SCREEN_W) = GetDeviceCaps(R(HDC, RT_DC), HORZRES);
        RU(RT_8D7B) = RU(RT_SCREEN_W);
        RU(RT_SCREEN_H) = GetDeviceCaps(R(HDC, RT_DC), VERTRES);
        RU(RT_8D7F) = RU(RT_SCREEN_H);
        R(uint8_t, RT_PALETTE_DEVICE) = 1; /* original: (RASTERCAPS & RC_PALETTE) != 0 */
        screen_size_from_desktop(R(uint8_t *, RT_HSL));
        R(uint8_t, RT_DIRECTDRAW) = (uint8_t)dd;
        palette_get_b80_2(HSLU, 0); /* (it ignores its arguments) */
        scene_palette_load();
        GetViewportOrgEx(R(HDC, RT_DC), (POINT *)(rt + RT_VIEW_ORG));
        R(uint8_t, RT_SMALL_SCREEN) = GetSystemMetrics(SM_CXSCREEN) < 0x281;
        R(uint8_t, RT_95DE) = R(uint8_t, RT_PALETTE_DEVICE) == 0;
        RU(RT_8DA7) = RB(RT_95DE);
    }
    R(uint8_t, RT_DIRECTDRAW) = (uint8_t)dd;
    R(uint8_t, RT_95EC) = (uint8_t)flags;
    if (!R(uint8_t, RT_DIRECTDRAW))
        ShowWindow(R(HWND, RT_HWND), SW_MAXIMIZE);
    RedrawWindow(R(HWND, RT_HWND), NULL, NULL, RDW_NOERASE);
    return 1;
}

U __cdecl RtSetGameInfo(uint8_t *rt, const U *info)
{
    SET(rt);
    RU(RT_GAME_INFO) = info[0];
    RU(RT_GAME_INFO + 4) = info[1];
    return info[1];
}

U __cdecl RtSetLoadProc(uint8_t *rt, U proc) { SET(rt); return RU(0x3c2f) = proc; }
U __cdecl RtSetUnloadProc(uint8_t *rt, U proc) { SET(rt); return RU(0x3c33) = proc; }
U __cdecl RtSetNoFade(uint8_t *rt, uint8_t on) { SET(rt); return R(uint8_t, RT_NO_FADE) = on; }
U __cdecl RtSetPaletteRealized(uint8_t *rt, uint8_t on) { SET(rt); return R(uint8_t, RT_PALETTE_REALIZED) = on; }

/* moves a sprite object (through the 0xb8 command) */
U __cdecl RtSetPos(uint8_t *rt, U id, int x, int y)
{
    uint8_t c[0x12];
    SET(rt);
    P(uint16_t, c, 0) = 0xb8;
    P(U, c, 2) = id;
    P(int32_t, c, 6) = x;
    P(int32_t, c, 10) = y;
    P(U, c, 14) = 0;
    return cmd_set_position(c);
}

U __cdecl RtSetRenderCopyProc(uint8_t *rt, U proc) { SET(rt); return RU(0x36d3) = proc; }
U __cdecl RtSetRenderFreeProc(uint8_t *rt, U proc) { SET(rt); return RU(0x36d7) = proc; }
U __cdecl RtSetRenderLoadProc(uint8_t *rt, U proc) { SET(rt); return RU(0x36db) = proc; }

U __cdecl RtSetSpriteClipBox(uint8_t *rt, U id, int l, int t, int r, int b)
{
    SET(rt);
    int32_t box[4] = {l, r, t, b}; /* (the original stores them in this order) */
    SpriteSetSpriteClipBox((uint8_t *)(uintptr_t)obj_entry(id)[1], box);
    return 0;
}

U __cdecl RtSetSpriteFlip(uint8_t *rt, U s, U fx, U fy)
{
    SET(rt);
    SpriteSetFlip(R(uint8_t *, RT_HSL), (uint8_t *)(uintptr_t)s, fx, fy);
    return 0;
}

U __cdecl RtSetSpriteRedraw(uint8_t *rt, U s, U on, int update)
{
    SET(rt);
    SpriteSetRedraw(R(uint8_t *, RT_HSL), (uint8_t *)(uintptr_t)s, on, update);
    return 0;
}

/* sets string n (from 1) of the current scene */
U __cdecl RtSetString(uint8_t *rt, U n, LPCSTR s)
{
    SET(rt);
    uint8_t *scene = R(uint8_t *, RT_SCENES) + RU(0x3dcf) * 0x10c;
    if (!n || n > P(U, scene, 0x104))
        return 0;
    return (U)(uintptr_t)lstrcpynA(P(char *, scene, 0x108) + (n - 1) * 0x201, s, 0x200);
}

/* sets the text of text object id (type 0x15); s is used as a format */
U __cdecl RtSetText(uint8_t *rt, U id, LPCSTR s)
{
    (void)rt;
    if (!obj_is(id, 0x15))
        return 0;
    uint8_t *t = (uint8_t *)(uintptr_t)obj_entry(id)[2];
    if (!s)
        return 0;
    wsprintfA((LPSTR)(t + 0x52), s);
    return text_render(t) & 0xff;
}

U __cdecl RtSetVar(uint8_t *rt, int n, U v) { SET(rt); return RU(RT_GLOBALS + n * 4) = v; }

U __cdecl RtSetVars(uint8_t *rt, int first, int n, const void *in)
{
    SET(rt);
    memcpy(rt + RT_GLOBALS + first * 4, in, n << 2);
    return (U)(uintptr_t)(rt + RT_GLOBALS + first * 4);
}

U __cdecl RtSetViewPortOrg(uint8_t *rt, const int32_t *pt)
{
    SET(rt);
    R(int32_t, RT_VIEW_ORG) = pt[0];
    R(int32_t, RT_VIEW_ORG + 4) = pt[1];
    return (U)pt[1];
}

U __cdecl RtSetQueue(uint8_t *rt, U id, U queue, uint8_t hide) { SET(rt); return cmd_set_queue(id, queue, hide); }
U __cdecl RtShowGDISurface(uint8_t *rt) { SET(rt); display_lost_focus(R(uint8_t *, RT_HSL)); return 0; }
U __cdecl RtRestoreFromGDISurface(uint8_t *rt) { SET(rt); display_got_focus(R(uint8_t *, RT_HSL)); return 0; }
U __cdecl RtSpriteTransfer(uint8_t *rt, U a, U b, U c, uint8_t d) { SET(rt); return sprite_transfer(a, b, c, d); }

U __cdecl RtUnload(uint8_t *rt, U id)
{
    SET(rt);
    if (id < RU(RT_FIRST_COPY))
        return (U)obj_release(id, ResGetType(R(ResFile *, RT_RES), id));
    return (U)obj_delete(id);
}

U __cdecl RtUpdateBrowse(uint8_t *rt) { SET(rt); hover_update(); return 0; }
U __cdecl RtFadeIn(uint8_t *rt, U arg) { SET(rt); scene_show_fn(arg); return 0; }
U __cdecl RtFadeOut(uint8_t *rt, U arg) { SET(rt); scene_hide_fn(arg); return 0; }

U __cdecl RtGetPalette(uint8_t *rt, U *size)
{
    SET(rt);
    *size = RU(RT_PALETTE_SIZE);
    return (U)(uintptr_t)(rt + RT_PALETTE);
}

/* 0x1001d7d0: the free physical memory, in 64 KB steps */
U __cdecl rt_measure_memory(void)
{
    MEMORYSTATUS ms;
    ms.dwLength = 0x20;
    GlobalMemoryStatus(&ms);
    RU(RT_MEM_FREE) = ((ms.dwTotalPhys >> 16) * (100 - ms.dwMemoryLoad)) / 100 << 16;
    return 1;
}

/* RtSetScreenRes (0x1001d3b3) and RtEnableDirectDraw (0x1001c2f2): empty in the
 * original (they return 0), too short to hook; ours for our own callers, and the
 * exports once we stop forwarding */
U __cdecl RtSetScreenRes(uint8_t *rt, U w, U h, U depth)
{
    (void)rt, (void)w, (void)h, (void)depth;
    return 0;
}
U __cdecl rt_enable_directdraw(void) { return 0; }
