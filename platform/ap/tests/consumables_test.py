"""Production bridge with synthetic native flags/cursor; no game assets needed."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def main(executable):
    env = os.environ.copy()
    with tempfile.TemporaryDirectory(prefix="sms-ap-consumables-") as directory:
        root = Path(directory)
        env["SMS_AP_BRIDGE"] = directory
        (root / "session.txt").write_text(
            "SMSAP1\n616263 0 1 3 0 1\n1\n1 86\n", encoding="ascii"
        )
        history = (
            "SMSAPH1\n616263 0 1 " + "11" * 16 + "\n4\n"
            "0 523140 10 2 0\n1 523014 11 2 0\n"
            "2 523140 12 2 0\n3 523004 13 2 1\n"
        )
        (root / "history.txt").write_text(history, encoding="ascii")

        def run(applied=0, lives=3):
            result = subprocess.run(
                [executable, "fixture", str(applied), str(lives)], env=env,
                capture_output=True, text=True, timeout=10,
            )
            assert result.returncode == 0, result.stderr
            return json.loads((root / "snapshot.json").read_text())

        snapshot = run()
        assert snapshot["ready"] and not snapshot["error"], snapshot
        assert snapshot["one_up_count"] == snapshot["one_up_applied_count"] == 2
        assert snapshot["lives"] == 5  # Every distinct indexed receipt grants.
        assert snapshot["received_shines"] == 1  # Parked blue does not block.
        assert snapshot["parked_indices"] == [1]
        journal = root / ("journal-" + "11" * 16 + ".txt")
        assert journal.read_text() == history
        assert run(2, 4)["lives"] == 4  # Restart after spending a saved life.
        assert run(1, 3)["lives"] == 4  # Older save restores cursor with lives.
        assert run(0, 98)["lives"] == 99  # Cap consumes all receipts.
        assert run(2, 98)["lives"] == 98  # No delayed payout after dropping cap.
        snapshot = run(3, 3)
        assert not snapshot["ready"] and "reconciliation failed" in snapshot["error"]
        assert snapshot["lives"] == 3
        (root / "history.txt").write_text(
            history.replace("0 523140 10 2 0", "0 523140 99 2 0"), encoding="ascii"
        )
        snapshot = run(0, 3)
        assert not snapshot["ready"] and "conflicts" in snapshot["error"]
        assert snapshot["lives"] == 3 and journal.read_text() == history
        (root / "history.txt").write_text(history, encoding="ascii")

        # A normal ending flag must publish during the movie/file-select state,
        # while receipt application and location scans remain paused.
        process = subprocess.Popen(
            [executable, "serve", "0"], env=env, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        try:
            def command(text):
                process.stdin.write(text + "\n")
                process.stdin.flush()

            command("ready 0")
            command("check 119")
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                try:
                    snapshot = json.loads((root / "snapshot.json").read_text())
                    if snapshot["pid"] == process.pid and snapshot["goal"]:
                        break
                except (OSError, ValueError):
                    pass
                time.sleep(0.02)
            assert snapshot["pid"] == process.pid and snapshot["goal"], snapshot
            assert not snapshot["ready"] and snapshot["checks"] == []
            assert snapshot["lives"] == 3  # No consumables during ending movie.
            command("quit")
            process.stdin.close()
            assert process.wait(timeout=5) == 0
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
    print("SMS AP production bridge: batched 1-UPs, replay, saved cursor reload, "
          "cap, conflict and ending-goal tests passed (synthetic native callbacks)")


if __name__ == "__main__":
    main(str(Path(sys.argv[1]).resolve()))
