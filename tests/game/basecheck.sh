#!/usr/bin/env bash
# usage: basecheck.sh OUTDIR - the base test on Darken mission 12 with orders on fixed ticks
# (start, place and sell a Power Plant): three runs on the original and two on ours, compared
# tick by tick. Ours must agree with the original at least as far as the original agrees
# with itself.
cd "$(dirname "$(readlink -f "$0")")/../.."
O=${1:-work/cmp/base}; mkdir -p "$O"; R=$PWD
export SCENARIO_NOTRACE=1 DOMINION_ISO_STATELOG_ORDERS=2 DOMINION_ISO_STATELOG_CLOCK=15
run() { DOMINION_ISO_STATELOG="Z:${R//\//\\}\\${O//\//\\}\\$1.log" tests/game/scenario.sh Darken12 "$O/$1" 90 >/dev/null 2>&1; }
for n in o1 o2 o3; do DOMINION_ISO_ORIGINAL="all,-IsoDisplayLoop" run $n; done
for n in p1 p2; do run $n; done
python3 - "$O" <<'P'
import sys, itertools; sys.path.insert(0,'tools/iso'); from statecmp import rd, first_diff
d=sys.argv[1]; L={n:rd(f'{d}/{n}.log') for n in ('o1','o2','o3','p1','p2')}
def agree(x,y):
    c,e=first_diff(x,y); return e[0] if e else max(c)+1
oo=[agree(L[a],L[b]) for a,b in itertools.combinations(('o1','o2','o3'),2)]
print('original against original, split at ticks', sorted(oo))
for p in ('p1','p2'): print(p,'against the originals, split at ticks', sorted(agree(L[o],L[p]) for o in ('o1','o2','o3')))
print('ours against ours', agree(L['p1'],L['p2']))
P
