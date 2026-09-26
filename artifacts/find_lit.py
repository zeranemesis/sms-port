"""Find which functions of our object reference a given .sdata2 literal.

Usage: python artifacts/find_lit.py <relative-object-path> <hex-bytes>

Locates the .sdata2 symbol whose contents equal the requested big-endian hex
bytes, then reports every instruction that pools that symbol, grouped by
enclosing function.
"""

import os
import re
import subprocess
import sys
import tempfile

OBJDUMP = "build/binutils/powerpc-eabi-objdump.exe"
OBJCOPY = "build/binutils/powerpc-eabi-objcopy.exe"


def main():
    rel = sys.argv[1]
    want = bytes.fromhex(sys.argv[2])

    fd, path = tempfile.mkstemp(suffix=".bin")
    os.close(fd)
    try:
        subprocess.run([OBJCOPY, "-O", "binary", "--only-section=.sdata2",
                        "build/GMSP01/src/%s.o" % rel, path],
                       capture_output=True)
        blob = open(path, "rb").read()
    finally:
        os.unlink(path)

    off = blob.find(want)
    if off < 0:
        print("literal %s not found in our .sdata2" % want.hex())
        return

    syms = subprocess.run([OBJDUMP, "-t", "build/GMSP01/src/%s.o" % rel],
                          capture_output=True, text=True).stdout
    name = None
    for line in syms.splitlines():
        p = line.split()
        if len(p) >= 6 and p[3] == '.sdata2':
            try:
                addr, size = int(p[0], 16), int(p[4], 16)
            except ValueError:
                continue
            if addr <= off < addr + size:
                name = p[5]
                break
    if name is None:
        print("no symbol covers .sdata2+0x%x" % off)
        return

    print("literal %s is %s (at .sdata2+0x%x), referenced from:"
          % (want.hex(), name, off))
    out = subprocess.run([OBJDUMP, "-d", "-j", ".text",
                          "build/GMSP01/src/%s.o" % rel],
                         capture_output=True, text=True).stdout
    fn = None
    for line in out.splitlines():
        m = re.match(r"^([0-9a-f]+) <(.+)>:", line)
        if m:
            fn = m.group(2)
            continue
        if name in line:
            print("  %-72s %s" % (fn[-72:], line.strip()))


if __name__ == '__main__':
    main()
