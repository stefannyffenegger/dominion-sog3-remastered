#!/usr/bin/env bash
# usage: scenario.sh SCENARIO OUTDIR [SECONDS] - plays any scenario through the normal menu
# path and records which not-yet-rewritten ISO.R32 functions ran (OUTDIR/reached.txt), with
# screenshots every 30 seconds. SCENARIO is a file name without .sdc, for example Human07.
# It runs from work/dhs, a throwaway install of symlinks to the player's game in which
# Human01.sdc is the chosen scenario, so no game file is changed. Extra environment
# (DOMINION_ISO_STATELOG and so on) is passed on. SCENARIO_ACTIONS names a script of
# interface clicks to run once the mission is on screen.
cd "$(dirname "$(readlink -f "$0")")/../.."
S=$1; O=$2; T=${3:-180}; R=$PWD
[ -f "work/dh/game/$S.sdc" ] || { echo "no scenario $S"; exit 1; }
tools/xvfb_session.sh stop >/dev/null 2>&1
rm -rf work/dhs; mkdir -p work/dhs/game "$O"; rm -f "$O"/*.png "$O/reached.txt"
cp -as "$R"/work/dh/game/. work/dhs/game/
ln -sf "$R/work/dh/game/$S.sdc" work/dhs/game/Human01.sdc
ln -s "$R/work/dh2/prefix" work/dhs/prefix
cp work/dh2/desktop_size work/dh2/video_mode work/dhs/
export DOMINION_HOME=$R/work/dhs
if [ -z "$SCENARIO_NOTRACE" ]; then # the state comparison runs without the trace
    export WINEDEBUG=+debugstr
    python3 tools/iso/coverage.py list work/tmp/iso/trace_cov.txt
    export DOMINION_ISO_TRACE="Z:${R//\//\\}\\work\\tmp\\iso\\trace_cov.txt"
fi
X=tools/xvfb_session.sh; D=${XVFB_DISPLAY:-:99}
$X start >/dev/null; sleep 15
for i in 1 2 3; do $X key Escape; sleep 3; done; sleep 6
$X click 320 75; sleep 3; $X click 320 136; sleep 4
$X click 440 151; sleep 1; $X click 320 322; sleep 6
for i in 1 2 3 4; do $X key Escape; sleep 6; done
$X click 510 425; sleep 4
DISPLAY=$D xdotool mousemove 320 200
[ -n "$SCENARIO_ACTIONS" ] && . "$SCENARIO_ACTIONS" # drives the interface: may use $X, $O, $D
for ((t = 0; t < T; t += 30)); do sleep 30; $X shot "$O/t$((t + 30)).png"; done
$X stop >/dev/null 2>&1; sleep 2
grep -aoE "first call [0-9a-f]+ [A-Za-z_0-9]+" work/dhs/xvfb_game.log | awk '{print $3, $4}' | sort -u > "$O/reached.txt"
grep -ac "page fault" work/dhs/xvfb_game.log | sed 's/^/page faults: /'
wc -l < "$O/reached.txt" | sed "s/^/$S reached: /"
