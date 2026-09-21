"""Say which layer each capture belongs to, from the pixels alone.

A THP video, an in-engine cutscene and real gameplay look alike at a glance
and prove entirely different things: the video proves one textured quad works,
the cutscene proves the geometry path works, and only gameplay proves the game
is playable. Reading a screenshot by eye got that wrong twice in one session -
once calling a cutscene "the 3D engine working", then calling the video's
tiled quads "gameplay drawing nothing".

The decisive cue is Super Mario Sunshine's HUD, which is drawn only in
gameplay: the coin counter at top left and the LIFE meter at top right, both
in a saturated orange-yellow that nothing else on screen uses in that corner.
So the classification is done on the HUD, and the world is judged separately
on the rest of the frame - which is what separates "gameplay, world rendered"
from "gameplay, world missing", the question worth answering.

Used together with capture_index.txt, which records the game log's byte
offset at each capture, a frame can be attributed both ways and the two
answers must agree.
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image

# The client area sits below the title bar; the game letterboxes inside it.
TITLE_BAR = 31
# The two HUD corners, in window coordinates at 1280x960.
COIN_BOX = (40, 60, 420, 220)
LIFE_BOX = (1040, 50, 1270, 280)


def _template():
    """The LIFE meter, taken from a frame whose HUD was confirmed by eye.

    A colour histogram was tried first and is not good enough: the Toads in
    the intro video wear red and yellow mushroom caps that land in both HUD
    corners, and the video was classified as gameplay. The meter is a fixed
    overlay at a fixed place, so matching its actual shape is both stricter
    and simpler than trying to describe its colours.
    """
    path = Path(__file__).with_name("hud_life_template.png")
    if not path.exists():
        raise SystemExit(
            f"missing {path} - crop LIFE_BOX out of a frame whose HUD you have "
            "confirmed by eye and save it there")
    a = np.asarray(Image.open(path).convert("L")).astype(float)
    return a


def correlate(region, template):
    """Normalised cross-correlation, so brightness behind the HUD cannot
    change the answer."""
    if region.shape != template.shape:
        return 0.0
    x = region - region.mean()
    y = template - template.mean()
    denom = float(np.sqrt((x * x).sum() * (y * y).sum()))
    if denom == 0.0:
        return 0.0
    return float((x * y).sum() / denom)


def classify(path, template=None):
    if template is None:
        template = _template()
    img = Image.open(path).convert("RGB")
    a = np.asarray(img)
    lifeRegion = np.asarray(img.convert("L")).astype(float)[
        LIFE_BOX[1]:LIFE_BOX[3], LIFE_BOX[0]:LIFE_BOX[2]]
    life = correlate(lifeRegion, template)

    # The world, judged away from the HUD corners so the HUD cannot stand in
    # for a scene that is not there.
    body = a[TITLE_BAR + 300:, :].astype(int)
    lum = body.mean(axis=2)
    lit = float((lum > 24).mean())
    rough = float(np.abs(np.diff(lum, axis=1)).mean())

    hud = life > 0.80
    # A rendered world is both lit and structured; a flat fade is lit but
    # smooth, and that difference is the whole point of measuring roughness
    # rather than brightness.
    world = lit > 0.15 and rough > 0.5

    if hud and world:
        verdict = "GAMEPLAY, world rendered"
    elif hud:
        verdict = "GAMEPLAY, world MISSING"
    elif world:
        verdict = "cutscene or video (no HUD)"
    else:
        verdict = "blank / transition"
    return life, lit, rough, verdict


def main(folder):
    paths = sorted(Path(folder).glob("play_*.png"))
    if not paths:
        print(f"no captures in {folder}")
        return 1
    template = _template()
    print(f"{'capture':<14} {'hudCorr':>8} {'lit%':>6} {'rough':>6}  verdict")
    counts = {}
    for p in paths:
        life, lit, rough, verdict = classify(p, template)
        counts[verdict] = counts.get(verdict, 0) + 1
        print(f"{p.name:<14} {life:8.3f} {lit*100:6.1f} {rough:6.2f}  {verdict}")
    print()
    for verdict, n in sorted(counts.items(), key=lambda kv: -kv[1]):
        print(f"  {n:3d}  {verdict}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
