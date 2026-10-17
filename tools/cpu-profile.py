"""Names the samples written by SMS_CPU_SAMPLE (platform/cpu_sampler.cpp).

    python tools/cpu-profile.py samples.txt [--frames 900-1500] [--slow gx-profile.log]
                                [--thread <id>|busiest] [--top 40]

Prints, for the chosen thread, the functions with the most samples where the
thread was running (self) and anywhere on its stack (inclusive). --slow keeps
only frames longer than 25 ms in that gx-profile log, to see what a missed
retrace spends its time on.
"""
from pathlib import Path
import argparse, bisect, collections, ctypes, ctypes.wintypes as wt, subprocess, sys

NM = 'C:/msys64/mingw64/bin/nm.exe'
OBJDUMP = 'C:/msys64/mingw64/bin/objdump.exe'


class NmModule:
    """A MinGW image: symbols from nm, rebased from the preferred ImageBase."""
    def __init__(self, path, base):
        head = subprocess.run([OBJDUMP, '-p', path], capture_output=True, text=True).stdout
        preferred = next(int(l.split()[1], 16) for l in head.splitlines() if l.startswith('ImageBase'))
        out = subprocess.run([NM, '-C', '--defined-only', path], capture_output=True, text=True,
                             encoding='utf-8', errors='replace').stdout
        syms = []
        for line in out.splitlines():
            parts = line.split(' ', 2)
            if len(parts) == 3 and parts[1] in 'tTwW':
                syms.append((int(parts[0], 16) - preferred + base, parts[2]))
        syms.sort()
        self.addrs = [a for a, _ in syms]
        self.names = [n for _, n in syms]

    def name(self, pc):
        i = bisect.bisect_right(self.addrs, pc) - 1
        return self.names[i] if i >= 0 else None


class DbgHelp:
    """MSVC images: dbghelp reads their PDBs."""
    def __init__(self):
        # The SDK's dbghelp reads current MSVC PDBs; System32's may not.
        sdk = Path('C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/dbghelp.dll')
        self.dll = ctypes.WinDLL(str(sdk) if sdk.exists() else 'dbghelp')
        self.proc = wt.HANDLE(0x5A5A)
        self.dll.SymSetOptions(0x2 | 0x4)  # SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS
        self.dll.SymInitializeW(self.proc, None, False)
        self.dll.SymLoadModuleExW.restype = ctypes.c_uint64
        self.dll.SymLoadModuleExW.argtypes = [wt.HANDLE, wt.HANDLE, wt.LPCWSTR, wt.LPCWSTR,
                                              ctypes.c_uint64, wt.DWORD, ctypes.c_void_p, wt.DWORD]

        class SYMBOL_INFO(ctypes.Structure):
            _fields_ = [('SizeOfStruct', wt.ULONG), ('TypeIndex', wt.ULONG),
                        ('Reserved', ctypes.c_uint64 * 2), ('Index', wt.ULONG), ('Size', wt.ULONG),
                        ('ModBase', ctypes.c_uint64), ('Flags', wt.ULONG), ('Value', ctypes.c_uint64),
                        ('Address', ctypes.c_uint64), ('Register', wt.ULONG), ('Scope', wt.ULONG),
                        ('Tag', wt.ULONG), ('NameLen', wt.ULONG), ('MaxNameLen', wt.ULONG),
                        ('Name', ctypes.c_char * 512)]
        self.info = SYMBOL_INFO()
        self.info.SizeOfStruct = 88  # sizeof(SYMBOL_INFO) in C: the fixed part, padded
        self.info.MaxNameLen = 511
        self.dll.SymFromAddr.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64),
                                         ctypes.c_void_p]

    def load(self, path, base, size):
        self.dll.SymLoadModuleExW(self.proc, None, path, None, base, size, None, 0)

    def name(self, pc):
        disp = ctypes.c_uint64()
        if self.dll.SymFromAddr(self.proc, pc, ctypes.byref(disp), ctypes.byref(self.info)):
            return self.info.Name.decode('utf-8', 'replace')
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('samples')
    ap.add_argument('--frames')
    ap.add_argument('--slow')
    ap.add_argument('--thread', default='busiest')
    ap.add_argument('--top', type=int, default=40)
    args = ap.parse_args()

    samples, modules = [], []
    for line in Path(args.samples).read_text().splitlines():
        p = line.split(' ', 3)
        if p[0] == 's':
            f = line.split()
            samples.append((int(f[1]), int(f[2]), [int(x, 16) for x in f[3:]]))
        elif p[0] == 'm':
            modules.append((int(p[1], 16), int(p[2], 16), p[3]))
    modules.sort()
    starts = [m[0] for m in modules]

    if args.frames:
        lo, hi = map(int, args.frames.split('-'))
        samples = [s for s in samples if lo <= s[1] <= hi]
    if args.slow:
        slow = set()
        for l in Path(args.slow).read_text().splitlines():
            r = l.split()
            if len(r) > 1 and float(r[1]) > 25:
                slow.add(int(r[0]))
        # The profile line for frame n is written as frame n ends.
        samples = [s for s in samples if s[1] in slow]

    per_thread = collections.Counter(s[0] for s in samples)
    print('samples per thread:', ', '.join(f'{t}:{n}' for t, n in per_thread.most_common(8)))
    tid = per_thread.most_common(1)[0][0] if args.thread == 'busiest' else int(args.thread)
    samples = [s for s in samples if s[0] == tid]

    dbg = DbgHelp()
    resolvers = {}
    def module_of(pc):
        i = bisect.bisect_right(starts, pc) - 1
        if i >= 0 and pc < modules[i][0] + modules[i][1]:
            return modules[i]
        return None
    cache = {}
    def name(pc):
        if pc in cache:
            return cache[pc]
        m = module_of(pc)
        label = f'?{pc:x}'
        if m:
            base, size, path = m
            short = Path(path).name
            if path not in resolvers:
                if short.lower() in ('sms.exe',) or 'mingw64' in path.lower():
                    resolvers[path] = NmModule(path, base)
                else:
                    dbg.load(path, base, size)
                    resolvers[path] = dbg
            n = resolvers[path].name(pc)
            label = f'{short}!{n}' if n else f'{short}!+{pc - base:x}'
        cache[pc] = label
        return label

    selfc, incl = collections.Counter(), collections.Counter()
    for _, _, pcs in samples:
        names = [name(pc) for pc in pcs]
        selfc[names[0]] += 1
        for n in set(names):
            incl[n] += 1
    total = len(samples)
    print(f'thread {tid}: {total} samples')
    print('\n-- self --')
    for n, c in selfc.most_common(args.top):
        print(f'{100 * c / total:6.2f}%  {n[:150]}')
    print('\n-- inclusive --')
    for n, c in incl.most_common(args.top):
        print(f'{100 * c / total:6.2f}%  {n[:150]}')


if __name__ == '__main__':
    sys.exit(main())
