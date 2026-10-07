#!/usr/bin/env python3
"""Trie les sites de code auto-modifiant signales par dolrecomp.

`docs/recompilation.md` porte un « point ouvert : code auto-modifiant ». Ce que
dolrecomp signale est precis et reproductible, mais la liste brute
(`generated/generated_smc.txt`, ~136 plages) ne dit pas ou elle se trouve, donc
personne ne peut la trier sans y passer une heure.

Lecture seule, sans build :

    python tools/port/triage_smc.py

Utilise `config/GMSP01/symbols.txt` (12 749 fonctions avec leur taille) pour
rattacher chaque plage a la fonction qui la contient, et regroupe par fonction.

## Ce que le signal de dolrecomp recouvre vraiment

`extern/dolrecomp/src/analysis/smc.c` (`analyze_smc_section`) ne signale que
deux choses :

1. une instruction **`icbi`** ;
2. un **stockage** dont l'adresse effective est *connue a la compilation* et qui
   tombe dans une section code (`code_range_overlaps` ne consider que les
   sections en `EMBEDDED_DATA_DOL`).

**Le signal a ete audite : ce sont des faux positifs.** `tools/port/smc_audit.py`
desassemble les 148 notes et rejoue la propagation ; la cause est que le balayage
est lineaire et ne respecte ni les branches ni les frontieres de fonctions, si
bien qu'un registre pose dans une fonction (souvent un pointeur de callback)
survit au `blr` suivant et se retrouve lu comme parametre dans la fonction
suivante. Aucun correctif de code auto-modifiant n'est requis pour le port. Le
detail est dans `docs/recompilation.md`.

Ce script reste utile pour l cartels : il rattache chaque site a sa fonction.

## Piege : le fichier liste des plages, pas des instructions

`smc_note` fusionne deux notes distantes de 4 octets ou moins. L'interieur d'une
plage contient donc des adresses qui n'ont jamais ete signalees, et compter les
octets revient a inventer des instructions. Sur `main.dol` : 135 plages, 259
octets, mais **seulement 148 notes reelles**. Seuls les bouts de plages sont
garantis notes, et ce sont eux qu'il faut attribuer a une fonction.
"""
import bisect
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SYMBOLS = ROOT / "config" / "GMSP01" / "symbols.txt"
SMC = ROOT / "generated" / "generated_smc.txt"

RE_SYM = re.compile(
    r"^(?P<name>\S+)\s*=\s*\.text:(?P<addr>0x[0-9A-Fa-f]+);\s*"
    r"//\s*type:function\s+size:(?P<size>0x[0-9A-Fa-f]+)"
)
RE_RANGE = re.compile(r"^0x(?P<start>[0-9A-Fa-f]+)-0x(?P<end>[0-9A-Fa-f]+)")


def load_functions():
    """(adresses croissantes, (debut, fin, nom)) - triee, pour une recherche
    binaire. Sans le tri, un bisect ne veut rien dire."""
    funcs = []
    if not SYMBOLS.is_file():
        sys.exit(f"{SYMBOLS} introuvable")
    for line in SYMBOLS.read_text(encoding="utf-8", errors="replace").splitlines():
        m = RE_SYM.match(line.strip())
        if not m:
            continue
        start = int(m.group("addr"), 16)
        size = int(m.group("size"), 16)
        funcs.append((start, start + size, m.group("name")))
    funcs.sort()
    return funcs


def enclosing(funcs, starts, addr):
    i = bisect.bisect_right(starts, addr) - 1
    if i < 0:
        return None
    start, end, name = funcs[i]
    return name if start <= addr < end else None


def main():
    if not SMC.is_file():
        sys.exit(f"{SMC} absent : lancer tools/port/recompile.py d'abord")

    funcs = load_functions()
    starts = [f[0] for f in funcs]

    ranges = []
    for line in SMC.read_text(encoding="utf-8", errors="replace").splitlines():
        m = RE_RANGE.match(line.strip())
        if m:
            ranges.append((int(m.group("start"), 16), int(m.group("end"), 16)))

    by_func = {}
    orphans = []
    # Seuls les bouts de plages sont garantis notes : l'interieur d'une plage
    # fusionnee n'a jamais ete signale (voir la note de module).
    notes = sorted({a for a, _ in ranges} | {b for _, b in ranges})
    for addr in notes:
        name = enclosing(funcs, starts, addr)
        if name is None:
            orphans.append(addr)
        else:
            by_func.setdefault(name, []).append(addr)

    nbytes = sum(e - s + 1 for s, e in ranges)
    merged = sum(1 for s, e in ranges if s < e)
    print(f"{len(ranges)} plages signalees ({nbytes} octets, dont "
          f"{merged} fusionnees) -> {len(notes)} notes reelles\n")

    if not by_func:
        print("aucun site ne tombe dans une fonction connue - les adresses "
              "sont-elles bien dans .text ?")
        return 1

    ranked = sorted(by_func.items(), key=lambda kv: (-len(kv[1]), kv[0]))
    print(f"{'notes':>12}  {'':>3}  fonction")
    print("-" * 72)
    for name, addrs in ranked[:40]:
        print(f"{len(addrs):>12}  {'':>3}  {name}")

    if len(ranked) > 40:
        print(f"... et {len(ranked) - 40} autres fonctions")

    total = sum(len(v) for v in by_func.values())
    print(f"\n{total} notes reparties sur {len(by_func)} fonctions, "
          f"{len(orphans)} hors fonction connue.")
    if ranked:
        top_name, top_addrs = ranked[0]
        print(f"La plus concernee est {top_name} avec {len(top_addrs)} "
              f"notes ({100.0 * len(top_addrs) / max(total, 1):.0f}%).")
    print("\nCes sites sont des faux positifs : voir tools/port/smc_audit.py "
          "et docs/recompilation.md.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
