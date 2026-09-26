"""For every unit missing __sinit, check whether the target .bss is our .bss
plus exactly N * 12 bytes of JSUList<T>::smList template statics.

Only units passing that check are candidates for the rogue MSound includes
that reproduce __sinit (see src/Enemy/effectObj.cpp).
"""

import json
import os
import re
import subprocess

NM = os.path.join("build", "binutils", "powerpc-eabi-nm.exe")
OBJDUMP = os.path.join("build", "binutils", "powerpc-eabi-objdump.exe")

report = json.load(open("build/GMSP01/report.json", encoding="utf-8"))


def objpath(unit, which):
    # unit name "mario/Enemy/riccohook" -> src/Enemy/riccohook.o
    rel = unit.split("/", 1)[1]
    return os.path.join("build", "GMSP01", which, rel + ".o")


def section_size(obj, name):
    out = subprocess.run([OBJDUMP, "-h", obj], capture_output=True, text=True).stdout
    for line in out.splitlines():
        m = re.match(r"\s*\d+\s+(\S+)\s+([0-9a-f]{8})\s", line)
        if m and m.group(1) == name:
            return int(m.group(2), 16)
    return None


def smlist_count(obj):
    out = subprocess.run([NM, "-n", "-S", obj], capture_output=True, text=True).stdout
    n = 0
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 4 and parts[2] in "bB" and parts[1] == "0000000c":
            n += 1
    return n


candidates = []
for unit in report["units"]:
    for fn in unit.get("functions", []):
        if "__sinit_" not in fn["name"] or "fuzzy_match_percent" in fn:
            continue
        candidates.append((unit["name"], fn["name"], int(fn["size"])))

print("%-30s %7s %7s %6s  %s" % ("UNIT", "TGT_BSS", "OUR_BSS", "SMLIST", "VERDICT"))
ok = []
for unit, sinit, size in sorted(candidates):
    t, o = objpath(unit, "obj"), objpath(unit, "src")
    if not (os.path.exists(t) and os.path.exists(o)):
        print("%-30s  skipped (object missing)" % unit.split("/", 1)[1])
        continue
    tb, ob = section_size(t, ".bss"), section_size(o, ".bss")
    sm = smlist_count(t)
    delta = tb - ob if (tb is not None and ob is not None) else -1
    verdict = "OK" if delta == sm * 12 and sm > 0 else "-"
    if verdict == "OK":
        ok.append(unit)
    print("%-30s %7s %7s %6d  %s" % (unit.split("/", 1)[1], tb, ob, sm, verdict))

print()
print("%d/%d candidate(s) verified" % (len(ok), len(candidates)))
for u in ok:
    print("   ", u)
