#!/usr/bin/env bash
# Prints the Wine desktop size (WxH) for an install: the game's own VideoMode setting in
# Dominion.ini, which the game's options screen can change, else the size setup_wine.sh chose.
#   tools/desktop_size.sh [dominion_home]
DH=${1:-${DOMINION_HOME:-$HOME/.local/share/dominion-remastered}}
MODE=$(sed -n 's/^VideoMode=\([0-9]*\).*/\1/p' "$DH/prefix/drive_c/windows/Dominion.ini" 2>/dev/null | head -n1)
case "$MODE" in
640 | 800 | 1024) echo "${MODE}x$((MODE * 3 / 4))" ;;
1280) echo 1280x1024 ;;
*) cat "$DH/desktop_size" 2>/dev/null || echo 640x480 ;;
esac
