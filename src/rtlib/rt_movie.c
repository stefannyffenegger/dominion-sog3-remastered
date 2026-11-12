/* Engine core, 0x10015150-0x100167f2: the engine's own movies (resource type 0xd),
 * played as a sprite's image. A movie is three resources: the commands (id), the
 * frame data (id + 1) and the frame table (id + 2). Frames and sound are streamed
 * from the resource file into ten 65,000-byte buffers ahead of playing. Up to four
 * movies at a time. Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_movie.h"

#define RU(off) R(U, off)
#define HSL R(uint8_t *, RT_HSL)
#define RES R(ResFile *, RT_RES)
#define AUDIO R(uint8_t *, 0xc90)

#define RT_MOVIE_COUNT 0x3e37
#define RT_MOVIES 0x3f1b      /* their object ids */
#define RT_SKIP_FRAMES 0x3dff /* frames a late movie may drop */
#define RT_SOUND_ON 0x95cb
#define RT_MOVIES_PAUSED 0x95ef
#define RT_PAUSED_AT 0xb74
#define MAX_MOVIES 4
#define BUF_SIZE 65000
#define WAVE_SIZE 0x18000

/* audio sounds are kept as 32-bit values */
#define AUDIO_PTR(v) ((uint8_t *)(uintptr_t)(v))
#define AudioCreateWave(a, src, info) AudioCreateWave(a, (const struct Source *)(src), (const uint8_t *)(info))
#define AudioPlayWave(a, s, p) AudioPlayWave(a, AUDIO_PTR(s), (const uint8_t *)(p))
#define AudioDestroyWave(a, s) AudioDestroyWave(a, AUDIO_PTR(s))

/* frame table entries, 10 bytes */
#define F_TYPE(m, i) ((m)->frames[(i) * 10])     /* 0 full frame, 1 change, 2 sound */
#define F_READ(m, i) ((m)->frames[(i) * 10 + 1]) /* already read, or dropped */
#define F_SIZE(m, i) P(U, (m)->frames, (i) * 10 + 2)
#define F_OFFSET(m, i) P(U, (m)->frames, (i) * 10 + 6)

#define CMD_TYPE(m) P(uint16_t, (m)->cmd, 0)

static Movie *movie_of(U id) { return (Movie *)(uintptr_t)obj_entry(id)[2]; }
static U *movie_ids(void) { return (U *)(g_rt + RT_MOVIES); }

static void next_buf(Movie *m, MovieBuf **b)
{
    if (++*b >= m->bufs_end)
        *b = m->bufs;
}

/* 0x1001534a: the run of frames from read_index that are next to each other in the
 * file and fit in the buffer; their count */
U __stdcall movie_read_run(Movie *m)
{
    U start = m->read_index, i = start;
    U room = BUF_SIZE - m->read_buf->fill;
    if (room < F_SIZE(m, i))
        return 0;
    m->read_offset = F_OFFSET(m, i);
    m->read_size = F_SIZE(m, i);
    for (;;) {
        m->read_index++;
        if (m->read_index >= m->count || F_READ(m, i + 1))
            break;
        U n = F_SIZE(m, i + 1) + m->read_size;
        if (n > room || m->read_size + m->read_offset != F_OFFSET(m, i + 1))
            break;
        m->read_size = n;
        i++;
    }
    return m->read_index - start;
}

/* 0x10015440: reads the run into the buffer */
int __stdcall movie_read_data(Movie *m)
{
    SpriteSetSendMsg(HSL, 0);
    void *old = ResSetCallBack(RES, (void(__stdcall *)(void))(void *)load_progress, 0x2000);
    MovieBuf *b = m->read_buf;
    int r = ResReadResourceData(RES, m->id_data + 1, b->data + b->fill, (int)m->read_size, (int)m->read_offset);
    b->fill += m->read_size;
    ResSetCallBack(RES, (void(__stdcall *)(void))old, 0x2000);
    SpriteSetSendMsg(HSL, 1);
    return r;
}

