/* The engine's resource file library ("Res"), rewritten from the original
 * (addresses 0x10066ad0-0x10069e90). See res.h for the file layout.
 * Behavior kept from the original on purpose is marked "original:". */
#include "orig.h"
#include "res.h"
#include "utils.h"
#include "compress.h"

/* ---- original engine functions we call (not rewritten yet) ---- */
#define AudioCreateMidi ORIG(void *(__stdcall *)(uint32_t, void *, void *), 0x100416e6)

/* The sprite code's run-length expander is hand-written assembly that doesn't keep EBX */
static void expand_rle_dib(void *dst, const void *src)
{
    orig_call2_unsafe(ORIG_PTR(0x1006a588), (uint32_t)(uintptr_t)dst, (uint32_t)(uintptr_t)src);
}

/* globals of the original */
#define g_res_module G(HMODULE, 0x1008f900)
#define g_res_dir ((char *)ORIG_PTR(0x1008f904)) /* folder of the engine DLL */
#define g_type_name ((char *)ORIG_PTR(0x1008f8e0)) /* ResEnumResTypes: name, then type */

#define INI "dominion.ini"
#define INI_SECTION "resource"

static const char *const TYPE_NAMES[] = {
    "unknown", "bitmap", "data", "file", "frame", "ground", "midi", "model", "mproto",
    "obj3d", "oproto", "table", "wave", "movie", "array", "cell", "group", "palette",
    "queue", "script", "sprite", "text", "tile", "title", "subtitle", "local", "entry",
};

/* ---- low-level file access ---- */

int __stdcall res_read(HMMIO f, void *buf, int n)
{
    return mmioRead(f, buf, n) == n;
}

int __stdcall res_seek(HMMIO f, LONG pos)
{
    return mmioSeek(f, pos, SEEK_SET) == pos;
}

LONG __stdcall res_tell(HMMIO f)
{
    return mmioSeek(f, 0, SEEK_CUR);
}

int __stdcall res_write(HMMIO f, const void *buf, int n)
{
    return mmioWrite(f, buf, n) == n;
}

/* The file itself, or the same name in the engine's folder. */
HMMIO __stdcall res_open(LPSTR path, DWORD flags)
{
    char dir[256], file[256], full[256];
    if (FileExist(path))
        return mmioOpenA(path, NULL, flags | MMIO_ALLOCBUF);
    GetPathAndFile(path, dir, file);
    MakeFullPath(full, g_res_dir, file);
    return mmioOpenA(full, NULL, flags | MMIO_ALLOCBUF);
}

ResEntry *__stdcall res_entry(ResFile *rf, uint32_t id)
{
    return (ResEntry *)((uint8_t *)rf->index[id / RES_BLOCK] + (id % RES_BLOCK) * 10);
}

void __stdcall res_notify(ResFile *rf, UINT msg)
{
    if (rf->notify)
        SendMessageA(rf->notify, msg, 0, 0);
}

/* Reads n bytes, calling the file's callback every cb_every bytes (a loading screen
 * keeps itself alive this way). The callback may read from the same file, so the
 * position is restored afterwards. */
int __stdcall res_read_chunked(ResFile *rf, HMMIO f, uint8_t *buf, int n)
{
    while (n) {
        int k = n < rf->cb_every ? n : rf->cb_every;
        if (!res_read(f, buf, k))
            return 0;
        buf += k;
        n -= k;
        rf->cb_done += k;
        if (n && rf->cb_done >= rf->cb_every) {
            int32_t done = rf->cb_done, every = rf->cb_every, unknown = rf->cb_unknown;
            LONG pos = res_tell(f);
            rf->callback();
            rf->cb_every = every;
            rf->cb_done = done - rf->cb_every;
            rf->cb_unknown = unknown;
            res_seek(f, pos);
        }
    }
    return 1;
}

static int read_data(ResFile *rf, HMMIO f, void *buf, int n)
{
    if (rf->callback)
        return res_read_chunked(rf, f, buf, n);
    return res_read(f, buf, n);
}

/* ---- opening ---- */

/* An old (version 2) header is shorter: the fields from 0x134 on are moved up. */
int __stdcall res_read_old_header(HMMIO f, ResHeader *h, uint32_t rest)
{
    uint8_t old[0x58 + 0x1dc];
    uint8_t sum = 0;
    uint16_t size;
    if (rest > sizeof old - 0x58 || !res_read(f, old + 0x58, (int)rest))
        return 0;
    memcpy(&h->entry_id, old + 0x58, 4);
    memcpy(h->script_end, old + 0x5c, 0xc0);
    memcpy(&h->n_scripts, old + 0x11c, 0x18); /* n_scripts .. n_variables */
    memcpy(&h->build_type, old + 0x134, 4);   /* build_type .. checksum */
    memcpy(h->keys, old + 0x138, sizeof h->keys);
    if (h->keys[KEY_INDEX].size == 0)
        return 0;
    memcpy(old, h, 0x58); /* the old header, contiguous, for the checksum */
    memcpy(&size, old + 2, 2);
    for (uint32_t i = 0; i < size && i < sizeof old; i++)
        sum += old[i];
    h->size = RES_HEADER_SIZE;
    return sum == 0;
}

