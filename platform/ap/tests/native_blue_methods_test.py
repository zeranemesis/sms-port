"""Headless shipped-method fixtures; no game assets or card/menu UI are exercised."""
import json
import os
from pathlib import Path
import subprocess
import sys


def main(executable, output):
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    assets = output / "empty-assets"
    assets.mkdir()
    cases = [("checks", 1, 0), ("shuffle3", 1, 3), ("shuffle16", 1, 16),
             ("shuffle17", 1, 17), ("shuffle24", 1, 24), ("trades", 2, 3),
             ("vanilla", 0, 3), ("legacy-save", 0, 0)]
    cases += [("coin-shines-shuffle", 1, 3), ("coin-shines-trades", 2, 3),
              ("coin-shines-vanilla", 0, 3)]
    results = []
    for name, mode, trades in cases:
        directory = output / name
        directory.mkdir()
        env = dict(os.environ, SMS_HEADLESS="1", SMS_AUDIO="0",
                   SMS_DISC_ROOT=str(assets), SMS_SAVE_DIR=str(directory),
                   SMS_AP_SELFTEST_BLUE_MODE=str(mode), SMS_AP_SELFTEST_TRADES=str(trades))
        if name.startswith("coin-shines"):
            env["SMS_AP_SELFTEST_COIN_SHINES"] = "1"
        if mode or name == "legacy-save":
            env["SMS_AP_BRIDGE"] = str(directory)
        else:
            env.pop("SMS_AP_BRIDGE", None)
        env.pop("SMS_DISC_IMAGE", None)
        argument = "--ap-native-save-selftest" if name == "legacy-save" else "--ap-native-blue-selftest"
        result = subprocess.run([executable, argument], env=env, capture_output=True,
                                text=True, timeout=30)
        (directory / "stdout.txt").write_text(result.stdout, encoding="utf-8")
        (directory / "stderr.txt").write_text(result.stderr, encoding="utf-8")
        assert result.returncode == 0, (name, result.returncode, result.stderr)
        if mode == 1:
            snapshot = json.loads((directory / "snapshot.json").read_text())
            assert snapshot["ready"] and not snapshot["error"], snapshot
            assert len(snapshot["checks"]) == 240 + trades + (8 if name.startswith("coin-shines") else 0), snapshot
            assert snapshot["received_blue_coins"] == (max(40, trades * 10) if trades else 0)
            assert not snapshot["parked_indices"]
        results.append({"case": name, "mode": mode, "trades": trades, "passed": True})
    (output / "report.json").write_text(json.dumps({
        "evidence": "Actual native flag/save methods and production bridge; no assets, menu or gameplay",
        "passed": True, "executable": executable, "cases": results,
    }, indent=2), encoding="utf-8")
    print(f"Native blue/save method fixtures passed: {len(results)} cases; evidence {output}")


if __name__ == "__main__":
    main(str(Path(sys.argv[1]).resolve()), sys.argv[2])
