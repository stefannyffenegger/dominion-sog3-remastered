#!/usr/bin/env bash
# usage: basecheck.sh OUTDIR - the base test on Darken mission 12 with orders on fixed ticks
# (start, place and sell a Power Plant): two runs on the original and one on ours, compared
# tick by tick. Ours must agree with the original at least as far as the original agrees
# with itself.
cd "$(dirname "$(readlink -f "$0")")/../.."
O=${1:-work/cmp/base}; mkdir -p "$O"; R=$PWD
export SCENARIO_NOTRACE=1 DOMINION_ISO_STATELOG_ORDERS=2 DOMINION_ISO_STATELOG_CLOCK=15
run() { DOMINION_ISO_STATELOG="Z:${R//\//\\}\\${O//\//\\}\\$1.log" tests/game/scenario.sh Darken12 "$O/$1" 90 >/dev/null 2>&1; }
DOMINION_ISO_ORIGINAL="all,-IsoDisplayLoop" run o1
DOMINION_ISO_ORIGINAL="all,-IsoDisplayLoop" run o2
run p
python3 - "$O" <<'P'
import sys; sys.path.insert(0,'tools/iso'); from statecmp import rd, first_diff
d=sys.argv[1]; a,b,p=[rd(f'{d}/{n}.log') for n in ('o1','o2','p')]
def agree(x,y):
    c,e=first_diff(x,y); return e[0] if e else max(c)+1
print('last ticks logged', max(a), max(b), max(p))
print('original runs agree up to tick', agree(a,b))
print('ours agrees with them up to ticks', agree(a,p), agree(b,p))
P
