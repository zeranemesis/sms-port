"""List every unit whose object is missing its __sinit_<TU>_cpp symbol.

In the original build those __sinit functions are all 764 bytes long, i.e.
they only run the 15 JSUList<T>::smList registrations that JALList.hpp's
template statics need.  Adding the two rogue MSound includes that pull JALList
in (see src/Enemy/effectObj.cpp and src/Enemy/effectEnemy.cpp) is what
reproduces them.
"""

import json

report = json.load(open("build/GMSP01/report.json", encoding="utf-8"))

rows = []
for unit in report["units"]:
    for fn in unit.get("functions", []):
        if "__sinit_" not in fn["name"]:
            continue
        if "fuzzy_match_percent" in fn:
            continue
        rows.append((fn["name"], int(fn["size"]), unit["name"]))

rows.sort(key=lambda r: r[2])
for name, size, unit in rows:
    print("%6d  %-46s  %s" % (size, name, unit))
print()
print("%d unit(s) missing __sinit, %d bytes total" % (len(rows), sum(r[1] for r in rows)))
