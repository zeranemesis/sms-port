"""Compare two report.json files unit by unit and print every change.

Usage: python artifacts/report_diff.py <before.json> <after.json>
"""

import json
import sys


def load(path):
    r = json.load(open(path, encoding="utf-8"))
    return {u["name"]: u for u in r["units"]}


def key(u, field):
    m = u.get("measures", {})
    return float(m.get(field) or 0)


def main():
    before, after = load(sys.argv[1]), load(sys.argv[2])

    names = sorted(set(before) | set(after))
    rows = []
    for name in names:
        b, a = before.get(name), after.get(name)
        if b is None or a is None:
            continue
        fb, fa = key(b, "fuzzy_match_percent"), key(a, "fuzzy_match_percent")
        db, da = key(b, "matched_data_percent"), key(a, "matched_data_percent")
        if abs(fa - fb) < 0.005 and abs(da - db) < 0.005:
            continue
        rows.append((fa - fb, da - db, fb, fa, db, da, name))

    rows.sort()
    print("%8s %8s %10s  UNIT" % ("DFUZZY", "DDATA", "FUZZY"))
    for df, dd, fb, fa, db, da, name in rows:
        flag = "  <-- REGRESSION" if df < -0.005 else ""
        print("%+7.2f%% %+7.2f%% %6.2f->%6.2f  %s%s" % (df, dd, fb, fa, name, flag))

    total_b = sum(key(u, "matched_code") for u in before.values())
    total_a = sum(key(u, "matched_code") for u in after.values())
    print()
    print("changed units : %d / %d" % (len(rows), len(names)))
    print("matched_code  : %d -> %d  (%+d bytes)" % (total_b, total_a, total_a - total_b))
    up = sum(1 for r in rows if r[0] > 0.005)
    down = sum(1 for r in rows if r[0] < -0.005)
    print("improved      : %d" % up)
    print("regressed     : %d" % down)


if __name__ == "__main__":
    main()
