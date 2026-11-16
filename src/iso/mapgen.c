/* The game logic, 0x1001a434-0x1001b380: the modifier keys (IsoGetKeyModifiers) and an
 * unfinished random map generator (IsoGenerateMap): a new map is cut into 9x9-cell
 * blocks, blocks on a ring get a numbered object group (objects 0x78-0x7b), and then
 * clusters of objects from a per-terrain table are scattered over free cells. Several
 * of its steps are empty in the original. */
#include "iso_core.h"
#include "mapgen.h"

/* not rewritten yet */
#define IsoCreateMap ORIG(int(__stdcall *)(uint8_t *, int, int, int, int, int, int, uint32_t, int), 0x1001d92f)
#define IsoGetTile ORIG(int32_t(__stdcall *)(uint8_t *, int, int), 0x100219df)
#define IsoAddObject ORIG(int(__stdcall *)(uint8_t *, int, int, int, int, int, int), 0x100aca6e)
#define g_srand ORIG(void(__cdecl *)(uint32_t), 0x100f4d60)
#define g_rand ORIG(int(__cdecl *)(void), 0x100f4d80)

#define ISO_OBJECT_MAP 0x1934 /* per cell: bit 0 taken, bits 16-27 the block's group */
#define ISO_ORIGIN_X 0xd709
#define ISO_ORIGIN_Y 0xd71e

/* a block of the coarse grid */
typedef struct { int32_t kind, unused, group; } Block;

/* per terrain: the map's two tile sets (0x1c bytes each) */
#define g_terrain_tiles(t) ((const uint32_t *)ORIG_PTR(0x10130bf0 + (t) * 0x1c))
/* object groups: per terrain 8 of (first object, count) */
#define g_object_groups(t, g) ((const int32_t *)ORIG_PTR(0x101306d0 + (t) * 0x40 + (g) * 8))
/* cluster sizes: 2 kinds of 8 ints (percent of the map) */
#define g_cluster_kind(k) ((const int32_t *)ORIG_PTR(0x101307d0 + (k) * 0x20))
/* per terrain and density: 5 clusters of (group, second group, kind) */
#define g_clusters(t, d) ((const uint8_t *)ORIG_PTR(0x10130810 + (t) * 0x48 + (d) * 0x12))

/* 0x1001a434: the modifier keys down: 1 Shift (add), 2 Ctrl (force), 4 Alt (order),
 * 8 the right button */
uint8_t __stdcall IsoGetKeyModifiers(uint8_t *iso)
{
    g_iso = iso;
    return (uint8_t)((I(int32_t, 0xd79b) ? 4 : 0) | (I(int32_t, 0xd7a7) ? 8 : 0) | (I(int32_t, 0xd797) ? 2 : 0) |
                     (I(int32_t, 0xd7af) != 0));
}

/* map cell (col, row) to pixels relative to the map origin */
static void cell_pixels(int col, int row, int *x, int *y)
{
    *x = (row & 1) * 0x20 + col * 0x40 - I(int32_t, ISO_ORIGIN_X);
    *y = row * 0x10 - I(int32_t, ISO_ORIGIN_Y);
}

/* 0x1001a4c0: clears the group bits of every cell */
void __stdcall mapgen_clear_groups(int w, int h)
{
    for (int c = 0; c < w; c++)
        for (int r = 0; r < h; r++) {
            int x, y, a, b;
            cell_pixels(c, r, &x, &y);
            int32_t cell = IsoVTilePos(g_iso, &x, &y, &a, &b);
            I(uint32_t *, ISO_OBJECT_MAP)[cell] &= 0xf000ffff;
        }
}

/* 0x1001a7c5: prints the block grid to the debugger */
static void __stdcall mapgen_dump(int bw, int bh, Block *grid)
{
    char line[8];
    for (int r = 0; r < bh; r++) {
        for (int c = 0; c < bw; c++) {
            wsprintfA(line, (const char *)ORIG_PTR(0x10130c8c), grid[r * bw + c].kind, grid[r * bw + c].group);
            OutputDebugStringA(line);
        }
        OutputDebugStringA((const char *)ORIG_PTR(0x10130c94));
    }
    OutputDebugStringA((const char *)ORIG_PTR(0x10130c98));
}

