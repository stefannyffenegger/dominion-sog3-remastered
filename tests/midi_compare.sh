#!/usr/bin/env bash
# Checks our MIDI preparation (src/rtlib/midi_prepare.c) against the original, bit for bit,
# under Wine.
# usage: tests/midi_compare.sh <unmodified Rtlib32.dll> [wine prefix]
set -euo pipefail
ORIG=$(realpath "$1")
PFX=${2:-${WINEPREFIX:-$HOME/.wine}}
export WINEPREFIX=$(realpath "$PFX") WINEDEBUG=-all
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
i686-w64-mingw32-gcc -O2 -Wall -o "$T/midi_compare.exe" "$ROOT/tests/midi_compare.c" "$ROOT/src/rtlib/midi_prepare.c"
cp "$ORIG" "$T/Rtlib32_ref.dll"
(cd "$T" && wine midi_compare.exe Rtlib32_ref.dll 2>/dev/null) | tr -d '\r'