/* 0x100151d7: fills the next free buffer; at the end the movie loops (first byte of
 * the frame table) or is done */
U __stdcall movie_fill(Movie *m)
{
    MovieBuf *b = m->read_buf;
    if (b->full)
        return 0;
    b->fill = b->pos = b->frames = 0;
    while (m->read_index < m->count) {
        if (F_READ(m, m->read_index)) {
            m->read_index++;
            continue;
        }
        U n = movie_read_run(m);
        if (!n)
            break;
        if (!movie_read_data(m))
            return 0;
        b->frames += n;
    }
    b->full = b->frames != 0;
    next_buf(m, &m->read_buf);
    if (m->read_index >= m->count) {
        if (!*m->table)
            m->done = 1;
        else
            m->read_index = 0;
    }
    return 1;
}

/* 0x10015150: tops up every movie's buffers */
void __cdecl movies_service(void)
{
    for (U i = 0; i < RU(RT_MOVIE_COUNT); i++) {
        Movie *m = movie_of(movie_ids()[i]);
        if (m && !m->done && !m->read_buf->full)
            movie_fill(m);
    }
}

/* 0x1001565e: allocates and fills the buffers (fewer when the movie is short) */
U __stdcall movie_fill_all(Movie *m)
{
    MovieBuf *b = m->read_buf = m->bufs;
    m->bufs_end = m->bufs + 10;
    for (; b < m->bufs_end && !m->done; b++) {
        b->data = winmalloc(BUF_SIZE, 0);
        if (!b->data || !movie_fill(m))
            return 0;
    }
    m->bufs_end = b;
    return 1;
}

/* 0x10015700: loads the frame table */
int __stdcall movie_load_table(Movie *m)
{
    U size;
    m->table = ResLoadResource(RES, m->id + 2, &size);
    if (!m->table)
        return 0;
    size -= 2;
    m->frames = m->table + 2;
    m->count = size / 10;
    m->data_offset = res_get_offset(RES, m->id + 1);
    return 1;
}

/* 0x10015797: back to the first command */
void __stdcall movie_rewind(Movie *m)
{
    m->play_buf = m->bufs;
    m->play_index = 0;
    m->cmd = m->cmds;
    m->cmd_left = m->cmds_size;
    m->wave_fill = 0;
    m->started = m->skip = m->late = m->skipped = 0;
}

/* 0x10015917: frees the buffers */
void __stdcall movie_free_bufs(Movie *m)
{
    for (U i = 0; i < 10; i++)
        if (m->bufs[i].data)
            winfree(m->bufs[i].data);
    memset(m->bufs, 0, sizeof m->bufs);
}

/* 0x10015995: stops the movie (and its sound); 0 */
U __stdcall movie_stop(Movie *m)
{
    if (m->wave)
        AudioDestroyWave(AUDIO, m->wave);
    m->done = 1;
    m->wave = NULL;
    m->wave_fill = 0;
    return 0;
}

/* 0x10015813: frees the movie; 0 */
U __stdcall movie_free(Movie *m)
{
    if (!m)
        return 0;
    movie_stop(m);
    movie_free_bufs(m);
    if (m->wave_buf)
        winfree(m->wave_buf);
    if (m->table)
        winfree(m->table);
    for (U i = 0; i < RU(RT_MOVIE_COUNT);) {
        if (movie_ids()[i] == m->id) {
            memmove(movie_ids() + i, movie_ids() + i + 1, (RU(RT_MOVIE_COUNT) - i) * 4);
            RU(RT_MOVIE_COUNT)--;
        } else {
            i++;
        }
    }
    LocalFree(m);
    return 0;
}

