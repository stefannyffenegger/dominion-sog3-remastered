# For each function start address given, print the first ret instruction's operand.
import sys, subprocess, re
dll = sys.argv[1]
for a in sys.argv[2:]:
    a = int(a, 16)
    out = subprocess.run(['objdump', '-d', '--no-show-raw-insn', f'--start-address={a:#x}', f'--stop-address={a+0x800:#x}', dll], capture_output=True, text=True).stdout
    m = re.search(r'\bret\s*(\$0x[0-9a-f]+)?', out)
    print(f'{a:#x}', m.group(0) if m else '?')
