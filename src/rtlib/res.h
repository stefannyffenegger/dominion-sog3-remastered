/* Resource files (DOMINION.bin + DOMINION.000, ...): the engine's "Res" library.
 *
 * A resource file is a main file (header, index, script buffers, tables) plus data files
 * with the same name and the extension .000, .001, ... Each resource has a 10-byte index
 * entry: type, data file number, offset, size. Types 1-12 are stored in the data files;
 * types 13-22 live inside the main file's script buffers and the entry holds an offset
 * into them (or, for resources made at run time, a pointer).
 *
 * The layouts below are the original's: the rest of the original engine reads these
 * structures directly, so every field stays where it was (checked by the asserts). */
#ifndef DOMINION_RTLIB_RES_H
#define DOMINION_RTLIB_RES_H

#include <stdint.h>
#include <windows.h>
#include <mmsystem.h>

#define RES_MAGIC 0x4c37
#define RES_VERSION 0x102
#define RES_OLD_VERSION 2
#define RES_HEADER_SIZE 0x1dc
#define RES_BLOCK 0x1964 /* index entries per allocation block */
#define RES_MAX_FILES 100
#define RES_MAX_SCRIPTS 0x30
#define RES_KEYS 15

/* Resource types (names from ResEnumResTypes) */
enum {
    RTYPE_BITMAP = 1, RTYPE_DATA, RTYPE_FILE, RTYPE_FRAME, RTYPE_GROUND, RTYPE_MIDI, RTYPE_MODEL, RTYPE_MPROTO,
    RTYPE_OBJ3D, RTYPE_OPROTO, RTYPE_TABLE, RTYPE_WAVE,                    /* 1-12: in the data files */
    RTYPE_MOVIE, RTYPE_ARRAY, RTYPE_CELL, RTYPE_GROUP, RTYPE_PALETTE, RTYPE_QUEUE,
    RTYPE_SCRIPT, RTYPE_SPRITE, RTYPE_TEXT, RTYPE_TILE,                    /* 13-22: in memory */
};
#define RES_IN_MEMORY(t) ((t) >= RTYPE_MOVIE && (t) <= RTYPE_TILE)

/* Key resources: tables stored in the main file */
enum {
    KEY_INDEX = 0,      /* 10-byte entries */
    KEY_ENTRIES = 1,    /* 0x52-byte records: id + name */
    KEY_NAMES = 2,      /* 0x2c-byte records: id + name, for ResGetIndex */
    KEY_VARIABLES = 3,  /* 8-byte records */
    KEY_CONSTSTR = 4,   /* constant strings */
    KEY_SCRIPTS = 5,    /* script buffers, split at header.script_end[] */
    KEY_PALETTE = 6,
    KEY_NAMETABLE = 7,
};

#pragma pack(push, 1)
typedef struct ResEntry {
    uint8_t type;   /* 0 = unused */
    uint8_t file;   /* data file number (.000 = 0) */
    uint32_t offset;
    uint32_t size;
} ResEntry;

typedef struct ResKey {
    uint32_t offset, size;
} ResKey;

typedef struct ResHeader {           /* at the start of the main file */
    uint16_t magic;                  /* 0x000 RES_MAGIC */
    uint16_t size;                   /* 0x002 RES_HEADER_SIZE */
    uint16_t format;                 /* 0x004 "32" */
    uint8_t title_len;               /* 0x006 */
    char title[0x4f];                /* 0x007 */
    uint16_t version;                /* 0x056 */
    uint32_t entry_id;               /* 0x058 */
    uint32_t script_end[RES_MAX_SCRIPTS]; /* 0x05c end offset of each script buffer */
    uint32_t n_scripts;              /* 0x11c */
    uint32_t max_fade_colors;        /* 0x120 */
    uint32_t max_trans_colors;       /* 0x124 */
    uint32_t n_dynamic;              /* 0x128 free index slots for resources made at run time */
    uint32_t n_strings;              /* 0x12c */
    uint32_t n_variables;            /* 0x130 */
    uint32_t max_scr_msg;            /* 0x134 */
    uint8_t unknown_138[0x20];       /* 0x138 */
    uint8_t build_type;              /* 0x158 */
    uint8_t unknown_159[2];          /* 0x159 */
    uint8_t checksum;                /* 0x15b all header bytes add up to 0 */
    uint8_t unknown_15c[8];          /* 0x15c */
    ResKey keys[RES_KEYS];           /* 0x164 */
} ResHeader;

