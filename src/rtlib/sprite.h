/* The sprite engine's sprite objects (sprite.c) */
#ifndef DOMINION_RTLIB_SPRITE_H
#define DOMINION_RTLIB_SPRITE_H

#include <stdint.h>
#include <windows.h>

typedef void(__stdcall *SpriteServiceFn)(uint32_t);

int __stdcall sprite_check(uint8_t *s);
uint8_t *__stdcall sprite_find_id(uint32_t id);
uint8_t *__stdcall sprite_anim_prev(uint8_t *s);
uint8_t *__cdecl sprite_alloc(void);
uint8_t *__stdcall sprite_new(int32_t level);
void __stdcall sprite_free_cels(uint8_t *s);
void __stdcall sprite_free(uint8_t *s);
HLOCAL __stdcall local_copy(HLOCAL h);
int __stdcall sprite_bounds_from_first_cel(uint8_t *s);
uint8_t *__stdcall SpriteCreate(uint8_t *hsl, int32_t level, uint32_t bitmap);
int __stdcall SpriteDestroy(uint8_t *hsl, uint8_t *s, int hide, int free_bitmaps);
void __stdcall SpriteDestroyAll(uint8_t *hsl, int hide);
uint8_t *__stdcall SpriteCopy(uint8_t *hsl, uint8_t *src);
int __stdcall SpriteAppendBitmap(uint8_t *hsl, uint8_t *s, uint32_t bitmap);
int __stdcall SpriteAppendBitmaps(uint8_t *hsl, uint8_t *s, const uint32_t *bitmaps, uint32_t n);
int __stdcall sprite_set_cels(uint8_t *s, uint32_t cel, uint32_t first, uint32_t last);
void __stdcall sprite_update_bounds(uint8_t *s);
void __stdcall sprite_set_positioned(uint8_t *s, uint32_t on, int update);
void __stdcall sprite_next_cel(uint8_t *s);
int __cdecl sprite_delay(int value, char indexed);
int __stdcall sprite_wait_done(uint8_t *s);
void __cdecl sprite_service(void);
uint32_t __stdcall sprite_done(uint8_t *s);
void __stdcall SpriteAnimate(uint8_t *hsl);
void __stdcall sprite_set_paused(uint8_t *s, uint8_t pause);
void __stdcall SpritePause(uint8_t *hsl, uint8_t *s, int pause);
int __stdcall SpriteGetBounds(uint8_t *hsl, uint8_t *s, RECT *r);
int __stdcall SpriteGetPosition(uint8_t *s, int32_t *pos);
void __stdcall SpriteOffset(uint8_t *hsl, uint8_t *s, int dx, int dy);
void __stdcall SpriteSetPosition(uint8_t *hsl, uint8_t *s, int x, int y, int redraw);
void __stdcall SpriteSetMotion(uint8_t *s, int32_t ms);
void __stdcall SpriteSetSpeed(uint8_t *s, int32_t ms);
void __stdcall SpriteSetServiceProc(uint8_t *hsl, SpriteServiceFn proc, uint32_t arg);
void __stdcall SpriteBreakLoops(uint8_t *s, uint32_t v);
void __stdcall SpriteCallBack(uint8_t *s, int on);
void __stdcall SpriteClearUpdate(uint8_t *s);
void __stdcall SpriteEnable(uint8_t *s, uint32_t on);
uint32_t __stdcall SpriteGetExtraId(uint8_t *s);
uint32_t __stdcall SpriteGetId(uint8_t *s);
int32_t __stdcall SpriteGetLevel(uint8_t *s);
uint32_t __stdcall SpriteGetNumCels(uint8_t *s);
uint32_t __stdcall SpriteGetUserData(uint8_t *s);
uint32_t __stdcall SpriteIsScrollable(uint8_t *s);
uint32_t __stdcall SpriteIsVisible(uint8_t *s);
int __stdcall SpriteIsValid(uint8_t *s);
HDC __stdcall SpriteGetDC(uint8_t *hsl);
uint32_t __stdcall SpriteGetNumSprites(uint8_t *hsl);
uint32_t __stdcall SpriteGetNumMsgSprites(uint8_t *hsl);
uint32_t __stdcall SpriteGetSprite(uint8_t *hsl, uint32_t unused);
int __stdcall SpriteGetCell(uint8_t *s, uint32_t i, uint32_t *out);
void __stdcall SpriteGetInfo(uint8_t *s, uint32_t *info);
void __stdcall SpriteCopyRect(uint8_t *dst, uint8_t *src, int32_t *r);
uint32_t __stdcall SpriteCurvePoint(uint32_t a, uint32_t n, uint32_t c, uint32_t d, uint32_t e,
                                    uint32_t f, uint32_t g, uint32_t h, uint32_t i);
int __stdcall SpriteLinePoint(uint32_t a, uint32_t n, uint32_t c, uint32_t d, uint32_t e,
                              uint32_t f, uint32_t g);

