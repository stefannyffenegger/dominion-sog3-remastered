/* The game logic, 0x10031996-0x1003254f: before a multiplayer match the host tells the
 * players which map it chose and describes its two files (the .wxp and the scenario), and
 * each player answers whether it has them and whether they are the same. */
#include "iso_core.h"
#include "mapsync.h"
#include "misc.h"

#define dp_send ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t, int, int, const void *, size_t), 0x10112050)
#define map_path ORIG(void(__cdecl *)(const char *, char *, int), 0x100480f7)
#define file_missing ORIG(void(__cdecl *)(const char *), 0x10048031)
#define message_box ORIG(void(__cdecl *)(int, const char *, const char *, int), 0x1005c515)
#define g_strcpy ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c70)
#define g_strcat ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c80)
#define g_malloc ORIG(void *(__stdcall *)(size_t, int), 0x100f4976)
#define g_free ORIG(void(__stdcall *)(void *), 0x100f499a)
#define strlen ORIG(size_t(__cdecl *)(const char *), 0x100f5750)
#define strcmp ORIG(int(__cdecl *)(const char *, const char *), 0x100f53a0)

#define NET_SESSION I(uint32_t, 0x1816)
#define MAP_NAME I(char *, 0x18f0)     /* the chosen map's name, 260 bytes */
#define MAP_NAME_OWN I(int32_t, 0x179f) /* that buffer is ours */
#define SDL_OVERRIDE I(uint32_t, 0xd9da) /* scenarios as text (.sdl), not compiled (.sdc) */
#define MAP_HEADER 0xbf40               /* the host's first 128 bytes of the .wxp */
#define MAP_PROBLEM I(uint8_t, 0xbfc0)
#define MSG_NAME 0xbba
#define MSG_INFO 0xbbb
#define MSG_EXISTS 0xbbc
#define MSG_VALID 0xbbd

/* what the host sends about its files */
#pragma pack(push, 1)
typedef struct {
    char header[0x80];
    FILETIME wxp_times[3];
    FILETIME scn_created, scn_accessed, scn_written;
    DWORD wxp_size, scn_size;
    uint32_t sdl_override;
} MapInfo;
#pragma pack(pop)

static void name_buffer(void)
{
    if (!MAP_NAME_OWN) {
        g_free(MAP_NAME);
        MAP_NAME = NULL;
    }
    if (!MAP_NAME) {
        MAP_NAME = g_malloc(0x104, 1);
        MAP_NAME_OWN = 1;
    }
}

/* 0x10031996: the host named a map: keep the name and answer which of its two files exist */
uint32_t __cdecl map_receive_name(uint8_t *iso, uint32_t player, const char *name)
{
    char path[0x104];
    uint32_t exists[2];
    g_iso = iso;
    if (!name || !*name)
        return 1;
    map_path(name, path, 1);
    name_buffer();
    g_strcpy(MAP_NAME, name);
    exists[0] = mmioOpenA(path, NULL, MMIO_EXIST) != 0;
    GetPrivateProfileStringA((const char *)ORIG_PTR(0x10136c5c), (const char *)ORIG_PTR(0x10136c54),
                             (const char *)ORIG_PTR(0x10136c50), path, 0x100, (const char *)ORIG_PTR(0x10136c40));
    g_strcat(path, name);
    path_unescape(path);
    /* the extension is replaced: SDL or SDC */
    g_strcpy(path + strlen(path) - 3, (const char *)ORIG_PTR(SDL_OVERRIDE ? 0x10136c68 : 0x10136c6c));
    DebugStr((const char *)ORIG_PTR(0x10136ca0), player, path);
    exists[1] = mmioOpenA(path, NULL, MMIO_EXIST) != 0;
    DebugStr((const char *)ORIG_PTR(0x10136cd0), exists[0]);
    DebugStr((const char *)ORIG_PTR(0x10136ce4), exists[1]);
    dp_send(NET_SESSION, player, 1, MSG_EXISTS, exists, sizeof exists);
    return dp_send(NET_SESSION, player, 1, MSG_NAME, name, strlen(name) + 1);
}

/* 0x10031c81: the host described its files: compare them with ours and answer. The .wxp
 * must have the same header and size, the scenario the same size and write time. Bits of
 * the problem byte: 1 no .wxp, 2 a different one, 0x20 no scenario, 0x10 a different one */
