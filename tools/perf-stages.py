"""Frame-time survey of Super Mario Sunshine's stages on the Aurora build.

One sms.exe process plays every stage in turn through SMS_STAGE_TOUR
(platform/stage_tour.cpp): no title screen, no file select, no debugger. The
saves, front-end data and settings are copies, so a run never touches
saves/card-a. Results go to artifacts/perf-stages-<time>[-label]/.

    python tools/perf-stages.py                     # every stage below
    python tools/perf-stages.py 2,0 9,0             # chosen stage,scenario pairs
    python tools/perf-stages.py --frames 1800 --label after

Each stage's numbers cover the frames between its start and the moment it was
asked to leave, minus the first --settle frames (load and intro fade).
"""
from pathlib import Path
import argparse, json, os, shutil, statistics, subprocess, sys, time

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / 'build/windows-64-pal-aurora/sms.exe'

STAGES = [
    ('1,0', 'Delfino Plaza'), ('0,0', 'Airport'),
    ('2,0', 'Bianco Hills'), ('3,0', 'Ricco Harbor'), ('4,0', 'Gelato Beach'),
    ('5,0', 'Pinna Beach'), ('13,0', 'Pinna Park'), ('6,0', 'Sirena Beach'),
    ('7,0', 'Hotel Delfino'), ('14,0', 'Casino'), ('8,0', 'Pianta Village'),
    ('9,0', 'Noki Bay'), ('16,0', 'Noki undersea'), ('55,0', 'Bianco boss'),
    ('56,0', 'Delfino boss'), ('57,0', 'Noki boss'), ('58,0', 'Pinna boss'),
    ('59,0', 'Ricco boss'), ('52,0', 'Corona secret'), ('60,0', 'Corona boss'),
]

# Mario idles, then the camera turns both ways and Mario walks, so a stage
# is seen from more than one angle. Fields repeat every stage period.
def presses(period, count):
    out = []
    for i in range(count):
        base = i * period
        out += [f'cstick_right@{base + period // 3}+180', f'cstick_left@{base + period // 2}+180',
                f'stick_up@{base + 2 * period // 3}+90']
    return ','.join(out)


def percentile(values, p):
    s = sorted(values)
    return s[min(len(s) - 1, int(p / 100 * len(s)))]