typedef struct ResFile {
    HMMIO main;                      /* 0x000 */
    HMMIO data[RES_MAX_FILES];       /* 0x004 */
    HWND notify;                     /* 0x194 gets message 0x590 when a bitmap can't be expanded */
    void(__stdcall *callback)(void); /* 0x198 called while reading big resources */
    ResEntry *index[11];             /* 0x19c blocks of RES_BLOCK entries */
    uint8_t *scripts[RES_MAX_SCRIPTS]; /* 0x1c8 */
    uint8_t *names;                  /* 0x288 KEY_NAMES */
    uint8_t *variables;              /* 0x28c KEY_VARIABLES */
    void *unknown_290;
    char *const_strings;             /* 0x294 KEY_CONSTSTR */
    uint8_t *entries;                /* 0x298 KEY_ENTRIES */
    void *unknown_29c;               /* 0x29c freed on close; never set here */
    ResHeader header;                /* 0x2a0 */
    int32_t cb_done;                 /* 0x47c bytes read since the last callback */
    int32_t cb_every;                /* 0x480 call the callback every this many bytes */
    int32_t cb_unknown;              /* 0x484 */
    uint32_t n_entries;              /* 0x488 */
    uint32_t n_index;                /* 0x48c */
    uint32_t n_variables;            /* 0x490 */
    uint32_t n_names;                /* 0x494 */
    uint32_t n_static;               /* 0x498 entries stored in the file; the rest are dynamic */
    uint32_t dirty;                  /* 0x49c header must be written on close */
    void *decomp;                    /* 0x4a0 bitmap decompressor */
    uint8_t cached[RES_MAX_FILES];   /* 0x4a4 data file copied to the hard disk */
    char base[0x100];                /* 0x508 main file path without extension */
} ResFile;
#pragma pack(pop)

_Static_assert(sizeof(ResEntry) == 10, "ResEntry");
_Static_assert(sizeof(ResHeader) == RES_HEADER_SIZE, "ResHeader");
_Static_assert(__builtin_offsetof(ResFile, header) == 0x2a0, "ResFile.header");
_Static_assert(__builtin_offsetof(ResFile, n_entries) == 0x488, "ResFile.n_entries");
_Static_assert(__builtin_offsetof(ResFile, cached) == 0x4a4, "ResFile.cached");
_Static_assert(sizeof(ResFile) == 0x608, "ResFile");

/* A bitmap ("DIB") as the engine keeps it in memory */
typedef struct ResDib {
    uint32_t header_size; /* 0x24 */
    int32_t width;
    uint32_t pitch;
    uint32_t height;
    uint32_t flags;       /* 0x80: run-length coded sprite rows */
    uint32_t unknown[3];
    uint8_t *bits;
} ResDib;
#define DIB_RLE 0x80

/* A reader the audio code can pull a MIDI or wave resource from */
typedef struct ResStream {
    HMMIO file;
    uint32_t pos;
    uint32_t start;
} ResStream;

