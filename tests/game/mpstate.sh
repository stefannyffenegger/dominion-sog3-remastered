#!/usr/bin/env bash
# usage: mpstate.sh OUTDIR PAIRING [SECONDS] - one two-player match with a unit state log on
# each side, compared tick by tick. PAIRING: oo (original on both), op (original hosts, ours
# joins), po (ours hosts, original joins), pp (ours on both). MP_ACTIONS names a script from
# tests/game/actions/ to play during the match, MP_MAP_ROW picks a map from the lobby's list
# (0 2_2Prong, the default, 1 2_Backwoods, ...). See docs/TESTING.md for the scenarios.
cd "$(dirname "$(readlink -f "$0")")/../.."
R=$PWD; O=$1; C=$2; T=${3:-60}; mkdir -p "$O"
export DOMINION_HOME=$R/work/dh2 DOMINION_HOME2=$R/work/dh3
unset MP_HOST_ORIGINAL MP_JOIN_ORIGINAL
[ "${C:0:1}" = o ] && export MP_HOST_ORIGINAL=all
[ "${C:1:1}" = o ] && export MP_JOIN_ORIGINAL=all
[ -n "${MP_JOIN_KEEP:-}" ] && export MP_JOIN_ORIGINAL=$MP_JOIN_KEEP  # functions the joiner keeps original (bisecting)
rm -f "$O/${C}_h.log" "$O/${C}_j.log"
export MP_HOST_LOG="Z:${R//\//\\}\\${O//\//\\}\\${C}_h.log" MP_JOIN_LOG="Z:${R//\//\\}\\${O//\//\\}\\${C}_j.log"
for attempt in 1 2; do # the menus sometimes miss a click before the lobby: start over once
    MP_STARTED_LOG=$O/${C}_h.log tools/mp_test.sh start | grep -v "match starting"
    grep -qa '^[0-9]' "$O/${C}_h.log" 2>/dev/null && break
    [ $attempt = 1 ] && { echo "no match, starting over"; tools/mp_test.sh stop >/dev/null 2>&1; sleep 3; rm -f "$O/${C}_h.log" "$O/${C}_j.log"; }
done
if [ -n "$MP_ACTIONS" ]; then . "$MP_ACTIONS" & fi
for ((t = 30; t <= T; t += 30)); do
    sleep 30
    DISPLAY=:99 timeout 10 magick import -window root "$O/${C}_h$t.png"; DISPLAY=:98 timeout 10 magick import -window root "$O/${C}_j$t.png"
done
tools/mp_test.sh stop >/dev/null 2>&1; sleep 2
echo "$C faults: $(grep -ac 'page fault' work/dh2/xvfb_game.log) $(grep -ac 'page fault' work/dh3/xvfb_game.log)"
python3 - "$O/${C}_h.log" "$O/${C}_j.log" <<'P'
import sys; sys.path.insert(0,'tools/iso'); from statecmp import rd, first_diff
a,b=rd(sys.argv[1]),rd(sys.argv[2]); c,d=first_diff(a,b)
print(f'  host {len(a)} ticks, joiner {len(b)}, common {len(c)}, differ {len(d)}' + (f', first at {d[0]}' if d else '') + (f', last common {c[-1]}' if c else ''))
P
