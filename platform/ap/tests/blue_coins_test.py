"""Production bridge wire checks against the independently exported source catalog."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main(executable, catalog_path):
    catalog = json.loads(Path(catalog_path).read_text(encoding="utf-8"))
    blues = [(row["id"], row["native_bit"]) for row in catalog["locations"]
             if row["category"] == "Blue Coin"]
    assert len(blues) == 240
    trade_bits = list(range(70, 86)) + list(range(108, 116))

    def run(mode=1, maximum=40, trades=3, rows=None, receipts=40, magic="SMSAP2"):
        if rows is None:
            rows = (blues if mode == 1 else []) + [(1000 + i, bit) for i, bit in
                                                  enumerate(trade_bits[:trades])]
        with tempfile.TemporaryDirectory(prefix="sms-blue-bridge-") as directory:
            root = Path(directory)
            env = dict(os.environ, SMS_AP_BRIDGE=directory)
            header = "616263 0 1 3 0 1"
            if magic == "SMSAP2":
                header += f" {mode} {maximum} {trades}"
            (root / "session.txt").write_text(
                f"{magic}\n{header}\n{len(rows)}\n" +
                "".join(f"{location} {bit}\n" for location, bit in rows), encoding="ascii")
            (root / "history.txt").write_text(
                "SMSAPH1\n616263 0 1 " + "11" * 16 + f"\n{receipts}\n" +
                "".join(f"{i} 523014 {i + 1} 2 1\n" for i in range(receipts)), encoding="ascii")
            result = subprocess.run([executable, "fixture-all"], env=env,
                                    capture_output=True, text=True, timeout=10)
            assert result.returncode == 0, result.stderr
            return json.loads((root / "snapshot.json").read_text()), result.stdout

    for trades in (0, 1, 3, 16, 17, 24):
        maximum = max(40, trades * 10)
        snapshot, output = run(maximum=maximum, trades=trades)
        assert snapshot["ready"] and not snapshot["error"], snapshot
        assert snapshot["received_blue_coins"] == 40 and not snapshot["parked_indices"]
        assert set(snapshot["checks"]) == {row[0] for row in blues} | set(range(1000, 1000 + trades))
        assert f"blue_total={min(40, trades * 10)}" in output
    snapshot, output = run(maximum=0, trades=0, receipts=0)
    assert snapshot["ready"] and len(snapshot["checks"]) == 240 and "blue_total=0" in output
    snapshot, output = run(mode=2, maximum=0)
    assert snapshot["ready"] and snapshot["received_blue_coins"] == 0
    assert snapshot["checks"] == [1000, 1001, 1002]
    assert snapshot["parked_indices"] == list(range(40)) and "blue_total=30" in output
    snapshot, output = run(magic="SMSAP1", mode=0, rows=[(1, 86)])
    assert snapshot["ready"] and snapshot["checks"] == [1]
    assert snapshot["received_blue_coins"] == 0 and "blue_total=240" in output
    # Existing Shine callbacks include every 100-coin flag in all modes.
    for mode in (0, 1, 2):
        rows = (list(blues) if mode == 1 else []) + [(2000 + bit, bit) for bit in range(100, 108)]
        snapshot, _ = run(mode=mode, trades=0, rows=rows, magic="SMSAP1" if mode == 0 else "SMSAP2")
        assert snapshot["ready"] and set(range(2100, 2108)) <= set(snapshot["checks"])
    # Bowser, unused stage bits, nozzles and disabled traders.
    for bit in (119, 140, 169, 200, 219, 250, 519, 520, 539, 550, 869, 870, 947, 70, 115):
        rows = list(blues)
        rows[0] = (rows[0][0], bit)
        snapshot, _ = run(trades=0, rows=rows)
        assert not snapshot["ready"] and snapshot["error"], (bit, snapshot)
    for rows in (blues[:-1], blues + [blues[0]], blues + [(999, blues[0][1])]):
        snapshot, _ = run(trades=0, rows=rows)
        assert not snapshot["ready"] and snapshot["error"]
    for mode, maximum, trades in ((0, 0, 0), (3, 0, 0), (1, 241, 0), (1, 0, 1), (1, 240, 25)):
        snapshot, _ = run(mode=mode, maximum=maximum, trades=trades, rows=blues)
        assert not snapshot["ready"] and snapshot["error"]
    print("SMS production bridge blue tests passed: 240 source mappings, both modes, "
          "all trade boundaries, currency cap, legacy compatibility and malformed sessions")


if __name__ == "__main__":
    main(str(Path(sys.argv[1]).resolve()), sys.argv[2])
