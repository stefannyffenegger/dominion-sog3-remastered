#!/usr/bin/env bash
# Checks our codecs (src/rtlib/codec.c) against the original engine, byte for byte:
#  - every bitmap of DOMINION.bin/.000 unpacked by ours (native) and by the original
#    engine's ResLoadResource (under Wine)
#  - every LZW block in the game files re-packed by our packer
#  - IMA ADPCM unpacking and packing on test signals
# usage: tests/codec_compare.sh <unmodified Rtlib32.dll> <game folder> [wine prefix]
set -euo pipefail
ORIG=$(realpath "$1"); GAME=$(realpath "$2")
PFX=${3:-${WINEPREFIX:-$HOME/.wine}}
export WINEPREFIX=$(realpath "$PFX") WINEDEBUG=-all
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
gcc -O2 -o "$T/bitmap_dump" "$ROOT/tests/bitmap_dump.c" "$ROOT/src/rtlib/codec.c"
gcc -O2 -o "$T/lzw_repack" "$ROOT/tests/lzw_repack.c" "$ROOT/src/rtlib/codec.c"
gcc -O2 -o "$T/ima_compare" "$ROOT/tests/ima_compare.c" "$ROOT/src/rtlib/codec.c"
i686-w64-mingw32-gcc -O2 -o "$T/bitmap_ref.exe" "$ROOT/tests/bitmap_ref.c"
# -O0: the original's IMA functions don't keep EBX, which an optimized caller relies on
i686-w64-mingw32-gcc -O0 -DREF -o "$T/ima_ref.exe" "$ROOT/tests/ima_compare.c"
cp "$ORIG" "$T/Rtlib32_ref.dll"

"$T/bitmap_dump" "$GAME/DOMINION.bin" > "$T/ours.txt"
(cd "$T" && wine bitmap_ref.exe Rtlib32_ref.dll "$(winepath -w "$GAME/DOMINION.bin")" 2>/dev/null) | tr -d '\r' > "$T/ref.txt"
if cmp -s "$T/ours.txt" "$T/ref.txt"; then
    echo "bitmaps: $(wc -l < "$T/ours.txt") identical ($(grep -vc rle "$T/ours.txt") unpacked, the rest are run-length sprites)"
else
    echo "bitmaps: DIFFERENT"; diff "$T/ref.txt" "$T/ours.txt" | head; exit 1
fi
"$T/lzw_repack" "$GAME/DOMINION.bin"
"$T/ima_compare" > "$T/ima_ours.txt"
(cd "$T" && wine ima_ref.exe Rtlib32_ref.dll 2>/dev/null) | tr -d '\r' > "$T/ima_ref.txt"
if cmp -s "$T/ima_ours.txt" "$T/ima_ref.txt"; then echo "IMA ADPCM: identical"; else echo "IMA ADPCM: DIFFERENT"; exit 1; fi
