#!/usr/bin/env bash
# Phase 1a: install Dominion: Storm Over Gift 3 into its own Wine prefix from the original
# disc images, apply the compatibility patches, and write a working Dominion.ini.
#
# Usage: tools/setup_wine.sh <dir with DOMINION1.iso and DOMINION2.iso> [video_mode]
#   video_mode: 640 (default), 800, 1024 or 1280
# Env:   DOMINION_HOME  install location (default: ~/.local/share/dominion-remastered)
# Then:  tools/run_wine.sh
set -euo pipefail

ISO_DIR=${1:?usage: setup_wine.sh <iso dir> [640|800|1024|1280]}
MODE=${2:-640}
HERE=$(cd "$(dirname "$0")" && pwd)
DH=${DOMINION_HOME:-$HOME/.local/share/dominion-remastered}
export WINEPREFIX="$DH/prefix" WINEDEBUG=-all
GAME="$DH/game"

case "$MODE" in 640|800|1024|1280) ;; *) echo "video mode must be 640/800/1024/1280"; exit 1;; esac
command -v wine >/dev/null || { echo "wine not found"; exit 1; }
command -v 7z >/dev/null || { echo "7z not found (dnf install p7zip-plugins)"; exit 1; }

mkdir -p "$DH"
TMP=$(mktemp -d "$DH/extract.XXXX")
trap 'rm -rf "$TMP"' EXIT

echo "== Extracting discs"
7z x -y -o"$TMP/cd1" "$ISO_DIR/DOMINION1.iso" >/dev/null
7z x -y -o"$TMP/cd2" "$ISO_DIR/DOMINION2.iso" >/dev/null

echo "== Unpacking game files"
rm -rf "$GAME"
python3 -I "$HERE/is5x.py" "$TMP/cd1/SETUP/DATA1.CAB" "$GAME" 2>/dev/null
# The Mplayer.com lobby client is dead; leave it out.
rm -rf "$GAME"/{System,Programs,Art,Commands,Sounds} "$GAME"/mpdomin.mpi

echo "== Copying cutscenes from both discs"
mkdir -p "$GAME/CINE"
cp "$TMP"/cd1/CINE/*.SMK "$GAME/CINE/"
cp -n "$TMP"/cd2/CINE/*.SMK "$GAME/CINE/"
cp "$TMP/cd1/Manual.pdf" "$GAME/" 2>/dev/null || true

echo "== Patching"
python3 -I "$HERE/patch_game.py" "$GAME"

echo "== Creating Wine prefix"
wineboot -i >/dev/null 2>&1 || true

echo "== Writing Dominion.ini (VideoMode=$MODE)"
# The game reads C:\windows\Dominion.ini. Backslashes in *Dir values are escaped (\\).
python3 -I - "$GAME/ISO.R32" "$WINEPREFIX/drive_c/windows/Dominion.ini" "$MODE" <<'EOF'
import re, sys
src, dst, mode = sys.argv[1:4]
d = open(src, 'rb').read()
i = d.index(b'[Dominion]')
t = d[i:d.index(b'\0', i)].decode('latin-1').replace('\r\n', '\n')
t = re.sub(r'^VideoMode=\d+', 'VideoMode=' + mode, t, flags=re.M)
t = re.sub(r'^CineDir=.*$', r'CineDir=.\\\\CINE\\\\', t, flags=re.M)
t = re.sub(r'^MapsDir=.*$', r'MapsDir=.\\\\', t, flags=re.M)
open(dst, 'w', newline='').write(t.replace('\n', '\r\n'))
EOF

H=$((MODE * 3 / 4)); [ "$MODE" = 1280 ] && H=1024
echo "$MODE" > "$DH/video_mode"
echo "${MODE}x${H}" > "$DH/desktop_size"
# No crash dialog; write a backtrace to the terminal instead.
wine reg add 'HKCU\Software\Wine\WineDbg' /v ShowCrashDialog /t REG_DWORD /d 0 /f >/dev/null 2>&1

echo "== Done. Start the game with: $HERE/run_wine.sh"