/* 0x10015501: opens movie `id` with its commands */
Movie *__stdcall movie_open(uint8_t *cmds, U size, U id)
{
    if (RU(RT_MOVIE_COUNT) >= MAX_MOVIES)
        return NULL;
    Movie *m = LocalAlloc(LPTR, sizeof(Movie));
    if (!m)
        return NULL;
    m->id_data = id;
    m->id = id;
    m->speed = 100;
    movie_ids()[RU(RT_MOVIE_COUNT)++] = m->id;
    obj_entry(m->id)[2] = (U)(uintptr_t)m;
    m->cmds = cmds;
    m->cmds_size = size;
    m->dib = SpriteDibCreate(1, 1, 1, 8);
    if (!m->dib)
        return (Movie *)(uintptr_t)movie_free(m);
    P(U, m->dib, 0x1c) = id;
    obj_entry(m->id)[0] = (U)(uintptr_t)m->dib;
    if (!movie_load_table(m) || !movie_fill_all(m))
        return (Movie *)(uintptr_t)movie_free(m);
    movie_rewind(m);
    return m;
}

/* 0x10015dea: moves on `n` bytes; a frame command gets its repeat count back */
void __stdcall movie_advance(Movie *m, U n)
{
    m->cmd += n;
    if (CMD_TYPE(m) == 1)
        P(U, m->cmd, 6) = P(U, m->cmd, 2);
}

/* 0x10016255: the bitmap's rectangle around its hot spot */
void __stdcall dib_rect(uint8_t *dib, RECT *r)
{
    int w = P(int, dib, 4), h = P(int, dib, 0xc);
    r->left = P(int, dib, 0x14) - (w - 1) / 2;
    r->right = w + r->left;
    r->top = P(int, dib, 0x18) - (h - 1) / 2;
    r->bottom = r->top + h;
}

/* 0x100161d0: a change frame: the part of the old frame it overlaps is kept */
void __stdcall movie_diff(uint8_t *old, uint8_t *new)
{
    RECT a, b, i;
    dib_rect(old, &a);
    dib_rect(new, &b);
    if (IntersectRect(&i, &a, &b))
        SpriteDibDiff((uint32_t)(uintptr_t) new, (uint32_t)(uintptr_t)old, (uint32_t)(i.right - i.left),
                      (uint32_t)(i.bottom - i.top), i.left - b.left, i.top - b.top, i.left - a.left, i.top - a.top);
}

/* 0x100163f2: a sound chunk from the buffers into the wave buffer (only with sound
 * on); waits when the data is not read yet */
U __stdcall movie_sound_chunk(Movie *m)
{
    MovieBuf *b = m->play_buf;
    if (!b->full) {
        m->waiting = 1;
        return 1;
    }
    U size = F_SIZE(m, m->play_index);
    if (R(uint8_t, RT_SOUND_ON)) {
        if (!m->wave_buf) {
            m->wave_buf = winmalloc(WAVE_SIZE, 0);
            if (!m->wave_buf)
                return 0;
        }
        if (m->wave_fill + size > WAVE_SIZE)
            return 0;
        CopyHugeBytes(m->wave_buf + m->wave_fill, b->data + b->pos, size);
        m->wave_fill += size;
    }
    b->pos += size;
    m->play_index++;
    if (--b->frames == 0) {
        b->full = 0;
        next_buf(m, &m->play_buf);
    }
    return 1;
}

/* 0x10016635: the wave's read callback: `n` bytes of sound, or 0 if not there yet */
U __stdcall movie_wave_read(Movie *m, void *buf, U n)
{
    if (!m)
        return 0;
    if (F_TYPE(m, m->play_index) == 2)
        movie_sound_chunk(m);
    if (m->wave_fill < n)
        return 0;
    CopyHugeBytes(buf, m->wave_buf, n);
    m->wave_fill -= n;
    if (m->wave_fill)
        CopyHugeBytes(m->wave_buf, m->wave_buf + n, m->wave_fill);
    return n;
}

