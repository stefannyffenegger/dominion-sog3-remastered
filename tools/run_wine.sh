#!/usr/bin/env bash
# Start the Wine install made by setup_wine.sh.
# The game runs in a Wine virtual desktop that matches its video mode (VideoMode in
# Dominion.ini, which the game's options can change; see desktop_size.sh): the engine
# re-reads the screen size, and a larger real desktop breaks its drawing code.
set -euo pipefail
DH=${DOMINION_HOME:-$HOME/.local/share/dominion-remastered}
export WINEPREFIX="$DH/prefix" WINEDEBUG=${WINEDEBUG:--all}
SIZE=$("$(dirname "$0")/desktop_size.sh" "$DH")
cd "$DH/game"
exec wine explorer /desktop=dominion,"$SIZE" "$(winepath -w "$DH/game/dominion.exe")"
