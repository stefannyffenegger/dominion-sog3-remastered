# usage: proto.py ADDR... - the decompile's prototype of each function
import re,sys
src=open('work/decomp/ISO.c').read()
for a in sys.argv[1:]:
    m=re.search(r'// ==== (\S+) @ '+a.lower()+r'\n\n(.*?)\n\{',src,re.S)
    print(a, ' '.join(m.group(2).split()) if m else '?')
