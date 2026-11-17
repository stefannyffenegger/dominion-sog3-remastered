/* Game logic: the map file list (filelist.c) */
#ifndef DOMINION_ISO_FILELIST_H
#define DOMINION_ISO_FILELIST_H

#include <stdint.h>

int __cdecl file_listed(const char *name, const char *ini, const char *key, const char *section, uint32_t n);
uint32_t __stdcall IsoBuildFileListIni(uint8_t *iso, uint32_t pattern_id, uint32_t key_id, uint32_t ini_id,
                                       uint32_t section_id, uint32_t n);
int __stdcall IsoBuildFileList(uint8_t *iso, uint32_t pattern_id);

#endif