/* 0x1001a964: marks block (x, y) (kind 2) and the one left of it (kind 1) with group
 * `group`, where still free */
void __stdcall mapgen_mark(int bw, int bh, int x, int y, int group, void *grid_)
{
    Block *grid = grid_;
    (void)bh;
    if (!grid[bw * y + x].kind) {
        grid[bw * y + x].kind = 2;
        grid[bw * y + x].group = group;
    }
    x--;
    if (!grid[bw * y + x].kind) {
        grid[bw * y + x].kind = 1;
        grid[bw * y + x].group = group;
    }
}

/* 0x1001a875: `count` groups on a ellipse through the blocks, starting at 45 degrees */
int __stdcall mapgen_ring(int w, int h, int bw, int bh, int count, void *grid_)
{
    Block *grid = grid_;
    (void)w, (void)h;
    int rx = bw / 2, ry = bh / 2;
    for (int i = 0; i < count; i++) {
        int angle = 0x1c2 + i * 0xe10 / count;
        if (angle >= 0xe10)
            angle -= 0xe10;
        int x = rx + Cosine(angle) * rx / 10000;
        int y = ry + Sine(angle) * ry / 10000;
        mapgen_mark(bw, bh, x, y, i + 1, grid);
    }
    mapgen_dump(bw, bh, grid);
    return 1;
}

/* 0x1001ad4c: block-relative (x, y) to map cell (col, row); whether it is on the map */
int __cdecl mapgen_cell(int x, int y, int *col, int *row, int w, int h)
{
    *col = y > x ? (x - y - 1) / 2 : (x - y) / 2;
    *row = y + x;
    return x < w && y < h;
}

/* 0x1001abfb: fills a 9x9 block with the group's object and marks its cells */
int __stdcall mapgen_fill_block(int terrain, int kind, uint32_t group, int x0, int y0, int w, int h)
{
    (void)terrain, (void)kind;
    for (int x = x0; x < x0 + 9; x++)
        for (int y = y0; y < y0 + 9; y++) {
            int col, row;
            if (!mapgen_cell(x, y, &col, &row, w, h))
                continue;
            int px, py, a, b;
            cell_pixels(col, row, &px, &py);
            IsoAddObject(g_iso, (int)(group - 1) % 4 + 0x78, px, py, 1, -1, -1);
            int32_t cell = IsoVTilePos(g_iso, &px, &py, &a, &b);
            uint32_t *m = &I(uint32_t *, ISO_OBJECT_MAP)[cell];
            *m = (*m & 0xf000ffff) | (group & 0xfff) << 16;
        }
    return 1;
}

/* 0x1001aa2f: fills the marked blocks (kinds 3-5 had a step of their own, empty) */
int __stdcall mapgen_fill(int w, int h, int bw, int bh, int terrain, int density, void *grid_)
{
    Block *grid = grid_;
    (void)density;
    int ox = (bw * 9 - w) / 2, oy = (bh * 9 - h) / 2;
    for (int r = 0; r < bh; r++)
        for (int c = 0; c < bw; c++) {
            int x = c * 9 - ox, y = r * 9 - oy;
            if (x < 0)
                x = 0;
            if (y < 0)
                y = 0;
            Block *b = &grid[bw * r + c];
            if (b->kind == 1 || b->kind == 2)
                mapgen_fill_block(terrain, b->kind, (uint32_t)b->group, x, y, w, h);
        }
    mapgen_dump(bw, bh, grid);
    return 1;
}

/* 0x1001b2b1: the cell at pixel (x, y): taken, and its group */
void __cdecl mapgen_cell_info(int x, int y, uint32_t *taken, uint32_t *group)
{
    int a[3], b;
    int32_t cell = IsoVTilePos(g_iso, &x, &y, a, &b);
    uint32_t m = I(uint32_t *, ISO_OBJECT_MAP)[cell];
    *taken = m & 1;
    *group = (m >> 16) & 0xfff;
}

static int rand_range(int lo, int hi) { return lo + g_rand() % (hi - lo + 1); }

