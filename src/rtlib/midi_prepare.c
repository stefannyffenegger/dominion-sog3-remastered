/* The engine's audio library, 0x10043bd0-0x100445f8: turning a Standard MIDI File into
 * the engine's event list: (time, message) pairs merged from all tracks and sorted, then
 * the times turned into waits in ms (a wait has the high bit set). Kept apart from the
 * rest so tests/midi_compare.c can check it against the original. Rewritten from the
 * original. */
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>
#include "midi_prepare.h"

#define F(type, p, off) (*(type *)((const uint8_t *)(p) + (off)))

/* the original's globals 0x100887e0-0x10088810 */
static const uint8_t *g_in;  /* reading position */
static uint32_t g_in_left;   /* bytes left */
static uint32_t g_division;  /* ticks per quarter note */
static uint32_t g_ticks;     /* the current track's time */
static uint32_t g_tempo_ticks; /* the tempo events' time (never reset; see midi_meta) */
static uint32_t g_tempo;     /* ms per quarter note */
static uint32_t g_size;      /* the result's size in bytes */
static uint32_t *g_events;   /* (time, message) pairs */
static uint32_t g_count, g_room;
static uint32_t g_tracks;
static uint32_t g_running_len; /* running status: data bytes and status */
static uint8_t g_running_status;

static uint32_t be16(const uint8_t *p) { return (uint32_t)p[0] << 8 | p[1]; }
static uint32_t be32(const uint8_t *p) { return be16(p) << 16 | be16(p + 2); }

/* 0x10043d94: the MThd chunk (format 0 or 1, ticks per quarter) */
int __cdecl midi_read_header(void)
{
    if (g_in_left < 14 || F(uint32_t, g_in, 0) != mmioFOURCC('M', 'T', 'h', 'd') || be16(g_in + 8) > 1 ||
        (g_in[12] & 0x80))
        return 0;
    g_tracks = be16(g_in + 10);
    g_division = be16(g_in + 12);
    g_in += 14;
    g_in_left -= 14;
    return g_in_left != 0;
}

/* 0x1004408d: a variable-length number */
uint32_t __cdecl midi_read_varlen(void)
{
    uint8_t b = *g_in++;
    if (!(b & 0x80))
        return b;
    uint32_t v = b & 0x7f;
    do {
        b = *g_in++;
        v = (b & 0x7f) + v * 0x80;
    } while (b & 0x80);
    return v;
}

/* 0x1004437c: where an event at time t goes (after those at the same time) */
int32_t __stdcall midi_find_slot(uint32_t t)
{
    int32_t hi = (int32_t)g_count - 1, lo = 0, mid = (lo + hi + 1) / 2;
    while (lo <= hi) {
        if (g_events[mid * 2] > t)
            hi = mid - 1;
        else
            lo = mid + 1;
        mid = (lo + hi + 1) / 2;
    }
    return mid;
}

static int grow(void)
{
    if (g_room <= g_count) {
        uint32_t *e = winrealloc(g_events, (g_room + 20) * 8, 0);
        if (!e)
            return 0;
        g_room += 20;
        g_events = e;
    }
    return 1;
}

static void insert(uint32_t t, uint32_t msg)
{
    uint32_t i = (uint32_t)midi_find_slot(t);
    if (i < g_count)
        memmove(g_events + i * 2 + 2, g_events + i * 2, (g_count - i) * 8);
    g_events[i * 2] = t;
    g_events[i * 2 + 1] = msg;
    g_count++;
}

/* 0x1004428b */
int __stdcall midi_add_event(uint32_t delta, uint32_t msg)
{
    if (!grow())
        return 0;
    g_ticks += delta;
    insert(g_ticks, msg);
    return 1;
}

/* 0x100444c5 */
int __stdcall midi_add_tempo(uint32_t delta, uint32_t tempo)
{
    if (!grow())
        return 0;
    g_tempo_ticks += delta;
    insert(g_tempo_ticks, tempo | 0x80000000u);
    return 1;
}

