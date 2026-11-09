/* Smacker.r32 replacement: plays the cutscenes.
 *
 * The original wrapped RAD's SMACKW32.DLL. This one uses our own decoder (smk.c),
 * draws into a DirectDraw primary surface and streams the sound through the game's
 * DirectSound object.
 *
 * How the game uses it (menu script, DOMINION.bin):
 *   SmackerCreateDirectDrawObjects(ctx, 0, 640, 480, 16)  switch to a 640x480x16 screen
 *   SmackerSetupView(ctx, 1, 1, dsound, 0, 0, 0, 1)       DirectDraw output, sound on
 *   h = SmackerOpenAnimation(ctx, string_id)              file name from the string table
 *   SmackerPlayOpenedAnimation(ctx, h, ..., msg, param, loop)
 *       playback then runs from the engine's main loop (SmackerUpdateHook); at the end
 *       message `msg` is posted to the script
 *   SmackerUnload(ctx)                                    Escape: stop now (posts msg too)
 *   SmackerDestroyDirectDrawObjects(ctx)                  back to the game's screen
 *
 * Playback method (Dominion.ini [Dominion] CinePlaybackMethod, default 2):
 *   0 = original size, 1 = doubled, 2 = doubled + smoothed, 3 = doubled + interlaced.
 *
 * Not carried over from the original: the debug keys (+/- change method, F frame
 * counter) of the optional "VideoOutput" window, which the game never creates, and the
 * windowed (non-DirectDraw) mode, which the game never uses. */
#include <stdint.h>
#include <string.h>
#define COBJMACROS
#include <windows.h>
#include <ddraw.h>
#include <dsound.h>
#include <mmsystem.h>

#include "rtlib32.h"
#include "smk.h"

enum { METHOD_1X, METHOD_2X, METHOD_2X_SMOOTH, METHOD_2X_INTERLACE };

typedef struct Movie {
    HANDLE file, mapping;
    const uint8_t *data;
    Smk *smk;
} Movie;

typedef struct AudioStream {
    IDirectSoundBuffer *buf;
    uint32_t size;        /* bytes */
    uint32_t write_pos;   /* next byte we write */
    uint32_t last_play;   /* play cursor at the previous update */
    uint64_t written;     /* bytes written in total */
    uint64_t played;      /* bytes played in total */
    int started;
} AudioStream;

typedef struct Smacker {
    void *app;
    HWND hwnd;
    HDC hdc;
    void *hsl;

    /* SmackerSetupView */
    int setup_done;
    int use_ddraw;
    int use_sound;
    IDirectSound *dsound;
    int method;

    /* SmackerCreateDirectDrawObjects */
    IDirectDraw2 *dd;
    IDirectDrawSurface *primary;
    IDirectDrawPalette *ddpal;
    int own_dd;    /* we created dd (the engine had none) */
    int mode_set;  /* we changed the display mode */
    uint32_t bpp;
    uint32_t prev_w, prev_h, prev_bpp, prev_refresh;

    /* current playback */
    Movie *movie;
    int playing;
    int active;
    int msg, msg_param, loop;
    POINT saved_org;
    DWORD start_ms;
    DWORD paused_at;
    uint32_t shown; /* frames shown since start */
    AudioStream audio;
    uint8_t rgb[256][3];
    uint32_t native[256]; /* palette in the surface's pixel format */
    int shift[3], keep[3];  /* surface pixel format: position and width of R, G, B */

    /* engine hook */
    int hooked;
    RtGameInfo saved_info;
} Smacker;

/* ---- helpers ---- */

static void *mem_alloc(size_t n)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n);
}

static void mem_free(void *p)
{
    if (p)
        HeapFree(GetProcessHeap(), 0, p);
}

static const SmkAlloc smk_alloc = {mem_alloc, mem_free};

static void debug(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    wvsprintfA(buf, fmt, ap);
    va_end(ap);
    OutputDebugStringA(buf);
}

void __stdcall SmackerUpdateHook(Smacker *sm);

static void hook_install(Smacker *sm)
{
    if (sm->hooked)
        return;
    sm->saved_info = *RtGetGameInfo(sm->app);
    RtGameInfo info = {sm, (void(__stdcall *)(void *))SmackerUpdateHook};
    RtSetGameInfo(sm->app, &info);
    sm->hooked = 1;
}

