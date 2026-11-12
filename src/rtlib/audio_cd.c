/* The engine's audio library, 0x10042b13-0x100435e1: CD audio over MCI, its volume
 * through the CD line of the mixer ("aux" device). Positions are kept as track << 24 |
 * minutes << 16 | seconds << 8 | frames; MCI's TMSF has those bytes the other way
 * round. Unused by Dominion (its music went through WAVEPLAY.R32). Rewritten from the
 * original. */
#include "orig.h"
#include "utils.h"
#include "audio.h"
#include "audio_int.h"

/* globals of the original */
#define g_cd_pos G(uint32_t, 0x10088482)      /* the last position read */
#define g_cd_from G(uint32_t, 0x10088486)
#define g_cd_end G(uint32_t, 0x1008848a)      /* last track, its length */
#define g_cd_saved_volume G(DWORD, 0x10088492) /* while muted */
#define g_cd_now G(uint32_t, 0x1008849a)
#define g_cd_deadline G(uint32_t, 0x1008849e)
#define g_cd_polls G(uint32_t, 0x100884a2)
#define g_cd_to G(uint32_t, 0x100884a6)
#define g_cd_sync G(uint32_t, 0x100884b6)     /* wait for the drive to start playing */

static uint32_t swap(uint32_t v) { return v << 24 | (v & 0xff00) << 8 | (v >> 8 & 0xff00) | v >> 24; }

/* 0x10042b7b */
int __stdcall cd_command(UINT msg, DWORD_PTR flags, DWORD_PTR parms)
{
    char text[0x100];
    MCIERROR e = mciSendCommandA(g_cd_open, msg, flags, parms);
    if (e)
        DebugStr("CDAudio Error: %s", mciGetErrorStringA(e, text, sizeof text) ? text : "Unknown Code");
    return e == 0;
}

/* 0x10042b13: stops and closes the drive */
uint32_t __cdecl cd_close(void)
{
    cd_command(MCI_STOP, MCI_WAIT, 0);
    if (g_cd_aux_found)
        auxSetVolume(g_cd_aux, g_cd_aux_volume);
    cd_command(MCI_CLOSE, MCI_WAIT, 0);
    g_cd_open = 0;
    g_cd_owner = NULL;
    audio_waves_restart(0);
    return 0;
}

/* 0x10042cf0 */
DWORD __cdecl cd_get_volume(void)
{
    DWORD v;
    if (!g_cd_aux_found)
        return 0;
    auxGetVolume(g_cd_aux, &v);
    return v;
}

/* 0x10042d2a */
int __stdcall cd_set_volume(DWORD v)
{
    return g_cd_aux_found && auxSetVolume(g_cd_aux, v) == MMSYSERR_NOERROR;
}

/* 0x10042cb0 */
void __cdecl cd_apply_mute(void)
{
    if (!F(uint32_t, g_audio, A_MUTED)) {
        cd_set_volume(g_cd_saved_volume);
    } else {
        g_cd_saved_volume = cd_get_volume();
        cd_set_volume(0);
    }
}

/* 0x10042d6d */
int __cdecl cd_device_stop(void)
{
    if (GetCurrentProcess() == g_cd_owner)
        cd_close();
    return g_cd_open != 0;
}

/* 0x10042df2 */
uint32_t __cdecl cd_position(void)
{
    MCI_STATUS_PARMS p;
    p.dwItem = MCI_STATUS_POSITION;
    if (!cd_command(MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&p))
        return 0;
    return swap((uint32_t)p.dwReturn);
}

uint32_t __stdcall AudioGetCDPosition(uint8_t *a)
{
    g_audio = a;
    if (g_cd_open && GetCurrentProcess() == g_cd_owner)
        return cd_position();
    return 0;
}

/* 0x10042eb9: opens the drive (the wave sounds pause meanwhile) */
int __cdecl cd_open(void)
{
    audio_waves_restart(1);
    MCI_OPEN_PARMSA open;
    open.lpstrDeviceType = "cdaudio";
    if (!cd_command(MCI_OPEN, MCI_OPEN_TYPE | MCI_WAIT, (DWORD_PTR)&open)) {
        audio_waves_restart(0);
        return 0;
    }
    g_cd_open = open.wDeviceID;
    g_cd_owner = GetCurrentProcess();
    MCI_SET_PARMS set;
    set.dwCallback = 0;
    set.dwTimeFormat = MCI_FORMAT_TMSF;
    cd_command(MCI_SET, MCI_SET_TIME_FORMAT, (DWORD_PTR)&set);
    MCI_STATUS_PARMS st;
    st.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
    cd_command(MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&st);
    uint8_t tracks = (uint8_t)st.dwReturn;
    st.dwTrack = tracks;
    st.dwItem = MCI_STATUS_LENGTH;
    cd_command(MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK, (DWORD_PTR)&st);
    uint32_t len = (uint32_t)st.dwReturn; /* minutes, seconds, frames */
    g_cd_end = ((uint32_t)tracks << 8 | (len & 0xff)) << 16 | (len >> 8 & 0xff) << 8;
    auxGetVolume(g_cd_aux, &g_cd_aux_volume);
    cd_set_volume((DWORD)g_cd_volume << 16 | g_cd_volume);
    if (F(uint32_t, g_audio, A_MUTED))
        cd_apply_mute();
    return 1;
}

uint32_t __stdcall AudioGetCDTracks(uint8_t *a)
{
    g_audio = a;
    if (g_cd_open)
        return g_cd_end >> 24;
    if (!cd_open())
        return 0;
    uint32_t n = g_cd_end >> 24;
    cd_close();
    return n;
}

