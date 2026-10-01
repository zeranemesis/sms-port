#!/usr/bin/env python3
"""Scan for systematic class-layout shifts in constructor bodies.

A wrong data member (typically a redundant re-declaration of an inherited
field, or a wrong array bound) shifts every *subsequent* member by a constant
delta.  That single mistake costs dozens of matching instructions across every
method of the class, and it is invisible in a per-function fuzzy score because
each function just looks "a bit off".

This compares the member offsets touched by every `__ct__*` function in the
target assembly against the same set in our own object file, and reports the
constant shift that best explains the difference.

usage:  python tools\\layout-shift-scan.py [unit-substring ...]
        (no arguments = scan every unit that has both a .s and an object)
"""

import os
import re
import subprocess
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASM_DIR = os.path.join(ROOT, "build", "GMSP01", "asm")
OBJ_DIR = os.path.join(ROOT, "build", "GMSP01", "obj")
OBJDUMP = os.path.join(ROOT, "build", "binutils", "powerpc-eabi-objdump.exe")

# stw/stb/sth/stfs/stwu <reg>, 0xNN(<reg>)   and   <reg>, 0xNN(<reg>)
STORE = re.compile(r"^\s*(?:stw|stb|sth|stfs|stfdu|stwu)\s+[^,]+,\s*(-?0x[0-9a-fA-F]+)\(")
# same for objdump output: "  1d84:\t98 df 01 64 \tstb     r6,356(r31)"
# NB: objdump prints these displacements in DECIMAL (or -0xNN when negative),
# not the 0xNN form used by the target .s listing.
STORE_OD = re.compile(r"\b(?:stw|stb|sth|stfs|stfdu|stwu)\s+[^,]+,\s*(-?(?:0x[0-9a-fA-F]+|\d+))\(")

# the object pointer register the ctor writes through
PTRS = ("r3", "r31", "r30", "r29")


def target_ctor_offsets(path):
    """-> {mangled_ctor_name: set(offsets)}"""
    out = defaultdict(set)
    cur = None
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if line.lstrip().startswith(".fn"):
                m = re.search(r'\.fn\s+"?([^",]+)"?', line)
                name = m.group(1) if m else ""
                cur = name if name.startswith("__ct__") else None
                continue
            if line.lstrip().startswith(".endfn"):
                cur = None
                continue
            if cur is None:
                continue
            code = line[36:] if len(line) > 36 else line
            m = STORE.match(code)
            if not m:
                continue
            off = int(m.group(1), 16)
            if off < 0 or off > 0x2000:
                continue
            # only member stores, not locals: the base register must be the
            # object pointer, and the offset must be small and word-ish
            if not any(("(%s)" % p) in code for p in PTRS):
                continue
            out[cur].add(off)
    return out


def our_ctor_offsets(objpath):
    """-> {mangled_ctor_name: set(offsets)}"""
    try:
        res = subprocess.run(
            [OBJDUMP, "-d", objpath], capture_output=True, text=True, errors="replace"
        )
    except OSError:
        return {}
    out = defaultdict(set)
    cur = None
    for line in res.stdout.splitlines():
        m = re.match(r"^[0-9a-f]+ <([^>]+)>:", line)
        if m:
            name = m.group(1)
            cur = name if name.startswith("__ct__") else None
            continue
        if cur is None:
            continue
        m = STORE_OD.search(line)
        if not m:
            continue
        try:
            off = int(m.group(1), 0)
        except ValueError:
            continue
        if off < 0 or off > 0x2000:
            continue
        if not any(("(%s)" % p) in line for p in PTRS):
            continue
        out[cur].add(off)
    return out


def best_shift(tgt, ours):
    """constant d maximising |{x in tgt : x+d in ours}| - |{x in tgt: x+d not in ours}|"""
    if not tgt:
        return 0, 0.0
    best = (0, -1.0)
    for d in range(-0x40, 0x41, 4):
        hit = sum(1 for x in tgt if (x + d) in ours)
        score = hit / float(len(tgt))
        if score > best[1]:
            best = (d, score)
    return best


def main():
    filters = [a.lower() for a in sys.argv[1:]]
    rows = []
    for dirpath, _dirs, files in os.walk(ASM_DIR):
        for name in files:
            if not name.endswith(".s"):
                continue
            unit = os.path.relpath(os.path.join(dirpath, name), ASM_DIR)[:-2]
            if filters and not any(f in unit.lower() for f in filters):
                continue
            asmp = os.path.join(dirpath, name)
            objp = os.path.join(OBJ_DIR, unit + ".o")
            if not os.path.exists(objp):
                continue
            tgt = target_ctor_offsets(asmp)
            ours = our_ctor_offsets(objp)
            if not tgt or not ours:
                continue
            for ctor in sorted(tgt):
                if ctor not in ours:
                    continue
                d, score = best_shift(tgt[ctor], ours[ctor])
                if d != 0 and score >= 0.45:
                    rows.append((unit, ctor, d, score, len(tgt[ctor])))
    if not rows:
        print("no systematic layout shift detected")
        return
    rows.sort(key=lambda r: (-r[3] * r[4], r[0]))
    print("%-42s %-46s %6s %6s %s" % ("unit", "constructor", "shift", "cover", "n"))
    print("-" * 112)
    for unit, ctor, d, score, n in rows:
        print("%-42s %-46s %+6d %5.0f%% %d" % (unit, ctor[:46], d, score * 100, n))


if __name__ == "__main__":
    main()