void __cdecl map_receive_info(uint8_t *iso, uint32_t player, const uint8_t *info)
{
    const MapInfo *m = (const MapInfo *)info;
    uint32_t valid[2] = {0, 0};
    char path[0x100], head[0x80];
    FILETIME created, accessed, written;
    DWORD size, got;
    HANDLE f;
    g_iso = iso;
    name_buffer();
    SDL_OVERRIDE = m->sdl_override;
    memcpy(g_iso + MAP_HEADER, info, 0x80);
    if (!*MAP_NAME) {
        valid[0] = valid[1] = 1;
        dp_send(NET_SESSION, player, 1, MSG_VALID, valid, sizeof valid);
        return;
    }
    map_path(MAP_NAME, path, 1);
    MAP_PROBLEM = 0;
    f = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        file_missing(path);
        MAP_PROBLEM = 1;
    } else {
        GetFileTime(f, &created, &accessed, &written);
        size = GetFileSize(f, NULL);
        ReadFile(f, head, 0x80, &got, NULL);
        CloseHandle(f);
        valid[0] = got == 0x80 && !memcmp(head, g_iso + MAP_HEADER, 0x80) && m->wxp_size == size;
        if (!valid[0])
            MAP_PROBLEM = 2;
    }
    map_path(MAP_NAME, path, 0);
    f = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        MAP_PROBLEM |= 0x20;
        file_missing(path);
    } else {
        GetFileTime(f, &created, &accessed, &written);
        size = GetFileSize(f, NULL);
        CloseHandle(f);
        valid[1] = m->scn_size == size && CompareFileTime(&written, &m->scn_written) == 0;
        if (!valid[1])
            MAP_PROBLEM |= 0x10;
    }
    dp_send(NET_SESSION, player, 1, MSG_VALID, valid, sizeof valid);
}

/* 0x10032169: the host describes its two files to a player */
uint32_t __cdecl map_send_info(uint8_t *iso, uint32_t player, const char *name)
{
    char path[0x100];
    MapInfo m;
    DWORD got;
    HANDLE f;
    g_iso = iso;
    if (!I(int32_t, 0x148))
        return 1;
    m.sdl_override = GetPrivateProfileIntA((const char *)ORIG_PTR(0x10136de4), (const char *)ORIG_PTR(0x10136dd8), 0,
                                           (const char *)ORIG_PTR(0x10136dc8));
    SDL_OVERRIDE = m.sdl_override;
    map_path(name, path, 1);
    if (!name || !*name)
        return 1;
    f = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        file_missing(path);
        return 0;
    }
    GetFileTime(f, &m.wxp_times[0], &m.wxp_times[1], &m.wxp_times[2]);
    m.wxp_size = GetFileSize(f, NULL);
    ReadFile(f, m.header, 0x80, &got, NULL);
    CloseHandle(f);
    if (got != 0x80) {
        message_box(0, (const char *)ORIG_PTR(0x10136e20), (const char *)ORIG_PTR(0x10136e14), 0);
        return 0;
    }
    if (strcmp(m.header, (const char *)ORIG_PTR(0x10136e34)) && strcmp(m.header, (const char *)ORIG_PTR(0x10136e38))) {
        wsprintfA((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x10136e3c), m.header);
        message_box(0, (const char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x10136e58), 0);
        return 0;
    }
    map_path(name, path, 0);
    f = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        file_missing(path);
        return 0;
    }
    GetFileTime(f, &m.scn_created, &m.scn_accessed, &m.scn_written);
    m.scn_size = GetFileSize(f, NULL);
    CloseHandle(f);
    return dp_send(NET_SESSION, player, 1, MSG_INFO, &m, sizeof m);
}

/* 0x100324ba, 0x10032503: a player's answers, only noted in the debug output */
uint32_t __cdecl map_note_exists(uint8_t *iso, uint32_t player, uint32_t exists)
{
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x10136ebc), player, exists);
    return 1;
}

uint32_t __cdecl map_note_valid(uint8_t *iso, uint32_t player, uint32_t valid)
{
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x10136eec), player, valid);
    return 1;
}
