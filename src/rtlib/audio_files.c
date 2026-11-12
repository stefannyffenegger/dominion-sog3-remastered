/* The engine's audio library, 0x1004226a-0x10042ac0: .wav files: a wave sound streamed
 * from a file, loading one whole, saving one. Besides plain PCM ("WAVE") it reads and
 * writes 7th Level's own compressed waves ("WAV7": a 10-byte header, then IMA ADPCM
 * with 4-bit samples). Unused by Dominion. Rewritten from the original. */
#include "orig.h"
#include "utils.h"
#include "compress.h"
#include "audio.h"
#include "audio_int.h"

#define FOURCC_WAVE mmioFOURCC('W', 'A', 'V', 'E')
#define FOURCC_WAV7 mmioFOURCC('W', 'A', 'V', '7')

static ImaState g_ima; /* 0x100887d0 */

/* 0x100424b0: enters the RIFF chunk; a WAV7 file is marked packed */
int __stdcall wave_file_riff(FileSource *fs)
{
    MMCKINFO ck;
    ck.fccType = 0;
    if (mmioDescend(fs->f, &ck, NULL, MMIO_FINDRIFF))
        return 0;
    if (ck.fccType == FOURCC_WAV7)
        fs->packed = 1;
    else if (ck.fccType != FOURCC_WAVE)
        return 0;
    return 1;
}

/* 0x100423e8: reads the 16-byte PCM format chunk */
int __stdcall wave_file_format(HMMIO f, WAVEFORMAT *fmt)
{
    MMCKINFO ck;
    ck.ckid = mmioFOURCC('f', 'm', 't', ' ');
    if (mmioDescend(f, &ck, NULL, MMIO_FINDCHUNK) || ck.cksize != 0x10 ||
        (DWORD)mmioRead(f, (HPSTR)fmt, 0x10) != ck.cksize || fmt->wFormatTag != WAVE_FORMAT_PCM)
        return 0;
    mmioAscend(f, &ck, 0);
    return 1;
}

/* 0x10042470: enters the data chunk; its size, 0 for none */
DWORD __stdcall wave_file_data(HMMIO f)
{
    MMCKINFO ck;
    ck.ckid = mmioFOURCC('d', 'a', 't', 'a');
    if (mmioDescend(f, &ck, NULL, MMIO_FINDCHUNK))
        ck.cksize = 0;
    return ck.cksize;
}

/* 0x10042335: info for AudioCreateWave (length +0, packed +6, format +0x10) */
int __stdcall wave_file_info(FileSource *fs, uint8_t *info)
{
    if (!wave_file_riff(fs) || !wave_file_format(fs->f, (WAVEFORMAT *)(info + 0x10)))
        return 0;
    F(DWORD, info, 0) = wave_file_data(fs->f);
    if (!F(DWORD, info, 0))
        return 0;
    if (fs->packed) {
        info[6] = 1;
        F(DWORD, info, 0) -= 10;
        mmioSeek(fs->f, 10, SEEK_CUR);
    }
    fs->pos = (uint32_t)mmioSeek(fs->f, 0, SEEK_CUR);
    return 1;
}

/* flags: 8 in memory, 4 packed, 1 no priority */
uint8_t *__stdcall AudioCreateWaveFileSound(uint8_t *a, LPSTR path, uint8_t flags)
{
    FileSource *fs = file_source_open(path);
    if (!fs)
        return NULL;
    struct {
        void *read, *rewind, *close;
        uint32_t v3, v4;
        void *arg;
    } src = {(void *)file_source_read, (void *)file_source_rewind, (void *)file_source_close_cb, 0, 0, fs};
    uint8_t info[0x20];
    memset(info, 0, sizeof info);
    if (flags & 8)
        info[4] = 1;
    if (flags & 4)
        info[6] = 1;
    if (flags & 1)
        info[5] = 1;
    if (!wave_file_info(fs, info))
        return file_source_close(fs);
    return AudioCreateWave(a, (const struct Source *)&src, info);
}

/* 0x100426f4: unpacks a WAV7 data chunk into 16-bit samples */
int __stdcall wave_file_unpack(HMMIO f, int16_t *out, const uint8_t *fmt)
{
    uint8_t *buf = winmalloc(0x2004, 0);
    if (!buf)
        return 0;
    int stereo = F(int16_t, fmt, 2) == 2;
    uint32_t grow = F(int16_t, fmt, 0xe) == 16 ? 4 : 2;
    memset(&g_ima, 0, sizeof g_ima);
    int ok = 1;
    for (int32_t left = F(int32_t, fmt, 0x10), n; left; left -= n) {
        n = left > 0x2003 ? 0x2004 : left;
        ok = mmioRead(f, (HPSTR)buf, n) == n;
        if (!ok)
            break;
        ima2pcm(&g_ima, out, buf, (uint32_t)n, stereo);
        out = (int16_t *)((uint8_t *)out + grow * (uint32_t)n);
    }
    winfree(buf);
    return ok;
}

