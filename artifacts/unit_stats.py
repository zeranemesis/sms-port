"""Print one unit's report numbers plus the per-section breakdown.

Usage: python artifacts/unit_stats.py <unit name> [before.json]
"""

import json
import sys


def measures(doc, name):
    for u in doc["units"]:
        if u["name"] == name:
            return u
    return None


def main():
    name = sys.argv[1]
    before_path = sys.argv[2] if len(sys.argv) > 2 else None

    after = json.load(open("build/GMSP01/report.json", encoding="utf-8"))
    ua = measures(after, name)
    if ua is None:
        raise SystemExit("unit not in report: " + name)
    ma = ua["measures"]

    if before_path:
        ub = measures(json.load(open(before_path, encoding="utf-8")), name)
        mb = ub["measures"] if ub else {}
        print("fuzzy  {:8.2f} -> {:.2f}".format(
            float(mb.get("fuzzy_match_percent") or 0),
            float(ma["fuzzy_match_percent"])))
        for key in ("matched_code", "matched_data"):
            tb, ta = mb.get(key), ma.get(key)
            pb = mb.get(key + "_percent"), ma.get(key + "_percent")
            print("{:8} {:>6} -> {:<6} ({:.2f} -> {:.2f})".format(
                key, tb, ta, float(pb[0] or 0), float(pb[1] or 0)))
        print("{:8} {}/{} -> {}/{}".format(
            "fns", mb.get("matched_functions"), mb.get("total_functions"),
            ma.get("matched_functions"), ma.get("total_functions")))
    else:
        print(name)
        print("  fuzzy {:.2f}".format(float(ma["fuzzy_match_percent"])))
        print("  code  {}/{} = {:.2f}".format(
            ma["matched_code"], ma["total_code"],
            float(ma["matched_code_percent"])))
        print("  data  {}/{} = {:.2f}".format(
            ma.get("matched_data"), ma.get("total_data"),
            float(ma.get("matched_data_percent") or 0)))

    print("  sections:")
    for s in ua.get("sections", []):
        pct = s.get("fuzzy_match_percent")
        if pct is None:
            continue
        print("    {:<8} size={:<6} fuzzy={:.2f}".format(
            s["name"], s.get("size"), float(pct)))


if __name__ == "__main__":
    main()
