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
MODULES="BmpFont.r32 Smacker.r32"

if [ "${1:-}" = --restore ]; then
    GAME=${2:-$DH/game}
    # only our modules: the byte-patched files (patch_game.py) stay patched
    for name in $MODULES; do
        orig=$GAME/$name.orig
        [ -e "$orig" ] || continue
        cp -p "$orig" "$GAME/$name"
        echo "restored $name"
    done
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