static void hook_remove(Smacker *sm)
{
    if (!sm->hooked)
        return;
    RtSetGameInfo(sm->app, &sm->saved_info);
    sm->hooked = 0;
}

/* ---- files ---- */

static void movie_close(Movie *m)
{
    if (!m)
        return;
    smk_close(m->smk);
    if (m->data)
        UnmapViewOfFile(m->data);
    if (m->mapping)
        CloseHandle(m->mapping);
    if (m->file != INVALID_HANDLE_VALUE)
        CloseHandle(m->file);
    mem_free(m);
}

static Movie *movie_open(const char *path)
{
    Movie *m = mem_alloc(sizeof *m);
    if (!m)
        return NULL;
    m->file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD size = m->file == INVALID_HANDLE_VALUE ? 0 : GetFileSize(m->file, NULL);
    if (size == 0 || size == INVALID_FILE_SIZE)
        goto fail;
    m->mapping = CreateFileMappingA(m->file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!m->mapping)
        goto fail;
    m->data = MapViewOfFile(m->mapping, FILE_MAP_READ, 0, 0, 0);
    if (!m->data)
        goto fail;
    m->smk = smk_open(m->data, size, &smk_alloc);
    if (!m->smk)
        goto fail;
    return m;
fail:
    debug("Smacker: cannot open %s\n", path);
    movie_close(m);
    return NULL;
}

/* ---- sound: a looping DirectSound buffer we keep filled ---- */

static void audio_stop(AudioStream *a)
{
    if (a->buf) {
        IDirectSoundBuffer_Stop(a->buf);
        IDirectSoundBuffer_Release(a->buf);
    }
    memset(a, 0, sizeof *a);
}

static int audio_start(Smacker *sm, const SmkTrack *t)
{
    AudioStream *a = &sm->audio;
    memset(a, 0, sizeof *a);
    if (!sm->use_sound || !sm->dsound || !t->present)
        return 0;
    WAVEFORMATEX wf = {0};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = (WORD)t->channels;
    wf.nSamplesPerSec = t->rate;
    wf.wBitsPerSample = (WORD)t->bits;
    wf.nBlockAlign = (WORD)(t->channels * t->bits / 8);
    wf.nAvgBytesPerSec = t->rate * wf.nBlockAlign;
    a->size = wf.nAvgBytesPerSec * 2; /* 2 seconds */
    a->size -= a->size % wf.nBlockAlign;

    DSBUFFERDESC desc = {0};
    desc.dwSize = sizeof desc;
    desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    desc.dwBufferBytes = a->size;
    desc.lpwfxFormat = &wf;
    if (IDirectSound_CreateSoundBuffer(sm->dsound, &desc, &a->buf, NULL) != DS_OK) {
        a->buf = NULL;
        return 0;
    }
    void *p1, *p2;
    DWORD n1, n2;
    if (IDirectSoundBuffer_Lock(a->buf, 0, a->size, &p1, &n1, &p2, &n2, 0) == DS_OK) {
        memset(p1, t->bits == 8 ? 0x80 : 0, n1);
        IDirectSoundBuffer_Unlock(a->buf, p1, n1, p2, n2);
    }
    return 1;
}

static void audio_fill(AudioStream *a, uint32_t pos, const uint8_t *src, uint32_t n, int silence)
{
    void *p1, *p2;
    DWORD n1, n2;
    if (IDirectSoundBuffer_Lock(a->buf, pos, n, &p1, &n1, &p2, &n2, 0) != DS_OK)
        return;
    if (silence) {
        memset(p1, src[0], n1);
        if (p2)
            memset(p2, src[0], n2);
    } else {
        memcpy(p1, src, n1);
        if (p2)
            memcpy(p2, src + n1, n2);
    }
    IDirectSoundBuffer_Unlock(a->buf, p1, n1, p2, n2);
}

/* Track how far DirectSound has played; blank what it played so an underrun is
 * silent instead of repeating old sound. */
