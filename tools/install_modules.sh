#!/usr/bin/env bash
# Copy our rebuilt modules into an installed game, keeping the originals as *.orig.
#
#   tools/install_modules.sh [game_dir] [build_dir]     install every module we have built
#   tools/install_modules.sh --restore [game_dir]       put the original modules back
#
# game_dir defaults to $DOMINION_HOME/game, build_dir to ./build
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
DH=${DOMINION_HOME:-$HOME/.local/share/dominion-remastered}
MODULES="BmpFont.r32 Smacker.r32 WAVEPLAY.R32 NETPLAY.R32 Rtlib32.dll ISO.R32"

if [ "${1:-}" = --restore ]; then
    GAME=${2:-$DH/game}
    # only our modules: the byte-patched files (patch_game.py) stay patched
    for name in $MODULES; do
        orig=$GAME/$name.orig
        [ -e "$orig" ] || continue
        cp -p "$orig" "$GAME/$name"
        echo "restored $name"
    done
    rm -f "$GAME/WAVEPLAY_orig.R32" "$GAME/Rtlib32_orig.dll" "$GAME/Rtlib32_orig.dll.orig" "$GAME/ISO_orig.dll"
    # the original WAVEPLAY needs the no-CD patch again
    python3 -I "$HERE/patch_game.py" "$GAME" >/dev/null
    exit 0
fi

GAME=${1:-$DH/game}
BUILD=${2:-$(dirname "$HERE")/build}
[ -d "$GAME" ] || { echo "game folder not found: $GAME"; exit 1; }

install_one() { # built file, name in game folder
    local src=$1 dst=$GAME/$2
    [ -f "$src" ] || return 0
    [ -f "$dst.orig" ] || cp -p "$dst" "$dst.orig"
    cp "$src" "$dst"
    echo "installed $2"
}
install_one "$BUILD/src/bmpfont/BmpFont.r32" BmpFont.r32
install_one "$BUILD/src/smacker/Smacker.r32" Smacker.r32
install_one "$BUILD/src/waveplay/WAVEPLAY.R32" WAVEPLAY.R32
install_one "$BUILD/src/netplay/NETPLAY.R32" NETPLAY.R32
# The engine: ours wraps the original, which it loads as Rtlib32_orig.dll. The .orig
# backup is the unpatched original; patch_game.py patches the wrapped copy.
if [ -f "$BUILD/src/rtlib/Rtlib32.dll" ]; then
    install_one "$BUILD/src/rtlib/Rtlib32.dll" Rtlib32.dll
    cp -p "$GAME/Rtlib32.dll.orig" "$GAME/Rtlib32_orig.dll"
    python3 -I "$HERE/patch_game.py" "$GAME" palette | grep -v "already patched" || true
fi

# The game logic: ours wraps the original, which it loads as ISO_orig.dll.
if [ -f "$BUILD/src/iso/ISO.R32" ]; then
    install_one "$BUILD/src/iso/ISO.R32" ISO.R32
    cp -p "$GAME/ISO.R32.orig" "$GAME/ISO_orig.dll" # a forward needs the .dll name
fi

if [ -f "$BUILD/src/waveplay/WAVEPLAY.R32" ]; then
    rm -f "$GAME/WAVEPLAY_orig.R32" # left over from the step-1 module
    # music ripped with tools/rip_music.sh
    if [ -d "$DH/music" ] && [ ! -e "$GAME/music" ]; then
        ln -s ../music "$GAME/music"
        echo "linked music from $DH/music"
    fi
    [ -e "$GAME/music" ] || echo "no music found: rip it with tools/rip_music.sh"
fi
