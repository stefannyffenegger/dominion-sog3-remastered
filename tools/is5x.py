"""Minimal InstallShield 5 (single-volume) cabinet extractor. Usage: is5x.py CAB OUTDIR [--list]"""
import os, struct, sys, zlib

cab_path, out = sys.argv[1], sys.argv[2]
list_only = "--list" in sys.argv
d = open(cab_path, "rb").read()
u32 = lambda o: struct.unpack_from("<I", d, o)[0]
u16 = lambda o: struct.unpack_from("<H", d, o)[0]

sig, ver, volinfo, cdo, cds = struct.unpack_from("<5I", d, 0)
assert sig == 0x28635349, "not ISc("
p = cdo + 0xC
fto = u32(p); p += 8
fts = u32(p); p += 4; p += 4
dir_count = u32(p); p += 4; p += 8
file_count = u32(p)
base = cdo + fto
table = [u32(base + 4 * i) for i in range(dir_count + file_count)]

def cstr(o):
    e = d.index(b"\0", o)
    return d[o:e].decode("latin-1")

dirs = [cstr(base + table[i]) for i in range(dir_count)]
print(f"version=0x{ver:08x} dirs={dir_count} files={file_count}", file=sys.stderr)
for i in range(file_count):
    q = base + table[dir_count + i]
    name_off = u32(q); diri = u16(q + 4); flags = u16(q + 8)
    exp = u32(q + 10); comp = u32(q + 14); doff = u32(q + 18 + 0x14)
    try:
        name = cstr(base + name_off)
    except Exception:
        print(f"skip entry {i} flags={flags:x} exp={exp}", file=sys.stderr); continue
    rel = os.path.join(dirs[diri].replace("\\", "/"), name) if diri < len(dirs) else name
    if flags & 8 or not name:
        continue
    if list_only:
        print(f"{exp:10d} {flags:02x} {rel}")
        continue
    if flags & 4:
        buf, pos, end = bytearray(), doff, doff + comp
        while pos < end:
            n = struct.unpack_from("<H", d, pos)[0]; pos += 2
            dec = zlib.decompressobj(-15)
            buf += dec.decompress(d[pos:pos + n] + b"\0")
            pos += n
        data = bytes(buf)
    else:
        data = d[doff:doff + exp]
    if len(data) != exp:
        print(f"WARN size mismatch {rel}: {len(data)} != {exp}", file=sys.stderr)
    dst = os.path.join(out, rel.lstrip("/"))
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    open(dst, "wb").write(data)
