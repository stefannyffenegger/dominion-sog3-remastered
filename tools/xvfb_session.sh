#!/usr/bin/env bash
# Run the Wine install on a hidden Xvfb display for automated testing.
#
#   tools/xvfb_session.sh start       start Xvfb + the game (log: $DOMINION_HOME/xvfb_game.log)
#   tools/xvfb_session.sh shot FILE   save a screenshot of the hidden display
#   tools/xvfb_session.sh key KEY...  send keys (xdotool names, e.g. Escape Return)
#   tools/xvfb_session.sh click X Y   left-click at game coordinates
#   tools/xvfb_session.sh stop        stop the game and Xvfb
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
DH=${DOMINION_HOME:-$HOME/.local/share/dominion-remastered}
DISP=${XVFB_DISPLAY:-:99}
SIZE=$(cat "$DH/desktop_size" 2>/dev/null || echo 640x480)
export WINEPREFIX="$DH/prefix"

case "${1:-}" in
start)
    Xvfb "$DISP" -screen 0 "${SIZE}x24" -nolisten tcp >"$DH/xvfb.log" 2>&1 &
    echo $! >"$DH/xvfb.pid"
    for _ in $(seq 50); do xdpyinfo -display "$DISP" >/dev/null 2>&1 && break; sleep 0.1; done
    DISPLAY=$DISP WAYLAND_DISPLAY= "$HERE/run_wine.sh" >"$DH/xvfb_game.log" 2>&1 &
    echo "started on $DISP"
    ;;
shot)
    DISPLAY=$DISP magick import -window root "${2:?file}"
    ;;
key)
    shift; DISPLAY=$DISP xdotool key --delay 150 "$@"
    ;;
click)
    DISPLAY=$DISP xdotool mousemove "${2:?x}" "${3:?y}" sleep 0.2 click 1
    ;;
stop)
    wineserver -k 2>/dev/null || true
    [ -f "$DH/xvfb.pid" ] && kill "$(cat "$DH/xvfb.pid")" 2>/dev/null || true
    rm -f "$DH/xvfb.pid"
    ;;
*)
    sed -n '2,9p' "$0"; exit 1
    ;;
esac