int __stdcall SpriteIsPicked(uint8_t *hsl, uint8_t *s, int x, int y);
uint8_t *__stdcall SpritePicked(uint8_t *hsl, int x, int y);
uint32_t __stdcall SpriteCelPicked(uint8_t *hsl, uint8_t *s, int x, int y);
int __stdcall SpriteReplaceCell(uint8_t *hsl, uint8_t *s, int x, int y, int redraw, uint32_t bitmap);
int __stdcall sprite_copy_cel(uint8_t *src, uint8_t *dst, uint32_t from, uint32_t to);
int __stdcall SpriteReplaceCellRange(uint8_t *hsl, uint8_t *src, uint8_t *dst, uint32_t from, uint32_t to, uint32_t n);
void __stdcall SpriteSetCell(uint8_t *hsl, uint8_t *s, uint32_t cel);
void __stdcall SpriteSetLevel(uint8_t *hsl, uint8_t *s, int32_t level);
void __stdcall sprite_set_level_relative(uint8_t *hsl, uint32_t a, uint32_t b, uint32_t c);
void __stdcall SpriteSetTopMost(uint8_t *hsl, uint8_t *s);
uint32_t __stdcall SpriteSwapOrder(uint8_t *hsl, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
void __cdecl SpriteSetProcessMsgs(uint8_t *hsl, uint8_t *s, int on);
void __stdcall SpriteSetColorMap(uint8_t *hsl, uint8_t *s, uint32_t map);
void __stdcall SpriteSetDebugProc(uint8_t *hsl, uint32_t proc, uint32_t arg);
void __stdcall SpriteSetDisplayProc(uint8_t *s, uint32_t proc, uint32_t arg);
void __stdcall SpriteSetBoundsProc(uint8_t *s, uint32_t proc);
void __stdcall SpriteSetExternalDraw(uint8_t *s, uint32_t v);
void __stdcall SpriteSetExtOffsets(uint8_t *hsl, uint8_t *s, uint32_t offsets);
void __stdcall SpriteSetExtraId(uint8_t *s, uint32_t id);
void __cdecl SpriteSetFlip(uint8_t *hsl, uint8_t *s, uint32_t fx, uint32_t fy);
void __stdcall SpriteSetId(uint8_t *s, uint32_t id);
void __stdcall SpriteSetMerge(uint8_t *s, uint32_t v);
void __stdcall SpriteSetMergeDefault(uint8_t *hsl, uint32_t v);
uint32_t __stdcall SpriteSetPaintHook(uint8_t *hsl, uint32_t proc, uint32_t arg, uint32_t *old_arg);
void __stdcall SpriteSetPickMode(uint8_t *s, uint32_t bounds, uint32_t mode);
void __cdecl SpriteSetRedraw(uint8_t *hsl, uint8_t *s, uint32_t on, int update);
int __stdcall SpriteSetSendMsg(uint8_t *hsl, int on);
void __stdcall SpriteSetSpriteClipBox(uint8_t *s, const int32_t *r);
void __stdcall SpriteSetVarTable(uint8_t *hsl, uint32_t table);
void __stdcall SpriteSetPriority(uint8_t *s, uint32_t priority);
void __stdcall SpriteSetSpotColor(uint8_t *hsl, uint8_t color);
void __stdcall SpriteSetSync(uint8_t *hsl, uint32_t on);
uint32_t __stdcall SpriteSetUserData(uint8_t *s, uint32_t v);
uint32_t __stdcall SpriteGetComposeBuffer(uint8_t *hsl);
void __stdcall SpriteSetGraphicsQuality(uint8_t *hsl, const uint32_t *a, const uint32_t *b, uint32_t unused1,
                                         uint32_t unused2);
void __stdcall sprite_set_interface(uint8_t *hsl, int off);
void __stdcall SpriteTransfer(uint8_t *src, uint8_t *dst);
void __stdcall SpriteScreenShake(uint8_t *hsl, int dx, int dy);
uint32_t __stdcall sprite_scroll_box(int32_t *r, int32_t *pos);
void __stdcall SpriteCalculateScrollBox(uint8_t *hsl, int32_t *r, int32_t *pos);
void __stdcall SpriteGetAuxillaryScrollPos(uint8_t *hsl, int32_t *pos);
void __stdcall SpriteGetScrollBox(uint8_t *hsl, int32_t *r);
void __stdcall SpriteGetScrollableClipBox(uint8_t *hsl, int32_t *r);
void __stdcall SpriteGetScrollPos(uint8_t *hsl, int32_t *pos);
void __stdcall SpriteOffsetAuxillaryScrollPos(uint8_t *hsl, int dx, int dy);
void __stdcall SpriteOffsetScrollPos(uint8_t *hsl, int dx, int dy);
void __stdcall SpriteSetAuxillaryScrollPos(uint8_t *hsl, const int32_t *pos);
void __stdcall sprite_offset_rects(uint8_t *s, int dx, int dy);
void __stdcall SpriteScrollOffset(uint8_t *hsl, uint8_t *s, int dx, int dy);
void __stdcall SpriteScrollPosition(uint8_t *hsl, uint8_t *s, int x, int y);

#endif
