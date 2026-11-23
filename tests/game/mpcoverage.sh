#!/usr/bin/env bash
# usage: mpcoverage.sh OUTDIR [SECONDS] - a two-player match on one machine (tools/mp_test.sh)
# with the coverage trace on both games: OUTDIR/reached.txt lists the functions not yet
# rewritten that either game ran, with a screenshot of each player every 30 seconds.
cd "$(dirname "$(readlink -f "$0")")/../.."
R=$PWD; O=$1; T=${2:-120}; mkdir -p "$O"; rm -f "$O"/*.png "$O/reached.txt"
export DOMINION_HOME=$R/work/dh2 DOMINION_HOME2=$R/work/dh3 WINEDEBUG=+debugstr
python3 tools/iso/coverage.py list work/tmp/iso/trace_cov.txt
export DOMINION_ISO_TRACE="Z:${R//\//\\}\\work\\tmp\\iso\\trace_cov.txt"
tools/mp_test.sh start >/dev/null 2>&1
for ((t = 30; t <= T; t += 30)); do
    sleep 30
    DISPLAY=:99 magick import -window root "$O/h$t.png"; DISPLAY=:98 magick import -window root "$O/j$t.png"
done
tools/mp_test.sh stop >/dev/null 2>&1; sleep 2
for h in "$DOMINION_HOME" "$DOMINION_HOME2"; do
    grep -aoE "first call [0-9a-f]+ [A-Za-z_0-9]+" "$h/xvfb_game.log" | awk '{print $3, $4}'
done | sort -u > "$O/reached.txt"
grep -ac "page fault" "$DOMINION_HOME/xvfb_game.log" "$DOMINION_HOME2/xvfb_game.log"
wc -l < "$O/reached.txt" | sed 's/^/two players reached: /'
