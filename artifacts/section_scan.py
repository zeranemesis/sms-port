"""Find units whose data sections are badly matched while the code is fine.

That combination is the signature of a *layout* problem rather than a
decompilation gap: a missing rogue include (System/DummyStrings.hpp shifts
every .rodata string offset, JALList.hpp's template statics shift .bss), a
const qualifier on a global, or wrong include order.  Those are all fixable
without writing any new function bodies.
"""

import json

report = json.load(open("build/GMSP01/report.json", encoding="utf-8"))

rows = []
for unit in report["units"]:
    m = unit.get("measures", {})
    fz = float(m.get("fuzzy_match_percent") or 0)
    if fz < 60:
        continue
    secs = {}
    for s in unit.get("sections", []):
        if s.get("fuzzy_match_percent") is None:
            continue
        secs[s["name"]] = (
            float(s["fuzzy_match_percent"]),
            int(s.get("size") or 0),
        )
    worst = None
    for name, (pct, size) in secs.items():
        if size < 48:
            continue
        if worst is None or pct < worst[1]:
            worst = (name, pct, size)
    if worst is None or worst[1] > 70:
        continue
    lost = worst[2] * (100 - worst[1]) / 100.0
    rows.append((lost, fz, worst[0], worst[1], worst[2], unit["name"]))

rows.sort(reverse=True)
print("%8s %7s  %-7s %8s %7s  UNIT" % ("LOST_B", "CODE%", "SECTION", "SEC_SIZE", "SEC%"))
for lost, fz, name, pct, size, unit in rows[:40]:
    print("%8.0f %6.1f%%  %-7s %8d %6.1f%%  %s" % (lost, fz, name, size, pct, unit))
