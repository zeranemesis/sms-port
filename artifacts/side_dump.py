"""Side-by-side hex dump of one section of two objects (32 bytes per row).

Usage: python artifacts/side_dump.py <relative-object-path> [section]
"""

import re
import subprocess
import sys

OBJDUMP = "build/binutils/powerpc-eabi-objdump.exe"


def section(obj, name):
    out = subprocess.run([OBJDUMP, "-s", "-j", name, obj],
                         capture_output=True, text=True).stdout
    data = bytearray()
    for line in out.splitlines():
        m = re.match(r"\s*([0-9a-f]{4,8})\s((?:[0-9a-f]{8}\s?){1,4})", line)
        if not m:
            continue
        for word in re.findall(r"[0-9a-f]{8}", m.group(2)):
            data += bytes.fromhex(word)
    return bytes(data)


def main():
    rel = sys.argv[1]
    name = "." + (sys.argv[2] if len(sys.argv) > 2 else "sdata2")
    t = section("build/GMSP01/obj/%s.o" % rel, name)
    o = section("build/GMSP01/src/%s.o" % rel, name)
    print("%s target %d / ours %d" % (name, len(t), len(o)))
    n = max(len(t), len(o))
    for i in range(0, n, 32):
        tk = t[i:i + 32]
        ok = o[i:i + 32]
        mark = "" if tk == ok else "   <<<"
        print("%03x  T %s" % (i, tk.hex(" ")))
        print("      O %s%s" % (ok.hex(" "), mark))


if __name__ == "__main__":
    main()
