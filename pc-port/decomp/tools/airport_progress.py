#!/usr/bin/env python3
"""Report decompilation candidates for the playable airport sequence.

This is intentionally an inventory, not a reachability claim.  It uses the
standard objdiff report and a conservative set of modules known to participate
in boot, stage loading, Mario control, camera, map, UI, and sound.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


PREFIXES = (
    "src/System/Application.cpp",
    "src/System/GCLogoDir.cpp",
    "src/System/MenuDir.cpp",
    "src/System/SelectDir.cpp",
    "src/System/MarDirector",
    "src/GC2D/CardSave",
    "src/GC2D/ConsoleStr",
    "src/GC2D/GCConsole2",
    "src/Player/Mario",
    "src/Camera/",
    "src/Map/",
    "src/MSound/",
    "src/System/MSoundMainSide.cpp",
)


def is_candidate(source_path: str) -> bool:
    return any(source_path.startswith(prefix) for prefix in PREFIXES)


def summarize(report: dict) -> dict:
    units = []
    totals = {"units": 0, "code": 0, "matched_code": 0,
              "functions": 0, "matched_functions": 0}
    for unit in report["units"]:
        source_path = unit.get("metadata", {}).get("source_path", "")
        if not is_candidate(source_path):
            continue
        measures = unit["measures"]
        code = int(measures.get("total_code", 0))
        matched_code = int(measures.get("matched_code", 0))
        functions = int(measures.get("total_functions", 0))
        matched_functions = int(measures.get("matched_functions", 0))
        units.append({
            "name": unit["name"],
            "source_path": source_path,
            "code": code,
            "matched_code": matched_code,
            "functions": functions,
            "matched_functions": matched_functions,
        })
        totals["units"] += 1
        totals["code"] += code
        totals["matched_code"] += matched_code
        totals["functions"] += functions
        totals["matched_functions"] += matched_functions
    units.sort(key=lambda unit: (unit["matched_code"] / unit["code"]
                                  if unit["code"] else 1, unit["source_path"]))
    totals["matched_code_percent"] = (
        100 * totals["matched_code"] / totals["code"] if totals["code"] else 100
    )
    totals["matched_functions_percent"] = (
        100 * totals["matched_functions"] / totals["functions"]
        if totals["functions"] else 100
    )
    return {"totals": totals, "units": units}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path,
                        default=Path("build/GMSP01/report.json"))
    parser.add_argument("--json", action="store_true", dest="as_json")
    args = parser.parse_args()

    with args.report.open(encoding="utf-8") as report_file:
        summary = summarize(json.load(report_file))
    if args.as_json:
        print(json.dumps(summary, indent=2))
        return

    totals = summary["totals"]
    print("Airport candidate inventory (not a reachability closure)")
    print("{}/{} code bytes matched ({:.2f}%); {}/{} functions exact".format(
        totals["matched_code"], totals["code"],
        totals["matched_code_percent"], totals["matched_functions"],
        totals["functions"],
    ))
    print("match      functions  source")
    for unit in summary["units"]:
        percent = 100 * unit["matched_code"] / unit["code"] if unit["code"] else 100
        print("{:6.2f}%  {:4}/{:<4}  {}".format(
            percent, unit["matched_functions"], unit["functions"], unit["source_path"]
        ))


if __name__ == "__main__":
    main()
