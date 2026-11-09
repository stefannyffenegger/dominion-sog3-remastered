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

if [ "${1:-}" = --restore ]; then
    GAME=${2:-$DH/game}
    for orig in "$GAME"/*.r32.orig "$GAME"/*.R32.orig "$GAME"/*.dll.orig; do
        [ -e "$orig" ] || continue
        # keep the byte-patched files (patch_game.py) patched: only restore our modules
        case "$(basename "$orig")" in BmpFont.r32.orig) ;; *) continue ;; esac
        cp -p "$orig" "${orig%.orig}"
        echo "restored $(basename "${orig%.orig}")"
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
