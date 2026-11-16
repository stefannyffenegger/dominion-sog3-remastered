# fasm.py addr [addr...]: compact asm of original functions, call targets and strings named
import re,subprocess,sys,struct,bisect
D='work/dh/game/ISO.R32.orig'
rows=[l.split() for l in open('work/tmp/iso/by_addr.txt')]
addrs=[int(r[0],16) for r in rows]; names={int(r[0],16):r[1] for r in rows}
ported={int(a,16):n for a,n in re.findall(r'PORTED\(0x([0-9a-f]+),\s*(\w+)\)',open('src/iso/ported.h').read())}
d=open(D,'rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]; n=struct.unpack_from('<H',d,pe+6)[0]; opt=struct.unpack_from('<H',d,pe+20)[0]
secs=[struct.unpack_from('<8sIIII',d,pe+24+opt+i*40) for i in range(n)]
def off(va):
    rva=va-0x10000000
    for nm,vs,va_,rs,ro in secs:
        if va_<=rva<va_+max(vs,rs): return rva-va_+ro
imp={}
for l in subprocess.run(['objdump','-p',D],capture_output=True,text=True).stdout.splitlines():
    m=re.match(r'\s+(000[0-9a-f]{5})\s+<none>\s+[0-9a-f]+\s+(\w+)', l)
    if m: imp[0x10000000+int(m.group(1),16)]=m.group(2)
def s(a):
    o=off(a)
    if o is None: return None
    b=d[o:o+48].split(b'\0')[0]
    return b.decode('latin1') if len(b)>=3 and all(32<=c<127 for c in b) else None
for x in sys.argv[1:]:
    a=int(x,16); e=addrs[bisect.bisect_right(addrs,a)]
    out=subprocess.run(['objdump','-d','--no-show-raw-insn','-M','intel',f'--start-address={a:#x}',f'--stop-address={e:#x}',D],capture_output=True,text=True).stdout
    print(f'== {a:x} {names.get(a)}')
    for l in out.splitlines()[7:]:
        if not l.strip(): continue
        l=re.sub(r'call\s+0x([0-9a-f]+)',lambda m:f'call {ported.get(int(m.group(1),16)) or names.get(int(m.group(1),16),m.group(1))}',l)
        l=re.sub(r'DWORD PTR ds:0x([0-9a-f]{8})',lambda m:'['+imp[int(m.group(1),16)]+']' if int(m.group(1),16) in imp else m.group(0),l)
        l=re.sub(r'0x(10[0-9a-f]{6})\b',lambda m:m.group(0)+(f' "{s(int(m.group(1),16))}"' if s(int(m.group(1),16)) else ''),l)
        l=l.replace('ds:0xDEAD','AUD')
        print(l.strip())
