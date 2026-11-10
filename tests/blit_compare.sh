#!/usr/bin/env bash
# Checks our C versions of the sprite engine's assembly routines (src/rtlib/blit.c)
# against the original, byte for byte, under Wine.
# usage: tests/blit_compare.sh <unmodified Rtlib32.dll> [wine prefix]
set -euo pipefail
ORIG=$(realpath "$1")
PFX=${2:-${WINEPREFIX:-$HOME/.wine}}
export WINEPREFIX=$(realpath "$PFX") WINEDEBUG=-all
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
i686-w64-mingw32-gcc -O2 -Wall -o "$T/blit_compare.exe" "$ROOT/tests/blit_compare.c" "$ROOT/src/rtlib/blit.c"
cp "$ORIG" "$T/Rtlib32_ref.dll"
(cd "$T" && wine blit_compare.exe Rtlib32_ref.dll 2>/dev/null) | tr -d '\r'
