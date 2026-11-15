/* The parts of the original engine runtime (Rtlib32.dll) that our modules call.
 * Signatures come from how the original modules call them; field names are ours. */
#ifndef DOMINION_RTLIB32_H
#define DOMINION_RTLIB32_H

#include <stdint.h>
#include <windows.h>

/* 8-bit bitmap ("DIB") as the engine stores it. Only the fields we use are named.
 * Rows are stored bottom-up, like a Windows DIB. */
typedef struct RtDib {
    uint32_t unknown_00;
    int32_t width;       /* visible width; for font glyphs, the advance */
    uint32_t pitch;      /* bytes per row; text code also uses it as the usable width */
    uint32_t height;
    uint8_t unknown_10[0x10];
    uint8_t *bits;
} RtDib;

/* Resources are returned as handles: a pointer to the object pointer. */
void **__cdecl RtGetResource(void *app, unsigned id);
const char *__cdecl RtGetString(void *app, unsigned id);
void *__cdecl RtGetHSL(void *app); /* the app's sprite/screen object */
void __cdecl DebugStr(const char *fmt, ...);

void __stdcall SpriteDibMerge(RtDib *dst, RtDib *src, int w, int h, int x, int y, int src_x,
                              int src_y);
void __stdcall SpriteDibColorMapMerge(RtDib *dst, RtDib *src, int w, int h, int x, int y,
                                      int src_x, int src_y, int unknown, int color_map);
void __stdcall SpriteDibSet(RtDib *dst, int x, int y, int w, int h, int color);
void __stdcall SpritePaintRect(void *hsl, RECT *rects, int count);

/* Application, window and display */
HWND __cdecl RtGetWindow(void *app);
HDC __cdecl RtGetDC(void *app);
void __cdecl RtGetDisplayMode(void *app, uint32_t *w, uint32_t *h, uint32_t *bpp, uint32_t *refresh);
int __cdecl RtSetDisplayMode(void *app, uint32_t w, uint32_t h, uint32_t bpp, uint32_t refresh,
                             int unknown1, int unknown2, int unknown3);
void __cdecl RtPostMsg(void *app, const void *msg); /* msg: 0x22-byte record, see RtMsg */

/* "Game info": the engine calls info->hook(info->ctx) once per main-loop pass. */
typedef struct RtGameInfo {
    void *ctx;
    void(__stdcall *hook)(void *ctx);
} RtGameInfo;
RtGameInfo *__cdecl RtGetGameInfo(void *app);
void __cdecl RtSetGameInfo(void *app, const RtGameInfo *info);

/* The engine's DirectDraw objects (IDirectDraw2 *) */
void *__stdcall SpriteGetDDraw(void *hsl);
void __stdcall SpriteCreateDirectDrawSurfaces(void *hsl);
void __stdcall SpriteReleaseDirectDrawSurfaces(void *hsl);
void __stdcall SpriteCreateDirectDrawPalette(void *hsl);
void __stdcall SpriteReleaseDirectDrawPalette(void *hsl);
int __stdcall SpriteRealizePalette2(void *hsl, int force);

/* Objects (resources loaded or copied at run time) */
unsigned __cdecl RtCopy(void *app, unsigned id);
unsigned __cdecl RtDelete(void *app, unsigned id);
unsigned __cdecl RtLoad(void *app, unsigned id);
int __cdecl RtRunMsg(void *app, void *rec);

/* Sprites: msg is a list of count 32-bit values */
void __stdcall SpriteEmptyQueue(void *hsl, void *sprite);
void __stdcall SpriteHide(void *hsl, void *sprite, int erase);
void __stdcall SpritePostMessage(void *hsl, void *sprite, uint32_t count, const uint32_t *msg);
void __stdcall SpriteSendMessage(void *hsl, void *sprite, uint32_t count, const uint32_t *msg);
void __stdcall SpriteSetLevel(void *hsl, void *sprite, int32_t level);
void __cdecl SpriteSetProcessMsgs(void *hsl, void *sprite, int on);
void __stdcall SpriteShow(void *hsl, void *sprite, uint32_t cel, int redraw);
void __stdcall SpriteScroll(void *hsl, int dx, int dy, int blit);
void __stdcall SpriteGetScrollPos(void *hsl, int32_t *pos);
unsigned __cdecl RtRegisterCursorSprite(void *app, void *sprite);
void __cdecl RtSetScrollBox(void *app, const int32_t *box);
void __cdecl RtCaptureFrame(void *app, char *prefix, uint32_t n);
unsigned __cdecl RtFadeIn(void *app, unsigned arg);
unsigned __cdecl RtGetVar(void *app, int n);
unsigned __cdecl RtSetPos(void *app, unsigned id, int x, int y);
void __stdcall SpriteSetBlitFX(void *sprite, uint32_t fx, uint32_t arg);
void __stdcall SpriteSetSync(void *hsl, uint32_t on);
unsigned __cdecl RtUnload(void *app, unsigned id);
int __stdcall SpriteGetBounds(void *hsl, void *sprite, RECT *r);
void __stdcall SpriteRefreshInterfaceArea(void *hsl, void *sprite);
void __stdcall SpriteRefreshRect(void *hsl, const RECT *r);
void __stdcall SpriteSetScrollPos(void *hsl, int x, int y);
unsigned __cdecl RtGetResType(void *app, unsigned id);
int __stdcall SpriteReplaceCellRange(void *hsl, void *src, void *dst, uint32_t from, uint32_t to, uint32_t n);
void __stdcall SpriteSetExtOffsets(void *hsl, void *sprite, void *offsets);
void __stdcall SpriteSetPosition(void *hsl, void *sprite, int x, int y, int redraw);
void *__stdcall winmalloc(SIZE_T size, int zero);
void __cdecl SpriteSetFlip(void *hsl, void *sprite, uint32_t fx, uint32_t fy);
void __cdecl SpriteSetRedraw(void *hsl, void *sprite, uint32_t on, int update);

#endif
