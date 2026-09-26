"""Word-level LCS diff of one section between target and our object.

Usage: python artifacts/cmp_words.py <relative-object-path> [section] [start-hex]

Both sections are split into big-endian 4 byte words, aligned with
difflib.SequenceMatcher and printed as a unified view:

    = word            present in both at the same position
    - word            only in the target
    + word            only in ours

Words that are ASCII fragments (of a string literal) are shown next to their
decoded bytes so that missing/extra strings are immediately recognizable.
"""

import difflib
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


def words(data):
    return [data[i:i + 4] for i in range(0, len(data) & ~3, 4)]


def show(w):
    text = "".join(chr(b) if 32 <= b < 127 else "." for b in w)
    return "0x%s  %r" % (w.hex(), text)


def main():
    rel = sys.argv[1]
    name = "." + (sys.argv[2] if len(sys.argv) > 2 else "sdata2")
    start = int(sys.argv[3], 16) // 4 if len(sys.argv) > 3 else 0

    t = words(section("build/GMSP01/obj/%s.o" % rel, name))
    o = words(section("build/GMSP01/src/%s.o" % rel, name))

    print("%s: target %d words, ours %d words" % (name, len(t), len(o)))
    sm = difflib.SequenceMatcher(None, t, o, autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            print("  == %3d..%3d  (%d words identical)"
                  % (i1, i2, i2 - i1))
            continue
        if tag in ("delete", "replace"):
            for k in range(i1, i2):
                print("  - +0x%03x  %s" % (k * 4, show(t[k])))
        if tag in ("insert", "replace"):
            for k in range(j1, j2):
                print("  + +0x%03x  %s" % (k * 4, show(o[k])))


if __name__ == "__main__":
    main()