/* 0x100162ce: command 3: starts the sound (format at the command); timing follows it */
U __stdcall movie_sound_start(Movie *m)
{
    if (F_TYPE(m, m->play_index) == 2)
        movie_sound_chunk(m);
    if (!R(uint8_t, RT_SOUND_ON))
        return 1;
    U cb[6] = {0};
    cb[0] = (U)(uintptr_t)movie_wave_read;
    cb[5] = (U)(uintptr_t)m;
    m->wave = AudioCreateWave(AUDIO, cb, m->cmd + 2);
    if (!m->wave)
        return 0;
    return AudioPlayWave(AUDIO, m->wave, NULL);
}

/* 0x10015f9d: command 1: shows the next frame (unless dropped or late); the command
 * repeats for its count. A sound chunk in the way is taken first. */
U __stdcall movie_frame(Movie *m, U *len)
{
    U i = m->play_index;
    if (F_TYPE(m, i) == 2)
        return movie_sound_chunk(m);
    if (!F_READ(m, i)) {
        MovieBuf *b = m->play_buf;
        if (!b->full) {
            m->waiting = 1;
            return m->waiting;
        }
        if (!m->late && (!m->skip || F_TYPE(m, i) == 0)) {
            uint32_t *rec = (uint32_t *)(b->data + b->pos);
            uint8_t *dib = ResExpandBitmapRec(RES, rec, (uint8_t *)(rec + 6));
            if (!dib)
                return 0;
            P(U, dib, 0x1c) = m->id_data;
            if (!(dib[0x10] & 2)) {
                if (dib[0x10] & 1)
                    movie_diff(m->dib, dib);
                winfree(m->dib);
                m->dib = dib;
                obj_entry(m->id)[0] = (U)(uintptr_t)m->dib;
            } else {
                SpriteDibMerge((uint32_t)(uintptr_t)m->dib, (uint32_t)(uintptr_t)dib, P(int, dib, 4), P(int, dib, 0xc),
                               0, 0, 0, 0);
                winfree(dib);
            }
            m->changed = 1;
            m->skip = 0;
        }
        b->pos += F_SIZE(m, i);
        if (--b->frames == 0) {
            b->full = 0;
            next_buf(m, &m->play_buf);
        }
    }
    m->delay = P(U, m->cmd, 10);
    if (--P(U, m->cmd, 6) == 0)
        *len = 14;
    m->play_index++;
    return 1;
}

/* 0x100163a1: command 5: a script command; nonzero while the movie still exists */
int __stdcall movie_script(Movie *m, U *len)
{
    U *e = obj_entry(m->id);
    cmd_exec(m->cmd + 2, (int *)len);
    return (Movie *)(uintptr_t)e[2] == m;
}

/* 0x10015e27: runs one command (the sprite is not used) */
U __stdcall movie_exec(Movie *m, uint8_t *sprite)
{
    (void)sprite;
    m->delay = 0;
    U len = 0, ok = 1;
    switch (CMD_TYPE(m)) {
    case 1:
        ok = movie_frame(m, &len);
        break;
    case 2: /* wait */
        m->delay = P(U, m->cmd, 2);
        len = 6;
        break;
    case 3:
        ok = movie_sound_start(m);
        m->timed = 1;
        m->next_time = timeGetTime();
        len = 0x22;
        break;
    case 4: /* timing on or off */
        m->timed = P(U, m->cmd, 2);
        m->next_time = timeGetTime();
        len = 6;
        break;
    case 5:
        if (!movie_script(m, &len))
            return 0;
        len += 2;
        break;
    default:
        ok = 0;
    }
    if (!ok)
        return movie_stop(m);
    if (!len)
        return 1;
    m->cmd_left -= len;
    if (m->cmd_left)
        movie_advance(m, len);
    return 1;
}

/* 0x10015c2a: marks every `every`-th unread change frame to be dropped, `n` of them */
void __stdcall movie_mark_drops(Movie *m, int n, int every)
{
    int k = every;
    U i;
    for (i = m->read_index; i < m->count && n; i++) {
        if (!F_READ(m, i) && F_TYPE(m, i) == 1 && --k == 0) {
            k = every;
            F_READ(m, i) = 1;
            n--;
        }
    }
    if (i >= m->count)
        m->skipped = 1;
}

