/* Engine core: the engine's own movies, played as a sprite's image (rt_movie.c) */
#ifndef DOMINION_RTLIB_RT_MOVIE_H
#define DOMINION_RTLIB_RT_MOVIE_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;

typedef struct MovieBuf {
    uint8_t *data;
    U fill;   /* bytes read into it */
    U pos;    /* bytes played */
    U frames; /* frames in it not played yet */
    U full;
} MovieBuf;

typedef struct Movie {
    uint8_t *cmd;  /* the next command */
    uint8_t *cmds;
    uint8_t *frames; /* the frame table's entries */
    uint8_t *dib;    /* the image shown */
    uint8_t *table;  /* the frame table resource */
    uint8_t *wave_buf;
    U next_time;
    U data_offset;
    U read_offset;
    U wave_fill;
    void *wave;
    U count; /* frames */
    U cmds_size;
    U delay;
    U read_index;
    U read_size;
    U play_index;
    U id_data;
    U id;
    U speed; /* percent */
    U owner; /* the sprite showing it */
    U cmd_left;
    U changed;
    U done;
    U waiting; /* for data not read yet */
    U late;
    U skip;    /* show only full frames until the next one */
    U skipped; /* frames were dropped once already */
    U started;
    U timed;   /* the clock runs from the start, not from each frame */
    MovieBuf *bufs_end;
    MovieBuf *read_buf;
    MovieBuf *play_buf;
    MovieBuf bufs[10];
} Movie;
_Static_assert(sizeof(Movie) == 0x14c, "Movie layout");

U __stdcall movie_read_run(Movie *m);
int __stdcall movie_read_data(Movie *m);
U __stdcall movie_fill(Movie *m);
void __cdecl movies_service(void);
U __stdcall movie_fill_all(Movie *m);
int __stdcall movie_load_table(Movie *m);
void __stdcall movie_rewind(Movie *m);
void __stdcall movie_free_bufs(Movie *m);
U __stdcall movie_stop(Movie *m);
U __stdcall movie_free(Movie *m);
Movie *__stdcall movie_open(uint8_t *cmds, U size, U id);
void __stdcall movie_advance(Movie *m, U n);
void __stdcall dib_rect(uint8_t *dib, RECT *r);
void __stdcall movie_diff(uint8_t *old, uint8_t *new);
U __stdcall movie_sound_chunk(Movie *m);
U __stdcall movie_wave_read(Movie *m, void *buf, U n);
U __stdcall movie_sound_start(Movie *m);
U __stdcall movie_frame(Movie *m, U *len);
int __stdcall movie_script(Movie *m, U *len);
U __stdcall movie_exec(Movie *m, uint8_t *sprite);
void __stdcall movie_mark_drops(Movie *m, int n, int every);
U __stdcall movie_catch_up(Movie *m, U now);
U __stdcall movie_restart(Movie *m, U fill);
U __stdcall movie_start(uint8_t *sprite, uint8_t *rec);
U __stdcall movie_step(uint8_t *sprite, uint8_t *rec);
void __stdcall movies_pause(U on);
void __stdcall movie_set_speed(Movie *m, U speed);
void __stdcall movie_stop_if(Movie *m);

#endif
