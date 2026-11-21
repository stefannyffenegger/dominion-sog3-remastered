#!/usr/bin/env bash
# usage: exit.sh - starts the game, goes to the main menu, closes it with Alt+F4;
# prints the number of game processes before (expected 2) and after (expected 0)
cd "$(dirname "$(readlink -f "$0")")/../.."
export DOMINION_HOME=${DOMINION_HOME:-$PWD/work/dh2}
X=tools/xvfb_session.sh; D=${XVFB_DISPLAY:-:99}
# count only this install's game processes (another install may be running on another display)
count() { n=0; for p in $(pgrep -f 'game.dominion.exe$'); do
  tr '\0' '\n' </proc/$p/environ 2>/dev/null | grep -qx "WINEPREFIX=$DOMINION_HOME/prefix" && n=$((n+1)); done; echo $n; }
$X start >/dev/null 2>&1; sleep 20
for i in 1 2 3 4; do DISPLAY=$D xdotool key Escape; sleep 3; done; sleep 6
echo "running: $(count)"
DISPLAY=$D xdotool key alt+F4; sleep 8
echo "after alt+F4: $(count)"
$X stop >/dev/null 2>&1