static void audio_update(AudioStream *a, uint8_t silence_byte)
{
    if (!a->buf || !a->started)
        return;
    DWORD play, write;
    if (IDirectSoundBuffer_GetCurrentPosition(a->buf, &play, &write) != DS_OK)
        return;
    uint32_t delta = (play + a->size - a->last_play) % a->size;
    if (delta) {
        audio_fill(a, a->last_play, &silence_byte, delta, 1);
        a->last_play = play;
        a->played += delta;
    }
    if (a->played > a->written) { /* underrun: continue writing at the play position */
        a->written = a->played;
        a->write_pos = play;
    }
}

static void audio_push(AudioStream *a, const uint8_t *pcm, uint32_t n)
{
    if (!a->buf || n == 0)
        return;
    uint32_t space = a->size - (uint32_t)(a->written - a->played) - 64;
    if (n > space) /* should not happen: the buffer holds 2 s */
        n = space;
    if ((int32_t)n <= 0)
        return;
    audio_fill(a, a->write_pos, pcm, n, 0);
    a->write_pos = (a->write_pos + n) % a->size;
    a->written += n;
    if (!a->started) {
        IDirectSoundBuffer_SetCurrentPosition(a->buf, 0);
        IDirectSoundBuffer_Play(a->buf, 0, 0, DSBPLAY_LOOPING);
        a->started = 1;
    }
}

/* ---- picture ---- */

static int mask_shift(DWORD mask)
{
    int s = 0;
    while (mask && !(mask & 1)) {
        mask >>= 1;
        s++;
    }
    return s;
}

static int mask_bits(DWORD mask)
{
    int n = 0;
    for (; mask; mask >>= 1)
        n += mask & 1;
    return n;
}

static void read_format(Smacker *sm, const DDPIXELFORMAT *pf)
{
    DWORD m[3] = {pf->dwRBitMask, pf->dwGBitMask, pf->dwBBitMask};
    for (int i = 0; i < 3; i++) {
        sm->shift[i] = mask_shift(m[i]);
        sm->keep[i] = mask_bits(m[i]);
        if (sm->keep[i] > 8)
            sm->keep[i] = 8;
    }
}

static uint32_t pack_rgb(const Smacker *sm, unsigned r, unsigned g, unsigned b)
{
    return (r >> (8 - sm->keep[0])) << sm->shift[0] | (g >> (8 - sm->keep[1])) << sm->shift[1] |
           (b >> (8 - sm->keep[2])) << sm->shift[2];
}

static void put_pixel(uint8_t *p, uint32_t bpp, uint32_t v)
{
    switch (bpp) {
    case 8: p[0] = (uint8_t)v; break;
    case 16: *(uint16_t *)p = (uint16_t)v; break;
    case 24: p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); break;
    default: *(uint32_t *)p = v; break;
    }
}

static void update_palette(Smacker *sm, const DDPIXELFORMAT *pf)
{
    const uint8_t *pal = sm->movie->smk->palette;
    read_format(sm, pf);
    for (int i = 0; i < 256; i++) {
        sm->rgb[i][0] = pal[i * 3];
        sm->rgb[i][1] = pal[i * 3 + 1];
        sm->rgb[i][2] = pal[i * 3 + 2];
        sm->native[i] = sm->bpp == 8 ? (uint32_t)i : pack_rgb(sm, pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]);
    }
    if (sm->bpp == 8 && sm->ddpal) { /* like the original: leave the 20 Windows colors alone */
        PALETTEENTRY e[236];
        for (int i = 0; i < 236; i++) {
            e[i].peRed = pal[(i + 10) * 3];
            e[i].peGreen = pal[(i + 10) * 3 + 1];
            e[i].peBlue = pal[(i + 10) * 3 + 2];
            e[i].peFlags = 0;
        }
        IDirectDrawPalette_SetEntries(sm->ddpal, 0, 10, 236, e);
    }
}

static int effective_method(const Smacker *sm)
{
    if (sm->bpp == 8 && sm->method == METHOD_2X_SMOOTH)
        return METHOD_2X; /* smoothing needs real colors */
    return sm->method;
}