int __stdcall res_read_header(HMMIO f, ResHeader *h)
{
    uint8_t first[0x58];
    uint16_t magic, size, version;
    if (!res_read(f, first, sizeof first))
        return 0;
    memcpy(&magic, first, 2);
    memcpy(&size, first + 2, 2);
    memcpy(&version, first + 0x56, 2);
    if (magic != RES_MAGIC)
        return 0;
    memcpy(h, first, sizeof first);
    if (version == RES_OLD_VERSION)
        return res_read_old_header(f, h, (uint32_t)size - 0x58);
    if (version == RES_VERSION) {
        uint8_t sum = 0;
        if (size < 0x58 || size > RES_HEADER_SIZE ||
            !res_read(f, (uint8_t *)h + 0x58, size - 0x58) || h->keys[KEY_INDEX].size == 0)
            return 0;
        for (uint32_t i = 0; i < size; i++)
            sum += ((uint8_t *)h)[i];
        h->size = RES_HEADER_SIZE;
        return sum == 0;
    }
    return 0;
}

uint8_t *__stdcall ResLoadKeyResource(ResFile *rf, uint32_t key, uint32_t *size)
{
    uint8_t *p;
    uint32_t n;
    if (!rf || key >= RES_KEYS)
        return NULL;
    n = rf->header.keys[key].size;
    if (size)
        *size = n;
    if (!n)
        return NULL;
    p = winmalloc(n, 0);
    if (!p)
        return NULL;
    if (!res_seek(rf->main, (LONG)rf->header.keys[key].offset) || !res_read(rf->main, p, (int)n))
        p = winfree(p);
    return p;
}

uint8_t *__stdcall ResLoadNameTable(ResFile *rf, uint32_t *size)
{
    return ResLoadKeyResource(rf, KEY_NAMETABLE, size);
}

/* The index: the stored entries, then room for the dynamic ones, in blocks. */
int __stdcall res_load_index(ResFile *rf)
{
    uint32_t stored = rf->header.keys[KEY_INDEX].size, left_stored, left;
    if (!stored)
        return 1;
    res_seek(rf->main, (LONG)rf->header.keys[KEY_INDEX].offset);
    rf->n_static = stored / 10;
    left_stored = rf->n_static;
    rf->n_index = rf->header.n_dynamic + left_stored;
    left = rf->n_index;
    for (int b = 0; left; b++) {
        uint32_t n = left < RES_BLOCK ? left : RES_BLOCK;
        rf->index[b] = winmalloc(n * 10, 1);
        if (!rf->index[b])
            return 0;
        left -= n;
        if (left_stored) {
            n = left_stored < RES_BLOCK ? left_stored : RES_BLOCK;
            if (!res_read(rf->main, rf->index[b], (int)n * 10))
                return 0;
            left_stored -= n;
        }
    }
    return 1;
}

int __stdcall res_load_entries(ResFile *rf)
{
    uint32_t n;
    if (!rf->header.keys[KEY_ENTRIES].size)
        return 1;
    rf->entries = ResLoadKeyResource(rf, KEY_ENTRIES, &n);
    if (!rf->entries)
        return 0;
    rf->n_entries = n / 0x52;
    return 1;
}

int __stdcall res_load_names(ResFile *rf)
{
    uint32_t n;
    if (!rf->header.keys[KEY_NAMES].size)
        return 1;
    rf->names = ResLoadKeyResource(rf, KEY_NAMES, &n);
    if (!rf->names)
        return 0;
    rf->n_names = n / 0x2c;
    return 1;
}

int __stdcall res_load_variables(ResFile *rf)
{
    uint32_t n;
    if (!rf->header.keys[KEY_VARIABLES].size)
        return 1;
    rf->variables = ResLoadKeyResource(rf, KEY_VARIABLES, &n);
    if (!rf->variables)
        return 0;
    rf->n_variables = n >> 3;
    return 1;
}

int __stdcall res_load_const_strings(ResFile *rf)
{
    if (!rf->header.keys[KEY_CONSTSTR].size)
        return 1;
    rf->const_strings = (char *)ResLoadKeyResource(rf, KEY_CONSTSTR, NULL);
    return rf->const_strings != NULL;
}

/* The script area is stored as one block and kept as separate buffers. */
int __stdcall res_load_scripts(ResFile *rf)
{
    uint32_t start = 0;
    if (!rf->header.keys[KEY_SCRIPTS].size)
        return 1;
    res_seek(rf->main, (LONG)rf->header.keys[KEY_SCRIPTS].offset);
    for (uint32_t i = 0; i < rf->header.n_scripts; i++) {
        uint32_t n = rf->header.script_end[i] - start;
        start = rf->header.script_end[i];
        rf->scripts[i] = winmalloc(n, 0);
        if (!rf->scripts[i] || !res_read(rf->main, rf->scripts[i], (int)n))
            return 0;
    }
    return 1;
}

