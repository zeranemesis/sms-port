# Byte-level section comparison for GC2D/CardSave (GMSP01)
# Compares target (build/GMSP01/obj) vs ours (build/GMSP01/src) per section.
# Reports:
#  - raw size per section
#  - positional byte match (over min length)
#  - symbol-mapped match: map target symbols to ours by name (or by section
#    offset for compiler-generated @-names which may be renumbered)
import re
import subprocess
import sys
from collections import OrderedDict

OBJDUMP = r"build\binutils\powerpc-eabi-objdump.exe"
TARGET = r"build\GMSP01\obj\GC2D\CardSave.o"
OURS = r"build\GMSP01\src\GC2D\CardSave.o"
SECTIONS = [".data", ".bss", ".rodata", ".sdata2", ".sdata", ".sbss", ".text", ".ctors"]


def section_bytes(obj, sec):
    r = subprocess.run(
        [OBJDUMP, "-s", "-j", sec, obj],
        capture_output=True, text=True)
    if "section" not in r.stdout and "Contents of section" not in r.stdout:
        return None
    data = bytearray()
    for line in r.stdout.splitlines():
        m = re.match(r"\s+([0-9a-f]+)\s((?:[0-9a-f]{8} ?)+)", line)
        if m:
            off = int(m.group(1), 16)
            if off != len(data):
                # fill gap (shouldn't happen)
                data.extend(b"\x00" * (off - len(data)))
            hexbytes = m.group(2).replace(" ", "")
            data.extend(bytes.fromhex(hexbytes))
    return bytes(data)


def symbols(obj):
    r = subprocess.run([OBJDUMP, "-t", obj], capture_output=True, text=True)
    out = []
    for line in r.stdout.splitlines():
        # 00000000 l     O .data	0000000c name
        m = re.match(
            r"([0-9a-f]{8})\s+([l gw])\s+([A-Za-z])\s+(\S+)\s+([0-9a-f]{8})\s+(.*)$",
            line)
        if m:
            out.append(dict(addr=int(m.group(1), 16), bind=m.group(2).strip(),
                            kind=m.group(3), sec=m.group(4),
                            size=int(m.group(5), 16), name=m.group(6).strip()))
    return out


def sym_match(t_syms, o_syms, sec, t_data, o_data):
    """Match target symbols of section `sec` against our symbols.
    Named symbols match by name; @-symbols match by section offset first,
    then by (size) fallback at same offset."""
    ours_by_name = {}
    for s in o_syms:
        if s["sec"] == sec:
            ours_by_name.setdefault(s["name"], []).append(s)
    matched = 0
    total = 0
    details = []
    for s in t_syms:
        if s["sec"] != sec or s["size"] == 0:
            continue
        cand = None
        if s["name"] in ours_by_name and ours_by_name[s["name"]]:
            cands = ours_by_name[s["name"]]
            # pick first unconsumed
            cand = cands.pop(0)
        else:
            # offset match for @-symbols
            for c in o_syms:
                if c["sec"] == sec and c["addr"] == s["addr"] and c["size"] == s["size"]:
                    cand = c
                    break
        total += s["size"]
        if cand is None:
            details.append((s["name"], s["addr"], s["size"], "MISSING", 0))
            continue
        a = t_data[s["addr"]:s["addr"] + s["size"]]
        b = o_data[cand["addr"]:cand["addr"] + cand["size"]]
        n = min(len(a), len(b))
        eq = sum(1 for i in range(n) if a[i] == b[i])
        matched += eq
        details.append((s["name"], s["addr"], s["size"],
                        "as " + cand["name"] + f"@{cand['addr']:x}", eq))
    return matched, total, details


def main():
    print(f"{'SECTION':<10} {'TGT':>8} {'OURS':>8} {'pos-match':>22} {'sym-match':>22}")
    for sec in SECTIONS:
        t = section_bytes(TARGET, sec)
        o = section_bytes(OURS, sec)
        if t is None and o is None:
            continue
        t = t or b""
        o = o or b""
        n = min(len(t), len(o))
        pos_eq = sum(1 for i in range(n) if t[i] == o[i])
        pos_pct = 100.0 * pos_eq / len(t) if t else 100.0
        ts = [s for s in symbols(TARGET) if s["sec"] == sec]
        os_ = [s for s in symbols(OURS) if s["sec"] == sec]
        if ts:
            m, tot, det = sym_match(ts, os_, sec, t, o)
            pct = 100.0 * m / tot if tot else 100.0
            syms = f"{m}/{tot} = {pct:5.1f}%"
        else:
            syms = "n/a (no target syms)"
        print(f"{sec:<10} {len(t):>8} {len(o):>8} "
              f"{pos_eq:>6}/{len(t):<6} {pos_pct:>6.1f}% {syms}")
        if sec in (".data", ".rodata", ".sdata2") and ts:
            for d in det:
                flag = "" if (isinstance(d[4], int) and d[4] == d[2]) else "  <-- MISMATCH"
                print(f"           {d[0]} @{d[1]:x} size {d[2]:#x} vs {d[3]}: {d[4]}{flag}")


if __name__ == "__main__":
    main()
