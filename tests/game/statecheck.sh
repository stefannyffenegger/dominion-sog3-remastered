#!/usr/bin/env bash
# usage: statecheck.sh - runs tests/game/statelog.sh with scripted orders on our build and
# compares the unit state, tick by tick, with the original's runs (work/cmp/state_o*.log,
# from DOMINION_ISO_ORIGINAL=all,-IsoDisplayLoop runs of tests/game/statelog.sh with
# DOMINION_ISO_STATELOG_ORDERS=1 DOMINION_ISO_STATELOG_CLOCK=15; our display loop drives the
# virtual clock).
# The original runs agree with each other up to about tick 950 (the engine's own timers
# still run on real time), so ours must agree at least as far as they agree.
cd "$(dirname "$(readlink -f "$0")")/../.."
export DOMINION_HOME=${DOMINION_HOME:-$PWD/work/dh2} DOMINION_ISO_STATELOG_ORDERS=1 DOMINION_ISO_STATELOG_CLOCK=15
L=${1:-work/cmp/state_p.log}
tests/game/statelog.sh $L 90 >/dev/null 2>&1; tools/xvfb_session.sh stop >/dev/null 2>&1
python3 - $L work/cmp/state_o*.log <<'P'
import sys, itertools; sys.path.insert(0,'tools/iso'); from statecmp import rd, first_diff
def agree(a,b):
    c,d=first_diff(a,b); return d[0] if d else max(c)+1
p=rd(sys.argv[1]); origs=[rd(f) for f in sys.argv[2:]]
print('ours equal to the original runs up to ticks', [agree(o,p) for o in origs])
print('the original runs agree with each other up to tick', min(agree(a,b) for a,b in itertools.combinations(origs,2)))
P