static uint32_t avg_native(const Smacker *sm, const uint8_t *idx, int n)
{
    unsigned r = 0, g = 0, b = 0;
    for (int i = 0; i < n; i++) {
        r += sm->rgb[idx[i]][0];
        g += sm->rgb[idx[i]][1];
        b += sm->rgb[idx[i]][2];
    }
    return pack_rgb(sm, r / n, g / n, b / n);
}

/* Draw the current frame centered on the locked surface. */
static void draw_frame(Smacker *sm, DDSURFACEDESC *d)
{
    const Smk *v = sm->movie->smk;
    int method = effective_method(sm);
    uint32_t scale = method == METHOD_1X ? 1 : 2;
    uint32_t bpp = sm->bpp, bytes = (bpp + 7) / 8;
    uint32_t out_w = v->width * scale, out_h = v->height * scale;
    if (out_w > d->dwWidth || out_h > d->dwHeight) { /* screen too small: draw 1:1 */
        method = METHOD_1X;
        scale = 1;
        out_w = v->width;
        out_h = v->height;
    }
    uint32_t x0 = out_w < d->dwWidth ? (d->dwWidth - out_w) / 2 : 0;
    uint32_t y0 = out_h < d->dwHeight ? (d->dwHeight - out_h) / 2 : 0;
    uint32_t w = out_w < d->dwWidth ? v->width : d->dwWidth / scale;
    uint32_t h = out_h < d->dwHeight ? v->height : d->dwHeight / scale;
    uint8_t *base = (uint8_t *)d->lpSurface + y0 * d->lPitch + x0 * bytes;

    for (uint32_t y = 0; y < h; y++) {
        const uint8_t *src = v->image + y * v->width;
        const uint8_t *below = y + 1 < v->height ? src + v->width : src;
        uint8_t *row0 = base + y * scale * d->lPitch;
        uint8_t *row1 = row0 + d->lPitch;
        for (uint32_t x = 0; x < w; x++) {
            uint32_t c = sm->native[src[x]];
            if (method == METHOD_1X) {
                put_pixel(row0 + x * bytes, bpp, c);
                continue;
            }
            uint8_t *p0 = row0 + 2 * x * bytes, *p1 = row1 + 2 * x * bytes;
            if (method == METHOD_2X_SMOOTH) {
                uint32_t xr = x + 1 < v->width ? x + 1 : x;
                uint8_t right[2] = {src[x], src[xr]};
                uint8_t down[2] = {src[x], below[x]};
                uint8_t quad[4] = {src[x], src[xr], below[x], below[xr]};
                put_pixel(p0, bpp, c);
                put_pixel(p0 + bytes, bpp, avg_native(sm, right, 2));
                put_pixel(p1, bpp, avg_native(sm, down, 2));
                put_pixel(p1 + bytes, bpp, avg_native(sm, quad, 4));
            } else {
                put_pixel(p0, bpp, c);
                put_pixel(p0 + bytes, bpp, c);
                uint32_t c1 = method == METHOD_2X_INTERLACE ? 0 : c;
                put_pixel(p1, bpp, c1);
                put_pixel(p1 + bytes, bpp, c1);
            }
        }
    }
}

static int lock_primary(Smacker *sm, DDSURFACEDESC *d)
{
    if (!sm->primary)
        return 0;
    for (int tries = 0; tries < 2; tries++) {
        memset(d, 0, sizeof *d);
        d->dwSize = sizeof *d;
        HRESULT hr = IDirectDrawSurface_Lock(sm->primary, NULL, d, DDLOCK_WAIT, NULL);
        if (hr == DD_OK)
            return 1;
        if (hr != DDERR_SURFACELOST || IDirectDrawSurface_Restore(sm->primary) != DD_OK)
            return 0;
    }
    return 0;
}

