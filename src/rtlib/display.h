/* The sprite engine's display layer (display.c) */
#ifndef DOMINION_RTLIB_DISPLAY_H
#define DOMINION_RTLIB_DISPLAY_H

#include <stdint.h>
#include <windows.h>

struct IDirectDraw2;
struct IDirectDrawSurface;
struct _DDSURFACEDESC;

void __cdecl dd_video_memory(uint32_t *total, uint32_t *free);
HRESULT __stdcall dd_enum_mode(struct _DDSURFACEDESC *m, void *unused);
HRESULT __stdcall dd_enum_attached(struct IDirectDrawSurface *s, struct _DDSURFACEDESC *desc,
                                   struct IDirectDrawSurface **out);
int __cdecl dd_create(void);
int __cdecl dd_destroy(void);
struct IDirectDraw2 *__stdcall SpriteGetDDraw(uint8_t *hsl);
int __cdecl dd_set_mode(uint32_t w, uint32_t h);
int __cdecl SpritePaintDirectDrawSurfacesBlack(uint8_t *hsl, uint32_t w, uint32_t h);
int __cdecl SpritePaintDirectDrawSurfacesBlack1(uint8_t *hsl, uint32_t w, uint32_t h);
int __cdecl dd_create_surfaces(int w, int h);
int __cdecl dd_release_surfaces(void);
int __cdecl display_set_size(uint32_t w, uint32_t h, uint32_t depth);
int __cdecl SpriteSetDisplayMode(uint8_t *hsl, uint32_t w, uint32_t h, uint32_t depth, int was_full,
                                 int full_screen, int change, uint32_t flip);
int __stdcall SpriteCreateDirectDrawSurfaces(uint8_t *hsl);
int __stdcall SpriteReleaseDirectDrawSurfaces(uint8_t *hsl);
int __stdcall SpriteCreateDirectDrawObject(uint8_t *hsl);
int __stdcall SpriteDestroyDirectDrawObject(uint8_t *hsl);
int __cdecl surface_restore(struct IDirectDrawSurface *s);
int __cdecl surfaces_restore_all(uint8_t *hsl);
void __cdecl ClearVideoMem(struct IDirectDraw2 *dd);
void __stdcall SpriteCreateDirectDrawPalette(uint8_t *hsl);
void __stdcall SpriteReleaseDirectDrawPalette(uint8_t *hsl);
void __cdecl palette_attach(void);
void __cdecl palette_release(void);
void __cdecl palette_apply(void);
void __cdecl display_lost_focus(uint8_t *hsl);
void __cdecl display_got_focus(uint8_t *hsl);
int __stdcall display_get_memory(uint8_t *hsl, uint32_t *total, uint32_t *free, uint32_t *buffers);
void __cdecl back_lock(void);
void __cdecl back_unlock(void);
void __cdecl back_unlock_rect(int32_t *rect);
void __cdecl back_scroll(int dx, int dy, int32_t *rect);
void __cdecl surf8c_scroll(int dx, int dy, int32_t *rect);
void __cdecl back_fill(int32_t *pos, int w, int h);
void __cdecl border_draw(int32_t *rect, struct _DDSURFACEDESC *desc);
void __cdecl border_paint(int32_t *rect);
int __stdcall display_set_border(uint8_t *hsl, int w, int h, int vw, int vh);
uint32_t __stdcall display_get_b5c(uint8_t *hsl);
int __cdecl iface_surface_create(int32_t *rect);
void __cdecl iface_surface_release(void);
void __cdecl dirty_reset(void);
void __cdecl dirty_clear(void);
void __cdecl dirty_add_sprite(uint8_t *s);
void __cdecl dirty_add_if_shown(uint8_t *s);
void __cdecl dirty_add_interface_sprites(void);
void __cdecl sprites_clear_dirty_flag(void);
void __cdecl iface_lock(void);
void __cdecl iface_unlock(void);
void __cdecl iface_blit(void);
void __cdecl iface_update(void);
int __cdecl display_true(void);
void __cdecl display_nop1(void);
int __cdecl display_false(void);
void __cdecl display_nop2(void);
void __stdcall display_nop4(uint32_t a, uint32_t b, uint32_t c, uint32_t d);
void __stdcall SpriteRefreshInterfaceArea(uint8_t *hsl, uint8_t *sprite);
void __cdecl blit_sprite_rect(uint8_t *s);
void __cdecl display_draw_sprites(void);
int __stdcall SpriteFlipPages(uint8_t *hsl);
void __stdcall display_flip_now(uint8_t *hsl);
int __cdecl surfaces_restore(void);
int __stdcall display_activate(uint8_t *hsl, int active);

void __cdecl palette_delete(void);
int __cdecl palette_create(void);
int __cdecl palette_create_reserved(void);
int __stdcall SpriteChangePalette(uint8_t *hsl, uint32_t first, uint32_t n, const uint32_t *colors, int animate,
                                  int same);
void __stdcall palette_to_dib(uint32_t first, uint32_t end, uint32_t unused);
void __stdcall SpriteGetPalette(uint8_t *hsl, void *out, uint32_t first, uint32_t n);
int __stdcall SpriteGetRgbPalette(uint8_t *hsl, void *out, uint32_t first, uint32_t n);
int __stdcall SpriteFadePalette(uint8_t *hsl, uint32_t step, uint32_t steps, uint32_t unused1, uint32_t unused2);
UINT __stdcall SpriteRealizePalette(uint8_t *hsl);
UINT __stdcall SpriteRealizePalette2(uint8_t *hsl, int background);
HPALETTE __stdcall SpriteSelectPalette(uint8_t *hsl, HDC dc);
int __stdcall SpriteSetPalette(uint8_t *hsl, const void *colors);
uint32_t __stdcall palette_get_b80(uint8_t *hsl);
uint32_t __stdcall palette_get_b80_2(uint32_t unused1, uint32_t unused2);
int __cdecl palette_false(void);
int __cdecl palette_true(void);
UINT __cdecl palette_realize(HDC dc, HPALETTE pal, int background);
void __stdcall palette_changed(uint8_t *hsl);
void __stdcall palette_set_painting(uint8_t *hsl, int painting, int apply);

#endif
