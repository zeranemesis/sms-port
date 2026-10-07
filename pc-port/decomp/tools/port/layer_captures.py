#!/usr/bin/env python3
"""Attribuer chaque capture a la couche qui l'a produite.

docs/port_todo.md section 1.0 : les trois couches se ressemblent a l'ecran et
ne prouvent pas la meme chose, et trois conclusions ont deja ete tirees de la
mauvaise. Le harnais de session (dolphinjet_play_session.ps1) ecrit deja
capture_index.txt avec l'horodatage et la taille du log au moment de la
capture ; ce script fait le joint entre cet index et le log, pour que chaque
image soit lisible comme une preuve plutot que comme une intuition.

Lecture seule, hors execution du jeu :

    python tools/port/layer_captures.py <dossier_de_session>

Le dossier est celui passe a -OutDir : il doit contenir capture_index.txt et
play_out.log. Affiche un tableau et ecrit layer_report.txt.

Ce que cet outil ne peut pas dire, et qu'il ne dit donc pas : si le jeu est
jouable. La table de docs/port_todo.md demande, pour le jeu reel, un HUD
affiche ET Mario controllable - les deux se constatent a l'oeil. Ici on s'arrete
a la couche que le journal nomme sans ambiguite.
"""

import re
import sys
from pathlib import Path

# Les trois couches, et ce qu'une belle image y prouve. La colonne "prouve" est
# le point : c'est elle qui interdit de lire une image comme la preuve d'un
# etat de jeu.
THP = "video THP"
ENGINE = "moteur 3D"
UNKNOWN = "indeterminee"

LAYER_PROVES = {
    THP: "un quad textureDecode - ne prouve ni le monde, ni le jeu",
    ENGINE: "le chemin geometrique - ni le monde, ni le jeu",
    UNKNOWN: "rien : le journal ne nomme aucune couche a cet instant",
}

RE_INDEX = re.compile(r"^(?P<name>\S+)\s+(?P<time>\S+)\s+logBytes=(?P<bytes>\d+)\s*$")
RE_THP = re.compile(rb"thp: open=(\d+)")
RE_COPY = re.compile(rb"copy 1s: (.*)")
RE_HEARTBEAT = re.compile(rb"heartbeat: .*?pc=(0x[0-9a-fA-F]+)")
RE_UNRESOLVED = re.compile(rb"UNRESOLVED")
RE_NO_COPY = re.compile(rb"the game is not asking for render-to-texture")


def read_index(path):
    rows = []
    # `utf-8-sig` et non `utf-8` : PowerShell 5.1 ecrit capture_index.txt avec
    # un BOM, qui n'est ni un espace ni de l'ASCII - `strip()` ne le retire pas,
    # et le nom de la premiere capture se retrouvait prefixe par U+FEFF.
    for line in path.read_text(encoding="utf-8-sig", errors="replace").splitlines():
        m = RE_INDEX.match(line.strip())
        if m:
            rows.append((m.group("name"), m.group("time"), int(m.group("bytes"))))
    return rows


def last(pattern, blob):
    found = None
    for m in pattern.finditer(blob):
        found = m
    return found


def rtt_verdict(line):
    """Rend ce que le rapport de copie dit du rendu vers texture.

    Le port emet une ligne par seconde (src/port/recomp_gx_copy.cpp). Trois
    reponses possibles, et elles ne se confondent pas : aucune demande, des
    demandes, ou des demandes qui n'ont pas ete resolues.
    """
    if line is None:
        return "-"
    if RE_NO_COPY.search(line):
        return "aucune demande"
    if RE_UNRESOLVED.search(line):
        return "demande NON resolue"
    return "demande"


def main(folder):
    folder = Path(folder)
    index_path = folder / "capture_index.txt"
    log_path = folder / "play_out.log"
    if not index_path.is_file():
        print(f"pas de capture_index.txt dans {folder}")
        return 1
    if not log_path.is_file():
        print(f"pas de play_out.log dans {folder}")
        return 1

    rows = read_index(index_path)
    if not rows:
        print("capture_index.txt ne contient aucune capture")
        return 1
    log = log_path.read_bytes()

    header = (f"{'capture':<14} {'instant':<26} {'couche':<12} {'rtt':<20} {'pc':<12}")
    print(header)
    print("-" * len(header))

    report_lines = [header, "-" * len(header)]
    unknown = 0

    for name, when, nbytes in rows:
        # Le log jusqu'a la capture, et pas apres : l'image reflete ce qui a ete
        # journalise jusqu'a ce moment-la, pas ce qui le sera ensuite.
        blob = log[:nbytes] if 0 <= nbytes <= len(log) else log

        thp = last(RE_THP, blob)
        if thp is None:
            layer = UNKNOWN
            unknown += 1
        elif thp.group(1) == b"1":
            layer = THP
        else:
            layer = ENGINE

        copy = last(RE_COPY, blob)
        rtt = rtt_verdict(copy.group(1) if copy else None)

        beat = last(RE_HEARTBEAT, blob)
        pc = beat.group(1).decode("ascii", "replace") if beat else "-"

        short_time = when[:19]
        print(f"{name:<14} {short_time:<26} {layer:<12} {rtt:<20} {pc:<12}")
        report_lines.append(f"{name:<14} {short_time:<26} {layer:<12} {rtt:<20} {pc:<12}")
        report_lines.append(f"    journal : {LAYER_PROVES[layer]}")
        if copy:
            report_lines.append(f"    copie   : {copy.group(1).decode('utf-8', 'replace')}")

    print()
    if unknown:
        print(f"{unknown} capture(s) sans marqueur de couche : le journal n'a rien dit avant, "
              "rejouez avec une session plus longue ou regardez le debut du log")
    print(f"{len(rows)} capture(s). Une capture '{ENGINE}' prouve le chemin geometrique, "
          "pas le monde affiche et pas le jeu : verifier le HUD et la main du joueur a l'oeil.")

    out = folder / "layer_report.txt"
    out.write_text("\n".join(report_lines) + "\n", encoding="utf-8")
    print(f"rapport ecrit dans {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
