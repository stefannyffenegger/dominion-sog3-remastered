#!/usr/bin/env bash
# Rip the CD-audio music from an original Dominion disc (tracks 2-6; track 1 is data).
# Put in disc 1 or 2 and run:
#   tools/rip_music.sh [device] [out_dir]
# Writes <out_dir>/<disc label>/trackNN.cdda.wav (lossless). Needs cd-paranoia (libcdio).
# Default out_dir: $DOMINION_HOME/music
set -euo pipefail
DEV=${1:-/dev/sr0}
DH=${DOMINION_HOME:-$HOME/.local/share/dominion-remastered}
OUT=${2:-$DH/music}
command -v cd-paranoia >/dev/null || { echo "cd-paranoia not found (dnf install libcdio-paranoia)"; exit 1; }

LABEL=$(blkid -o value -s LABEL "$DEV" 2>/dev/null || true)
case "$LABEL" in DOMINION1|DOMINION2) ;; *) echo "not a Dominion disc (label: '${LABEL}')"; exit 1;; esac
mkdir -p "$OUT/$LABEL"
cd "$OUT/$LABEL"
echo "== Ripping $LABEL to $PWD"
cd-paranoia -d "$DEV" -B -w "2-6"
md5sum track*.wav >md5.txt
echo "== Done. Next disc: eject with 'eject $DEV'"
