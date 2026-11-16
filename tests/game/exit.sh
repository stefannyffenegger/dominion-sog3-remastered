#!/usr/bin/env bash
# usage: exit.sh - starts the game, goes to the main menu, closes it with Alt+F4;
# prints the number of game processes before (expected 2) and after (expected 0)
cd "$(dirname "$(readlink -f "$0")")/../.."
export DOMINION_HOME=${DOMINION_HOME:-$PWD/work/dh2}
X=tools/xvfb_session.sh; D=${XVFB_DISPLAY:-:99}
$X start >/dev/null 2>&1; sleep 20
for i in 1 2 3 4; do DISPLAY=$D xdotool key Escape; sleep 3; done; sleep 6
echo "running: $(pgrep -f 'game.dominion.exe$' | wc -l)"
DISPLAY=$D xdotool key alt+F4; sleep 8
echo "after alt+F4: $(pgrep -f 'game.dominion.exe$' | wc -l)"
$X stop >/dev/null 2>&1