/* 0x1001b062: scatters clusters of objects from groups g1 and g2 over free cells */
int __stdcall mapgen_clusters(int terrain, int g1, int g2, int n_min, int n_max, int w_min, int w_max, int h_min,
                              int h_max, int d_min, int d_max)
{
    int n1 = g_object_groups(terrain, g1)[1];
    int total = g_object_groups(terrain, g2)[1] + n1;
    int n = rand_range(n_min, n_max);
    for (int i = 0; i < n; i++) {
        int cw = rand_range(w_min, w_max);
        int ch = rand_range(h_min, h_max);
        int d = rand_range(d_min, d_max);
        int count = d * ch * cw / 100;
        int cx = g_rand() % I(int32_t, ISO_MAP_W);
        int cy = g_rand() % I(int32_t, ISO_MAP_H);
        for (int j = 0; j < count; j++) {
            int dx = g_rand() % (cw * 2 + 1) - cw;
            int dy = g_rand() % (ch * 2 + 1) - ch;
            int col = dx + cx, row = dy + cy;
            if (IsoGetTile(g_iso, col, row) < 0)
                continue;
            int px, py;
            uint32_t taken, group;
            cell_pixels(col, row, &px, &py);
            mapgen_cell_info(px, py, &taken, &group);
            if (taken || group)
                continue;
            int k = g_rand() % total;
            int obj = k < n1 ? g_object_groups(terrain, g1)[0] + k : g_object_groups(terrain, g2)[0] + k - n1;
            IsoAddObject(g_iso, obj, px, py, 1, -1, -1);
        }
    }
    return 1;
}

/* 0x1001ae2c: the terrain's clusters for the density */
int __stdcall mapgen_add_clusters(int w, int h, int terrain, int density)
{
    int area = h * w, ok = 1, i;
    for (i = 0; i < 5 && ok; i++) {
        const uint8_t *c = g_clusters(terrain, density) + i * 3;
        if (!c[0] || c[2] >= 2)
            continue;
        const int32_t *k = g_cluster_kind(c[2]);
        int n_min = (int)((uint32_t)(k[0] * area) / 100), n_max = (int)((uint32_t)(k[1] * area) / 100);
        int w_min = k[2] * w / 100, w_max = k[3] * w / 100;
        int h_min = k[4] * h / 100, h_max = k[5] * h / 100;
        if (n_max < 10)
            n_max = 10;
        if (w_max < 2)
            w_max = 2;
        if (h_max < 2)
            h_max = 2;
        ok = mapgen_clusters(terrain, c[0], c[1], n_min, n_max, w_min, w_max, h_min, h_max, k[6], k[7]);
    }
    DebugStr((const char *)ORIG_PTR(0x10130c9c), terrain, density, i);
    return ok;
}

/* 0x1001a585: makes a new w x h map of `terrain` and fills it (seed `seed`) */
int __stdcall mapgen_generate(int w, int h, int seed, int terrain, int density, int ring, int unused)
{
    (void)unused;
    if (IsoCreateMap(g_iso, w, h, terrain, 1, 1, 1, g_terrain_tiles(terrain)[0], (int)g_terrain_tiles(terrain)[1])) {
        DebugStr((const char *)ORIG_PTR(0x10130c60));
        return 0;
    }
    g_srand((uint32_t)((terrain + w + h) * 100000 + seed));
    int bw = w / 9 + 1, bh = h / 9 + 1;
    Block *grid = (Block *)winmalloc((uint32_t)(bh * bw * 0xc), 0);
    if (!grid)
        return 0;
    for (int i = 0; i < bh * bw; i++) {
        grid[i].kind = 0;
        grid[i].group = 0;
    }
    if (!mapgen_ring(w, h, bw, bh, ring, grid))
        return 0;
    /* 0x1001aa18 and 0x1001ae15, steps between, only return 1 */
    if (!mapgen_fill(w, h, bw, bh, terrain, density, grid))
        return 0;
    if (!mapgen_add_clusters(w, h, terrain, density))
        return 0;
    mapgen_clear_groups(w, h);
    if (grid)
        winfree(grid);
    return 1;
}

/* 0x1001b334 */
void __cdecl IsoGenerateMap(uint8_t *iso, int w, int h, int seed, int terrain, int density, int ring, int unused)
{
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x10130ccc));
    mapgen_generate(w, h, seed, terrain, density, ring, unused);
}

/* 0x1001b315 */
void __cdecl IsoAPIGetRandomMapInfo(uint8_t *iso)
{
    g_iso = iso;
}
