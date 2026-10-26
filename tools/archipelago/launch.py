"""Archipelago for the separate PAL Aurora build. Ordinary saves stay separate."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
import urllib.request
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[2]
RELEASE = "https://git.peliarch.ca/alaric/sunshine-archipelago/releases/download/wip-2026-10-15/"
ARCHIVE = "Sunshine-Archipelago-WIP-Windows-2026-10-15.zip"
SHA256 = "59cf22072d99ecc91b50dd9ff24876048d1782d6b7bbc99c098f571418121a80"
BUNDLE_NAME = "Sunshine-Archipelago-WIP-Windows"


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def install(cache=None):
    destination = ROOT / "third_party" / "archipelago" / BUNDLE_NAME
    if not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="sms-ap-") as temp:
            archive = Path(cache) if cache else Path(temp) / ARCHIVE
            if not cache:
                print("Téléchargement du client Archipelago Sunshine (55 Mio)…", flush=True)
                urllib.request.urlretrieve(RELEASE + ARCHIVE, archive)
            if digest(archive) != SHA256:
                raise RuntimeError("SHA-256 incorrect : le client ne sera pas installé.")
            with zipfile.ZipFile(archive) as package:
                for name in package.namelist():
                    parts = Path(name.replace("\\", "/")).parts
                    if not parts or parts[0] != BUNDLE_NAME or ".." in parts or ":" in name:
                        raise RuntimeError("Chemin invalide dans le paquet Archipelago.")
                package.extractall(temp)
            # Same-volume staging makes installation atomic, without replacing a profile.
            staging = destination.parent / ("install-" + uuid.uuid4().hex)
            try:
                shutil.copytree(Path(temp) / BUNDLE_NAME, staging)
                staging.rename(destination)
            finally:
                if staging.exists():
                    shutil.rmtree(staging)
    pins = json.loads((destination / "package.json").read_text(encoding="utf-8"))
    for name, expected in pins["sha256"].items():
        path = (destination / name).resolve()
        if not path.is_relative_to(destination.resolve()) or digest(path) != expected:
            raise RuntimeError(f"Fichier du client modifié ou manquant : {name}")
    return destination, pins


def profile_path(name):
    if not re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9_-]{0,47}", name):
        raise ValueError("Profil : 1 à 48 lettres, chiffres, tirets ou underscores.")
    return ROOT / "saves" / "archipelago" / name


def wait_file(path, process, seconds=25):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if process.poll() is not None:
            raise RuntimeError(f"Le processus s’est arrêté. Voir les journaux : {path.parent}")
        if path.exists():
            return
        time.sleep(.1)
    raise RuntimeError(f"Délai dépassé. Voir les journaux : {path.parent}")


def stop(process):
    if process and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def launch(args):
    run = profile_path(args.profile)
    game = ROOT / "build" / "windows-64-pal-archipelago" / "sms.exe"
    assets = ROOT / "rom" / "GMSP01-extracted" / "files"
    if args.plan:
        print(json.dumps(dict(game=str(game), built=game.is_file(), assets=str(assets),
                              profile=str(run), server=args.server or "solo local",
                              slot=args.slot, normal_saves_untouched=True), indent=2))
        return
    if not args.verify_server and (not game.is_file() or not assets.is_dir()):
        raise RuntimeError("Compiler Construire-Archipelago.cmd et configurer les données PAL avant de jouer.")
    bundle, pins = install(args.client_archive)
    run.mkdir(parents=True, exist_ok=True)
    for name in ("card-a", "bridge", "frontend"):
        (run / name).mkdir(exist_ok=True)
    import msvcrt
    with (run / "launcher.lock").open("a+b") as lock:
        lock.seek(0)
        msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
        control = run / "logs" / uuid.uuid4().hex
        control.mkdir(parents=True)
        env = dict(os.environ)
        env["PATH"] = str(game.parent) + os.pathsep + r"C:\msys64\mingw64\bin" + os.pathsep + env.get("PATH", "")
        env.update(SMS_AP_BRIDGE=str(run / "bridge"), SMS_SAVE_DIR=str(run / "card-a"),
                   SMS_FRONTEND_DIR=str(run / "frontend"), SMS_DISC_ROOT=str(assets),
                   SMS_SETTINGS=str(run / "settings.txt"), SMS_BINDINGS=str(run / "bindings.txt"),
                   SMS_VI_DETERMINISTIC="0", SMS_VI_PRECISE_SLEEP="1")
        for key in ("SMS_DISC_IMAGE", "SMS_VI_HZ", "SMS_AUTOPRESS", "SMS_SKIP_MOVIES", "SMS_MOVIE",
                    "SMS_CUBESHELF_MOD_LIST", "SMS_MOD", "SMS_CODE_MODS", "SMS_NET_MODE"):
            env.pop(key, None)
        for name in ("settings.txt", "bindings.txt"):
            if not (run / name).exists():
                shutil.copyfile(ROOT / name, run / name)
        children, logs = [], []
        server = None

        def start(command, name):
            log = (control / name).open("wb")
            logs.append(log)
            child = subprocess.Popen(command, cwd=run, env=env, stdin=subprocess.DEVNULL,
                                     stdout=log, stderr=subprocess.STDOUT,
                                     creationflags=subprocess.CREATE_NO_WINDOW)
            children.append(child)
            return child

        try:
            endpoint, slot = args.server, args.slot
            if not endpoint:
                slot = pins["slot"]
                server = start([str(bundle / "runtime/python/python.exe"),
                                str(bundle / "tools/recomp_local_server.py"),
                                "--ap-root", str(bundle / "runtime/ap"),
                                "--seed", str(bundle / pins["seed"]), "--save", str(run / "ap-save.apsave"),
                                "--ready-file", str(control / "ready.json"),
                                "--stop-file", str(control / "stop"), "--parent-pid", str(os.getpid())], "server.log")
                wait_file(control / "ready.json", server)
                endpoint = json.loads((control / "ready.json").read_text())["endpoint"]
            if not slot:
                raise ValueError("Indiquer --slot pour rejoindre un serveur.")
            command = [str(bundle / "bin/peliarch-sunshine.exe"), endpoint, slot, str(run / "bridge")]
            if args.password_env:
                command += ["--password-env", args.password_env]
            auth = start(command + ["--headless-seconds", "3", "--snapshot-file", str(control / "auth.json")], "auth.log")
            auth.wait(timeout=25)
            snapshot = json.loads((control / "auth.json").read_text())
            if auth.returncode or snapshot.get("ap") != "Authenticated" or snapshot.get("delivery") == "Blocked":
                raise RuntimeError(f"Connexion ou options Archipelago refusées. Journaux : {control}")
            if args.verify_server:
                print(json.dumps(dict(authenticated=True, seed=snapshot.get("seed"), slot=slot,
                                      game_started=False, logs=str(control))))
                return
            session = run / "bridge/session.txt"
            previous = session.stat().st_mtime_ns if session.exists() else 0
            client = start(command + ["--gui"], "client.log")
            wait_file(session, client)
            deadline = time.monotonic() + 25
            while session.stat().st_mtime_ns == previous:
                if client.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("Le client n’a pas confirmé la session courante.")
                time.sleep(.1)
            game_process = start([str(game), "--no-launcher"], "game.log")
            print(f"Archipelago connecté : {slot}. Sauvegarde dédiée : {run}", flush=True)
            while game_process.poll() is None and client.poll() is None:
                time.sleep(.25)
        finally:
            for child in reversed(children):
                if child is not server:
                    stop(child)
            if server:
                (control / "stop").write_text("stop\n")
                try:
                    server.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    stop(server)
            for log in logs:
                log.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", help="ws://hôte:port ou wss://hôte:port ; sinon partie solo locale")
    parser.add_argument("--slot", help="Nom exact du joueur dans la salle")
    parser.add_argument("--profile", default="solo", help="Profil séparé pour chaque seed/joueur")
    parser.add_argument("--password-env", help="Nom de la variable contenant le mot de passe")
    parser.add_argument("--plan", action="store_true")
    parser.add_argument("--verify-server", action="store_true")
    parser.add_argument("--client-archive", type=Path, help=argparse.SUPPRESS)
    launch(parser.parse_args())
