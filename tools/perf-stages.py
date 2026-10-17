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

# Every stage,scenario with an archive in data/stageArc.bin, named by it. The
# test maps (11, 12) and the options screen (15) are left out.
ALL_STAGES = [
    ('0,0', 'airport0'),
    ('0,1', 'airport1'),
    ('1,0', 'dolpic0'),
    ('1,1', 'dolpic1'),
    ('1,2', 'dolpic10'),
    ('1,5', 'dolpic5'),
    ('1,6', 'dolpic6'),
    ('1,7', 'dolpic7'),
    ('1,8', 'dolpic8'),
    ('1,9', 'dolpic9'),
    ('2,0', 'bianco0'),
    ('2,1', 'bianco1'),
    ('2,2', 'bianco2'),
    ('2,3', 'bianco3'),
    ('2,4', 'bianco4'),
    ('2,5', 'bianco7'),
    ('2,6', 'bianco6'),
    ('2,7', 'bianco5'),
    ('3,0', 'ricco0'),
    ('3,1', 'ricco1'),
    ('3,2', 'ricco2'),
    ('3,3', 'ricco3'),
    ('3,4', 'ricco4'),
    ('3,5', 'ricco5'),
    ('3,6', 'ricco6'),
    ('3,7', 'ricco7'),
    ('4,0', 'mamma0'),
    ('4,1', 'mamma1'),
    ('4,2', 'mamma2'),
    ('4,3', 'mamma3'),
    ('4,4', 'mamma4'),
    ('4,5', 'mamma5'),
    ('4,6', 'mamma6'),
    ('4,7', 'mamma7'),
    ('5,0', 'pinnaBeach0'),
    ('5,1', 'pinnaBeach1'),
    ('5,2', 'pinnaBeach3'),
    ('5,3', 'pinnaBeach2'),
    ('5,4', 'pinnaBeach4'),
    ('5,5', 'pinnaBeach4'),
    ('5,6', 'pinnaBeach4'),
    ('5,7', 'pinnaBeach4'),
    ('6,0', 'sirena0'),
    ('6,1', 'sirena1'),
    ('6,2', 'sirena2'),
    ('6,3', 'sirena3'),
    ('6,4', 'sirena4'),
    ('6,5', 'sirena5'),
    ('6,6', 'sirena6'),
    ('6,7', 'sirena7'),
    ('7,0', 'delfino0'),
    ('7,1', 'delfino1'),
    ('7,2', 'delfino2'),
    ('7,3', 'delfino3'),
    ('7,4', 'delfino4'),
    ('8,0', 'monte0'),
    ('8,1', 'monte5'),
    ('8,2', 'monte2'),
    ('8,3', 'monte1'),
    ('8,4', 'monte4'),
    ('8,5', 'monte3'),
    ('8,6', 'monte6'),
    ('8,7', 'monte7'),
    ('9,0', 'mare0'),
    ('9,1', 'mare1'),
    ('9,2', 'mare2'),
    ('9,3', 'mare3'),
    ('9,4', 'mare4'),
    ('9,5', 'mare5'),
    ('9,6', 'mare6'),
    ('9,7', 'mare7'),
    ('13,0', 'pinnaParco0'),
    ('13,1', 'pinnaParco1'),
    ('13,2', 'pinnaParco2'),
    ('13,3', 'pinnaParco3'),
    ('13,4', 'pinnaParco4'),
    ('13,5', 'pinnaParco5'),
    ('13,6', 'pinnaParco6'),
    ('13,7', 'pinnaParco7'),
    ('13,8', 'pinnaDemo0'),
    ('13,9', 'pinnaDemo1'),
    ('13,10', 'pinnaDemo2'),
    ('14,0', 'casino0'),
    ('14,1', 'casino1'),
    ('16,0', 'mareUndersea'),
    ('20,0', 'dolpic_ex0'),
    ('21,0', 'dolpic_ex1'),
    ('22,0', 'dolpic_ex2'),
    ('23,0', 'dolpic_ex3'),
    ('24,0', 'dolpic_ex4'),
    ('25,0', 'dolpic_ex5'),
    ('26,0', 'dolpic_ex6'),
    ('27,0', 'dolpic_ex7'),
    ('28,0', 'bia_ex0'),
    ('29,0', 'bia_ex1'),
    ('30,0', 'rico_ex0'),
    ('31,0', 'rico_ex1'),
    ('32,0', 'mam_ex0'),
    ('33,0', 'mam_ex1'),
    ('34,0', 'pinna_ex0'),
    ('35,0', 'pinna_ex1'),
    ('36,0', 'pinna_ex2'),
    ('37,0', 'pinna_ex3'),
    ('38,0', 'pinna_ex4'),
    ('39,0', 'pinna_ex5'),
    ('40,0', 'sirena_ex0'),
    ('41,0', 'sirena_ex1'),
    ('42,0', 'monte_ex0'),
    ('43,0', 'monte_ex1'),
    ('44,0', 'mare_ex0'),
    ('45,0', 'mare_ex1'),
    ('46,0', 'coro_ex0'),
    ('47,0', 'coro_ex1'),
    ('48,0', 'coro_ex2'),
    ('49,0', 'coro_ex3'),
    ('50,0', 'coro_ex4'),
    ('51,0', 'coro_ex5'),
    ('52,0', 'coro_ex6'),
    ('55,0', 'biancoBoss'),
    ('56,0', 'delfinoBoss'),
    ('57,0', 'mareBoss'),
    ('58,0', 'pinnaBoss0'),
    ('58,1', 'pinnaBoss1'),
    ('59,0', 'ricco8'),
    ('60,0', 'coronaBoss'),
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


def run_tour(stages, out, args):
    """Plays stages in one sms.exe process. Returns (exit code, per-stage results)."""
    out.mkdir(parents=True)
    shutil.copytree(ROOT / 'saves/card-a', out / 'card-a')
    (out / 'frontend').mkdir()
    shutil.copy2(ROOT / 'settings.txt', out / 'settings.txt')
    # Frame rate 60 doubles the fields per game frame; leave room for loads.
    period = args.frames * (1 if args.fps == '60' else 2) + 600
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SMS_', 'AURORA_'))}
    env.update(
        PATH=r'C:\msys64\mingw64\bin;' + env['PATH'],
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

    timeout = 120 + len(stages) * (args.frames // 30 + 60)
    with (out / 'stdout.log').open('w') as so, (out / 'stderr.log').open('w') as se:
        try:
            code = subprocess.run([str(EXE), '--no-launcher'], cwd=ROOT, env=env, stdout=so,
                                  stderr=se, timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            code = 'timeout'

    times, draws = {}, {}
    if (out / 'gx-profile.log').exists():
        for line in (out / 'gx-profile.log').read_text().splitlines():
            r = line.split()
            if len(r) > 3:
                times[int(r[0])] = float(r[1])
                draws[int(r[0])] = int(r[3])
    events = []
    if (out / 'tour.log').exists():
        events = [l.split() for l in (out / 'tour.log').read_text().splitlines() if l.strip()]
    results = []
    for (warp, name) in stages:
        s, c = warp.split(',')
        starts = [int(e[3]) for e in events if e[0] == 'start' and e[1] == s and e[2] == c]
        leaves = [int(e[3]) for e in events if e[0] == 'leave' and e[1] == s and e[2] == c]
        r = {'warp': warp, 'name': name, 'run': out.name}
        if not starts:
            r['status'] = 'not reached'
        elif not leaves:
            r['status'] = 'crashed' if code != 0 else 'not finished'
        else:
            r.update(summarise(times, draws, range(starts[-1] + args.settle, leaves[-1])))
            r['first_frame'] = starts[-1]
        results.append(r)
    return code, results


def print_result(r):
    print(f"{r['warp']:>6} {r['name']:<22} {r['status']:<12} fps {r.get('fps', '-'):>6} "
          f"slow {r.get('slow_pct', '-'):>5}% hitch {r.get('hitch_pct', '-'):>5}% "
          f"p99 {r.get('p99_ms', '-'):>6} max {r.get('max_ms', '-'):>7} "
          f"draws {r.get('draws_median', '-')}", flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('stages', nargs='*')
    parser.add_argument('--all', action='store_true', help='every stage and episode in ALL_STAGES')
    parser.add_argument('--frames', type=int, default=1500, help='game frames per stage')
    parser.add_argument('--settle', type=int, default=240, help='frames ignored after each start')
    parser.add_argument('--chunk', type=int, default=25, help='stages per sms.exe process')
    parser.add_argument('--label', default='')
    parser.add_argument('--dump-every', type=int, default=0)
    parser.add_argument('--fps', default='60')
    parser.add_argument('--extra-env', action='append', default=[], help='NAME=VALUE')
    args = parser.parse_args()
    names = dict(STAGES)
    names.update(dict(ALL_STAGES))
    if args.stages:
        stages = [(w, names.get(w, 'stage ' + w)) for w in args.stages]
    else:
        stages = list(ALL_STAGES if args.all else STAGES)
    if args.all:
        # stageArc.bin names archives the disc does not have (pinnaDemo1,
        # most pinna_ex*): the game cannot load those either.
        scene = ROOT / 'rom/GMSP01-extracted/files/data/scene'
        missing = [w for w, n in stages
                   if not (scene / f'{n}.szs').exists() and not (scene / f'{n}.arc').exists()]
        if missing:
            print(f"skipping {len(missing)} stages with no archive on the disc: {' '.join(missing)}")
        stages = [(w, n) for w, n in stages if w not in missing]

    base = ROOT / 'artifacts' / ('perf-stages-' + time.strftime('%Y%m%d-%H%M%S') +
                                 (('-' + args.label) if args.label else ''))
    base.mkdir(parents=True)
    started = time.monotonic()
    results, runs = [], []
    remaining = stages
    while remaining:
        batch = remaining[:args.chunk]
        out = base / f'run{len(runs):02d}' if len(stages) > args.chunk or runs else base / 'run'
        code, batch_results = run_tour(batch, out, args)
        runs.append({'run': out.name, 'exit': code, 'stages': [w for w, _ in batch]})
        done = [r for r in batch_results if r['status'] not in ('not reached',)]
        for r in done:
            print_result(r)
        results += done
        if not done:
            # Nothing even started: record the rest rather than loop forever.
            for r in batch_results:
                print_result(r)
            results += batch_results
            remaining = remaining[len(batch):]
            continue
        # A crash or an early quit leaves later stages unreached: carry on
        # after the last stage that started, in a new process.
        remaining = remaining[len(done):]
    summary = {'seconds': round(time.monotonic() - started, 1), 'frames_per_stage': args.frames,
               'settle': args.settle, 'runs': runs, 'stages': results}
    (base / 'results.json').write_text(json.dumps(summary, indent=2) + '\n')
    crashed = [r['warp'] for r in results if r['status'] == 'crashed']
    print(f"{len(results)} stages in {summary['seconds']} s, crashed: {crashed or 'none'} -> {base}")


if __name__ == '__main__':
    sys.exit(main())
