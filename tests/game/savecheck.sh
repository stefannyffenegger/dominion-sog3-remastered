#!/usr/bin/env bash
# usage: savecheck.sh OUTDIR - saved games on Darken mission 12. The base test's orders, saved
# at tick 600 by the original (twice) and ours; the files are compared. Then each of an
# original's and our save is loaded by both, and the state after loading is compared.
cd "$(dirname "$(readlink -f "$0")")/../.."
O=${1:-work/cmp/save}; mkdir -p "$O"; R=$PWD; ORIG="all,-IsoDisplayLoop"
export SCENARIO_NOTRACE=1 DOMINION_ISO_STATELOG_CLOCK=15
log() { echo "Z:${R//\//\\}\\${O//\//\\}\\$1.log"; }
save() { # name, original or not
    DOMINION_ISO_ORIGINAL=$2 DOMINION_ISO_STATELOG_ORDERS=2 DOMINION_ISO_STATELOG_SAVE=fixed.WSP \
        DOMINION_ISO_STATELOG="$(log "$1")" tests/game/scenario.sh Darken12 "$O/$1" 25 >/dev/null 2>&1
    cp work/dhs/game/fixed.WSP "$O/$1.WSP"
}
load() { # name, save file, original or not
    DOMINION_ISO_ORIGINAL=$3 SCENARIO_SAVE="$R/$2" SCENARIO_ACTIONS=$R/tests/game/actions/load.sh \
        DOMINION_ISO_STATELOG="$(log "$1")" tests/game/scenario.sh Darken12 "$O/$1" 40 >/dev/null 2>&1
}
[ -f "$O/sp.WSP" ] || { save so1 "$ORIG"; save so2 "$ORIG"; save sp ""; } # kept between runs
load oo "$O/so1.WSP" "$ORIG"; load po "$O/so1.WSP" ""
load op "$O/sp.WSP" "$ORIG"; load pp "$O/sp.WSP" ""
python3 - "$O" <<'P'
import sys
d=sys.argv[1]
def diff(a,b):
    x,y=open(f'{d}/{a}.WSP','rb').read(),open(f'{d}/{b}.WSP','rb').read()
    return len(x),len(y),sum(p!=q for p,q in zip(x,y))
print('save sizes and differing bytes: original/original',diff('so1','so2'),' original/ours',diff('so1','sp'))
def after(n):  # the ticks after the last time the tick went backwards (the load)
    rows=[l.split() for l in open(f'{d}/{n}.log',errors='replace') if l[0].isdigit()]
    s=0
    for i in range(1,len(rows)):
        if int(rows[i][0])<int(rows[i-1][0]): s=i
    return {int(r[0]):r[1:3] for r in rows[s:]}, s>0
L={n:after(n) for n in ('oo','po','op','pp')}
print('load happened:',{n:L[n][1] for n in L}, 'first tick',{n:min(L[n][0]) for n in L})
def agree(a,b):
    x,y=L[a][0],L[b][0]; c=sorted(set(x)&set(y)); e=[t for t in c if x[t]!=y[t]]
    return (c[0],e[0] if e else f'none up to {c[-1]}') if c else None
print('original save: original/ours load (from, first split)',agree('oo','po'))
print('our save: original/ours load',agree('op','pp'))
print('original save against our save, loaded by the original',agree('oo','op'))
P