static void clear_screen(Smacker *sm)
{
    if (sm->primary) {
        DDBLTFX fx = {0};
        fx.dwSize = sizeof fx;
        fx.dwFillColor = 0;
        IDirectDrawSurface_Blt(sm->primary, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
        return;
    }
    RECT rc;
    if (sm->hwnd && GetClientRect(sm->hwnd, &rc)) {
        HDC dc = GetDC(sm->hwnd);
        PatBlt(dc, rc.left, rc.top, rc.right, rc.bottom, BLACKNESS);
        ReleaseDC(sm->hwnd, dc);
    }
}

static void present(Smacker *sm, int palette_changed)
{
    DDSURFACEDESC d;
    if (!lock_primary(sm, &d))
        return;
    if (palette_changed)
        update_palette(sm, &d.ddpfPixelFormat);
    draw_frame(sm, &d);
    IDirectDrawSurface_Unlock(sm->primary, d.lpSurface);
}

/* ---- playback ---- */

static void save_method(const Smacker *sm)
{
    char num[4] = {(char)('0' + sm->method), 0};
    WritePrivateProfileStringA("Dominion", "CinePlaybackMethod", num, "Dominion.ini");
}

/* End of a movie (or Escape): clean up and tell the script. */
static void finish(Smacker *sm)
{
    clear_screen(sm);
    audio_stop(&sm->audio);
    movie_close(sm->movie);
    sm->movie = NULL;
    sm->playing = 0;
    SetViewportOrgEx(sm->hdc, sm->saved_org.x, sm->saved_org.y, NULL);
    if (SpriteRealizePalette2(sm->hsl, 1))
        InvalidateRect(sm->hwnd, NULL, TRUE);
    if (sm->msg) {
        hook_remove(sm);
        /* engine message record: id at +2, a flag at +9, parameter at +10 */
        uint8_t m[0x22] = {0};
        memcpy(m + 2, &sm->msg, 4);
        m[9] = 1;
        memcpy(m + 10, &sm->msg_param, 4);
        RtPostMsg(sm->app, m);
    }
    save_method(sm);
}

static void next_frame(Smacker *sm)
{
    Smk *v = sm->movie->smk;
    if (!smk_next_frame(v)) {
        finish(sm);
        return;
    }
    const SmkTrack *t = &v->track[0];
    if (sm->audio.buf && t->pcm_size)
        audio_push(&sm->audio, t->pcm, t->pcm_size);
    present(sm, v->palette_changed || v->cur_frame == 0);
    sm->shown++;
    if (v->cur_frame + 1 >= v->frames) {
        if (!sm->loop) {
            finish(sm);
            return;
        }
        smk_rewind(v);
    }
}

void __stdcall SmackerUpdateHook(Smacker *sm)
{
    if (!sm->playing)
        return;
    DWORD now = timeGetTime();
    if (!sm->active) { /* window inactive: pause */
        if (!sm->paused_at) {
            sm->paused_at = now ? now : 1;
            if (sm->audio.buf)
                IDirectSoundBuffer_Stop(sm->audio.buf);
        }
        return;
    }
    if (sm->paused_at) {
        sm->start_ms += now - sm->paused_at;
        sm->paused_at = 0;
        if (sm->audio.buf && sm->audio.started)
            IDirectSoundBuffer_Play(sm->audio.buf, 0, 0, DSBPLAY_LOOPING);
    }
    const Smk *v = sm->movie->smk;
    uint8_t silence = v->track[0].bits == 8 ? 0x80 : 0;
    audio_update(&sm->audio, silence);
    uint64_t due_us = (uint64_t)sm->shown * v->frame_us;
    if (sm->shown == 0 || (uint64_t)(now - sm->start_ms) * 1000 >= due_us) {
        if (sm->shown == 0)
            sm->start_ms = now;
        next_frame(sm);
    }
}

void __stdcall SmackerActivate(Smacker *sm, int active)
{
    sm->active = active;
}

/* ---- exports ---- */

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)inst, (void)reason, (void)reserved;
    return TRUE;
}

Smacker *__cdecl RtLibInit(void *app)
{
    Smacker *sm = mem_alloc(sizeof *sm);
    if (!sm)
        return NULL;
    sm->app = app;
    sm->hwnd = RtGetWindow(app);
    sm->hdc = RtGetDC(app);
    sm->hsl = RtGetHSL(app);
    sm->bpp = 8;
    sm->method = METHOD_2X_SMOOTH;
    return sm;
}

void __cdecl RtLibFree(Smacker *sm)
{
    mem_free(sm);
}