/* Data files: path without extension + ".000", ".001", ... for every number used. */
int __stdcall res_open_data_files(ResFile *rf, LPCSTR path)
{
    char name[260];
    char *dot;
    lstrcpyA(name, path);
    dot = NULL;
    for (char *p = name; *p; p++)
        if (*p == '.')
            dot = p;
    if (!dot)
        return 0;
    *dot = 0;
    lstrcpyA(rf->base, name);
    for (uint32_t i = 0; i < rf->n_index; i++) {
        ResEntry *e = res_entry(rf, i);
        if (e->type && e->type < RTYPE_MOVIE && !rf->data[e->file]) {
            HMMIO f;
            wsprintfA(dot, ".%03i", e->file);
            f = res_open(name, MMIO_READ);
            if (!f)
                return 0;
            rf->data[e->file] = f;
        }
    }
    return 1;
}

ResFile *__stdcall ResOpenFile(LPSTR path, uint32_t flags)
{
    ResFile *rf = LocalAlloc(LPTR, sizeof *rf);
    if (!rf)
        return NULL;
    rf->main = res_open(path, flags);
    if (!rf->main) {
        LocalFree(rf);
        return NULL;
    }
    if (!res_read_header(rf->main, &rf->header))
        return (ResFile *)(uintptr_t)ResCloseFile(rf);
    rf->decomp = DecompressBitmapInit();
    if (!rf->decomp || !res_load_index(rf) || !res_load_entries(rf) || !res_load_names(rf) ||
        !res_load_variables(rf) || !res_load_const_strings(rf) || !res_load_scripts(rf) ||
        !res_open_data_files(rf, path))
        return (ResFile *)(uintptr_t)ResCloseFile(rf);
    return rf;
}

void __stdcall res_close_data_files(ResFile *rf)
{
    for (int i = 0; i < RES_MAX_FILES; i++) {
        if (rf->data[i]) {
            mmioClose(rf->data[i], 0);
            rf->data[i] = NULL;
            if (rf->cached[i])
                res_uncache(rf, i);
        }
    }
}

int __stdcall ResCloseFile(ResFile *rf)
{
    if (!rf)
        return 0;
    if (rf->decomp)
        DecompressBitmapExit(rf->decomp);
    if (rf->dirty)
        res_write_header(rf);
    res_close_data_files(rf);
    if (rf->main)
        mmioClose(rf->main, 0);
    if (rf->unknown_29c)
        winfree(rf->unknown_29c);
    if (rf->entries)
        winfree(rf->entries);
    for (int i = 0; i < 11; i++)
        if (rf->index[i])
            winfree(rf->index[i]);
    for (int i = 0; i < RES_MAX_SCRIPTS; i++)
        if (rf->scripts[i])
            winfree(rf->scripts[i]);
    if (rf->names)
        winfree(rf->names);
    if (rf->const_strings)
        winfree(rf->const_strings);
    if (rf->variables)
        winfree(rf->variables);
    LocalFree(rf);
    return 0;
}

/* ---- loading resources ---- */

/* Unpacks a bitmap into a new block: header, then pitch * height pixels. */
uint8_t *__stdcall res_expand_bitmap(ResFile *rf, ResDib *src, uint32_t *size)
{
    uint32_t n = src->pitch * src->height + src->header_size;
    uint8_t *p = winmalloc(n, 0);
    ResDib *dst = (ResDib *)p;
    if (!p)
        return NULL;
    if (!(src->flags & DIB_RLE)) {
        memcpy(p, src, sizeof *src);
        if (!DecompressBitmapData(rf->decomp, src->bits, p + src->header_size,
                                  src->pitch * src->height)) {
            res_notify(rf, 0x590);
            return winfree(p);
        }
        dst->bits = p + src->header_size;
    } else {
        expand_rle_dib(p, src);
        dst->bits = p + src->header_size;
        dst->flags &= ~DIB_RLE;
    }
    if (size)
        *size = n;
    return p;
}

/* A stored bitmap: a 0x18-byte record (width, height, flags, ...), then the packed
 * pixels. Returned as a ResDib; run-length coded sprites stay coded. */
uint8_t *__stdcall res_load_bitmap(ResFile *rf, HMMIO f, uint32_t size, uint32_t *out_size)
{
    const uint32_t extra = 0xc;
    uint8_t *p = winmalloc(size + extra, 0);
    uint32_t rec[6];
    ResDib *dib = (ResDib *)p;
    if (!p)
        return NULL;
    if (!read_data(rf, f, p + extra, (int)size))
        return winfree(p);
    memcpy(rec, p + extra, sizeof rec);
    dib->header_size = sizeof(ResDib);
    dib->width = (int32_t)rec[0];
    dib->pitch = (rec[0] + 3) & ~3u;
    dib->height = rec[1];
    dib->flags = rec[2];
    dib->unknown[0] = rec[3];
    dib->unknown[1] = rec[4];
    dib->unknown[2] = rec[5];
    dib->bits = p + sizeof(ResDib);
    if (!(dib->flags & DIB_RLE)) {
        uint8_t *full = res_expand_bitmap(rf, dib, out_size);
        winfree(p);
        return full;
    }
    if (out_size)
        *out_size = extra + size;
    return p;
}

/* Returns a new block with the resource (free it with winfree), or for the in-memory
 * types a pointer into the script buffers (or the stored pointer). */
