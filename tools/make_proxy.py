"""Build a logging stand-in for one of the game's DLLs.

The stand-in has the same exports. Each call writes the export name and the first
8 stack arguments to the Wine debug log (OutputDebugString; run with
WINEDEBUG=+debugstr), then jumps to the same export in the renamed original.
The call itself is untouched, so it works for cdecl and stdcall alike.

Usage: python3 tools/make_proxy.py <original dll> <renamed original name> <out dir> [max calls logged per export] [--call]
  e.g. python3 tools/make_proxy.py work/bin/Smacker.r32 Smacker_orig.r32 work/proxy/smacker
Then copy the original to <game>/<renamed original name> and <out dir>/proxy.dll to <game>/<original name>.

--call: for modules whose exports are all cdecl. The stand-in calls the original itself
and logs after it returns: the return value and, for each argument that points to
readable memory, the dword it points to (shows output parameters). Calls that return 0
are only counted, not logged, once an export has been logged max times.
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
    call_mode = "--call" in sys.argv
    args = [a for a in sys.argv[1:] if a != "--call"]
    src, orig_name, outdir = args[0:3]
    max_calls = int(args[3]) if len(args) > 3 else 20
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
    if call_mode:
        c += [
            "typedef DWORD(__cdecl *fn8)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);",
            "static DWORD deref(DWORD p) {",
            "    if (p < 0x10000 || IsBadReadPtr((void *)p, 4)) return 0xdeadbeef;",
            "    return *(DWORD *)p;",
            "}",
            "DWORD __cdecl proxy_call(DWORD i, DWORD *a) {",
            "    if (!proxy_target[0]) load();",
            "    DWORD r = ((fn8)proxy_target[i])(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);",
            f"    if (r == 0 && counts[i] >= {max_calls}) return r;",
            f"    if (InterlockedIncrement(&counts[i]) > {max_calls} && r == 0) return r;",
            "    char buf[400];",
            '    wsprintfA(buf, "proxy %s(%08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx) = %08lx'
            ' [%lx %lx %lx %lx %lx %lx %lx]\\n", names[i], a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], r,',
            "              deref(a[1]), deref(a[2]), deref(a[3]), deref(a[4]), deref(a[5]), deref(a[6]), deref(a[7]));",
            "    OutputDebugStringA(buf);",
            "    return r;",
            "}",
        ]
        for i, (_, nm) in enumerate(exps):
            c += [
                f"DWORD __cdecl stub{i}(DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5, DWORD a6, DWORD a7) {{",
                "    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;",
                f"    return proxy_call({i}, &a0);",
                "}",
            ]
    for i, (_, nm) in enumerate([] if call_mode else exps):
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
