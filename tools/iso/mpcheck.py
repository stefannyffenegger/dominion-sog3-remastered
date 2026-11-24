#!/usr/bin/env python3
# usage: mpcheck.py HOST.log JOINER.log - compares a two-player match logged with
# DOMINION_ISO_STATELOG_UNITS=1, unit by unit. Even the original's two sides are not bit-identical
# after a sale (the leaving men differ in one flag and briefly in position), so this reports what
# must match (which units exist, type, owner, health) apart from how far positions drift, and
# each player's units at the last tick both logged. In a fight health differs even on the
# original's two sides; which units survive does not.
import sys
def rd(p):
    ticks, cur = {}, []
    for l in open(p, errors='replace'):
        if l.startswith('  '):
            cur.append(l.split())
        elif l[0].isdigit():
            ticks[int(l.split()[0])] = {u[0]: u[1:] for u in cur}
            cur = []
    return ticks
def pos(u):
    return tuple(map(int, u[2].strip('()').split(',')))
a, b = rd(sys.argv[1]), rd(sys.argv[2])
common = sorted(set(a) & set(b))
exact = [t for t in common if a[t] != b[t]]
units = [t for t in common if set(a[t]) != set(b[t])]
core = [t for t in common if any(a[t][i][0:2] + a[t][i][4:5] != b[t][i][0:2] + b[t][i][4:5] for i in set(a[t]) & set(b[t]))]
gap = lambda t: max((max(abs(p - q) for p, q in zip(pos(a[t][i]), pos(b[t][i]))) for i in set(a[t]) & set(b[t])), default=0)
gaps = [gap(t) for t in common]
print(f'{len(common)} ticks, {len(exact)} not identical, unit set differs on {len(units)}, '
      f'type/owner/health on {len(core)}, largest position gap {max(gaps)}, at the end {gaps[-1]}')
if core:
    print('  first type/owner/health difference at tick', core[0])
count = lambda side, t: ' '.join(f'p{p} {sum(u[1] == f"p{p}" for u in side[t].values())}' for p in range(2))
print(f'  units at tick {common[-1]}: host {count(a, common[-1])}, joiner {count(b, common[-1])}')
