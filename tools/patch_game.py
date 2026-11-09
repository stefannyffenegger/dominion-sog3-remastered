"""Apply compatibility patches to an installed Dominion: Storm Over Gift 3 (v1.1).

Each patch checks the original bytes first, so it refuses to touch unknown versions.
A .orig backup is written the first time a file is patched.

Usage: python3 tools/patch_game.py <game_dir> [--list]

Patches:
  nocd     WAVEPLAY.R32  DSCheckCDIdentity -> always "found" (mov eax,1; ret)
  palette  Rtlib32.dll   RtSetDisplayMode: treat the display as a 256-color palette
                         device even when GDI reports true color. Without it, modern
                         systems take an untested true-color path whose screen
                         transition reads compressed sprites as raw ones and crashes
                         on the main menu.
"""
import os, shutil, struct, sys

# (name, file, rva, original bytes, patched bytes)
PATCHES = [
    ("nocd", "WAVEPLAY.R32", 0x65F4, "558bec81ec64010000", "b801000000c3"),
    # mov byte [eax+0x95ed], 0  ->  mov byte [eax+0x95ed], 1  (two call sites)
    ("palette", "Rtlib32.dll", 0x1D04A, "c680ed95000000", "c680ed95000001"),
    ("palette", "Rtlib32.dll", 0x1DC02, "c680ed95000000", "c680ed95000001"),
    # sprite engine's own copy of the flag (sprite+0x9fc): without it no GDI or
    # DirectDraw palette is ever created and the 8-bit screen stays black.
    # and eax,0x100  ->  mov eax,0x100
    ("palette", "Rtlib32.dll", 0x4C752, "2500010000", "b800010000"),
    ("palette", "Rtlib32.dll", 0x55685, "2500010000", "b800010000"),
    # mov dword [eax+0x9fc], 0  ->  1
    ("palette", "Rtlib32.dll", 0x55EB3, "c780fc09000000000000", "c780fc09000001000000"),
]


def rva_to_offset(d, rva):
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    sh = pe + 24 + struct.unpack_from("<H", d, pe + 20)[0]
    for i in range(nsec):
        _, vsize, va, rsize, raw = struct.unpack_from("<8sIIII", d, sh + 40 * i)
        if va <= rva < va + max(vsize, rsize):
            return rva - va + raw
    raise ValueError("RVA not in any section")


def find_file(game_dir, name):
    for f in os.listdir(game_dir):
        if f.lower() == name.lower():
            return os.path.join(game_dir, f)
    raise FileNotFoundError(name)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    if "--list" in sys.argv:
        for p in PATCHES:
            print(f"{p[0]:8s} {p[1]:13s} rva=0x{p[2]:x}")
        return
    game_dir = sys.argv[1]
    for name, fname, rva, orig, new in PATCHES:
        path = find_file(game_dir, fname)
        data = bytearray(open(path, "rb").read())
        off = rva_to_offset(data, rva)
        orig_b, new_b = bytes.fromhex(orig), bytes.fromhex(new)
        cur = bytes(data[off:off + len(new_b)])
        if cur == new_b:
            print(f"{name}: {fname}@0x{rva:x} already patched")
            continue
        if bytes(data[off:off + len(orig_b)]) != orig_b:
            sys.exit(f"{name}: {fname}@0x{rva:x} unexpected bytes - different game version?")
        if not os.path.exists(path + ".orig"):
            shutil.copy2(path, path + ".orig")
        data[off:off + len(new_b)] = new_b
        open(path, "wb").write(data)
        print(f"{name}: patched {fname}@0x{rva:x} (file offset 0x{off:x})")


if __name__ == "__main__":
    main()
