#!/usr/bin/env bash
# Two-player LAN test on one machine: two Wine installs, each on its own hidden Xvfb
# display, host and join over TCP/IP, start 2_2Prong. Expects our NETPLAY.R32: the
# joiner types the address into the game's own "Enter host address" box. (The original
# module shows DirectPlay's "Locate Session" dialog instead.)
#
#   tools/mp_test.sh setup   create the second install ($DOMINION_HOME2) next to the first
#   tools/mp_test.sh start   start both games, host + join, start the match
#   tools/mp_test.sh shot    screenshot both displays into work/screens/mp_{host,join}.png
#   tools/mp_test.sh stop    stop both
#
# MP_HOST_ORIGINAL=all or MP_JOIN_ORIGINAL=all runs that player on the original ISO code,
# to test our network messages against the original's. MP_HOST_LOG and MP_JOIN_LOG (Windows
# paths without spaces) write each player's unit state log.
#
# Click positions are for the 640x480 menus. Both games use Wine's GDI renderer while the
# test runs: with OpenGL on Xvfb (software rendering) the second game froze in a screen
# flip. GDI sometimes shows a garbled frame instead; take several shots.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
DH1=${DOMINION_HOME:-$HOME/.local/share/dominion-remastered}
DH2=${DOMINION_HOME2:-$DH1-player2}
X="$HERE/xvfb_session.sh"
host() { env ${MP_HOST_ORIGINAL:+DOMINION_ISO_ORIGINAL=$MP_HOST_ORIGINAL} ${MP_HOST_LOG:+DOMINION_ISO_STATELOG=$MP_HOST_LOG} DOMINION_HOME=$DH1 XVFB_DISPLAY=:99 "$X" "$@"; }
join() { env ${MP_JOIN_ORIGINAL:+DOMINION_ISO_ORIGINAL=$MP_JOIN_ORIGINAL} ${MP_JOIN_LOG:+DOMINION_ISO_STATELOG=$MP_JOIN_LOG} DOMINION_HOME=$DH2 XVFB_DISPLAY=:98 "$X" "$@"; }
type_on() { DISPLAY=$1 xdotool type --delay 100 "$2"; }
clear_field() { for _ in $(seq 20); do DISPLAY=$1 xdotool key BackSpace; done; }
renderer() { # gdi | default
    local dh
    for dh in "$DH1" "$DH2"; do
        if [ "$1" = gdi ]; then
            WINEPREFIX=$dh/prefix wine reg add 'HKCU\Software\Wine\Direct3D' /v renderer /t REG_SZ /d gdi /f
        else
            WINEPREFIX=$dh/prefix wine reg delete 'HKCU\Software\Wine\Direct3D' /v renderer /f
        fi >/dev/null 2>&1 || true
        WINEPREFIX=$dh/prefix wineserver -w
    done
}

# main menu -> Multi Player -> TCP/IP -> callsign
to_session_list() {
    local who=$1 disp=$2 name=$3
    for _ in 1 2 3 4; do $who key Escape; sleep 3; done; sleep 5
    $who click 320 136; sleep 4
    $who click 260 171; sleep 3
    $who click 350 326; clear_field "$disp"; type_on "$disp" "$name"; $who key Return; sleep 6
}

case "${1:-}" in
setup)
    mkdir -p "$DH2"
    ln -sfn "$DH1/game" "$DH2/game"
    cp "$DH1/desktop_size" "$DH1/video_mode" "$DH2/"
    export WINEPREFIX="$DH2/prefix" WINEDEBUG=-all
    wineboot -i >/dev/null 2>&1 || true
    cp "$DH1/prefix/drive_c/windows/Dominion.ini" "$DH2/prefix/drive_c/windows/"
    for dll in dplay dplayx dpwsock dpwsockx; do
        wine reg add 'HKCU\Software\Wine\DllOverrides' /v "$dll" /t REG_SZ /d native /f >/dev/null 2>&1
    done
    wine reg add 'HKCU\Software\Wine\WineDbg' /v ShowCrashDialog /t REG_DWORD /d 0 /f >/dev/null 2>&1
    wineserver -w
    ;;
start)
    renderer gdi
    host start; join start; sleep 20
    to_session_list host :99 host
    host click 320 381; sleep 5           # Create
    host click 400 195; clear_field :99; type_on :99 testgame; host key Return; sleep 8
    to_session_list join :98 joiner
    join click 328 321; sleep 3           # Search: "Enter host address" (blank = search the LAN)
    clear_field :98; type_on :98 127.0.0.1
    join key Return; sleep 10             # finds the game and joins it
    join click 86 108; sleep 2            # Go (ready)
    host click 86 93; sleep 4
    host click 330 381                    # Start game
    echo "match starting"
    ;;
shot)
    mkdir -p "$ROOT/work/screens"
    host shot "$ROOT/work/screens/mp_host.png"; join shot "$ROOT/work/screens/mp_join.png"
    ;;
stop)
    host stop; join stop
    renderer default
    ;;
*)
    sed -n '2,10p' "$0"; exit 1
    ;;
esac
