#!/usr/bin/env python3
"""callees.py LO HI: every call target inside [LO,HI) of the original DLL, with
our ported name (or 'orig'), its ret size, and how often it is called."""
import re, subprocess, sys, collections
D = 'work/dh/game/ISO.R32.orig'
lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
ported = {}
for l in open('src/iso/ported.h'):
    m = re.match(r'PORTED\(0x([0-9a-f]+),\s*(\w+)\)', l)
    if m: ported[int(m.group(1), 16)] = m.group(2)
asm = subprocess.run(['objdump', '-d', '--no-show-raw-insn', '-M', 'intel', f'--start-address={lo:#x}', f'--stop-address={hi:#x}', D], capture_output=True, text=True).stdout
cnt = collections.Counter(); ind = collections.Counter()
for l in asm.splitlines():
    m = re.search(r'\tcall\s+0x([0-9a-f]+)', l)
    if m: cnt[int(m.group(1), 16)] += 1
    m = re.search(r'\tcall\s+DWORD PTR ds:0x([0-9a-f]+)', l)
    if m: ind[int(m.group(1), 16)] += 1
def ret(a):
    o = subprocess.run(['objdump', '-d', '--no-show-raw-insn', '-M', 'intel', f'--start-address={a:#x}', f'--stop-address={a+0x3000:#x}', D], capture_output=True, text=True).stdout
    for l in o.splitlines():
        m = re.search(r'\tret\s*(0x[0-9a-f]+)?', l)
        if m: return int(m.group(1), 16) if m.group(1) else 0
names = {}
for l in open('work/decomp/ISO.c'):
    m = re.match(r'// ==== (\S+) @ ([0-9a-f]+)', l)
    if m: names[int(m.group(2), 16)] = m.group(1)
for a in sorted(cnt):
    print(f'{a:#x} {ported.get(a, "orig:" + names.get(a, "?")):32} ret {ret(a)} x{cnt[a]}')
for a in sorted(ind): print(f'import {a:#x} x{ind[a]}')