/* Exports (all stdcall but ResEnumResTypes) */
ResFile *__stdcall ResOpenFile(LPSTR path, uint32_t flags);
ResFile *__stdcall ResCreateFile(LPSTR path);
int __stdcall ResCloseFile(ResFile *rf);
uint8_t *__stdcall ResLoadResource(ResFile *rf, uint32_t id, uint32_t *size);
uint8_t *__stdcall ResLoadKeyResource(ResFile *rf, uint32_t key, uint32_t *size);
uint8_t *__stdcall ResLoadNameTable(ResFile *rf, uint32_t *size);
uint32_t __stdcall ResLoadPalette(ResFile *rf, void *out);
int __stdcall ResReadResourceData(ResFile *rf, uint32_t id, void *buf, int size, int offset);
uint32_t __stdcall ResCopyResource(ResFile *rf, uint32_t id);
uint32_t __stdcall ResDynamicResource(ResFile *rf, uint8_t type, uint32_t data);
int __stdcall ResDeleteResource(ResFile *rf, uint32_t id);
uint8_t *__stdcall ResExpandBitmapDib(ResFile *rf, ResDib *dib);
uint8_t *__stdcall ResExpandBitmapRec(ResFile *rf, uint32_t *rec, uint8_t *bits);
int __stdcall ResExtractFile(ResFile *rf, uint32_t id, HMMIO out);
uint32_t __stdcall ResGetTitle(ResFile *rf, void *out, uint32_t size);
uint32_t __stdcall ResCountResources(ResFile *rf, uint32_t *n_static);
uint32_t __stdcall ResCountStrings(ResFile *rf);
uint32_t __stdcall ResCountVariables(ResFile *rf);
int __stdcall ResEnumEntries(ResFile *rf, int(__stdcall *cb)(void *, uint32_t, uint32_t), uint32_t user);
int __stdcall ResEnumResources(ResFile *rf, int(__stdcall *cb)(uint8_t, uint32_t, uint32_t));
int __cdecl ResEnumResTypes(ResFile *rf, int(__stdcall *cb)(void *));
int __stdcall ResEnumVariables(ResFile *rf, int(__stdcall *cb)(uint32_t, uint32_t));
uint8_t __stdcall ResGetBuildType(ResFile *rf);
char *__stdcall ResGetConstStr(ResFile *rf, uint32_t offset);
uint32_t __stdcall ResGetIndex(ResFile *rf, LPCSTR name);
uint32_t __stdcall ResGetMaxFadeColors(ResFile *rf);
uint32_t __stdcall ResGetMaxScrMsg(ResFile *rf);
uint32_t __stdcall ResGetMaxTransColors(ResFile *rf);
uint32_t __stdcall ResGetSize(ResFile *rf, uint32_t id);
uint8_t __stdcall ResGetType(ResFile *rf, uint32_t id);
void __stdcall ResSetNotify(ResFile *rf, HWND wnd);
void *__stdcall ResSetCallBack(ResFile *rf, void(__stdcall *cb)(void), int32_t every);
void __stdcall ResGetHeader(ResFile *rf, ResHeader *out);
void __stdcall ResWriteHeader(ResFile *rf, const ResHeader *in);
void __stdcall ResSetBuildType(ResFile *rf, uint8_t type);
void __stdcall ResSetEntryId(ResFile *rf, uint32_t id);
void __stdcall ResSetMaxScrMsg(ResFile *rf, uint32_t n);
void __stdcall ResSetStringCount(ResFile *rf, uint32_t n);
void __stdcall ResSetTitle(ResFile *rf, LPCSTR title, LPCSTR subtitle);
void __stdcall ResSetVariableCount(ResFile *rf, uint32_t n);
int __stdcall ResWriteIndexBufs(ResFile *rf, ResEntry **blocks, uint32_t n, uint32_t n_dynamic);
int __stdcall ResWriteKeyResource(ResFile *rf, uint32_t key, const void *data, uint32_t size);
void __stdcall ResWritePalette(ResFile *rf, const void *data, uint32_t n_colors, uint32_t trans,
                               uint32_t fade);
void __stdcall ResWriteScriptBuf(ResFile *rf, const void *data, uint32_t size,
                                 const uint32_t *script_end, uint32_t n);
void *__stdcall ResCreateMidiEvent(ResFile *rf, uint32_t a, uint32_t id);
uint32_t __stdcall ResCreateWaveEvent(ResFile *rf, uint32_t a, uint32_t id,
                                      uint32_t(__stdcall *create)(uint32_t, void *, void *));
