"""Decode a section of two objects into tokens and compare them as multisets.

A token is one symbol from the object's symbol table:
  size 4 -> float, size 8 -> double, otherwise the raw bytes (a string).

Usage: python artifacts/tok_diff.py <relative-object-path> [section]
"""

import os
import re
import struct
import subprocess
import sys
import tempfile
from collections import Counter

OBJDUMP = "build/binutils/powerpc-eabi-objdump.exe"
OBJCOPY = "build/binutils/powerpc-eabi-objcopy.exe"


def raw_section(obj, name):
    fd, path = tempfile.mkstemp(suffix=".bin")
    os.close(fd)
    try:
        subprocess.run([OBJCOPY, "-O", "binary", "--only-section=" + name,
                        obj, path], capture_output=True)
        with open(path, "rb") as fh:
            return fh.read()
    finally:
        os.unlink(path)


def syms(obj, name):
    out = subprocess.run([OBJDUMP, "-t", obj], capture_output=True,
                         text=True).stdout
    res = []
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 6 and p[3] == name:
            res.append((int(p[0], 16), int(p[4], 16), p[5]))
    return sorted(res)


def tokens(obj, name):
    data = raw_section(obj, name)
    if not data:
        raise SystemExit("could not parse " + obj)

    toks = []
    for off, size, name_ in syms(obj, name):
        blob = bytes(data[off:off + size])
        if size == 4 and len(blob) == 4:
            toks.append(("f", struct.unpack(">f", blob)[0]))
        elif size == 8 and len(blob) == 8:
            toks.append(("d", struct.unpack(">d", blob)[0]))
        else:
            toks.append(("s", blob))
    return toks


def fmt(t):
    kind, v = t
    if kind == "f":
        return "f %-12g" % v
    if kind == "d":
        return "d %-12g" % v
    return "s %r" % v


def main():
    rel = sys.argv[1]
    name = "." + (sys.argv[2] if len(sys.argv) > 2 else "sdata2")
    t = tokens("build/GMSP01/obj/%s.o" % rel, name)
    o = tokens("build/GMSP01/src/%s.o" % rel, name)

    print("%s: target %d tokens, ours %d tokens" % (name, len(t), len(o)))
    ct, co = Counter(t), Counter(o)
    for tok in sorted(set(ct) | set(co), key=repr):
        if ct[tok] != co[tok]:
            print("  %-4s x%d (ours) vs x%d (target)  %s"
                  % ("EXTRA" if co[tok] > ct[tok] else "MISSING",
                     co[tok], ct[tok], fmt(tok)))
    print("--- target order ---")
    print("  " + " | ".join(fmt(x) for x in t))
    print("--- ours order ---")
    print("  " + " | ".join(fmt(x) for x in o))


if __name__ == "__main__":
    main()
