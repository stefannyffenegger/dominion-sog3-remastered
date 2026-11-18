/* The game logic, 0x10057deb-0x1005833c: the interface's spoken messages ("int_%04d.wav",
 * 0-99): one at a time from a queue of four (the original's data at 0x1017c518: wave,
 * playing wave; -1 empty), the next one started when the previous one is done. */
#include "iso_core.h"
#include "sound.h"
#include "intfwave.h"

#define orig_memset ORIG(void *(__cdecl *)(void *, int, uint32_t), 0x100f4c10)

#define Q_WAVE(i) G(int32_t, 0x1017c518 + (i) * 8)
#define Q_PLAYING(i) G(int32_t, 0x1017c51c + (i) * 8)
#define VIEW_X ((int16_t)I(int32_t, 0xcd82))
#define VIEW_Y ((int16_t)I(int32_t, 0xcd86))

/* 0x1005810f (formerly player_announce through ORIG): spoken message `wave` for
 * `player` (only the local player hears it), at once or queued */
void __cdecl player_announce(uint32_t player, uint32_t wave, int32_t queued)
{
    char name[0x80];
    if (I(int32_t, 0x17f5) == 1 || wave >= 100)
        return;
    wsprintfA(name, (const char *)ORIG_PTR(0x1013eaac), wave);
    if (I(uint32_t, 0x1b65) != player)
        return;
    if (!queued) {
        sound_at(name, VIEW_X, VIEW_Y, 1, 1, 0, 0);
        return;
    }
    if (!I(int32_t, 0x184a))
        return;
    if (!I(int32_t, 0x140)) {
        orig_memset(ORIG_PTR(0x1017c518), -1, 0x20);
        I(int32_t, 0x140) = 1;
    }
    char waiting = 0;
    int32_t slot = -1;
    int dup = 0;
    for (int i = 0; i < 4; i++) {
        if (Q_WAVE(i) == -1) {
            if (slot == -1 && !dup) {
                Q_WAVE(i) = (int32_t)wave;
                slot = i;
            }
        } else {
            waiting++;
            if (Q_WAVE(i) == (int32_t)wave) {
                dup = 1;
                if (slot != -1) {
                    Q_WAVE(slot) = -1;
                    Q_PLAYING(slot) = -1;
                }
            }
        }
    }
    if (!waiting) {
        Q_PLAYING(slot) = (int32_t)wave;
        sound_at(name, VIEW_X, VIEW_Y, 1, 1, I(int32_t, 0x184a), 0);
    }
}

/* 0x10057deb */
void __cdecl IsoAddIntfWaveToQueue(uint8_t *iso, uint32_t player, uint32_t wave, int32_t value, int32_t direct)
{
    char name[0x80];
    g_iso = iso;
    if (I(int32_t, 0x17f5) == 1 && !direct)
        return;
    if (wave == 0xffffffffu || !direct) {
        if (value != -1)
            I(int32_t, 0x184a) = value;
        if (wave != 0xffffffffu)
            player_announce(player, wave, 1);
    } else if ((int32_t)wave >= 0 && (int32_t)wave < 100) {
        wsprintfA(name, (const char *)ORIG_PTR(0x1013ea8c), wave);
        sound_at(name, VIEW_X, VIEW_Y, 0, 0, 0, 0);
    }
}

/* 0x10057eda: a wave named by a string resource, at once */
void __cdecl IsoJustPlayTheDamnWaveFile(uint8_t *iso, uint32_t str)
{
    g_iso = iso;
    const char *name = RtGetString(I(void *, 0x180e), str);
    if (name)
        sound_at(name, VIEW_X, VIEW_Y, 0, 0, 0, 0);
}

/* 0x10057f48: the queue emptied */
void __cdecl intf_wave_queue_clear(void)
{
    for (int i = 0; i < 4; i++) {
        Q_WAVE(i) = -1;
        Q_PLAYING(i) = -1;
    }
}

/* 0x10057f95: a spoken message is done: the queue moves up and its next one starts */
void __cdecl IsoIntfWaveQueueDone(uint8_t *iso, int32_t wave)
{
    char name[0x80];
    g_iso = iso;
    for (int i = 0; i < 4; i++)
        if (Q_PLAYING(i) == wave) {
            Q_WAVE(i) = -1;
            Q_PLAYING(i) = -1;
            break;
        }
    for (int i = 1; i < 4; i++) {
        int32_t playing = Q_PLAYING(i);
        Q_WAVE(i - 1) = Q_WAVE(i);
        Q_PLAYING(i - 1) = playing;
    }
    orig_memset(ORIG_PTR(0x1017c530), -1, 8);
    int32_t next = Q_WAVE(0);
    if (next != -1 && next >= 0 && next < 100) {
        wsprintfA(name, (const char *)ORIG_PTR(0x1013ea9c), next);
        Q_PLAYING(0) = next;
        sound_at(name, VIEW_X, VIEW_Y, 1, 1, I(int32_t, 0x184a), 0);
    }
}
