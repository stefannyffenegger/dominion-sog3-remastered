#!/usr/bin/env bash
# Checks our floating-point math (src/rtlib/math87.c) against the original, bit for bit,
# under Wine.
# usage: tests/math_compare.sh <unmodified Rtlib32.dll> [wine prefix]
set -euo pipefail
ORIG=$(realpath "$1")
PFX=${2:-${WINEPREFIX:-$HOME/.wine}}
export WINEPREFIX=$(realpath "$PFX") WINEDEBUG=-all
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
i686-w64-mingw32-gcc -O2 -Wall -o "$T/math_compare.exe" "$ROOT/tests/math_compare.c" "$ROOT/src/rtlib/math87.c"
cp "$ORIG" "$T/Rtlib32_ref.dll"
(cd "$T" && wine math_compare.exe Rtlib32_ref.dll 2>/dev/null) | tr -d '\r'
