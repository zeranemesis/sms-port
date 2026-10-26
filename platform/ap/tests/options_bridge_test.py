"""SMSAP3 positive/negative controls through the compiled production parser."""
import itertools
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main(executable, catalog):
    catalog = json.loads(Path(catalog).read_text())
    blues = [(r["id"], r["native_bit"]) for r in catalog["locations"] if r["category"] == "Blue Coin"]
    nozzles = [(r["id"], r["native_bit"]) for r in catalog["locations"] if r["category"] == "Nozzle"]
    coins = [(r["id"], r["native_bit"]) for r in catalog["locations"] if r["name"].endswith(" - 100 Coins")]
    def run(start=0, level=1, mode=0, boxes=0, episodes=0, death=0, required=50, rows=None):
        if rows is None: rows = coins + (blues if mode == 1 else []) + (nozzles if boxes else [])
        with tempfile.TemporaryDirectory(prefix="sms-options-bridge-") as directory:
            root = Path(directory)
            (root / "session.txt").write_text(f"SMSAP3\n616263 0 1 {required} {start} {level} {mode} 0 0 {episodes} {boxes} {death}\n{len(rows)}\n" +
                "".join(f"{location} {bit}\n" for location, bit in rows))
            result = subprocess.run([executable, "fixture-all"], env=dict(os.environ, SMS_AP_BRIDGE=directory),
                                    capture_output=True, text=True, timeout=10)
            assert result.returncode == 0, result.stderr
            return json.loads((root / "snapshot.json").read_text())
    count = 0
    for start, level, mode, boxes, episodes, death in itertools.product(range(3), range(2), range(3), range(2), range(2), range(2)):
        snapshot = run(start, level, mode, boxes, episodes, death)
        assert snapshot["ready"] and not snapshot["error"], snapshot
        assert len(snapshot["checks"]) == 8 + (240 if mode == 1 else 0) + (10 if boxes else 0)
        assert snapshot["nozzle_mask"] == (1 if start == 0 else 2 if start == 1 else 0)
        count += 1
    for field, value in (("start", 3), ("level", 2), ("mode", 3), ("boxes", 2), ("episodes", 2), ("death", 2), ("required", 346)):
        assert run(**{field: value})["error"], (field, value)
    for rows in (nozzles[:-1], nozzles + [nozzles[0]], nozzles + [(9999, 871)], nozzles + [(1, 119)]):
        assert run(boxes=1, rows=rows)["error"]
    assert run(start=2, rows=[(1, 86)])["error"]
    assert run(boxes=0, rows=nozzles)["error"]
    assert run(mode=1, rows=blues[:-1])["error"]
    print(f"SMSAP3 production parser passed {count} option combinations and malformed/partial/disabled controls")


if __name__ == "__main__":
    main(str(Path(sys.argv[1]).resolve()), sys.argv[2])
