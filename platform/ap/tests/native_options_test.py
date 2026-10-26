"""Actual native actor/director/save methods; empty assets, no rendered campaign."""
import itertools
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
    cases = []
    for level, start, blue, boxes, episodes in itertools.product(range(2), range(3), range(3), range(2), range(2)):
        name = f"level-{level}-start-{start}-blue-{blue}-boxes-{boxes}-episodes-{episodes}"
        directory = output / name
        directory.mkdir()
        env = dict(os.environ, SMS_HEADLESS="1", SMS_AUDIO="0", SMS_DISC_ROOT=str(assets),
                   SMS_SAVE_DIR=str(directory), SMS_AP_BRIDGE=str(directory),
                   SMS_AP_SELFTEST_LEVEL=str(level), SMS_AP_SELFTEST_START=str(start),
                   SMS_AP_SELFTEST_BLUE_MODE=str(blue), SMS_AP_SELFTEST_BOXES=str(boxes),
                   SMS_AP_SELFTEST_EPISODES=str(episodes))
        env.pop("SMS_DISC_IMAGE", None)
        result = subprocess.run([executable, "--ap-native-options-selftest"], env=env,
                                capture_output=True, text=True, timeout=30)
        (directory / "stdout.txt").write_text(result.stdout)
        (directory / "stderr.txt").write_text(result.stderr)
        assert result.returncode == 0, (name, result.returncode, result.stderr[-3000:])
        snapshot = json.loads((directory / "snapshot.json").read_text())
        assert snapshot["death_count"] == 1 and snapshot["death_applied"] == 1, snapshot
        cases.append({"name": name, "passed": True})
    report = {"passed": True, "evidence": "Actual ApplicationAPFrame, Mario/WaterGun state, director fireGetNozzle callback, SelectMenu navigation and native card save/load; no collisions/rendered menu/campaign",
              "executable": executable, "cases": cases}
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Native runtime option matrix passed: {len(cases)} cases; {output}")


if __name__ == "__main__":
    main(str(Path(sys.argv[1]).resolve()), sys.argv[2])
