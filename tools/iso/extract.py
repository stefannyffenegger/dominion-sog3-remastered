# usage: extract.py LO HI [OUT] - the decompile (work/decomp/ISO.c) of the functions at
# addresses LO <= a < HI (hex), in address order, written to OUT (default: stdout), and a
# list of them with their length in lines on stderr
import re, sys
lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
src = open('work/decomp/ISO.c').read()
parts = [p for p in re.split(r'(?m)^(?=// ==== )', src)
         if (m := re.match(r'// ==== \S+ @ ([0-9a-f]+)', p)) and lo <= int(m.group(1), 16) < hi]
parts.sort(key=lambda p: int(re.match(r'// ==== \S+ @ ([0-9a-f]+)', p).group(1), 16))
for p in parts:
    m = re.match(r'// ==== (\S+) @ ([0-9a-f]+)', p)
    print(m.group(2), m.group(1), p.count('\n'), file=sys.stderr)
out = open(sys.argv[3], 'w') if len(sys.argv) > 3 else sys.stdout
out.write(''.join(parts))