uint8_t *__stdcall ResLoadResource(ResFile *rf, uint32_t id, uint32_t *size)
{
    ResEntry *e;
    uint8_t *p;
    if (!rf || id >= rf->n_index)
        return NULL;
    e = res_entry(rf, id);
    if (!RES_IN_MEMORY(e->type)) {
        HMMIO f = rf->data[e->file];
        if (!res_seek(f, (LONG)e->offset))
            return NULL;
        rf->cb_unknown = 0;
        if (e->type == RTYPE_BITMAP)
            return res_load_bitmap(rf, f, e->size, size);
        p = winmalloc(e->size, 0);
        if (!p)
            return NULL;
        if (!read_data(rf, f, p, (int)e->size))
            return winfree(p);
        if (e->type == RTYPE_TABLE) {
            /* a table points at its own rows */
            *(uint8_t **)p = p + 0x3c;
            *(uint32_t *)(p + 8) = 1;
        }
        if (size)
            *size = e->size;
        return p;
    }
    if (size)
        *size = e->size;
    p = (uint8_t *)(uintptr_t)e->offset;
    if (id < rf->n_static) {
        /* stored: an offset into the script area -> the buffer that holds it */
        uint32_t i = 0;
        while (i < rf->header.n_scripts && rf->header.script_end[i] <= (uint32_t)(uintptr_t)p)
            i++;
        if (i)
            p -= rf->header.script_end[i - 1];
        p = rf->scripts[i] + (uintptr_t)p;
    }
    return p;
}

int __stdcall ResReadResourceData(ResFile *rf, uint32_t id, void *buf, int size, int offset)
{
    ResEntry *e;
    HMMIO f;
    if (!rf || id >= rf->n_index)
        return 0;
    e = res_entry(rf, id);
    f = rf->data[e->file];
    if (!res_seek(f, (LONG)e->offset + offset))
        return 0;
    rf->cb_unknown = 0;
    return read_data(rf, f, buf, size) ? size : 0;
}

uint32_t __stdcall ResLoadPalette(ResFile *rf, void *out)
{
    uint32_t n;
    if (!rf || !out || !rf->header.keys[KEY_PALETTE].size)
        return 0;
    if (!res_seek(rf->main, (LONG)rf->header.keys[KEY_PALETTE].offset))
        return 0;
    n = rf->header.keys[KEY_PALETTE].size;
    if (!res_read(rf->main, out, (int)n))
        return 0;
    return n >> 2; /* colors */
}

uint8_t *__stdcall ResExpandBitmapDib(ResFile *rf, ResDib *dib)
{
    return res_expand_bitmap(rf, dib, NULL);
}

/* rec: width, height, flags and three more fields of a stored bitmap record */
uint8_t *__stdcall ResExpandBitmapRec(ResFile *rf, uint32_t *rec, uint8_t *bits)
{
    ResDib dib;
    memset(&dib, 0, sizeof dib);
    dib.header_size = sizeof dib;
    dib.width = (int32_t)rec[0];
    dib.pitch = (rec[0] + 3) & ~3u;
    dib.height = rec[1];
    dib.flags = rec[2];
    dib.unknown[0] = rec[3];
    dib.unknown[1] = rec[4];
    dib.unknown[2] = rec[5];
    dib.bits = bits;
    return res_expand_bitmap(rf, &dib, NULL);
}

/* ---- dynamic resources (made at run time) ---- */

static uint32_t free_slot(ResFile *rf, ResEntry **slot)
{
    uint32_t i = rf->n_static;
    while (i < rf->n_index && (*slot = res_entry(rf, i))->type)
        i++;
    return i;
}

/* A sprite copy starts at frame 0, unmoved. */
void __stdcall res_reset_sprite(uint32_t *s)
{
    s[5] = 0;
    s[4] = 0;
    s[1] = 0;
    s[0] = 0;
    s[3] = s[2];
    ((uint8_t *)s)[0x22] = 0;
    ((uint8_t *)s)[0x20] = 0;
}

/* Copies a resource into a free dynamic slot; returns the new id (0 = failed). */
uint32_t __stdcall ResCopyResource(ResFile *rf, uint32_t id)
{
    ResEntry *src, *dst = NULL;
    uint32_t slot;
    uint8_t type;
    if (!rf || id >= rf->n_index)
        return 0;
    src = res_entry(rf, id);
    type = src->type;
    if (!type)
        return 0;
    slot = free_slot(rf, &dst);
    if (slot >= rf->n_index)
        return 0;
    if (!RES_IN_MEMORY(type)) {
        memcpy(dst, src, 10);
    } else {
        uint32_t n = src->size;
        uint8_t *copy = winmalloc(n, 0), *from;
        if (!copy)
            return 0;
        from = ResLoadResource(rf, id, NULL);
        if (!from) {
            winfree(copy);
            return 0;
        }
        CopyHugeBytes(copy, from, n);
        dst->offset = (uint32_t)(uintptr_t)copy;
        dst->size = n;
        dst->type = type;
        if (type == RTYPE_SPRITE)
            res_reset_sprite((uint32_t *)copy);
    }
    return slot;
}

