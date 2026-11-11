/* Engine core, 0x1001c176-0x1001f550: the Rt* exports the game calls. Each sets the
 * current state block first, as the original does. Most are getters and setters or
 * pass the call on; the callee's result is returned where the original leaves it in
 * EAX. Calls go to the original's addresses, which reach our code where it is ported.
 * Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_exports.h"

typedef uint32_t U;
#define SET(rt) (g_rt = (uint8_t *)(rt))
#define C1(a, x) ORIG(U(__stdcall *)(U), a)((U)(x))
#define C2(a, x, y) ORIG(U(__stdcall *)(U, U), a)((U)(x), (U)(y))
#define C3(a, x, y, z) ORIG(U(__stdcall *)(U, U, U), a)((U)(x), (U)(y), (U)(z))
#define C4(a, x, y, z, w) ORIG(U(__stdcall *)(U, U, U, U), a)((U)(x), (U)(y), (U)(z), (U)(w))
#define C5(a, x, y, z, w, v) ORIG(U(__stdcall *)(U, U, U, U, U), a)((U)(x), (U)(y), (U)(z), (U)(w), (U)(v))
#define RB(off) ((U)R(uint8_t, off))
#define RU(off) R(U, off)
#define HSLU RU(RT_HSL)

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

#define handler_record_run(p) C1(0x100220dd, p)

U __cdecl RtCopy(uint8_t *rt, U id) { SET(rt); return obj_copy(id); }
U __cdecl RtCursor(uint8_t *rt, uint8_t id) { SET(rt); return C1(0x10025880, id); }
U __cdecl RtDeviceBitmapToDibData(uint8_t *rt, U bm, int8_t a, uint8_t b, uint8_t c)
{
    SET(rt);
    return C4(0x10017f60, bm, a, b, c);
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
    return R(uint8_t, RT_DIRECTDRAW) ? C5(0x1004ac90, HSLU, a, b, c, d) : 0;
}

/* a new resource of `type` with data at its entry */
U __cdecl RtDynamicResource(uint8_t *rt, int8_t type, U data)
{
    SET(rt);
    U id = C3(0x1006709c, RU(RT_RES), type, 0);
    if (!id)
        return 0;
    obj_entry(id)[0] = data;
    return id;
}

U __cdecl RtEnableDirectDraw(void) { return 0; }
U __cdecl RtFlipPages(uint8_t *rt) { SET(rt); return R(uint8_t, RT_DIRECTDRAW) ? C1(0x1004af58, HSLU) : 0; }
U __cdecl RtGetAllocatedMemSize(uint8_t *rt) { SET(rt); return ORIG(U(__cdecl *)(void), 0x1003e284)(); }
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
            r = (U)(uintptr_t)lstrcpyA(name, (LPCSTR)(uintptr_t)C1(0x100259a6, RU(RT_SCRIPT_NAME)));
        if (text)
            r = C2(0x1003d82c, text, R(uint32_t *, RT_SCRIPT_OFFSETS)[RU(RT_SCRIPT_INDEX)] + RU(RT_SCRIPT_TEXT));
    }
    return r;
}

U __cdecl RtGetDC(uint8_t *rt) { SET(rt); return RU(RT_DC); }
U __cdecl RtGetDirectDrawMode(uint8_t *rt) { SET(rt); return RB(RT_DIRECTDRAW); }
U __cdecl RtGetDirectDrawStats(uint8_t *rt, U a, U b, U c) { SET(rt); return C4(0x1004cc3b, HSLU, a, b, c); }

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

U __cdecl RtGetNumSprites(uint8_t *rt) { SET(rt); return C1(0x1004f0bb, HSLU); }
U __cdecl RtGetNoFade(uint8_t *rt) { SET(rt); return RB(RT_NO_FADE); }
U __cdecl RtGetPaletteRealized(uint8_t *rt) { SET(rt); return RB(RT_PALETTE_REALIZED); }

