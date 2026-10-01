#!/usr/bin/env python3
"""Lance DolRecomp sur un dump GMSP01 deja extrait par dtk.

Construit d'abord l'outil `dolrecomp` (extern/dolrecomp, backend C simple,
sans LLVM ni ModernGekko - voir docs/recompilation.md), puis l'invoque sur
le DOL et la MAP fournis pour produire le C genere dans --out.

Ce script ne fait rien de plus que documenter et rejouer exactement la
commande dolrecomp attendue ; il ne sait pas extraire un disque (voir `dtk
disc extract` dans docs/recompilation.md) et ne fabrique aucune donnee de
jeu. Sans dump legalement possede, il n'y a rien a lui donner en entree.

Usage :
    python tools/port/recompile.py --dol orig/GMSP01/sys/main.dol \\
        --map orig/GMSP01/files/marioEU.MAP --out generated
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOLRECOMP_DIR = ROOT / "extern" / "dolrecomp"


def build_dolrecomp() -> Path:
    build_dir = DOLRECOMP_DIR / "build"
    exe_name = "dolrecomp.exe" if sys.platform == "win32" else "dolrecomp"
    # Single-config generators (Ninja, Makefiles) place the binary directly
    # under build_dir; multi-config generators (Visual Studio, the CMake
    # default on Windows) nest it under a per-config subdirectory instead.
    candidates = [build_dir / exe_name, build_dir / "Release" / exe_name]

    def find_exe() -> Path | None:
        for candidate in candidates:
            if candidate.exists():
                return candidate
        return None

    exe = find_exe()
    if exe is not None:
        return exe

    if not any(DOLRECOMP_DIR.iterdir()):
        sys.exit(
            f"{DOLRECOMP_DIR} est vide : lancer d'abord "
            "`git submodule update --init extern/dolrecomp`."
        )

    print("Construction de dolrecomp (backend C, sans LLVM)...")
    subprocess.run(
        ["cmake", "-S", str(DOLRECOMP_DIR), "-B", str(build_dir),
         "-DCMAKE_BUILD_TYPE=Release"],
        check=True,
    )
    subprocess.run(
        ["cmake", "--build", str(build_dir), "--config", "Release",
         "--target", "dolrecomp"],
        check=True,
    )
    exe = find_exe()
    if exe is None:
        sys.exit(f"dolrecomp n'a pas produit d'exécutable sous {build_dir}")
    return exe


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dol", required=True, type=Path,
                         help="main.dol extrait (dtk disc extract)")
    parser.add_argument("--map", required=True, type=Path,
                         help="fichier .MAP correspondant (ex. marioEU.MAP)")
    parser.add_argument("--out", default=ROOT / "generated", type=Path,
                         help="dossier de sortie pour le C genere (defaut: generated/)")
    parser.add_argument("--jobs", type=int, default=None,
                         help="nombre de fichiers C paralleles (defaut: dolrecomp choisit)")
    parser.add_argument("--chunk-instructions", type=int, default=None,
                         metavar="N",
                         help="instructions par fichier .c genere (128..4096, "
                              "defaut 4096 = 219 fichiers ; 128 = 6978 fichiers)")
    args = parser.parse_args()

    if args.chunk_instructions is not None and not 128 <= args.chunk_instructions <= 4096:
        sys.exit(f"--chunk-instructions doit etre entre 128 et 4096 "
                 f"(recu {args.chunk_instructions})")

    if not args.dol.is_file():
        sys.exit(f"DOL introuvable : {args.dol}")
    if not args.map.is_file():
        sys.exit(f"MAP introuvable : {args.map}")

    dolrecomp = build_dolrecomp()

    # dolrecomp's own output-directory convention (see its --help) nests
    # everything one level deeper than what it's given - "GameCube/Wii U:
    # writes output-dir/generated/generated.c", plus output-dir/generated/
    # chunks/*.c for the per-function split. Each chunk includes its sibling
    # generated.h via a relative "../generated.h", so that chunks/ directory
    # structure is load-bearing and must be preserved as-is (verified: a
    # first attempt that flattened chunks/*.c alongside generated.h broke
    # every chunk's include). Run dolrecomp into a staging directory and
    # move its "generated" output directory to become --out directly, rather
    # than reshuffling files inside it.
    staging = args.out.parent / f"{args.out.name}.dolrecomp-staging"
    if staging.exists():
        shutil.rmtree(staging)

    command = [str(dolrecomp), "--gamecube", "--cpu", "gekko", "--backend", "c"]
    if args.jobs:
        command.append(f"-j{args.jobs}")
    command += ["--map", str(args.map), str(args.dol), str(staging)]

    # Le nombre de fichiers .c produits ne se regle PAS par
    # `--partition-instructions` : ce drapeau n'est lu que dans le chemin LLVM
    # (`build_llvm_ranges`, src/app/pipeline.c) et le port utilise le backend C.
    # Verifie : 64, 256 et le defaut donnent tous 219 fichiers, et `diff -r`
    # ne signale aucune difference entre les sorties. Le backend C lit la
    # variable d'environnement DOLRECOMP_C_CHUNK_INSTRUCTIONS
    # (`c_chunk_instructions`, src/app/pipeline.c), plage 128..4096.
    env = os.environ.copy()
    if args.chunk_instructions is not None:
        env["DOLRECOMP_C_CHUNK_INSTRUCTIONS"] = str(args.chunk_instructions)

    print("Lancement :", " ".join(command))
    if args.chunk_instructions is not None:
        print(f"           DOLRECOMP_C_CHUNK_INSTRUCTIONS={args.chunk_instructions}")
    subprocess.run(command, check=True, env=env)

    produced = staging / "generated"
    if not produced.is_dir():
        sys.exit(f"dolrecomp n'a pas produit {produced}")

    if args.out.exists():
        shutil.rmtree(args.out)
    shutil.move(str(produced), str(args.out))
    shutil.rmtree(staging)

    c_count = len(list(args.out.glob("chunks/*.c")))
    print(f"\n{c_count} fichiers .c sous {args.out}/chunks. Cette sortie n'est pas "
          "encore reliee au build dolphinjet (CMakeLists.txt) : voir "
          "docs/recompilation.md pour la suite (le pont vers Aurora via "
          "src/port/recomp_host.cpp).")


if __name__ == "__main__":
    main()