/* Puts data (a pointer) in a free dynamic slot as a resource of the given type. */
uint32_t __stdcall ResDynamicResource(ResFile *rf, uint8_t type, uint32_t data)
{
    ResEntry *e = NULL;
    uint32_t slot = free_slot(rf, &e);
    if (slot >= rf->n_index)
        return 0;
    e->type = type;
    e->offset = data; /* original: the size is left as it was */
    return slot;
}

int __stdcall ResDeleteResource(ResFile *rf, uint32_t id)
{
    ResEntry *e;
    if (!rf || id >= rf->n_index || id < rf->n_static)
        return 0;
    e = res_entry(rf, id);
    if (RES_IN_MEMORY(e->type))
        winfree((void *)(uintptr_t)e->offset);
    memset(e, 0, 10);
    return 1;
}

/* ---- extracting a file resource ---- */

/* Packed data, unpacked with the bitmap decompressor */
int __stdcall res_extract_packed(void *decomp, HMMIO in, HMMIO out, uint32_t packed, uint32_t size)
{
    uint8_t *src = winmalloc(packed, 0), *dst;
    int ok;
    if (!src)
        return 0;
    dst = winmalloc(size, 0);
    ok = dst && res_read(in, src, (int)packed) && DecompressBitmapData(decomp, src, dst, size) &&
         mmioWrite(out, (HPSTR)dst, (LONG)size) == (LONG)size;
    if (dst)
        winfree(dst);
    winfree(src);
    return ok;
}

/* A stored file (types 1-12): a 6-byte head (u32 unpacked size, u8 packed flag, u8),
 * then the data. Writes it to out. */
int __stdcall ResExtractFile(ResFile *rf, uint32_t id, HMMIO out)
{
    ResEntry *e;
    HMMIO f;
    uint8_t head[6];
    uint32_t unpacked, left;
    LONG pos;
    if (!rf || id >= rf->n_index)
        return 0;
    e = res_entry(rf, id);
    if (!e->type || e->type >= RTYPE_MOVIE)
        return 0;
    f = rf->data[e->file];
    pos = (LONG)e->offset;
    /* original: reads the head through the main file's handle, not the data file's */
    if (!res_seek(rf->main, pos) || !res_read(rf->main, head, 6))
        return 0;
    memcpy(&unpacked, head, 4);
    left = ResGetSize(rf, id) - 6;
    pos += 6;
    if (head[4] == 0) {
        uint8_t *buf = winmalloc(64000, 0);
        int ok = 1;
        if (!buf)
            return 0;
        while (ok && (int32_t)left > 0) {
            uint32_t n = (int32_t)left > 63999 ? 64000 : left;
            ok = res_seek(f, pos) && res_read(f, buf, (int)n) &&
                 mmioWrite(out, (HPSTR)buf, (LONG)n) == (LONG)n;
            if (ok) {
                pos += (LONG)n;
                left -= n;
                if (rf->callback)
                    rf->callback();
            }
        }
        winfree(buf);
        return ok;
    }
    return res_extract_packed(rf->decomp, f, out, left, unpacked);
}

/* ---- information ---- */

uint32_t __stdcall ResGetTitle(ResFile *rf, void *out, uint32_t size)
{
    /* original: a NULL file is only refused when size < 1 */
    if (!rf && (int32_t)size < 1)
        return 0;
    if (size > 0x50)
        size = 0x50;
    memcpy(out, &rf->header.title_len, size);
    return size;
}

uint32_t __stdcall ResCountResources(ResFile *rf, uint32_t *n_stored)
{
    /* original: subtracts the dynamic count from the total */
    if (n_stored)
        *n_stored = rf->n_index - rf->header.n_dynamic;
    return rf->n_index;
}

uint32_t __stdcall ResCountStrings(ResFile *rf)
{
    return rf->header.n_strings;
}

uint32_t __stdcall ResCountVariables(ResFile *rf)
{
    return rf->header.n_variables;
}

/* cb(name, id, user) for each record of KEY_ENTRIES; stops when cb returns 0 */
int __stdcall ResEnumEntries(ResFile *rf, int(__stdcall *cb)(void *, uint32_t, uint32_t), uint32_t user)
{
    uint8_t *p;
    if (!rf || !cb)
        return 0;
    p = rf->entries;
    for (uint32_t i = 0; i < rf->n_entries; i++, p += 0x52) {
        uint32_t id;
        memcpy(&id, p, 4);
        if (!cb(p + 4, id, user))
            return 0;
    }
    return 1;
}

int __stdcall ResEnumResources(ResFile *rf, int(__stdcall *cb)(uint8_t, uint32_t, uint32_t))
{
    if (!rf || !cb)
        return 0;
    for (uint32_t i = 0; i < rf->n_index; i++) {
        ResEntry *e = res_entry(rf, i);
        if (!cb(e->type, e->offset, e->size))
            break;
    }
    return 1;
}

/* cb(record) with the name of each type 1-26 and, at +0x19, its number */
int __cdecl ResEnumResTypes(ResFile *rf, int(__stdcall *cb)(void *))
{
    if (!rf || !cb)
        return 0;
    for (uint32_t t = 1; t < 0x1b; t++) {
        lstrcpyA(g_type_name, TYPE_NAMES[t]);
        memcpy(g_type_name + 0x19, &t, 4);
        if (!cb(g_type_name))
            break;
    }
    return 1;
}

