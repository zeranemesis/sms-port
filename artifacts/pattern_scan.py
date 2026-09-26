"""Find symbols that are missing from MANY different objects at once.

A symbol showing up as missing in a lot of units is almost never a real
decompilation gap - it means our translation units are systematically not
emitting something the original ones did (missing include, wrong linkage,
missing template instantiation).  __sinit_<TU>_cpp was exactly that: 28 units
at once, all reproduced by one pair of rogue includes.
"""

import json
import re
from collections import defaultdict

report = json.load(open("build/GMSP01/report.json", encoding="utf-8"))


def family(mangled):
    """Collapse the TU-specific part so __sinit_<TU>_cpp groups together."""
    s = re.sub(r"__sinit_\w+_cpp", "__sinit_<TU>_cpp", mangled)
    s = re.sub(r"@32@(__dt__\w+)", r"@32@\1", s)
    return s


hits = defaultdict(lambda: [0, 0, []])  # family -> [count, total bytes, units]
for unit in report["units"]:
    seen = set()
    for fn in unit.get("functions", []):
        if "fuzzy_match_percent" in fn:
            continue
        fam = family(fn["name"])
        if fam not in seen:
            seen.add(fam)
            hits[fam][0] += 1
            hits[fam][2].append(unit["name"])
        hits[fam][1] += int(fn["size"])

rows = sorted(hits.items(), key=lambda kv: (-kv[1][0], -kv[1][1]))
print("%5s %8s  PATTERN" % ("UNITS", "BYTES"))
for fam, (n, b, units) in rows[:45]:
    print("%5d %8d  %s" % (n, b, fam[:70]))
    if n >= 6:
        print("        %s" % ", ".join(u.split("/", 1)[1] for u in units[:14]))
