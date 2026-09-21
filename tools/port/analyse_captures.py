"""Judge a run's captures with numbers, as a companion to looking at them.

Neither half is enough on its own here. Counters in this port have reported a
healthy frame while the screen was black, and a capture once turned out to be
a photograph of an unrelated window; equally, a person spotted a frame flicker
in seconds that hours of instrumentation had missed. So this prints statistics
AND expects the images to be opened.

The measurement that earns its place is the frame-to-frame difference: a
process that has stopped drawing but not crashed passes every liveness check
this project has, and shows up here as consecutive captures that are
bit-identical.
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image


def stats(path):
    img = Image.open(path).convert("RGB")
    a = np.asarray(img).astype(np.int16)
    lum = a.mean(axis=2)

    # Roughness, not the mean: random bytes average to 128, which is also the
    # mean of a real picture. Only the local variation separates the two.
    rough = float(np.abs(np.diff(lum, axis=1)).mean())

    return {
        "size": img.size,
        "black": float((lum < 8).mean() * 100.0),
        "pale": float((lum > 247).mean() * 100.0),
        "mean": float(lum.mean()),
        "rough": rough,
        "lum": lum,
    }


def main(folder):
    paths = sorted(Path(folder).glob("play_*.png"))
    if not paths:
        print(f"no captures in {folder}")
        return 1

    print(f"{'capture':<14} {'size':>10} {'black%':>7} {'pale%':>7} {'mean':>6} "
          f"{'rough':>6} {'vs prev':>8}")
    prev = None
    frozen = []
    for p in paths:
        s = stats(p)
        if prev is not None and prev.shape == s["lum"].shape:
            delta = float(np.abs(s["lum"] - prev).mean())
            shown = f"{delta:8.2f}"
            if delta < 0.05:
                frozen.append(p.name)
        else:
            shown = "       -"
        prev = s["lum"]
        print(f"{p.name:<14} {str(s['size']):>10} {s['black']:7.1f} {s['pale']:7.1f} "
              f"{s['mean']:6.1f} {s['rough']:6.2f} {shown}")

    print()
    if frozen:
        print(f"FROZEN: {len(frozen)} capture(s) identical to the one before: "
              f"{', '.join(frozen)}")
    else:
        print("no frozen capture: the picture changed between every pair")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