int __stdcall ResEnumVariables(ResFile *rf, int(__stdcall *cb)(uint32_t, uint32_t))
{
    uint32_t *v;
    if (!rf || !cb)
        return 0;
    if (!rf->n_variables)
        return 1;
    v = (uint32_t *)rf->variables;
    for (uint32_t i = 0; i < rf->n_variables && cb(v[0], v[1]); i++)
        v += 2;
    return 1;
}

uint8_t __stdcall ResGetBuildType(ResFile *rf)
{
    return rf ? rf->header.build_type : 0;
}

char *__stdcall ResGetConstStr(ResFile *rf, uint32_t offset)
{
    if (!rf || !rf->const_strings || offset >= rf->header.keys[KEY_CONSTSTR].size)
        return NULL;
    return rf->const_strings + offset;
}

uint32_t __stdcall res_get_entry_id(ResFile *rf)
{
    return rf ? rf->header.entry_id : 0;
}

/* The id of a named resource; 0 for "", -1 if unknown */
uint32_t __stdcall ResGetIndex(ResFile *rf, LPCSTR name)
{
    int len;
    uint8_t *p;
    if (!rf || !rf->n_names || !name)
        return 0xffffffff;
    len = lstrlenA(name);
    if (!len)
        return 0;
    p = rf->names;
    for (uint32_t i = 0; i < rf->n_names; i++, p += 0x2c) {
        if (lstrlenA((char *)p + 4) == len && !lstrcmpA(name, (char *)p + 4)) {
            uint32_t id;
            memcpy(&id, p, 4);
            return id;
        }
    }
    return 0xffffffff;
}

uint32_t __stdcall ResGetMaxFadeColors(ResFile *rf)
{
    return rf ? rf->header.max_fade_colors : 0;
}

uint32_t __stdcall ResGetMaxScrMsg(ResFile *rf)
{
    if (!rf)
        return 0;
    return rf->header.max_scr_msg ? rf->header.max_scr_msg : 0x10c;
}

uint32_t __stdcall ResGetMaxTransColors(ResFile *rf)
{
    return rf ? rf->header.max_trans_colors : 0;
}

uint32_t __stdcall res_get_offset(ResFile *rf, uint32_t id)
{
    if (!rf || id >= rf->n_index)
        return 0;
    return res_entry(rf, id)->offset;
}

uint32_t __stdcall ResGetSize(ResFile *rf, uint32_t id)
{
    if (!rf || id >= rf->n_index)
        return 0;
    return res_entry(rf, id)->size;
}

uint8_t __stdcall ResGetType(ResFile *rf, uint32_t id)
{
    if (!rf || id >= rf->n_index)
        return 0;
    return res_entry(rf, id)->type;
}

int __stdcall res_is_dynamic(ResFile *rf, uint32_t id)
{
    return rf->n_static <= id;
}

void __stdcall ResSetNotify(ResFile *rf, HWND wnd)
{
    rf->notify = wnd;
}

void *__stdcall ResSetCallBack(ResFile *rf, void(__stdcall *cb)(void), int32_t every)
{
    void *old = (void *)rf->callback;
    rf->callback = cb;
    rf->cb_every = every;
    rf->cb_done = 0;
    return old;
}

/* ---- writing (the script compiler and editors) ---- */

/* Sets the checksum and writes the header at the start of the file. */
int __stdcall res_write_header(ResFile *rf)
{
    uint8_t sum = 0;
    LONG pos;
    int ok;
    rf->dirty = 0;
    rf->header.checksum = 0;
    for (uint32_t i = 0; i < RES_HEADER_SIZE; i++)
        sum += ((uint8_t *)&rf->header)[i];
    rf->header.checksum = (uint8_t)-sum;
    pos = res_tell(rf->main);
    mmioSeek(rf->main, 0, SEEK_SET);
    ok = res_write(rf->main, &rf->header, RES_HEADER_SIZE);
    mmioSeek(rf->main, pos, SEEK_SET);
    return ok;
}

ResFile *__stdcall ResCreateFile(LPSTR path)
{
    ResFile *rf = LocalAlloc(LPTR, sizeof *rf);
    if (!rf)
        return NULL;
    rf->main = mmioOpenA(path, NULL, MMIO_CREATE | MMIO_ALLOCBUF | MMIO_READWRITE);
    if (!rf->main) {
        LocalFree(rf);
        return NULL;
    }
    rf->header.magic = RES_MAGIC;
    rf->header.format = 0x3233;
    rf->header.size = RES_HEADER_SIZE;
    rf->header.version = RES_VERSION;
    if (!res_write(rf->main, &rf->header, RES_HEADER_SIZE))
        return (ResFile *)(uintptr_t)ResCloseFile(rf);
    return rf;
}

void __stdcall ResGetHeader(ResFile *rf, ResHeader *out)
{
    memcpy(out, &rf->header, RES_HEADER_SIZE);
}

void __stdcall ResWriteHeader(ResFile *rf, const ResHeader *in)
{
    memcpy(&rf->header, in, RES_HEADER_SIZE);
    res_write_header(rf);
    res_write_header(rf); /* original: twice */
}

void __stdcall ResSetBuildType(ResFile *rf, uint8_t type)
{
    rf->dirty = 1;
    rf->header.build_type = type;
}

