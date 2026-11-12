/* Resource files, 0x10069f90-0x1006a588: loading many bitmaps at once (sorted by place
 * in the data files, neighbours read in one go of up to 64,000 bytes). The run-length
 * expander that follows (0x1006a588) is in blit.c. Rewritten from the original. */
#include "orig.h"
#include "res.h"
#include "utils.h"

#define BATCH_READ 64000

static uint8_t *g_buf; /* the read buffer (a global of the original's, 0x1008fa04) */

/* 0x1006a3cf: sorts the entries (and their ids alongside) by data file, then offset */
void __stdcall res_batch_sort(ResEntry *e, uint32_t *ids, uint32_t n)
{
    for (uint32_t i = 1; i < n; i++) {
        if (e[i - 1].file == e[i].file && e[i].offset >= e[i - 1].offset)
            continue;
        uint32_t j = 0;
        while (j < i && e[j].file <= e[i].file && e[j].offset <= e[i].offset)
            j++;
        if (j < i) {
            ResEntry t = e[i];
            uint32_t id = ids[i];
            memmove(e + j + 1, e + j, (i - j) * sizeof *e);
            memmove(ids + j + 1, ids + j, (i - j) * sizeof *ids);
            e[j] = t;
            ids[j] = id;
        }
    }
}

/* 0x1006a1fc: how many entries from `e` lie back to back in one data file within one
 * read; *size gets their total size (the first alone may be larger than a read) */
uint32_t __stdcall res_batch_run(ResFile *rf, const ResEntry *e, uint32_t *size, uint32_t n)
{
    (void)rf;
    const ResEntry first = *e;
    uint32_t total = first.size, count = 1;
    for (;;) {
        e++;
        if (--n == 0 || e->file != first.file || first.offset + total != e->offset)
            break;
        uint32_t t = e->size + total;
        if (t > BATCH_READ)
            break;
        total = t;
        count++;
    }
    *size = total;
    return count;
}

/* 0x1006a2ae: a stored bitmap read into memory, as a ResDib in a new block (expanded
 * unless run-length coded) */
uint8_t *__stdcall res_batch_bitmap(ResFile *rf, const uint32_t *src, const ResEntry *e, uint32_t *size)
{
    const uint32_t extra = 0xc;
    uint8_t *p = winmalloc(e->size + extra, 0);
    ResDib *dib = (ResDib *)p;
    if (!p)
        return NULL;
    uint32_t rec[6];
    memcpy(rec, src, sizeof rec);
    CopyHugeBytes(p + sizeof(ResDib), src + 6, e->size - sizeof rec);
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
        uint8_t *full = res_expand_bitmap(rf, dib, size);
        winfree(p);
        return full;
    }
    if (size)
        *size = e->size + extra;
    return p;
}

/* 0x1006a0b1: reads the sorted entries run by run and hands each bitmap to `loaded`
 * (data, size, type 1, id) */
int __stdcall res_batch_read(ResFile *rf, ResBatchLoaded loaded, const ResEntry *e, const uint32_t *ids, uint32_t n)
{
    while (n) {
        uint32_t size;
        uint32_t k = res_batch_run(rf, e, &size, n);
        if (size > BATCH_READ) {
            uint8_t *b = winrealloc(g_buf, size, 0);
            if (!b)
                return 0;
            g_buf = b;
        }
        HMMIO f = rf->data[e->file];
        if (mmioSeek(f, (LONG)e->offset, SEEK_SET) != (LONG)e->offset)
            return 0;
        if ((uint32_t)mmioRead(f, (HPSTR)g_buf, (LONG)size) != size)
            return 0;
        uint32_t off = 0;
        for (uint32_t i = 0; i < k; i++) {
            uint32_t bsize;
            uint8_t *bm = res_batch_bitmap(rf, (const uint32_t *)(g_buf + off), e, &bsize);
            if (bm)
                loaded((uint32_t *)bm, bsize, 1, *ids);
            off += e->size;
            e++;
            ids++;
            n--;
        }
    }
    return 0;
}

/* 0x10069f90: loads the bitmaps `ids` (sorted in place) and hands each to `loaded` */
int __stdcall res_load_batch(ResFile *rf, ResBatchLoaded loaded, uint32_t *ids, uint32_t n)
{
    ResEntry *e = winmalloc(n * sizeof *e, 0);
    if (!e)
        return 0;
    for (uint32_t i = 0; i < n; i++)
        e[i] = *res_entry(rf, ids[i]);
    if (n > 1)
        res_batch_sort(e, ids, n);
    g_buf = winmalloc(BATCH_READ, 0);
    if (g_buf)
        res_batch_read(rf, loaded, e, ids, n);
    if (g_buf)
        g_buf = winfree(g_buf);
    winfree(e);
    return 1;
}
