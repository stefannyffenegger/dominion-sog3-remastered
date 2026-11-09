"""Build a logging stand-in for one of the game's DLLs.

The stand-in has the same exports. Each call writes the export name and the first
8 stack arguments to the Wine debug log (OutputDebugString; run with
WINEDEBUG=+debugstr), then jumps to the same export in the renamed original.
The call itself is untouched, so it works for cdecl and stdcall alike.

Usage: python3 tools/make_proxy.py <original dll> <renamed original name> <out dir> [max calls logged per export]
  e.g. python3 tools/make_proxy.py work/bin/Smacker.r32 Smacker_orig.r32 work/proxy/smacker
Then copy the original to <game>/<renamed original name> and <out dir>/proxy.dll to <game>/<original name>.
"""
import os
import struct
import subprocess
import sys


def exports(path):
    d = open(path, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    opt = pe + 24
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    sh = opt + struct.unpack_from("<H", d, pe + 20)[0]
    secs = [struct.unpack_from("<8sIIII", d, sh + 40 * i) for i in range(nsec)]

    def off(rva):
        for _, vs, va, rs, raw in secs:
            if va <= rva < va + max(vs, rs):
                return rva - va + raw
        raise ValueError(rva)

    e = off(struct.unpack_from("<I", d, opt + 96)[0])
    base = struct.unpack_from("<I", d, e + 16)[0]
    _, nn, af, an, ao = struct.unpack_from("<IIIII", d, e + 20)
    out = []
    for i in range(nn):
        name = d[off(struct.unpack_from("<I", d, off(an) + 4 * i)[0]):].split(b"\0")[0].decode()
        ordinal = struct.unpack_from("<H", d, off(ao) + 2 * i)[0] + base
        out.append((ordinal, name))
    return sorted(out)


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    src, orig_name, outdir = sys.argv[1:4]
    max_calls = int(sys.argv[4]) if len(sys.argv) > 4 else 20
    exps = exports(src)
    os.makedirs(outdir, exist_ok=True)
    n = len(exps)

    c = [
        "#include <windows.h>",
        f"static const char *names[{n}] = {{" + ", ".join(f'"{nm}"' for _, nm in exps) + "};",
        f"void *proxy_target[{n}];",
        f"static LONG counts[{n}];",
        "static void load(void) {",
        f'    HMODULE h = LoadLibraryA("{orig_name}");',
        "    if (!h) { OutputDebugStringA(\"proxy: cannot load original\\n\"); return; }",
        f"    for (int i = 0; i < {n}; i++) proxy_target[i] = (void *)GetProcAddress(h, names[i]);",
        "}",
        # called with all registers saved; s points at: idx, return address, args...
        "void __cdecl proxy_log(DWORD *s) {",
        "    DWORD i = s[0];",
        "    if (!proxy_target[0]) load();",
        f"    if (InterlockedIncrement(&counts[i]) > {max_calls}) return;",
        "    char buf[300];",
        '    wsprintfA(buf, "proxy %s(%08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx) ret=%08lx\\n",',
        "              names[i], s[2], s[3], s[4], s[5], s[6], s[7], s[8], s[9], s[1]);",
        "    OutputDebugStringA(buf);",
        "}",
        "BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p) { (void)h; (void)r; (void)p; return TRUE; }",
    ]
    for i, (_, nm) in enumerate(exps):
        c += [
            f"__attribute__((naked)) void stub{i}(void) {{",
            "    __asm__ volatile(",
            f'        "pushl ${i}\\n\\t"',
            '        "pushal\\n\\t"',
            '        "leal 32(%esp), %eax\\n\\t"',  # -> idx
            '        "pushl %eax\\n\\t"',
            '        "call _proxy_log\\n\\t"',
            '        "addl $4, %esp\\n\\t"',
            '        "popal\\n\\t"',
            '        "addl $4, %esp\\n\\t"',
            f'        "jmp *_proxy_target+{4 * i}\\n\\t");',
            "}",
        ]
    open(os.path.join(outdir, "proxy.c"), "w").write("\n".join(c) + "\n")
    df = [f"LIBRARY {os.path.basename(src)}", "EXPORTS"]
    df += [f"    {nm}=stub{i} @{o}" for i, (o, nm) in enumerate(exps)]
    open(os.path.join(outdir, "proxy.def"), "w").write("\n".join(df) + "\n")
    subprocess.check_call([
        "i686-w64-mingw32-gcc", "-O1", "-shared", "-o", os.path.join(outdir, "proxy.dll"),
        os.path.join(outdir, "proxy.c"), os.path.join(outdir, "proxy.def"),
        "-nostdlib", "-Wl,-e,_DllMain@12", "-lkernel32", "-luser32",
    ])
    print(f"{n} exports -> {outdir}/proxy.dll")


if __name__ == "__main__":
    main()