void __stdcall ResExitLibrary(uint32_t unused);
void *__stdcall ResInitLibrary(HMODULE module);

/* Internal functions of the original that other engine code also calls */
ResEntry *__stdcall res_entry(ResFile *rf, uint32_t id);
uint8_t *__stdcall res_expand_bitmap(ResFile *rf, ResDib *src, uint32_t *size);
void __stdcall res_notify(ResFile *rf, UINT msg);
LONG __stdcall res_tell(HMMIO f);
void __stdcall res_close_data_files(ResFile *rf);
void __stdcall res_reset_sprite(uint32_t *s);
int __stdcall res_extract_packed(void *decomp, HMMIO in, HMMIO out, uint32_t packed, uint32_t size);
int __stdcall res_read(HMMIO f, void *buf, int n);
int __stdcall res_seek(HMMIO f, LONG pos);
uint8_t *__stdcall res_load_bitmap(ResFile *rf, HMMIO f, uint32_t size, uint32_t *out_size);
int __stdcall res_read_chunked(ResFile *rf, HMMIO f, uint8_t *buf, int n);
int __stdcall res_load_entries(ResFile *rf);
int __stdcall res_load_index(ResFile *rf);
int __stdcall res_load_variables(ResFile *rf);
int __stdcall res_load_scripts(ResFile *rf);
int __stdcall res_load_const_strings(ResFile *rf);
int __stdcall res_load_names(ResFile *rf);
HMMIO __stdcall res_open(LPSTR path, DWORD flags);
int __stdcall res_open_data_files(ResFile *rf, LPCSTR path);
int __stdcall res_read_header(HMMIO f, ResHeader *h);
int __stdcall res_read_old_header(HMMIO f, ResHeader *h, uint32_t rest);
uint32_t __stdcall res_get_entry_id(ResFile *rf);
uint32_t __stdcall res_get_offset(ResFile *rf, uint32_t id);
int __stdcall res_is_dynamic(ResFile *rf, uint32_t id);
int __stdcall res_write_header(ResFile *rf);
int __stdcall res_write(HMMIO f, const void *buf, int n);
void __stdcall res_stream_free(HLOCAL s);
int __stdcall res_stream_read(ResStream *s, void *buf, int n);
uint32_t __stdcall res_stream_rewind(ResStream *s);
ResStream *__stdcall res_stream_open(ResFile *rf, uint32_t id, void *head, int n);
ResStream *__stdcall res_stream_close(HLOCAL s);
void __cdecl res_delete_cached_files(void);
void __stdcall res_mark_cached(LPCSTR path, int cached);
void __stdcall res_uncache(ResFile *rf, int file);
void __stdcall res_cache_path(ResFile *rf, int file, char *out);
void __stdcall res_cache_file(ResFile *rf, int file, uint32_t unused);
int __stdcall res_copy_file(ResFile *rf, HMMIO dst, HMMIO src);
void __stdcall res_drop_cache(ResFile *rf, int file);

/* res_batch.c: loading many bitmaps at once, run-length expansion */
typedef int(__stdcall *ResBatchLoaded)(uint32_t *data, uint32_t size, uint32_t type, uint32_t id);
void __stdcall res_batch_sort(ResEntry *e, uint32_t *ids, uint32_t n);
uint32_t __stdcall res_batch_run(ResFile *rf, const ResEntry *e, uint32_t *size, uint32_t n);
uint8_t *__stdcall res_batch_bitmap(ResFile *rf, const uint32_t *src, const ResEntry *e, uint32_t *size);
int __stdcall res_batch_read(ResFile *rf, ResBatchLoaded loaded, const ResEntry *e, const uint32_t *ids, uint32_t n);
int __stdcall res_load_batch(ResFile *rf, ResBatchLoaded loaded, uint32_t *ids, uint32_t n);

#endif
