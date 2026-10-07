#!/usr/bin/env python3
"""Sonde continue de performance + de crash pour un build ModernGekko.

Contrairement à `moderngekko_smoke_test.py` (un seul point de mesure, pass/
fail, pensé pour la CI), ce script est une sonde qu'on laisse tourner :
il échantillonne `status.txt` (via `--automation-dir`, donc `fps`/`vps`/
`speed` viennent réellement de `Core::System::GetPerfMetrics`, pas d'un
titre de fenêtre) à intervalle régulier pendant toute la durée du run, et
journalise chaque échantillon dans un CSV. Si le process s'arrête tout seul
avant qu'on le lui demande, c'est un crash : le code de sortie et la fin de
stderr sont capturés dans le rapport.

Preuve de niveau SCRIPTED uniquement (voir la discipline de preuve du
projet) : ça mesure la stabilité et la vitesse d'un run automatisé, pas
d'une partie jouée par un humain.

Usage :
    python tools/port/moderngekko_probe.py \\
        --moderngekko-run C:/.../moderngekko-run.exe \\
        --game C:/.../extracted/GMSP01 \\
        --module C:/mgmllvm/GMSP01/<hash>/gGMSP01_recomp.dll \\
        --duration 90
"""
import argparse
import csv
import subprocess
import sys
import time
from pathlib import Path


def read_status(automation_dir: Path) -> dict[str, str]:
    status_path = automation_dir / "status.txt"
    if not status_path.exists():
        return {}
    values: dict[str, str] = {}
    for line in status_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "=" not in line:
            continue
        key, _, value = line.partition("=")
        values[key] = value
    return values


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--moderngekko-run", required=True, type=Path)
    parser.add_argument("--game", required=True, type=Path)
    parser.add_argument("--module", required=True, type=Path)
    parser.add_argument("--automation-dir", type=Path, default=None,
                         help="par defaut, un dossier temporaire jetable")
    parser.add_argument("--duration", type=float, default=90.0, help="secondes totales de sonde")
    parser.add_argument("--sample-interval", type=float, default=2.0, help="secondes entre deux echantillons")
    parser.add_argument("--boot-timeout", type=float, default=60.0)
    parser.add_argument("--csv-out", type=Path, default=Path("moderngekko_probe.csv"))
    args = parser.parse_args()

    for path in (args.moderngekko_run, args.module):
        if not path.exists():
            print(f"introuvable : {path}", file=sys.stderr)
            return 2

    automation_dir = args.automation_dir
    owns_automation_dir = False
    if automation_dir is None:
        import tempfile
        automation_dir = Path(tempfile.mkdtemp(prefix="moderngekko-probe-"))
        owns_automation_dir = True
    automation_dir.mkdir(parents=True, exist_ok=True)

    command = [
        str(args.moderngekko_run),
        "--game", str(args.game),
        "--module", str(args.module),
        "--allow-interpreter",
        "--automation-dir", str(automation_dir),
    ]

    print(f"[probe] lancement : {' '.join(command)}")
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    samples: list[dict[str, str]] = []
    crashed = False
    crash_exit_code: int | None = None
    crash_stderr_tail = ""
    booted = False

    try:
        start = time.monotonic()
        boot_deadline = start + args.boot_timeout
        while not booted and time.monotonic() < boot_deadline:
            if process.poll() is not None:
                crashed = True
                crash_exit_code = process.returncode
                break
            status = read_status(automation_dir)
            if status.get("booted") == "1":
                booted = True
            time.sleep(0.5)

        if not booted and not crashed:
            print(f"[probe] FAIL: booted=1 jamais atteint apres {args.boot_timeout:.0f}s", file=sys.stderr)
            return 1

        if not crashed:
            print("[probe] booted=1 atteint, debut de l'echantillonnage")
            end = time.monotonic() + args.duration
            while time.monotonic() < end:
                if process.poll() is not None:
                    crashed = True
                    crash_exit_code = process.returncode
                    break
                status = read_status(automation_dir)
                sample = {
                    "t": f"{time.monotonic() - start:.1f}",
                    "state": status.get("state", ""),
                    "fps": status.get("fps", ""),
                    "vps": status.get("vps", ""),
                    "speed": status.get("speed", ""),
                    "frame_count": status.get("frame_count", ""),
                    "present_count": status.get("present_count", ""),
                    "last_error": status.get("last_error", ""),
                }
                samples.append(sample)
                print(f"[probe] t={sample['t']:>6}s state={sample['state']:<8} "
                      f"fps={sample['fps']:<8} vps={sample['vps']:<8} speed={sample['speed']}")
                time.sleep(args.sample_interval)

        if crashed:
            _, stderr = process.communicate(timeout=10)
            crash_stderr_tail = "\n".join(stderr.strip().splitlines()[-30:])
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
        if owns_automation_dir:
            import shutil
            shutil.rmtree(automation_dir, ignore_errors=True)

    if samples:
        with args.csv_out.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=list(samples[0].keys()))
            writer.writeheader()
            writer.writerows(samples)
        print(f"[probe] {len(samples)} echantillons ecrits dans {args.csv_out}")

    print()
    print("=" * 60)
    if crashed:
        print(f"CRASH: le process s'est arrete tout seul (code {crash_exit_code})")
        if crash_stderr_tail:
            print("--- fin de stderr ---")
            print(crash_stderr_tail)
        return 1

    speeds = [float(s["speed"]) for s in samples if s["speed"]]
    if speeds:
        print(f"PAS DE CRASH. {len(samples)} echantillons sur {args.duration:.0f}s.")
        print(f"speed: min={min(speeds):.3f} max={max(speeds):.3f} "
              f"moyenne={sum(speeds) / len(speeds):.3f}")
        errors = {s["last_error"] for s in samples if s["last_error"]}
        if errors:
            print(f"last_error observes pendant le run : {errors}")
    else:
        print("PAS DE CRASH, mais aucun echantillon de vitesse recupere.")
    print("=" * 60)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
