#!/usr/bin/env python3
"""Classify why non-matching functions are not matching.

objdiff's diff markers make the distinction cheap and objective:

  ' '  exact
  '~'  same opcode, different operands  (register renumbering / frame offsets)
  '|'  different opcode
  '>'  present in ours only
  '<'  present in the target only

A function whose diff contains ONLY '~' lines differs in no opcode at all - every
instruction is the same, only the register allocation and the stack-frame offsets
differ. That is the MWCC 1.2.5 inlining / stack-padding class of mismatch, and it
is not something the source can express directly.

A function that shows '|', '>' or '<' really does compute something different.

This script buckets the non-matching functions of every unit below a threshold so the
remaining work can be triaged by cause instead of by unit.

usage:  python tools\\why-not-100.py [--units-below 99] [--unit SUBSTR]
"""

import argparse
import json
import os
import re
import subprocess
import sys
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORT = os.path.join(ROOT, "build", "GMSP01", "report.json")
DECOMP = os.path.join(ROOT, "tools", "decomp-diff.py")

# unit -> the functions objdiff reports as non-matching, taken from report.json
LINE = re.compile(r"^(nonmatching|missing)\s+([\d.]+)%\s+(\d+)B\s+(\S+)\s+(.*)$")
MARK = re.compile(r"^([~|<>])")


def nonmatching_functions(unit):
    with open(REPORT, encoding="utf-8") as fh:
        data = json.load(fh)
    out = []
    for u in data["units"]:
        if u["name"] != unit:
            continue
        for f in u.get("functions", []):
            pct = f.get("fuzzy_match_percent")
            if pct is None or not f.get("name"):
                continue
            if pct < 100.0:            # only the functions that do NOT match
                out.append((f["name"], int(f.get("size") or 0), pct))
        break
    return out


def classify(unit, name):
    """-> one of: exact, frame_only, real, missing"""
    res = subprocess.run(
        [sys.executable, DECOMP, "-u", unit, "-d", name, "--no-collapse"],
        capture_output=True, text=True, errors="replace", cwd=ROOT,
    )
    out = res.stdout
    if "MISSING" in out or "not found" in out.lower():
        return "missing"
    if "match (" not in out:
        # the name filter matched nothing (ambiguous/renamed) - do not
        # mistake "no diff printed" for "no difference"
        return "unclassified"
    saw_operand = False
    saw_opcode = False
    saw_ours = False
    saw_theirs = False
    for line in out.splitlines():
        m = MARK.match(line)
        if not m:
            continue
        ch = m.group(1)
        if ch == "~":
            saw_operand = True
        elif ch == "|":
            saw_opcode = True
        elif ch == ">":
            saw_ours = True
        else:
            saw_theirs = True
    if saw_opcode:
        return "wrong_opcode"
    if saw_theirs and not saw_ours:
        return "target_only"       # we never emit what the ROM emits
    if saw_ours and not saw_theirs:
        return "ours_only"         # we emit extra code the ROM does not
    if saw_theirs and saw_ours:
        return "both_sides"
    return "frame_only" if saw_operand else "exact"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--units-below", type=float, default=99.0)
    ap.add_argument("--unit", default=None, help="substring filter on the unit name")
    ap.add_argument("--max-units", type=int, default=60)
    args = ap.parse_args()

    with open(REPORT, encoding="utf-8") as fh:
        data = json.load(fh)

    units = []
    for u in data["units"]:
        f = (u.get("measures") or {}).get("fuzzy_match_percent", 0.0)
        if u["name"].startswith("mario/") and f < args.units_below:
            if args.unit and args.unit.lower() not in u["name"].lower():
                continue
            units.append((f, u["name"]))
    units.sort()
    units = units[: args.max_units]

    buckets = Counter()
    bytes_by_bucket = Counter()
    detail = []

    for _f, unit in units:
        for name, size, _pct in nonmatching_functions(unit):
            bucket = classify(unit, name)
            buckets[bucket] += 1
            bytes_by_bucket[bucket] += size
            detail.append((bucket, size, unit, name))

    total = sum(buckets.values())
    total_bytes = sum(bytes_by_bucket.values())
    print("scanned %d units, %d non-matching functions, %d target bytes\n"
          % (len(units), total, total_bytes))
    print("%-12s %8s %12s  %s" % ("bucket", "count", "target bytes", "meaning"))
    print("-" * 78)
    meaning = {
        "wrong_opcode": "different instruction - genuinely undecoded logic",
        "target_only":  "ROM emits instructions we never produce (missing call/logic)",
        "ours_only":    "we emit instructions the ROM does not (wrong/extra code)",
        "both_sides":   "instructions differ on both sides",
        "frame_only":   "every opcode matches; only registers/frame offsets differ",
        "missing":      "the symbol is absent from our object entirely",
        "unclassified": "the name filter matched nothing - needs a manual look",
        "exact":        "no diff found (already matching)",
    }
    for b in ("wrong_opcode", "target_only", "ours_only", "both_sides",
              "frame_only", "missing", "unclassified", "exact"):
        if buckets[b]:
            print("%-12s %8d %12d  %s"
                  % (b, buckets[b], bytes_by_bucket[b], meaning[b]))
    if total:
        print("\n%.1f%% of the residual bytes are pure frame/ABI padding, i.e. NOT "
              "expressible in source" % (100.0 * bytes_by_bucket["frame_only"]
                                         / max(1, total_bytes)))

    print("\n--- biggest frame_only items (MWCC padding, not expressible) ---")
    n = 0
    for b, size, unit, name in sorted(detail, key=lambda d: -d[1]):
        if b == "frame_only":
            print("%7d B  %-38s %s" % (size, unit, name[:66]))
            n += 1
            if n >= 10:
                break
    print("\n--- biggest genuinely-undecoded items ---")
    n = 0
    for b, size, unit, name in sorted(detail, key=lambda d: -d[1]):
        if b in ("wrong_opcode", "target_only", "both_sides", "missing"):
            print("%7d B  %-38s %-16s %s" % (size, unit, b, name[:52]))
            n += 1
            if n >= 15:
                break


if __name__ == "__main__":
    main()
