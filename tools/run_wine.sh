#!/usr/bin/env bash
# Start the Wine install made by setup_wine.sh.
# The game runs in a Wine virtual desktop that matches its video mode: the engine
# re-reads the screen size, and a larger real desktop breaks its drawing code.
set -euo pipefail
DH=${DOMINION_HOME:-$HOME/.local/share/dominion-remastered}
export WINEPREFIX="$DH/prefix" WINEDEBUG=${WINEDEBUG:--all}
SIZE=$(cat "$DH/desktop_size" 2>/dev/null || echo 640x480)
cd "$DH/game"
exec wine explorer /desktop=dominion,"$SIZE" "$(winepath -w "$DH/game/dominion.exe")"
