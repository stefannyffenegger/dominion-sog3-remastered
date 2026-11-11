/* Engine core: the object table (rt_obj.c) */
#ifndef DOMINION_RTLIB_RT_OBJ_H
#define DOMINION_RTLIB_RT_OBJ_H

#include <stdint.h>
#include <windows.h>

uint32_t __cdecl RtGetLibraryState(uint8_t *rt, LPCSTR name);
uint32_t *__stdcall obj_entry(uint32_t id);
int __stdcall obj_check_type(uint32_t id, uint32_t type);
int __stdcall obj_is(uint32_t id, uint32_t type);
uint32_t __stdcall obj_loaded(uint32_t id, uint32_t type);
uint8_t *__stdcall obj_load(uint32_t id, uint32_t *size);
void __stdcall obj_ensure(uint32_t id);
void __stdcall obj_ensure_sized(uint32_t id);
int __cdecl obj_release(uint32_t id, uint32_t type);
void __cdecl obj_release_all(void);
uint32_t __stdcall obj_copy(uint32_t id);
int __stdcall obj_delete(uint32_t id);
int __stdcall obj_batch_loaded(uint32_t *data, uint32_t size, uint32_t type, uint32_t id);
int __stdcall bitmap_merge_objects(uint32_t dst, uint32_t src, int x, int y, int mask);
void __stdcall compose_reset(int destroy);

void __stdcall rel_bitmap(uint32_t id);
int __stdcall rel_bitmap_ref(uint32_t id);
void __stdcall rel_data(uint32_t id);
int __stdcall rel_group(uint32_t id);
void __stdcall rel_midi(uint32_t id);
void __stdcall rel_type_d(uint32_t id);
void __stdcall rel_custom(uint32_t id, uint32_t type);
int __stdcall rel_sprite(uint32_t id);
void __stdcall sprite_release_members(uint32_t sprite, uint32_t *e);
void __stdcall rel_type_b(uint32_t id);
int __stdcall rel_tile_map(uint32_t id);
void __stdcall members_release(const uint32_t *ids, uint32_t n);
int __stdcall sprite_forget(uint32_t id);
void __stdcall rel_type_15(uint32_t id);
void __stdcall rel_wave(uint32_t id);

int __stdcall load_sized(uint32_t id, uint32_t type);
int __stdcall load_group(uint32_t id);
int __stdcall collect_bitmaps(void *arr, uint32_t id, uint32_t type);
int __stdcall load_sprite(uint32_t id);
int __stdcall load_bitmap(uint32_t id);
int __stdcall load_bitmap_ref(uint32_t id);
int __stdcall load_group_members(uint32_t id);
int __stdcall load_midi(uint32_t id);
int __stdcall load_type_d(uint32_t id);
int __stdcall load_custom(uint32_t id, uint32_t type);
int __stdcall load_type_b(uint32_t id);
int __stdcall load_tile_map(uint32_t id);
int __stdcall tile_map_members(uint32_t *out, const uint32_t *ids, uint32_t n);
int __stdcall sprite_create(uint32_t id);
int __stdcall compose_palette(uint8_t *d);
int __stdcall sprite_members(uint8_t *s, const uint32_t *ids, uint32_t n);
int __stdcall load_type_15(uint32_t id);
int __stdcall load_wave(uint32_t id);

#endif