U __cdecl RtGetResource(uint8_t *rt, U id)
{
    SET(rt);
    if (!id && ORIG(int(__cdecl *)(int, const char *, int, const char *, const char *), 0x10028a40)(
                   2, "D:\\DEV\\RUNTIME\\SRC\\rtlib.cpp", 0x17f, NULL, "Id > 0") == 1)
        DebugBreak();
    return (U)(uintptr_t)obj_entry(id);
}

U __cdecl RtGetSceneName(uint8_t *rt) { SET(rt); return RU(RT_SCENES); }
U __cdecl RtGetShadowTable(uint8_t *rt) { SET(rt); return (U)(uintptr_t)ORIG_PTR(0x1007b7e8); }
U __cdecl RtGetString(uint8_t *rt, U id) { SET(rt); return C1(0x100259a6, id); }
U __cdecl RtGetVar(uint8_t *rt, int n) { SET(rt); return RU(RT_GLOBALS + n * 4); }
U __cdecl RtGetVarPtr(uint8_t *rt) { SET(rt); return (U)(uintptr_t)(rt + RT_GLOBALS); }

U __cdecl RtGetVars(uint8_t *rt, int first, int n, void *out)
{
    SET(rt);
    memcpy(out, rt + RT_GLOBALS + first * 4, n << 2);
    return (U)(uintptr_t)out;
}

U __cdecl RtGetVariableString(uint8_t *rt, U id) { SET(rt); return C1(0x10025a6a, id); }
/* (the one export that doesn't set the state block) */
U __cdecl RtGetWaveDeviceFormat(uint8_t *rt, U out) { (void)rt; return C2(0x10041299, RU(RT_AUDIO), out); }
U __cdecl RtGetWindow(uint8_t *rt) { SET(rt); return RU(RT_HWND); }
U __cdecl RtGetWindowBox(uint8_t *rt) { SET(rt); return RB(RT_BKG_SCROLLS); }
U __cdecl RtIsDirectDraw(uint8_t *rt) { SET(rt); return RB(RT_DIRECTDRAW); }
U __cdecl RtIsIncrementalRepaint(uint8_t *rt) { SET(rt); return C1(0x1004ad61, HSLU); }

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
    U r = C3(0x10050dab, HSLU, dx, dy);
    if (RU(RT_CALL_HOOK)) {
        U a[2] = {0xbe, RU(RT_CALL_HOOK_ARG)};
        r = orig_stdcall_unsafe(R(void *, RT_CALL_HOOK), 2, a);
    }
    return r;
}

U __cdecl RtPause(uint8_t *rt, uint8_t on) { SET(rt); R(uint8_t, RT_PAUSED) = on; return on; }
U __cdecl RtPauseMovies(uint8_t *rt, uint8_t on) { SET(rt); return C1(0x100166eb, on); }
U __cdecl RtPauseTimers(uint8_t *rt, uint8_t on) { SET(rt); return C1(0x10025037, on); }
U __cdecl RtPost(uint8_t *rt, U handler, U id, U arg) { SET(rt); return C3(0x10025e97, handler, id, arg); }
U __cdecl RtPostMsg(uint8_t *rt, U msg) { SET(rt); return C1(0x10025f5d, msg); }

U __cdecl RtRegisterCursorSprite(uint8_t *rt, U s)
{
    SET(rt);
    RU(RT_CURSOR_SPRITE) = s;
    return C2(0x10057eed, HSLU, s);
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
U __cdecl RtSetBlitLookupTable(uint8_t *rt, int which, U table) { SET(rt); return C3(0x1005bbf7, HSLU, which, table); }
U __cdecl RtSetBriefingScreenStatus(uint8_t *rt, int on) { SET(rt); return C2(0x1004fac5, HSLU, on); }
U __cdecl RtSetComponent(uint8_t *rt, uint8_t v) { SET(rt); R(uint8_t, RT_COMPONENT) = v; return v; }

U __cdecl RtSetComposeCallback(uint8_t *rt, U a, U b, U c, int d, int e, int f)
{
    SET(rt);
    U args[7] = {HSLU, a, b, c, (U)d, (U)e, (U)f};
    return orig_stdcall_unsafe(ORIG_PTR(0x10055c7e), 7, args);
}