/* 0x10015bc9: over half a second behind: drops frames (once) */
U __stdcall movie_catch_up(Movie *m, U now)
{
    if (now < m->next_time + 500)
        return 0;
    if (!m->skipped)
        movie_mark_drops(m, R(int, RT_SKIP_FRAMES), 1);
    m->skip = 1;
    return m->skip;
}

/* 0x10016562: back to the start, the buffers emptied and (with `fill`) filled again */
U __stdcall movie_restart(Movie *m, U fill)
{
    movie_rewind(m);
    m->read_index = 0;
    for (U i = 0; i < m->count; i++)
        F_READ(m, i) = 0;
    m->read_buf = m->bufs;
    m->done = 0;
    for (MovieBuf *b = m->read_buf; b < m->bufs_end && !m->done; b++) {
        b->full = 0;
        if (fill && !movie_fill(m))
            return 0;
    }
    return 1;
}

/* 0x10015cf7: a sprite starts showing the movie named by the image record (the
 * movie belongs to the first sprite to show it) */
U __stdcall movie_start(uint8_t *sprite, uint8_t *rec)
{
    uint8_t c[16];
    memcpy(c, rec, 16);
    cmdw_inplace(c);
    Movie *m = movie_of(P(U, c, 2));
    if (!m)
        return 0;
    P(U, rec, 6) = m->id;
    U owner = SpriteGetId(sprite);
    if (m->owner && m->owner != owner)
        return 0;
    m->owner = owner;
    if (m->started && !movie_restart(m, 1))
        return movie_stop(m);
    movie_advance(m, 0);
    m->next_time = timeGetTime();
    m->started = 1;
    return m->started;
}

/* 0x100159ec: the sprite engine's movie step: runs the commands that are due; the
 * record's flag says the image changed */
U __stdcall movie_step(uint8_t *sprite, uint8_t *rec)
{
    if (!P(U, rec, 6) && !movie_start(sprite, rec))
        return 0;
    Movie *m = movie_of(P(U, rec, 6));
    if (!m)
        return 0;
    rec[10] = 0;
    if (!m->speed)
        return 1;
    DWORD now = timeGetTime();
    if (now < m->next_time)
        return 1;
    m->changed = 0;
    m->waiting = 0;
    m->late = m->timed && movie_catch_up(m, now);
    do {
        if (!m->cmd_left)
            return movie_stop(m);
        if (!movie_exec(m, sprite))
            return 0;
        rec[10] = (uint8_t)m->changed;
        if (rec[10])
            P(uint8_t *, rec, 0xc) = m->dib;
        if (!m->timed)
            m->next_time = now;
        if (m->delay && m->speed)
            m->delay = m->delay * 100 / m->speed;
        m->next_time += m->delay;
    } while (m->next_time < now && !m->waiting && m->late);
    return 1;
}

/* 0x100166eb: RtPauseMovies: while paused the clocks stand still */
void __stdcall movies_pause(U on)
{
    DWORD now = timeGetTime();
    R(uint8_t, RT_MOVIES_PAUSED) = (uint8_t)on;
    if (R(uint8_t, RT_MOVIES_PAUSED)) {
        R(DWORD, RT_PAUSED_AT) = now;
        return;
    }
    DWORD at = R(DWORD, RT_PAUSED_AT);
    for (U i = 0; i < RU(RT_MOVIE_COUNT); i++) {
        Movie *m = movie_of(movie_ids()[i]);
        if (m)
            m->next_time += now - at;
    }
}

/* 0x100167ab: playing speed in percent (0 holds) */
void __stdcall movie_set_speed(Movie *m, U speed)
{
    if (m)
        m->speed = speed;
}

/* 0x100167d0 */
void __stdcall movie_stop_if(Movie *m)
{
    if (m)
        movie_stop(m);
}
