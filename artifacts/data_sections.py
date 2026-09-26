"""Show .sdata2 as floats and the .rodata diff window for one unit.

Usage: python artifacts/data_sections.py <path-to-object-relative-to-build/GMSP01> \
           [ours|target] ...
"""

import re
import struct
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


def floats(data, base=0):
    for off in range(0, len(data) - 3, 4):
        (v,) = struct.unpack(">f", data[off:off + 4])
        yield base + off, data[off:off + 4], v


def main():
    rel = sys.argv[1]
    start = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0
    end = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x100

    for tag, which in (("target", "obj"), ("ours", "src")):
        obj = "build/GMSP01/%s/%s.o" % (which, rel)
        s2 = section(obj, ".sdata2")
        print("=== %s .sdata2 (%d bytes) ===" % (tag, len(s2)))
        for off, raw, v in floats(s2):
            print("  +0x%03x  0x%08x  %-14.6g" % (off, struct.unpack(">I", raw)[0], v))

    print()
    print("=== .rodata window 0x%x..0x%x ===" % (start, end))
    blobs = {}
    for tag, which in (("target", "obj"), ("ours", "src")):
        blobs[tag] = section("build/GMSP01/%s/%s.o" % (which, rel), ".rodata")
    for off in range(start, end, 16):
        t = blobs["target"][off:off + 16]
        o = blobs["ours"][off:off + 16]
        mark = "  " if t == o else "!!"
        print("%s +0x%03x  %s" % (mark, off, t.hex(" ")))
        if t != o:
            print("        ours %s" % o.hex(" "))


if __name__ == "__main__":
    main()
