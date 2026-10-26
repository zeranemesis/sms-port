"""Repeat the private Android Bianco Hills test without changing game settings.

Requires an installed debug APK, user-owned game data and an explicit ADB serial.
The reference flag restores native FIFO bookkeeping for an A/B comparison.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = 'org.smsunified.aurora'
FRAME = re.compile(r'(\d+) ([\d.]+) idle ([\d.]+) submit ([\d.]+) end ([\d.]+) acquire ([\d.]+) retraces (\d+) late ([\d.]+)')

def summarize(text, last_frame=1500):
    rows = [list(map(float, m.groups())) for line in text.splitlines()
            if (m := FRAME.fullmatch(line)) and 300 <= int(m[1]) <= last_frame]
    if not rows:
        raise RuntimeError('No complete gameplay timing rows in frames 300..1500')
    samples = sorted(r[1] for r in rows)
    return dict(samples=len(rows), first_frame=rows[0][0], last_frame=rows[-1][0],
                mean_ms=statistics.mean(samples), median_ms=statistics.median(samples),
                p90_ms=samples[int((len(samples)-1)*.9)],
                p99_ms=samples[int((len(samples)-1)*.99)],
                over40_percent=sum(v > 40 for v in samples)*100/len(samples))

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--serial', required=True)
    p.add_argument('--label', required=True)
    p.add_argument('--reference', action='store_true')
    p.add_argument('--surface-reference', action='store_true')
    p.add_argument('--stage', type=int, choices=range(11), default=2)
    p.add_argument('--scenario', type=int, choices=range(9), default=0)
    p.add_argument('--frames', type=int, default=1800)
    p.add_argument('--collect-only', action='store_true')
    p.add_argument('--cold-shaders', action='store_true')
    p.add_argument('--screenshot', action='store_true')
    a = p.parse_args()
    if not 300 <= a.frames <= 36000:
        p.error('frames must be in 300..36000')
    if not re.fullmatch(r'[A-Za-z0-9_-]+', a.label):
        p.error('label must contain only letters, digits, underscore or hyphen')
    adb = [str(Path(os.environ['LOCALAPPDATA'])/'Android/Sdk/platform-tools/adb.exe'), '-s', a.serial]
    def run(*args, check=True):
        return subprocess.run(adb+list(args), check=check, capture_output=True)
    out = ROOT/'artifacts'/('android-bench-'+a.label)
    out.mkdir(parents=True, exist_ok=True)
    def thermal():
        result = run('shell','dumpsys','thermalservice',check=False).stdout.decode(errors='replace')
        return list(dict.fromkeys(l.strip() for l in result.splitlines()
                if 'Thermal Status:' in l or 'Temperature{' in l))
    initial_thermal = thermal()
    launched = time.monotonic()
    screenshot_taken = False
    if not a.collect_only:
        launch = ['shell', 'am', 'start', '-S', '-W', '-n',
                  PACKAGE+'/org.smsunified.mobile.LauncherActivity', '--ez', 'launch_game', 'true',
                  '--ez', 'stage_diagnostic', 'true', '--ez', 'performance_profile', 'true',
                  '--ei', 'diagnostic_stage', str(a.stage), '--ei', 'diagnostic_scenario', str(a.scenario),
                  '--ei', 'diagnostic_frames', str(a.frames)]
        if a.reference:
            launch += ['--ez', 'fifo_reference', 'true']
        if a.surface_reference:
            launch += ['--ez', 'surface_reference', 'true']
        if a.cold_shaders:
            launch += ['--ez', 'shader_cold_test', 'true']
        run(*launch)
        time.sleep(2)
    deadline = time.monotonic()+max(180, a.frames/15+60)
    while run('shell', 'pidof', PACKAGE+':game', check=False).stdout.strip():
        if time.monotonic() > deadline:
            raise RuntimeError('Diagnostic did not exit in 180 seconds; no process was stopped')
        if a.screenshot and not screenshot_taken and time.monotonic()-launched > 15:
            activities = run('shell','dumpsys','activity','activities',check=False).stdout.decode(errors='replace')
            if any(PACKAGE in line and ('mResumedActivity' in line or 'topResumedActivity' in line) for line in activities.splitlines()):
                (out/'stage.png').write_bytes(run('exec-out','screencap','-p').stdout)
                screenshot_taken = True
        time.sleep(2)
    final_thermal = thermal()
    for name in ['perf-frames.log', 'perf-scheduler.log', 'perf-render-frames.log',
                 'perf-pipelines.log', 'perf-begin.log', 'perf-worker.log', 'game-runtime.log', 'settings.txt']:
        (out/name).write_bytes(run('exec-out', 'run-as', PACKAGE, 'cat', 'files/'+name).stdout)
    runtime = (out/'game-runtime.log').read_text(errors='replace').split('[android] one session per game process')[-1]
    (out/'game-runtime.log').write_text(runtime, encoding='utf-8')
    apk = ROOT/'build/android-game-app/Sunshine-Aurora-arm64-v8a.apk'
    validation_error = None
    try:
        stats = summarize((out/'perf-frames.log').read_text(), min(1500, a.frames))
    except RuntimeError as error:
        stats = None
        validation_error = str(error)
    if f'[tour] stage {a.stage},{a.scenario} ' not in runtime:
        validation_error = 'Requested stage not reached; timings may be stale'
        stats = None
    result = dict(label=a.label, fifo_reference=a.reference, surface_reference=a.surface_reference, cold_shaders=a.cold_shaders, stage=f'{a.stage},{a.scenario}', frame_window=[300, min(1500, a.frames)],
                  apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),
                  settings=(out/'settings.txt').read_text(),
                  thermal_start=initial_thermal, thermal_end=final_thermal,
                  tour_done='[tour] done' in runtime,
                  validation_error=validation_error, stats=stats)
    (out/'result.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result['stats'], indent=2), flush=True)

if __name__ == '__main__':
    main()
