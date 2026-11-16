# usage: statediff.py A.log B.log [N] - the unit lines that differ at the first N differing ticks
import sys
def rd(p):
    d={};cur=[]
    for l in open(p,errors='replace'):
        if l.startswith('  '): cur.append(l.strip())
        elif l[0].isdigit(): d[l.split()[0]]=(l.split()[1:],cur); cur=[]
    return d
a,b=rd(sys.argv[1]),rd(sys.argv[2]); n=int(sys.argv[3]) if len(sys.argv)>3 else 1
for t in sorted(set(a)&set(b),key=int):
    if a[t][0]!=b[t][0]:
        print('tick',t)
        for x,y in zip(a[t][1],b[t][1]):
            if x!=y: print('  A',x,'\n  B',y)
        n-=1
        if not n: break