/* 0x10044105: a channel message (with running status) */
int __stdcall midi_channel_event(uint32_t delta, uint8_t status)
{
    uint16_t len;
    switch (status & 0xf0) {
    case 0x80: case 0x90: case 0xa0: case 0xb0: case 0xe0:
        len = 2;
        break;
    case 0xc0: case 0xd0:
        len = 1;
        break;
    default: /* running status: this byte was data */
        len = (uint16_t)g_running_len;
        status = g_running_status;
        g_in--;
    }
    g_running_len = len;
    g_running_status = status;
    uint32_t msg = status;
    if (len > 0)
        msg |= (uint32_t)g_in[0] << 8;
    if (len > 1)
        msg |= (uint32_t)g_in[1] << 16;
    if (!midi_add_event(delta, msg))
        return 0;
    g_in += len;
    return 1;
}

/* 0x10044408: a meta event; only the tempo is kept. original: the length is read as one
 * byte, and the time of other meta events is lost (tempo events keep their own clock) */
int __stdcall midi_meta(uint32_t delta)
{
    uint8_t type = *g_in++, len = *g_in++;
    if (type == 0x51) {
        uint32_t us = 0;
        for (uint16_t i = 0; i < 3; i++)
            us = g_in[i] + us * 0x100;
        g_tempo = us / 1000;
        midi_add_tempo(delta, g_tempo);
    }
    g_in += len;
    return 1;
}

/* 0x100445b8: a SysEx message, skipped (original: its length read as one byte) */
int __stdcall midi_sysex(uint32_t delta)
{
    (void)delta;
    g_in++;
    uint8_t len = *g_in++;
    g_in += len;
    return 1;
}

/* 0x10043fbe */
int __stdcall midi_read_events(uint32_t len)
{
    int ok = 1;
    const uint8_t *end = g_in + len;
    while (g_in < end && ok) {
        uint32_t delta = midi_read_varlen();
        uint8_t status = *g_in++;
        if (status == 0xff)
            ok = midi_meta(delta);
        else if (status == 0xf0 || status == 0xf7)
            ok = midi_sysex(delta);
        else
            ok = midi_channel_event(delta, status);
    }
    g_in_left -= len;
    return ok;
}

/* 0x10043ef0: an MTrk chunk */
int __stdcall midi_read_track(uint32_t unused)
{
    (void)unused;
    if (g_in_left < 8 || F(uint32_t, g_in, 0) != mmioFOURCC('M', 'T', 'r', 'k'))
        return 0;
    uint32_t len = be32(g_in + 4);
    g_in += 8;
    g_in_left -= 8;
    g_ticks = 0;
    return midi_read_events(len);
}

/* 0x10043ca4: times become waits in ms (at most 5 s each), tempo events go */
int __cdecl midi_timing(void)
{
    uint32_t left = g_count, last = 0, n = 0;
    const uint32_t *in = g_events;
    uint32_t *out = g_events;
    while (left--) {
        uint32_t t = in[0], msg = in[1];
        in += 2;
        if ((int32_t)msg < 0) {
            g_tempo = msg & 0x7fffffff;
            continue;
        }
        if (last < t) {
            uint32_t wait = (t - last) * g_tempo / g_division;
            if (wait > 5000)
                return 0;
            *out++ = wait | 0x80000000u;
            n++;
            last = t;
        }
        *out++ = msg;
        n++;
    }
    g_size = n * 4;
    return 1;
}

/* The engine's form of a MIDI file, in a new block; out[0] its size */
uint32_t *__stdcall AudioPrepareMidiData(const uint8_t *data, uint32_t size, uint32_t *out)
{
    g_in = data;
    g_in_left = size;
    g_division = g_ticks = g_tempo_ticks = g_tempo = g_size = 0;
    g_events = NULL;
    g_count = g_room = g_tracks = 0;
    g_running_status = 0; /* original: cleared with the block above, the length too */
    g_running_len = 0;
    int ok = midi_read_header();
    for (uint32_t i = 0; i < g_tracks && ok; i++)
        ok = midi_read_track(i);
    if (!ok)
        return winfree(g_events);
    g_size = g_count << 3;
    if (!midi_timing())
        return winfree(g_events);
    if (out)
        out[0] = g_size;
    return g_events;
}
