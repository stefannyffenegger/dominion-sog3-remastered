# usage: retcheck.py N  -> compare ret N of the last N PORTED entries (ours vs original), and hook room
import re,subprocess,sys
dll='build/src/iso/ISO.R32'
nm=subprocess.run(['nm',dll],capture_output=True,text=True).stdout
sym={}
for l in nm.splitlines():
    p=l.split()
    if len(p)==3 and p[1] in 'Tt': sym.setdefault(re.sub(r'^_','',p[2]).split('@')[0],int(p[0],16))
def ret(path,a):
    out=subprocess.run(['objdump','-d','--no-show-raw-insn','-M','intel',f'--start-address={a:#x}',f'--stop-address={a+0x2000:#x}',path],capture_output=True,text=True).stdout
    m=re.search(r'\t(ret\s*(0x[0-9a-f]+)?|jmp\s+(e[a-d]x|[0-9a-f]+ <_\w+@\d+>))',out)
    if not m: return '?'
    if m.group(1).startswith('jmp'): return 'tail:'+m.group(1).split()[-1]
    return int(m.group(2),16) if m.group(2) else 0
starts=sorted(int(l.split()[0],16) for l in open('work/tmp/iso/by_addr.txt'))
for l in open('src/iso/ported.h').read().strip().splitlines()[-int(sys.argv[1]):]:
    mm=re.match(r'PORTED\(0x([0-9a-f]+), (\w+)\)',l)
    if not mm: continue
    a,n=mm.groups(); a=int(a,16)
    o=ret('work/dh/game/ISO.R32.orig',a); m=ret(dll,sym[n])
    if o!=m: print('CHECK',n,'orig',o,'ours',m)
    i=starts.index(a)
    if starts[i+1]-a<21: print('short',n,starts[i+1]-a)
print('done')