void __stdcall ResSetEntryId(ResFile *rf, uint32_t id)
{
    rf->dirty = 1;
    rf->header.entry_id = id;
}

void __stdcall ResSetMaxScrMsg(ResFile *rf, uint32_t n)
{
    rf->header.max_scr_msg = n;
    rf->dirty = 1;
}

void __stdcall ResSetStringCount(ResFile *rf, uint32_t n)
{
    rf->header.n_strings = n;
    rf->dirty = 1;
}

/* The title block holds "title\0subtitle\0", its length in the first byte. */
void __stdcall ResSetTitle(ResFile *rf, LPCSTR title, LPCSTR subtitle)
{
    int a, b;
    rf->dirty = 1;
    memset(&rf->header.title_len, 0, 0x50);
    a = lstrlenA(title) + 1;
    if (a > 0x50)
        a = 0x50;
    b = lstrlenA(subtitle) + 1;
    if (a + b > 0x50)
        b = 0x50 - a;
    lstrcpynA(rf->header.title + a, subtitle, b);
    lstrcpynA(rf->header.title, title, a);
    rf->header.title_len = (uint8_t)(a + b);
}

void __stdcall ResSetVariableCount(ResFile *rf, uint32_t n)
{
    rf->header.n_variables = n;
    rf->dirty = 1;
}

int __stdcall ResWriteIndexBufs(ResFile *rf, ResEntry **blocks, uint32_t n, uint32_t n_dynamic)
{
    rf->dirty = 1;
    rf->header.n_dynamic = n_dynamic;
    rf->header.keys[KEY_INDEX].size = n * 10;
    rf->header.keys[KEY_INDEX].offset = n ? (uint32_t)res_tell(rf->main) : 0;
    while (n) {
        uint32_t k = n < RES_BLOCK ? n : RES_BLOCK;
        n -= k;
        if (!res_write(rf->main, *blocks++, (int)k * 10))
            return 0;
    }
    return 1;
}

int __stdcall ResWriteKeyResource(ResFile *rf, uint32_t key, const void *data, uint32_t size)
{
    if (!rf || key >= RES_KEYS)
        return 0;
    rf->dirty = 1;
    rf->header.keys[key].size = size;
    if (!size) {
        rf->header.keys[key].offset = 0;
        return 1;
    }
    rf->header.keys[key].offset = (uint32_t)res_tell(rf->main);
    return res_write(rf->main, data, (int)size);
}

void __stdcall ResWritePalette(ResFile *rf, const void *data, uint32_t n_colors, uint32_t trans,
                               uint32_t fade)
{
    rf->header.max_trans_colors = trans;
    rf->header.max_fade_colors = fade;
    ResWriteKeyResource(rf, KEY_PALETTE, data, n_colors << 2);
}

void __stdcall ResWriteScriptBuf(ResFile *rf, const void *data, uint32_t size,
                                 const uint32_t *script_end, uint32_t n)
{
    rf->header.n_scripts = n;
    for (uint32_t i = 0; i < n; i++)
        rf->header.script_end[i] = script_end[i];
    ResWriteKeyResource(rf, KEY_SCRIPTS, data, size);
}

/* ---- streams for the audio code ---- */

void __stdcall res_stream_free(HLOCAL s)
{
    LocalFree(s);
}

int __stdcall res_stream_read(ResStream *s, void *buf, int n)
{
    if (mmioSeek(s->file, (LONG)s->pos, SEEK_SET) != (LONG)s->pos || mmioRead(s->file, buf, n) != n)
        return 0;
    s->pos += (uint32_t)n;
    return n;
}

uint32_t __stdcall res_stream_rewind(ResStream *s)
{
    s->pos = s->start;
    return s->pos;
}

ResStream *__stdcall res_stream_close(HLOCAL s)
{
    if (s)
        LocalFree(s);
    return NULL;
}

/* Opens resource id for streaming; its first n bytes go to head. */
ResStream *__stdcall res_stream_open(ResFile *rf, uint32_t id, void *head, int n)
{
    ResStream *s = LocalAlloc(LPTR, sizeof *s);
    ResEntry *e;
    if (!s)
        return NULL;
    e = res_entry(rf, id);
    s->file = rf->data[e->file];
    if (!s->file || mmioSeek(s->file, (LONG)e->offset, SEEK_SET) != (LONG)e->offset ||
        mmioRead(s->file, head, n) != n)
        return res_stream_close(s);
    s->pos = e->offset + (uint32_t)n;
    s->start = s->pos;
    return s;
}

/* What the audio code gets: three functions and the stream */
typedef struct ResStreamFuncs {
    int(__stdcall *read)(ResStream *, void *, int);
    uint32_t(__stdcall *rewind)(ResStream *);
    void(__stdcall *free)(HLOCAL);
    uint32_t unused[2];
    ResStream *stream;
} ResStreamFuncs;

void *__stdcall ResCreateMidiEvent(ResFile *rf, uint32_t a, uint32_t id)
{
    uint8_t head[8];
    ResStreamFuncs f;
    f.stream = res_stream_open(rf, id, head, sizeof head);
    if (!f.stream)
        return NULL;
    f.read = res_stream_read;
    f.rewind = res_stream_rewind;
    f.free = res_stream_free;
    f.unused[0] = f.unused[1] = 0;
    return AudioCreateMidi(a, &f, head);
}