/* Returns the playback method in use, like the original. */
int __cdecl SmackerSetupView(Smacker *sm, int use_ddraw, int use_sound, IDirectSound *dsound,
                             int unused1, int unused2, int unused3, int flags)
{
    (void)unused1, (void)unused2, (void)unused3, (void)flags;
    sm->setup_done = 1;
    sm->use_ddraw = use_ddraw != 0;
    sm->use_sound = use_sound != 0;
    sm->dsound = dsound;
    int m = (int)GetPrivateProfileIntA("Dominion", "CinePlaybackMethod", METHOD_2X_SMOOTH,
                                       "Dominion.ini");
    sm->method = (m < 0 || m > 3) ? METHOD_2X_SMOOTH : m;
    return sm->method;
}

/* Take over the screen: w x h at bpp bits, exclusive. Returns the primary surface. */
IDirectDrawSurface *__cdecl SmackerCreateDirectDrawObjects(Smacker *sm, int own_window, uint32_t w,
                                                           uint32_t h, uint32_t bpp)
{
    (void)own_window; /* the game passes 0; we always use the game window */
    static const GUID iid_dd2 = {0xb3a6f3e0, 0x2b43, 0x11cf, {0xa2, 0xde, 0x00, 0xaa, 0x00, 0xb9, 0x33, 0x56}};
    sm->bpp = bpp;
    sm->dd = SpriteGetDDraw(sm->hsl);
    if (sm->dd) {
        SpriteReleaseDirectDrawPalette(sm->hsl);
        SpriteReleaseDirectDrawSurfaces(sm->hsl);
    } else {
        IDirectDraw *dd1;
        if (DirectDrawCreate(NULL, &dd1, NULL) != DD_OK)
            return NULL;
        HRESULT hr = IDirectDraw_QueryInterface(dd1, &iid_dd2, (void **)&sm->dd);
        IDirectDraw_Release(dd1);
        if (hr != DD_OK) {
            sm->dd = NULL;
            return NULL;
        }
        sm->own_dd = 1;
    }
    IDirectDraw2_SetCooperativeLevel(sm->dd, sm->hwnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
    RtGetDisplayMode(sm->app, &sm->prev_w, &sm->prev_h, &sm->prev_bpp, &sm->prev_refresh);
    if (IDirectDraw2_SetDisplayMode(sm->dd, w, h, bpp, 0, 0) != DD_OK) {
        debug("Smacker: cannot set %ux%ux%u\n", w, h, bpp);
        return NULL;
    }
    sm->mode_set = 1;

    DDSURFACEDESC d = {0};
    d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS;
    d.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if (IDirectDraw2_CreateSurface(sm->dd, &d, &sm->primary, NULL) != DD_OK) {
        sm->primary = NULL;
        return NULL;
    }
    if (bpp == 8) {
        PALETTEENTRY e[256];
        HDC dc = GetDC(sm->hwnd);
        GetSystemPaletteEntries(dc, 0, 256, e);
        ReleaseDC(sm->hwnd, dc);
        for (int i = 0; i < 256; i++)
            e[i].peFlags = (i >= 10 && i < 246) ? PC_NOCOLLAPSE : 0;
        if (IDirectDraw2_CreatePalette(sm->dd, DDPCAPS_8BIT, e, &sm->ddpal, NULL) == DD_OK)
            IDirectDrawSurface_SetPalette(sm->primary, sm->ddpal);
        else
            sm->ddpal = NULL;
    }
    return sm->primary;
}

void __cdecl SmackerDestroyDirectDrawObjects(Smacker *sm)
{
    if (sm->primary) {
        IDirectDrawSurface_Release(sm->primary);
        sm->primary = NULL;
    }
    if (sm->ddpal) {
        IDirectDrawPalette_Release(sm->ddpal);
        sm->ddpal = NULL;
    }
    if (sm->own_dd && sm->dd) {
        IDirectDraw2_SetCooperativeLevel(sm->dd, sm->hwnd, DDSCL_NORMAL);
        if (sm->mode_set)
            IDirectDraw2_RestoreDisplayMode(sm->dd);
        IDirectDraw2_Release(sm->dd);
    } else if (sm->dd && sm->mode_set) {
        IDirectDraw2_SetDisplayMode(sm->dd, sm->prev_w, sm->prev_h, sm->prev_bpp, 0, 0);
    }
    sm->dd = NULL;
    sm->own_dd = sm->mode_set = 0;
    SpriteCreateDirectDrawSurfaces(sm->hsl);
    SpriteCreateDirectDrawPalette(sm->hsl);
    hook_remove(sm);
    SpriteRealizePalette2(sm->hsl, 1);
}

Movie *__cdecl SmackerOpenAnimation(Smacker *sm, unsigned string_id)
{
    const char *path = RtGetString(sm->app, string_id);
    return path ? movie_open(path) : NULL;
}

void __cdecl SmackerCloseAnimation(Smacker *sm, Movie *m)
{
    (void)sm;
    movie_close(m);
}

/* Start playing an opened movie; it is closed when it ends. Arguments 3-5 are unused. */
int __cdecl SmackerPlayOpenedAnimation(Smacker *sm, Movie *m, int unused1, int unused2, int unused3,
                                       int msg, int msg_param, int loop)
{
    (void)unused1, (void)unused2, (void)unused3;
    if (!m)
        return 0;
    if (sm->playing) { /* replace a running movie without posting its message */
        audio_stop(&sm->audio);
        movie_close(sm->movie);
    }
    hook_install(sm);
    sm->msg = msg;
    sm->msg_param = msg_param;
    sm->loop = loop;
    sm->movie = m;
    SetViewportOrgEx(sm->hdc, 0, 0, &sm->saved_org);
    smk_rewind(m->smk);
    audio_start(sm, &m->smk->track[0]);
    clear_screen(sm);
    sm->shown = 0;
    sm->paused_at = 0;
    sm->active = 1;
    sm->playing = 1;
    return 1;
}

int __cdecl SmackerPlayAnimation(Smacker *sm, unsigned string_id, int unused1, int unused2,
                                 int unused3, int msg, int msg_param, int loop)
{
    Movie *m = SmackerOpenAnimation(sm, string_id);
    return m ? SmackerPlayOpenedAnimation(sm, m, unused1, unused2, unused3, msg, msg_param, loop) : 0;
}

void __cdecl SmackerUnload(Smacker *sm)
{
    if (sm->playing)
        finish(sm);
}

/* Redraw the current frame (after the screen was lost or cleared). */
void __cdecl SmackerRefresh(Smacker *sm)
{
    clear_screen(sm);
    if (sm->movie && sm->movie->smk->cur_frame != UINT32_MAX)
        present(sm, 1);
}

int __cdecl SmackerSetScreenRes(Smacker *sm, uint32_t w, uint32_t h)
{
    uint32_t bpp = 16;
    if (w == 0 || h == 0) { /* back to the previous mode */
        hook_remove(sm);
        w = sm->prev_w;
        h = sm->prev_h;
        bpp = sm->prev_bpp;
    }
    RtGetDisplayMode(sm->app, &sm->prev_w, &sm->prev_h, &sm->prev_bpp, &sm->prev_refresh);
    return RtSetDisplayMode(sm->app, w, h, bpp, sm->prev_refresh, 0, 0, 1);
}

void __cdecl SmackerMessageBox(Smacker *sm, unsigned caption_id, unsigned text_id, UINT type)
{
    MessageBoxA(NULL, RtGetString(sm->app, text_id), RtGetString(sm->app, caption_id), type);
}

/* The original only remembered the context here. */
void __cdecl SmackerChangeBlitType(Smacker *sm)
{
    (void)sm;
}

/* Clear w x h of video memory by creating and filling a surface of that size. */
void __cdecl ClearVideoMem(IDirectDraw2 *dd, int w, int h)
{
    if (!dd)
        return;
    IDirectDraw2_FlipToGDISurface(dd);
    DDSURFACEDESC d = {0};
    d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    d.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
    d.dwWidth = w;
    d.dwHeight = h;
    IDirectDrawSurface *s;
    if (IDirectDraw2_CreateSurface(dd, &d, &s, NULL) != DD_OK)
        return;
    DDBLTFX fx = {0};
    fx.dwSize = sizeof fx;
    IDirectDrawSurface_Blt(s, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
    IDirectDrawSurface_Release(s);
}
