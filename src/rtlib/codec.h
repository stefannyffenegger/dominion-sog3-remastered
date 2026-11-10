/* The engine's data codecs, as portable C (no Windows):
 *  - bitmaps: blocks of LZW (10/11/12-bit codes) or raw bytes, see codec.c
 *  - sound: IMA ADPCM, 4 bits per sample
 * Used by compress.c inside the engine and by native tools and tests. */
#ifndef DOMINION_CODEC_H
#define DOMINION_CODEC_H

#include <stddef.h>
#include <stdint.h>

/* Bitmap block headers: u16 = method | size of the block's data */
#define PACK_RAW 0x0000
#define PACK_RLE 0x2000 /* written by the compressor in some settings; the decoder refuses it */
#define PACK_LZW10 0x4000
#define PACK_LZW11 0x6000
#define PACK_LZW12 0x8000
#define PACK_END 0xe000
#define PACK_METHOD 0xe000
#define PACK_SIZE 0x1fff

#define LZW_MAX_BITS 13
#define LZW_HASH 0x2345 /* hash table size of the compressor (9029, prime) */

typedef struct LzwDecoder {
    uint16_t prefix[1 << LZW_MAX_BITS];
    uint8_t suffix[1 << LZW_MAX_BITS];
    uint8_t length[1 << LZW_MAX_BITS]; /* string length - 1; original: one byte */
} LzwDecoder;

typedef struct LzwEncoder {
    uint16_t code[LZW_HASH];
    uint16_t prefix[LZW_HASH];
    uint8_t suffix[LZW_HASH];
} LzwEncoder;

/* Unpacks a whole bitmap stream (blocks up to PACK_END) into exactly size bytes.
 * src_size bounds reading (pass (size_t)-1 if unknown). Returns 1 if the stream ended
 * exactly at size. */
int bitmap_unpack(LzwDecoder *z, const uint8_t *src, size_t src_size, uint8_t *dst, size_t size);

/* One LZW block's data (a series of sub-chunks: u16 length + codes). Writes at most
 * room bytes; returns the number written, 0 on error. */
size_t lzw_unpack_block(LzwDecoder *z, int bits, const uint8_t *src, size_t n, uint8_t *dst,
                        size_t room);

/* Packs n bytes (sub-chunks of up to 0x2000) with LZW; returns bytes written to out. */
size_t lzw_pack(LzwEncoder *z, int bits, const uint8_t *in, size_t n, uint8_t *out);

/* Run-length packing (PACK_RLE); returns bytes written, stops early on an empty
 * literal run (original behavior). */
size_t rle_pack(const uint8_t *in, int n, uint8_t *out);

/* IMA ADPCM. index is kept times 8 (0..704), as the original stores it. */
typedef struct ImaState {
    int32_t sample[2];
    uint16_t index[2];
} ImaState;

/* n input bytes -> 2n samples (mono: both nibbles, low first; stereo: low = left) */
void ima_unpack(ImaState *s, int16_t *out, const uint8_t *in, uint32_t n, int stereo);
/* n/4 output bytes from n bytes of 16-bit samples */
void ima_pack(ImaState *s, uint8_t *out, const int16_t *in, uint32_t n, int stereo);

#endif
