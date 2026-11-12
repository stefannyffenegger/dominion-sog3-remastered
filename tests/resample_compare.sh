#!/usr/bin/env bash
# Checks our PCM conversion (src/rtlib/resample.c) against the original, bit for bit,
# under Wine.
# usage: tests/resample_compare.sh <unmodified Rtlib32.dll> [wine prefix]
set -euo pipefail
ORIG=$(realpath "$1")
PFX=${2:-${WINEPREFIX:-$HOME/.wine}}
export WINEPREFIX=$(realpath "$PFX") WINEDEBUG=-all
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
i686-w64-mingw32-gcc -O2 -Wall -o "$T/resample_compare.exe" "$ROOT/tests/resample_compare.c" "$ROOT/src/rtlib/resample.c"
cp "$ORIG" "$T/Rtlib32_ref.dll"
(cd "$T" && wine resample_compare.exe Rtlib32_ref.dll 2>/dev/null) | tr -d '\r'
