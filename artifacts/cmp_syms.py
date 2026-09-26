"""Diff the .sdata2 (or any section) symbol tables of two objects.

Usage: python artifacts/cmp_syms.py <relative-object-path> [section]

Prints every symbol present in only one object, and every symbol whose
address or size differs, in address order.
"""

import subprocess
import sys

OBJDUMP = "build/binutils/powerpc-eabi-objdump.exe"


def syms(obj, name):
    out = subprocess.run([OBJDUMP, "-t", obj], capture_output=True,
                         text=True).stdout
    res = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 6 or parts[3] != name:
            continue
        res.append((int(parts[0], 16), int(parts[4], 16), parts[5]))
    return sorted(res)


def main():
    rel = sys.argv[1]
    name = "." + (sys.argv[2] if len(sys.argv) > 2 else "sdata2")
    t = syms("build/GMSP01/obj/%s.o" % rel, name)
    o = syms("build/GMSP01/src/%s.o" % rel, name)

    tset = {s[0]: s for s in t}
    oset = {s[0]: s for s in o}
    print("%s: target %d symbols, ours %d symbols" % (name, len(t), len(o)))

    for a in sorted(set(tset) | set(oset)):
        ts, os_ = tset.get(a), oset.get(a)
        if ts and os_:
            if ts[1] != os_[1]:
                print("  SIZE  +0x%03x  target %d %r  ours %d %r"
                      % (a, ts[1], ts[2], os_[1], os_[2]))
        elif ts:
            print("  ONLY-T +0x%03x  size %d %r" % (a, ts[1], ts[2]))
        else:
            print("  ONLY-O +0x%03x  size %d %r" % (a, os_[1], os_[2]))


if __name__ == "__main__":
    main()
