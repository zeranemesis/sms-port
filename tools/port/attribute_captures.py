"""Join each capture to the game's own state at the moment it was taken.

The classifier reads the picture; this reads what the FIFO was carrying at the
same instant, and puts the two side by side. Either alone has misled this
project: a beautiful frame proved to be a pre-rendered video, and a FIFO full
of quads proved to be that same video drawn as tiles rather than gameplay
drawing nothing.

The join needs no clock synchronisation. The harness appends the game log's
byte offset to capture_index.txt as it saves each frame, so the log lines that
were current are exactly the ones just before that offset.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from classify_captures import _template, classify  # noqa: E402


def lines_before(log_bytes, offset, needles, count=1):
    """The last `count` lines matching any needle, at or before `offset`."""
    head = log_bytes[:offset].decode("utf-8", errors="replace").splitlines()
    found = []
    for line in reversed(head):
        if any(n in line for n in needles):
            found.append(line.strip())
            if len(found) >= count:
                break
    return list(reversed(found))


def short(line, marker):
    if marker not in line:
        return "-"
    return line.split(marker, 1)[1].strip()[:96]


def main(folder):
    folder = Path(folder)
    index = folder / "capture_index.txt"
    log = folder / "play_out.log"
    if not index.exists():
        print(f"no {index.name}: this run predates the harness recording log offsets, "
              "so its frames cannot be attributed. Re-run to get one.")
        return 1
    log_bytes = log.read_bytes() if log.exists() else b""
    template = _template()

    print(f"{'capture':<14} {'hudCorr':>8} {'verdict':<28} {'thp':<10} fifo")
    for raw in index.read_text(encoding="utf-8").splitlines():
        parts = raw.split("\t")
        if len(parts) < 3:
            continue
        name, _when, logField = parts[0], parts[1], parts[2]
        offset = int(logField.split("=", 1)[1])
        path = folder / name
        if not path.exists():
            continue
        hud, _lit, _rough, verdict = classify(path, template)

        thpLine = lines_before(log_bytes, offset, ["thp: open="])
        drawLine = lines_before(log_bytes, offset, ["draw 1s:"])
        thp = "open=?"
        if thpLine:
            thp = "open=" + thpLine[0].split("thp: open=", 1)[1].split(" ", 1)[0]
        fifo = short(drawLine[0], "prims=") if drawLine else "-"
        print(f"{name:<14} {hud:8.3f} {verdict:<28} {thp:<10} {fifo}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
