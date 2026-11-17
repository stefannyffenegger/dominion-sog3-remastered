/* Game logic: the map file list (filelist.c) */
#ifndef DOMINION_ISO_FILELIST_H
#define DOMINION_ISO_FILELIST_H

#include <stdint.h>

int __cdecl file_listed(const char *name, const char *ini, const char *key, const char *section, uint32_t n);
uint32_t __stdcall IsoBuildFileListIni(uint8_t *iso, uint32_t pattern_id, uint32_t key_id, uint32_t ini_id,
                                       uint32_t section_id, uint32_t n);
int __stdcall IsoBuildFileList(uint8_t *iso, uint32_t pattern_id);

int __stdcall IsoGetSDLFilenameIndex(uint8_t *iso, uint32_t var);
int __stdcall IsoGetWXPInfo(uint8_t *iso, int index, uint32_t dir_var, uint32_t name_var, uint32_t title_var,
                            uint32_t *info, int *is_wxm);
int __cdecl map_file_info(char *path, uint32_t *info);
int __stdcall IsoFreeFileList(uint8_t *iso);

#include <windows.h>
MMRESULT __cdecl IsoRenameFile(uint8_t *iso, uint32_t from_id, uint32_t to_id);
BOOL __cdecl IsoDeleteFile(uint8_t *iso, uint32_t name_id, int in_save_dir);
void __cdecl IsoGetMapFilename(uint8_t *iso, uint32_t name_id);

#endif
