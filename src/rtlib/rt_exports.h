/* Engine core: the Rt* exports (rt_exports.c) */
#ifndef DOMINION_RTLIB_RT_EXPORTS_H
#define DOMINION_RTLIB_RT_EXPORTS_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
U __cdecl RtCopy(uint8_t *rt, U id);
U __cdecl RtCursor(uint8_t *rt, uint8_t id);
U __cdecl RtDeviceBitmapToDibData(uint8_t *rt, U bm, int8_t a, uint8_t b, uint8_t c);
U __cdecl RtDelete(uint8_t *rt, U id);
U __cdecl RtDirectDrawRect(uint8_t *rt, U a, U b, U c, U d);
U __cdecl RtDynamicResource(uint8_t *rt, int8_t type, U data);
U __cdecl RtEnableDirectDraw(void);
U __cdecl RtFlipPages(uint8_t *rt);
U __cdecl RtGetAllocatedMemSize(uint8_t *rt);
U __cdecl RtGetBkgColor(uint8_t *rt);
U __cdecl RtGetComponent(uint8_t *rt);
U __cdecl RtGetCurrentScript(uint8_t *rt, LPSTR name, char *text);
U __cdecl RtGetDC(uint8_t *rt);
U __cdecl RtGetDirectDrawMode(uint8_t *rt);
U __cdecl RtGetDirectDrawStats(uint8_t *rt, U a, U b, U c);
U __cdecl RtGetDisplayMode(uint8_t *rt, int *w, int *h, int *bpp, U *dd);
U __cdecl RtGetFade(uint8_t *rt);
U __cdecl RtGetGameInfo(uint8_t *rt);
U __cdecl RtGetHAL(uint8_t *rt);
U __cdecl RtGetHResfile(uint8_t *rt);
U __cdecl RtGetHRL(uint8_t *rt);
U __cdecl RtGetHSL(uint8_t *rt);
U __cdecl RtGetHiliteTable(uint8_t *rt);
U __cdecl RtGetModulePath(uint8_t *rt, LPSTR out, int size);
U __cdecl RtGetNumSprites(uint8_t *rt);
U __cdecl RtGetNoFade(uint8_t *rt);
U __cdecl RtGetPaletteRealized(uint8_t *rt);
U __cdecl RtGetResource(uint8_t *rt, U id);
U __cdecl RtGetSceneName(uint8_t *rt);
U __cdecl RtGetShadowTable(uint8_t *rt);
U __cdecl RtGetString(uint8_t *rt, U id);
U __cdecl RtGetVar(uint8_t *rt, int n);
U __cdecl RtGetVarPtr(uint8_t *rt);
U __cdecl RtGetVars(uint8_t *rt, int first, int n, void *out);
U __cdecl RtGetVariableString(uint8_t *rt, U id);
U __cdecl RtGetWaveDeviceFormat(uint8_t *rt, U out);
U __cdecl RtGetWindow(uint8_t *rt);
U __cdecl RtGetWindowBox(uint8_t *rt);
U __cdecl RtIsDirectDraw(uint8_t *rt);
U __cdecl RtIsIncrementalRepaint(uint8_t *rt);
U __cdecl RtLevel(uint8_t *rt, U id, int level);
U __cdecl RtLoad(uint8_t *rt, U id);
U __cdecl RtMapPoint(uint8_t *rt, POINT *pt);
U __cdecl RtOffsetAuxillaryScrollPos(uint8_t *rt, int dx, int dy);
U __cdecl RtPause(uint8_t *rt, uint8_t on);
U __cdecl RtPauseMovies(uint8_t *rt, uint8_t on);
U __cdecl RtPauseTimers(uint8_t *rt, uint8_t on);
U __cdecl RtPost(uint8_t *rt, U handler, U id, U arg);
U __cdecl RtPostMsg(uint8_t *rt, U msg);
U __cdecl RtRegisterCursorSprite(uint8_t *rt, U s);
U __cdecl RtResLoaded(uint8_t *rt, U id);
U __cdecl RtRun(uint8_t *rt, U handler, U a, U b);
U __cdecl RtRunMsg(uint8_t *rt, uint8_t *rec);
U __cdecl RtSetBkgColor(uint8_t *rt, U color);
U __cdecl RtSetBlitLookupTable(uint8_t *rt, int which, U table);
U __cdecl RtSetBriefingScreenStatus(uint8_t *rt, int on);
U __cdecl RtSetComponent(uint8_t *rt, uint8_t v);
U __cdecl RtSetComposeCallback(uint8_t *rt, U a, U b, U c, int d, int e, int f);

#endif
