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

#endif
