"""Show exactly where two objects' .rodata / .sdata2 differ.

Usage: python artifacts/cmp_sections.py <relative-object-path> [rodata|sdata2]

Prints, for the requested section:
  * the size of both sections,
  * the first offset where they diverge and the bytes there,
  * the tails beyond the common prefix,
  * for .sdata2, both sections decoded as big-endian floats so that missing
    or extra literal constants are visible as a shift in the list.
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


def hexline(data, base):
    rows = []
    for off in range(0, len(data), 16):
        rows.append("  +0x%03x  %s" % (base + off, data[off:off + 16].hex(" ")))
    return rows


def floats(data):
    out = []
    for off in range(0, len(data) - 3, 4):
        (v,) = struct.unpack(">f", data[off:off + 4])
        out.append((off, struct.unpack(">I", data[off:off + 4])[0], v))
    return out


def main():
    rel = sys.argv[1]
    which = sys.argv[2] if len(sys.argv) > 2 else "rodata"
    name = "." + which

    t = section("build/GMSP01/obj/%s.o" % rel, name)
    o = section("build/GMSP01/src/%s.o" % rel, name)

    print("%s: target %d  ours %d  delta %+d" % (name, len(t), len(o), len(o) - len(t)))
    n = min(len(t), len(o))
    first = next((i for i in range(n) if t[i] != o[i]), None)
    if first is None:
        print("  common prefix (%d bytes) identical" % n)
    else:
        print("  first difference at +0x%x" % first)
        print("  target %s" % t[first:first + 32].hex(" "))
        print("  ours   %s" % o[first:first + 32].hex(" "))

    print("  --- target tail ---")
    print("\n".join(hexline(t[n:], n)))
    print("  --- ours tail ---")
    print("\n".join(hexline(o[n:], n)))

    if which == "sdata2":
        print("  --- target floats ---")
        for off, raw, v in floats(t):
            print("  +0x%03x  0x%08x  %g" % (off, raw, v))
        print("  --- ours floats ---")
        for off, raw, v in floats(o):
            print("  +0x%03x  0x%08x  %g" % (off, raw, v))


if __name__ == "__main__":
    main()