uint32_t __stdcall AudioGetCDVolume(uint8_t *a)
{
    g_audio = a;
    return cd_get_volume();
}

/* 0x100430ba: a track's length in ms */
uint32_t __stdcall cd_track_length(uint8_t track)
{
    MCI_STATUS_PARMS p;
    p.dwTrack = track;
    p.dwItem = MCI_STATUS_LENGTH;
    if (!cd_command(MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK, (DWORD_PTR)&p))
        return 0;
    return ((uint32_t)(p.dwReturn >> 8 & 0xff) + (uint32_t)(p.dwReturn & 0xff) * 60) * 1000;
}

/* the lengths of tracks from-to into out; returns how many. original: the context
 * isn't set, and the drive is closed afterwards when it was open before (not when this
 * opened it) */
uint32_t __stdcall AudioGetCDTrackLengths(uint8_t *a, uint8_t from, uint8_t to, uint32_t *out)
{
    (void)a;
    int was_open = g_cd_open != 0;
    if (!g_cd_open && !cd_open())
        return 0;
    uint32_t n = 0;
    while (from <= to) {
        *out = cd_track_length(from++);
        if (!*out)
            break;
        n++;
        out++;
    }
    if (was_open)
        cd_close();
    return n;
}

int __stdcall AudioIsCDPlaying(uint8_t *a)
{
    g_audio = a;
    return g_cd_open != 0;
}

/* 0x1004339e: waits up to a second for the drive to leave the start position */
void __cdecl cd_sync(void)
{
    g_cd_polls = 0;
    g_cd_now = timeGetTime();
    g_cd_deadline = g_cd_now + 1000;
    for (;;) {
        g_cd_pos = cd_position();
        if (g_cd_from != g_cd_pos)
            break;
        g_cd_now = timeGetTime();
        if (g_cd_deadline <= g_cd_now)
            break;
        g_cd_polls++;
    }
    DebugStr("sync_cd: CurPos=%08lx nPolls=%li Time=%li", g_cd_pos, g_cd_polls, g_cd_now - g_cd_deadline + 1000);
}

/* 0x100432e6: the first play measures whether the drive starts late; if it does, later
 * plays wait for it */
void __cdecl cd_profile(void)
{
    cd_sync();
    if (g_cd_now < g_cd_deadline && g_cd_polls > 2 && g_cd_from >> 24 == g_cd_pos >> 24 &&
        (g_cd_pos >> 16 & 0xff) == (g_cd_from >> 16 & 0xff) && (g_cd_pos >> 8 & 0xff) == (g_cd_from >> 8 & 0xff) &&
        (int)(g_cd_pos & 0xff) < (int)(g_cd_from & 0xff) + 2)
        g_cd_sync = 1;
    g_cd_profile = 0;
}

/* 0x100431a1: plays from-to (to 0: to the end) */
uint32_t __stdcall cd_play(uint32_t from, uint32_t to)
{
    if (!g_cd_open && !cd_open())
        return 0;
    if (from >= g_cd_end)
        return cd_close();
    g_cd_from = from;
    g_cd_pos = from;
    g_cd_to = !to || to > g_cd_end ? g_cd_end : to;
    MCI_PLAY_PARMS p;
    p.dwFrom = swap(g_cd_from);
    p.dwTo = swap(g_cd_to);
    cd_command(MCI_PLAY, MCI_FROM | MCI_TO, (DWORD_PTR)&p);
    if (g_cd_profile)
        cd_profile();
    else if (g_cd_sync)
        cd_sync();
    return 1;
}

int __stdcall AudioPlayCD(uint8_t *a, uint32_t from, uint32_t to)
{
    g_audio = a;
    if (g_cd_owner && GetCurrentProcess() != g_cd_owner)
        return 0;
    return (int)cd_play(from, to);
}

/* 0x1004349d: plays tracks from-to, to the last frame of `to` */
uint32_t __stdcall cd_play_tracks(uint8_t from, uint8_t to)
{
    if (!g_cd_open && !cd_open())
        return 0;
    if (g_cd_end <= from)
        return cd_close();
    uint32_t end;
    if ((g_cd_end >> 24 & 0xff) < to) {
        end = g_cd_end;
    } else {
        MCI_STATUS_PARMS p;
        p.dwTrack = to;
        p.dwItem = MCI_STATUS_LENGTH;
        cd_command(MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK, (DWORD_PTR)&p);
        uint8_t frames = (uint8_t)(p.dwReturn >> 16);
        uint8_t last = frames ? (uint8_t)(frames - 1) : 0;
        end = ((uint32_t)to << 8 | (p.dwReturn & 0xff)) << 16 | (p.dwReturn >> 8 & 0xff) << 8 | last;
    }
    return cd_play((uint32_t)from << 24, end);
}

int __stdcall AudioPlayCDTrack(uint8_t *a, uint8_t track)
{
    g_audio = a;
    if (!g_cd_owner || GetCurrentProcess() == g_cd_owner)
        cd_play_tracks(track, track);
    return g_cd_open != 0;
}

/* remembered as CdVolume in dominion.ini */
int __stdcall AudioSetCDVolume(uint8_t *a, DWORD v)
{
    g_audio = a;
    int ok = cd_set_volume(v);
    if (ok) {
        g_cd_volume = (uint16_t)v;
        audio_save_cd_volume();
    }
    return ok;
}
