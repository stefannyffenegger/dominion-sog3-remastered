/* Engine core: command line, service calls, small exports, screensaver (rt_service.c) */
#ifndef DOMINION_RTLIB_RT_SERVICE_H
#define DOMINION_RTLIB_RT_SERVICE_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
char *__stdcall cmdline_title(char *p);
char *__stdcall cmdline_switch(char *p);
void __stdcall cmdline_parse(LPCSTR cmdline, char *path);
void __cdecl RtService(uint8_t *rt);
void __cdecl RtDominionService(uint8_t *rt);
U __cdecl RtGetBrowseEvents(uint8_t *rt, U *out);
U __cdecl RtGetClickRects(uint8_t *rt, U *count);
U __cdecl RtGetCopyId(uint8_t *rt, U id);
U __cdecl RtGetErrFile(uint8_t *rt, U *line);
U __cdecl RtGetNumResources(uint8_t *rt);
U __cdecl RtGetResName(uint8_t *rt, U id);
U __cdecl RtGetResNames(uint8_t *rt);
U __cdecl RtGetResOffsets(uint8_t *rt);
U __cdecl RtGetResType(uint8_t *rt, U id);
U __cdecl RtGetSignonPath(uint8_t *rt);
U __cdecl RtGetTitleString(uint8_t *rt);
U __cdecl RtGetTrackInfo(uint8_t *rt, U *out);
U __cdecl RtGetViewportOrg(uint8_t *rt);
U __cdecl RtIsPaused(uint8_t *rt);
U __cdecl RtIsRunning(uint8_t *rt);
U __cdecl RtIsTrackingRect(uint8_t *rt);
U __cdecl RtMessage(uint8_t *rt, U hwnd, U id, U arg);
U __cdecl RtProcessRunMsgs(uint8_t *rt);
U __cdecl RtSceneIn(uint8_t *rt, U scene, U arg);
U __cdecl RtSceneInCallback(uint8_t *rt, U scene, U arg, U fn, U data);
U __cdecl RtSceneOut(uint8_t *rt);
U __cdecl RtSceneRestart(uint8_t *rt);
U __cdecl RtSetBkgIndex(uint8_t *rt, U v);
U __cdecl RtSetCallProc(uint8_t *rt, U proc, U arg);
U __cdecl RtSetCursorId(uint8_t *rt, U v);
U __cdecl RtSetDebug(uint8_t *rt, uint8_t v);
U __cdecl RtSetInteractive(uint8_t *rt, uint8_t v);
U __cdecl RtSetPlayer(uint8_t *rt, uint8_t v);
U __cdecl RtSetScriptProc(uint8_t *rt, U proc, U arg);
U __cdecl RtSetWindowBox(uint8_t *rt, uint8_t v);
U __cdecl saver_may_close(void);
void __cdecl saver_quit(void);
U __stdcall saver_mouse_move(U pos);
LRESULT __stdcall saver_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

#endif
