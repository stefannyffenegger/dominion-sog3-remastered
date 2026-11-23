#!/usr/bin/env python3
"""Which not-yet-rewritten ISO.R32 functions do the test scenarios reach?

  coverage.py list FILE         write the trace list (for DOMINION_ISO_TRACE)
  coverage.py report DIR...     sum up the reached.txt files under work/cmp/scn/<name>
"""
import re, sys
END = 0x100f46f4  # library stubs start here

def unported():
    done = {int(m, 16) for m in re.findall(r'PORTED\(0x([0-9a-f]+)', open('src/iso/ported.h').read())}
    rows = [l.split() for l in open('work/tmp/iso/by_addr.txt')]
    return {int(r[0], 16): (r[1], int(r[2])) for r in rows
            if len(r) >= 3 and int(r[0], 16) < END and int(r[0], 16) not in done}

fns = unported()
if sys.argv[1] == 'list':
    with open(sys.argv[2], 'w') as f:
        for a, (n, s) in sorted(fns.items()):
            if s >= 5:  # the trace hook overwrites five bytes
                f.write(f'{a:08x} {n}\n')
else:
    total = set()
    for d in sys.argv[2:]:
        got = {int(l.split()[0], 16) for l in open(d + '/reached.txt') if l.strip()} & set(fns)
        new = got - total
        total |= got
        print(f'{d.rstrip("/").split("/")[-1]:16} reached {len(got):4}  new {len(new):4}  new bytes {sum(fns[a][1] for a in new):7}')
    print(f'together {len(total)} of {len(fns)} functions, {sum(fns[a][1] for a in total)} of {sum(s for _, s in fns.values())} bytes')
