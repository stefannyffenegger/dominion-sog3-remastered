#!/usr/bin/env bash
# Compare our Smacker decoder with ffmpeg, byte for byte, on one or more .SMK files.
#   tests/smk_compare.sh FILE.SMK...
# Needs: gcc, ffmpeg. Builds tests/smk_dump into a temp folder.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
gcc -O2 -Wall -Wextra -I"$ROOT/src/smacker" -o "$TMP/smk_dump" "$ROOT/tests/smk_dump.c" "$ROOT/src/smacker/smk.c"

fail=0
for f in "$@"; do
    name=$(basename "$f")
    "$TMP/smk_dump" "$f" "$TMP/ours.rgb" "$TMP/ours.pcm" 2>"$TMP/log" || { echo "FAIL $name: decoder error"; fail=1; continue; }
    ffmpeg -v error -i "$f" -map 0:v -f rawvideo -pix_fmt rgb24 "$TMP/ref.rgb" -y
    fmt=s16le; grep -q ' 8 bit' "$TMP/log" && fmt=u8
    ffmpeg -v error -i "$f" -map 0:a:0 -f $fmt "$TMP/ref.pcm" -y 2>/dev/null || : >"$TMP/ref.pcm"
    v=same; a=same
    cmp -s "$TMP/ours.rgb" "$TMP/ref.rgb" || v="DIFFERENT ($(stat -c%s "$TMP/ours.rgb") vs $(stat -c%s "$TMP/ref.rgb") bytes)"
    cmp -s "$TMP/ours.pcm" "$TMP/ref.pcm" || a="DIFFERENT ($(stat -c%s "$TMP/ours.pcm") vs $(stat -c%s "$TMP/ref.pcm") bytes)"
    [ "$v" = same ] && [ "$a" = same ] && echo "ok   $name" || { echo "FAIL $name: video $v, audio $a"; fail=1; }
done
exit $fail