def summarise(times, draws, segment):
    t = [times[i] for i in segment if i in times]
    if len(t) < 120:
        return {'status': 'too few frames', 'frames': len(t)}
    d = [draws.get(i, 0) for i in segment if i in times]
    worst = sorted((i for i in segment if i in times), key=lambda i: -times[i])[:8]
    return dict(status='ok', frames=len(t), fps=round(1000 * len(t) / sum(t), 2),
                median_ms=round(statistics.median(t), 2), p95_ms=round(percentile(t, 95), 2),
                p99_ms=round(percentile(t, 99), 2), max_ms=round(max(t), 2),
                slow_pct=round(100 * sum(v > 25 for v in t) / len(t), 2),
                hitch_pct=round(100 * sum(v > 40 for v in t) / len(t), 2),
                draws_median=int(statistics.median(d)), worst_frames=worst)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('stages', nargs='*')
    parser.add_argument('--frames', type=int, default=1500, help='game frames per stage')
    parser.add_argument('--settle', type=int, default=240, help='frames ignored after each start')
    parser.add_argument('--label', default='')
    parser.add_argument('--dump-every', type=int, default=0)
    parser.add_argument('--fps', default='60')
    parser.add_argument('--extra-env', action='append', default=[], help='NAME=VALUE')
    args = parser.parse_args()
    names = dict(STAGES)
    stages = [(w, names.get(w, 'stage ' + w)) for w in (args.stages or [w for w, _ in STAGES])]

    out = ROOT / 'artifacts' / ('perf-stages-' + time.strftime('%Y%m%d-%H%M%S') +
                                (('-' + args.label) if args.label else ''))
    out.mkdir(parents=True)
    shutil.copytree(ROOT / 'saves/card-a', out / 'card-a')
    (out / 'frontend').mkdir()
    shutil.copy2(ROOT / 'settings.txt', out / 'settings.txt')
    # Frame rate 60 doubles the fields per game frame; leave room for loads.
    period = args.frames * (1 if args.fps == '60' else 2) + 600
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SMS_', 'AURORA_'))}
    env.update(
        PATH='C:\\msys64\\mingw64\\bin;' + env['PATH'],
        SMS_SETTINGS=str(out / 'settings.txt'), SMS_BINDINGS=str(ROOT / 'bindings.txt'),
        SMS_SAVE_DIR=str(out / 'card-a'), SMS_FRONTEND_DIR=str(out / 'frontend'),
        SMS_DISC_ROOT=str(ROOT / 'rom/GMSP01-extracted/files'), SMS_SKIP_MOVIES='1',
        SMS_STAGE_TOUR=';'.join(w for w, _ in stages), SMS_STAGE_TOUR_FRAMES=str(args.frames),
        SMS_STAGE_TOUR_LOG=str(out / 'tour.log'),
        SMS_AUTOPRESS=presses(period, len(stages)),
        SMS_FRAME_RATE=args.fps, SMS_GX_STATS='0', SMS_VI_DETERMINISTIC='0',
        SMS_VI_PRECISE_SLEEP='1', SMS_AURORA_BACKEND='d3d12', SMS_WINDOW_MODE='windowed',
        SMS_MENU_ON_START='0', SMS_GX_FRAMELOG=str(out / 'framelog.txt'),
        SMS_GX_PROFILE=str(out / 'gx-profile.log'), SMS_GX_PROFILE_LIGHT='1',
        SMS_AURORA_FRAME_PROFILE=str(out / 'frame-profile.log'),
        SMS_AURORA_PEEK_PROFILE=str(out / 'peek-profile.log'),
        SMS_AURORA_PIPELINE_PROFILE=str(out / 'pipeline-profile.log'))
    if args.dump_every:
        (out / 'frames').mkdir()
        env.update(SMS_GX_DUMP_DIR=str(out / 'frames'), SMS_GX_DUMP_EVERY=str(args.dump_every),
                   SMS_GX_DUMP_FROM='1')
    for item in args.extra_env:
        k, _, v = item.partition('=')
        env[k] = v

    started = time.monotonic()
    timeout = 120 + len(stages) * (args.frames // 30 + 60)
    with (out / 'stdout.log').open('w') as so, (out / 'stderr.log').open('w') as se:
        try:
            code = subprocess.run([str(EXE), '--no-launcher'], cwd=ROOT, env=env, stdout=so,
                                  stderr=se, timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            code = 'timeout'

    times, draws = {}, {}
    for line in (out / 'gx-profile.log').read_text().splitlines():
        r = line.split()
        if len(r) > 3:
            times[int(r[0])] = float(r[1])
            draws[int(r[0])] = int(r[3])
    events = [l.split() for l in (out / 'tour.log').read_text().splitlines() if l.strip()]
    results = []
    for (warp, name) in stages:
        s, c = warp.split(',')
        starts = [int(e[3]) for e in events if e[0] == 'start' and e[1] == s and e[2] == c]
        leaves = [int(e[3]) for e in events if e[0] == 'leave' and e[1] == s and e[2] == c]
        r = {'warp': warp, 'name': name}
        if not starts or not leaves:
            r['status'] = 'not reached'
        else:
            r.update(summarise(times, draws, range(starts[-1] + args.settle, leaves[-1])))
            r['first_frame'] = starts[-1]
        results.append(r)
        print(f"{warp:>6} {name:<15} {r['status']:<8} fps {r.get('fps', '-'):>6} "
              f"slow {r.get('slow_pct', '-'):>5}% hitch {r.get('hitch_pct', '-'):>5}% "
              f"p99 {r.get('p99_ms', '-'):>6} max {r.get('max_ms', '-'):>6} "
              f"draws {r.get('draws_median', '-')}", flush=True)
    summary = {'exit': code, 'seconds': round(time.monotonic() - started, 1),
               'frames_per_stage': args.frames, 'settle': args.settle, 'stages': results}
    (out / 'results.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f"exit {code} in {summary['seconds']} s -> {out}")


if __name__ == '__main__':
    sys.exit(main())
