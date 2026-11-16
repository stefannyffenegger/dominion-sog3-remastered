# usage: reach.py lo hi -> unported functions in [lo,hi) with their unported callers
import re,sys,bisect
ported={int(m,16) for m in re.findall(r'PORTED\(0x([0-9a-f]+)',open('src/iso/ported.h').read())}
rows=[l.split() for l in open('work/tmp/iso/by_addr.txt')]
addrs=[int(r[0],16) for r in rows]; names={int(r[0],16):r[1] for r in rows}
import os
live={int(l.split()[0],16) for f in ('work/tmp/iso/live.txt',) if os.path.exists(f) for l in open(f)}
lo,hi=int(sys.argv[1],16),int(sys.argv[2],16)
callers={}
for l in open('work/tmp/iso/iso.asm'):
    m=re.match(r'([0-9a-f]{8}):\s+(?:call|jmp)\s+0x([0-9a-f]{8})',l)
    if m:
        t=int(m.group(2),16)
        if lo<=t<hi and t in names:
            a=int(m.group(1),16); f=addrs[bisect.bisect_right(addrs,a)-1]
            if f!=t: callers.setdefault(t,set()).add(f)
for i,a in enumerate(addrs):
    if lo<=a<hi and a not in ported:
        cs=callers.get(a,set()); un=[c for c in cs if c not in ported]
        print(f'{a:08x} {addrs[i+1]-a:5d} {"LIVE" if a in live else "    "} callers {len(cs)} unported: '+' '.join(f'{c:08x}' for c in sorted(un)[:8]))
