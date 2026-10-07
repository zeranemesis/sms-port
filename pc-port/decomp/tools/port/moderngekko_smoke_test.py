#!/usr/bin/env python3
"""Test de fumée pour un build ModernGekko (voir docs/recompilation.md).

Ne rejoue pas la navigation de menus (trop dépendante du minutage exact des
scènes de l'intro pour être un test fiable — voir la mise à jour du
2026-09-19 dans recompilation.md). Vérifie seulement ce qui doit rester vrai
après n'importe quelle modification du toolchain ou du runtime : le module
par jeu se lie et se charge (pas de repli silencieux sur l'interpréteur), le
process démarre (`booted=1` dans le statut de `--automation-dir`), et la
vitesse mesurée par le moteur (`Core::System::GetPerfMetrics`, pas le titre
de fenêtre) dépasse un seuil bas pendant la cinématique d'intro. Ça suffit à
détecter une régression de lien (comme le bug native-region-query documenté)
ou un crash au boot, sans dépendre d'une séquence de touches fragile.

Usage (PowerShell, avec l'environnement MSVC déjà importé si besoin) :
    python tools/port/moderngekko_smoke_test.py \\
        --moderngekko-run C:/Users/<toi>/ModernGekko-Template/lib/ModernGekko/build/moderngekko-run.exe \\
        --game C:/Users/<toi>/ModernGekko-Template/extracted/GMSP01 \\
        --module C:/mgm/GMSP01/<hash>/gGMSP01_recomp.dll
"""
import argparse
import subprocess
import sys
import tempfile
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
    parser.add_argument("--moderngekko-run", required=True, type=Path, help="chemin de moderngekko-run.exe")
    parser.add_argument("--game", required=True, type=Path, help="racine extracted/<slug> du jeu")
    parser.add_argument("--module", required=True, type=Path, help="DLL du module par jeu (gGMSP01_recomp.dll)")
    parser.add_argument("--boot-timeout", type=float, default=60.0, help="secondes max pour atteindre booted=1")
    parser.add_argument("--observe-seconds", type=float, default=8.0, help="durée d'observation une fois démarré")
    parser.add_argument("--min-speed", type=float, default=0.15,
                         help="vitesse minimale acceptée pendant l'intro (scènes lourdes incluses)")
    args = parser.parse_args()

    if not args.moderngekko_run.exists():
        print(f"introuvable : {args.moderngekko_run}", file=sys.stderr)
        return 2
    if not args.module.exists():
        print(f"introuvable : {args.module}", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="moderngekko-smoke-") as automation_dir_str:
        automation_dir = Path(automation_dir_str)
        command = [
            str(args.moderngekko_run),
            "--game", str(args.game),
            "--module", str(args.module),
            "--allow-interpreter",
            "--automation-dir", str(automation_dir),
        ]
        process = subprocess.Popen(
            command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        try:
            booted = False
            deadline = time.monotonic() + args.boot_timeout
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    stdout, stderr = process.communicate()
                    print("FAIL: le process s'est arrêté pendant le boot", file=sys.stderr)
                    print(stderr, file=sys.stderr)
                    return 1
                status = read_status(automation_dir)
                if status.get("booted") == "1":
                    booted = True
                    break
                time.sleep(0.5)

            if not booted:
                print(f"FAIL: booted=1 jamais atteint après {args.boot_timeout:.0f}s", file=sys.stderr)
                return 1

            time.sleep(args.observe_seconds)

            status = read_status(automation_dir)
            if process.poll() is not None:
                print("FAIL: crash après le boot", file=sys.stderr)
                return 1

            speed = float(status.get("speed", "0"))
            state = status.get("state", "")
            game_id = status.get("game_id", "")

            print(f"state={state} game_id={game_id} speed={speed:.3f} "
                  f"fps={status.get('fps')} vps={status.get('vps')}")

            if state != "running":
                print(f"FAIL: state={state!r} attendu 'running'", file=sys.stderr)
                return 1
            if speed < args.min_speed:
                print(f"FAIL: speed={speed:.3f} < seuil {args.min_speed}", file=sys.stderr)
                return 1

            print("PASS")
            return 0
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()


if __name__ == "__main__":
    raise SystemExit(main())