uint32_t __stdcall ResCreateWaveEvent(ResFile *rf, uint32_t a, uint32_t id,
                                      uint32_t(__stdcall *create)(uint32_t, void *, void *))
{
    uint8_t head[0x20];
    ResStreamFuncs f;
    f.stream = res_stream_open(rf, id, head, sizeof head);
    if (!f.stream)
        return 0;
    f.read = res_stream_read;
    f.rewind = res_stream_rewind;
    f.free = res_stream_free;
    f.unused[0] = f.unused[1] = 0;
    return create(a, &f, head);
}

/* ---- library ---- */

void __stdcall ResExitLibrary(uint32_t unused)
{
    (void)unused;
}

void *__stdcall ResInitLibrary(HMODULE module)
{
    char path[256], file[256];
    g_res_module = module;
    GetModuleFileNameA(module, path, sizeof path);
    GetPathAndFile(path, g_res_dir, file);
    res_delete_cached_files();
    return &g_res_module;
}

/* ---- data files copied to the hard disk (when playing from CD) ----
 * Copies are listed in dominion.ini [resource], so that copies left behind by a crash
 * are deleted at the next start. */

void __stdcall res_mark_cached(LPCSTR path, int cached)
{
    WritePrivateProfileStringA(INI_SECTION, path, cached ? "delete" : NULL, INI);
}

void __cdecl res_delete_cached_files(void)
{
    char *list = winmalloc(65000, 1), *p;
    DWORD left;
    if (!list)
        return;
    left = GetPrivateProfileStringA(INI_SECTION, NULL, "", list, 65000, INI);
    p = list;
    while (left && *p) {
        int n;
        res_mark_cached(p, 0);
        if (FileExist(p))
            mmioOpenA(p, NULL, MMIO_DELETE);
        n = lstrlenA(p);
        left -= (DWORD)n;
        if (left)
            p += n + 1;
    }
    winfree(list);
}

/* The copy of data file "file" in the engine's folder */
void __stdcall res_cache_path(ResFile *rf, int file, char *out)
{
    char name[256], dir[256], base[256];
    wsprintfA(name, "%s.%03i", rf->base, file);
    GetPathAndFile(name, dir, base);
    MakeFullPath(out, g_res_dir, base);
}

void __stdcall res_uncache(ResFile *rf, int file)
{
    char path[256];
    if (rf->data[file] || !rf->cached[file])
        return;
    res_cache_path(rf, file, path);
    if (FileExist(path)) {
        mmioOpenA(path, NULL, MMIO_DELETE);
        rf->cached[file] = 0;
        res_mark_cached(path, 0);
    }
}

int __stdcall res_copy_file(ResFile *rf, HMMIO dst, HMMIO src)
{
    uint8_t *buf = winmalloc(0xffff, 0);
    LONG left, pos = 0;
    if (!buf)
        return 0;
    left = mmioSeek(src, 0, SEEK_END);
    while (left) {
        LONG n = left > 0xfffe ? 0xffff : left;
        mmioSeek(src, pos, SEEK_SET);
        mmioSeek(dst, pos, SEEK_SET);
        if (mmioRead(src, (HPSTR)buf, n) != n || mmioWrite(dst, (HPSTR)buf, n) != n)
            break;
        pos += n;
        left -= n;
        if (left && rf->callback)
            rf->callback();
    }
    winfree(buf);
    return left == 0;
}

/* 0x10069c2f: copies a data file to the local cache. The original takes a third
 * argument it never uses (the script built-in passes one). */
void __stdcall res_cache_file(ResFile *rf, int file, uint32_t unused)
{
    (void)unused;
    char path[264];
    HMMIO f;
    if (!rf->data[file] || rf->cached[file])
        return;
    res_cache_path(rf, file, path);
    if (FileExist(path))
        return;
    f = mmioOpenA(path, NULL, MMIO_CREATE | MMIO_ALLOCBUF | MMIO_READWRITE);
    if (!f)
        return;
    if (!res_copy_file(rf, f, rf->data[file])) {
        mmioClose(f, 0);
        mmioOpenA(path, NULL, MMIO_DELETE);
        return;
    }
    mmioClose(f, 0);
    f = mmioOpenA(path, NULL, MMIO_ALLOCBUF);
    mmioClose(rf->data[file], 0);
    rf->data[file] = f;
    rf->cached[file] = 1;
    res_mark_cached(path, 1);
}

void __stdcall res_drop_cache(ResFile *rf, int file)
{
    char path[264];
    if (!rf->data[file] || !rf->cached[file])
        return;
    res_cache_path(rf, file, path);
    if (!FileExist(path))
        return;
    mmioClose(rf->data[file], 0);
    mmioOpenA(path, NULL, MMIO_DELETE);
    res_mark_cached(path, 0);
    wsprintfA(path, "%s.%03i", rf->base, file);
    rf->data[file] = mmioOpenA(path, NULL, MMIO_ALLOCBUF);
    rf->cached[file] = 0;
}