/* 0x10042598: reads the format and the whole data; out: the format, the stored size
 * (+0x10), the size in memory (+0x14) */
uint8_t *__stdcall wave_file_read(HMMIO f, int packed, uint8_t *out)
{
    if (!wave_file_format(f, (WAVEFORMAT *)out))
        return NULL;
    F(DWORD, out, 0x10) = wave_file_data(f);
    if (!F(DWORD, out, 0x10))
        return NULL;
    F(DWORD, out, 0x14) = F(DWORD, out, 0x10);
    if (packed) {
        if (F(DWORD, out, 0x10) < 10 || mmioSeek(f, 10, SEEK_CUR) == -1)
            return NULL;
        F(DWORD, out, 0x10) -= 10;
        F(DWORD, out, 0x14) <<= F(int16_t, out, 0xe) == 16 ? 2 : 1;
    }
    uint8_t *data = winmalloc(F(DWORD, out, 0x14), 0); /* original: not checked */
    int ok = packed ? wave_file_unpack(f, (int16_t *)data, out)
                    : mmioRead(f, (HPSTR)data, F(LONG, out, 0x14)) == F(LONG, out, 0x14);
    if (!ok)
        data = winfree(data);
    return data;
}

/* 0x10042524: a whole .wav file in memory */
uint8_t *__stdcall wave_file_load(LPSTR path, uint8_t *out)
{
    FileSource *fs = file_source_open(path);
    if (!fs)
        return NULL;
    uint8_t *data = wave_file_riff(fs) ? wave_file_read(fs->f, (int)fs->packed, out) : NULL;
    file_source_close(fs);
    return data;
}

/* 0x10042a7d: CompressWave's output */
int __stdcall wave_file_write_cb(uint32_t f, void *buf, uint32_t n)
{
    return (uint32_t)mmioWrite((HMMIO)(uintptr_t)f, buf, (LONG)n) == n;
}

/* 0x10042a18 */
int __stdcall wave_file_write_packed(HMMIO f, const int16_t *data, uint32_t n, const uint8_t *fmt)
{
    struct WavePacker *p = CompressWaveInit(F(int16_t, fmt, 2), n, wave_file_write_cb, (uint32_t)(uintptr_t)f, 1);
    if (!p)
        return 0;
    int r = CompressWaveData(p, data, n);
    CompressWaveExit(p);
    return r;
}

/* 0x10042830: saves a .wav file, 16-bit data packed as WAV7 if asked; a failed file is
 * deleted */
int __stdcall wave_file_save(LPSTR path, const uint8_t *fmt, const int16_t *data, uint32_t size, int pack)
{
    HMMIO f = mmioOpenA(path, NULL, MMIO_CREATE | MMIO_WRITE);
    if (!f)
        return 0;
    if (F(int16_t, fmt, 0xe) != 16)
        pack = 0;
    MMCKINFO riff, fmtck, datack;
    riff.fccType = pack ? FOURCC_WAV7 : FOURCC_WAVE;
    fmtck.ckid = mmioFOURCC('f', 'm', 't', ' ');
    fmtck.cksize = 0x10;
    datack.ckid = mmioFOURCC('d', 'a', 't', 'a');
    datack.cksize = size;
    if (!mmioCreateChunk(f, &riff, MMIO_CREATERIFF) && !mmioCreateChunk(f, &fmtck, 0) &&
        mmioWrite(f, (const char *)fmt, 0x10) == 0x10 && !mmioAscend(f, &fmtck, 0) &&
        !mmioCreateChunk(f, &datack, 0)) {
        int failed = pack ? !wave_file_write_packed(f, data, size, fmt)
                          : (uint32_t)mmioWrite(f, (const char *)data, (LONG)size) != size;
        if (!failed && !mmioAscend(f, &datack, 0) && !mmioAscend(f, &riff, 0)) {
            mmioClose(f, 0);
            return 1;
        }
    }
    mmioClose(f, 0);
    mmioOpenA(path, NULL, MMIO_DELETE);
    return 0;
}
