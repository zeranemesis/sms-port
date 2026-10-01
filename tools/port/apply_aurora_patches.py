#!/usr/bin/env python3
"""Applique les patchs Aurora locaux dans extern/aurora.

`CMakeLists.txt` dit que les sous-modules sont des commits epingles sur lesquels
sont posees des modifications locales non commitees, et que « CI doit les
appliquer a l'extraction ». Cette etape n'etait fait nulle part dans
l'arbre, et elle est obligatoire : du code **commite** du port en depend.

Ce n'est pas cosmetique. `src/port/recomp_gx_fifo.cpp` utilise
`aurora::gx::fifo::g_guestMemoryResolver` et `aurora::gx::fifo::process_stream`,
et les deux viennent de `aurora-fifo-stream.patch`. Sans lui, meme le port
d'avant ne compile pas - ce qui se lit « ce port n'a jamais ete compile » plutot
que « une etape de build a ete sautee ».

## Pourquoi une seule fois que le plus court

`aurora-render-fixes.patch` ne s'applique pas en entier : son unique hunk sur
`lib/gx/gx.cpp` touche une zone que `aurora-gx-diagnostics.patch` a deja
modifiee. Le sauter casse le build autrement, et c'est cet echec la qui
interesse :

    lib/gx/gx.cpp(476): error C2039: 'tlutObjId' is not a member of
                                     'aurora::gfx::TextureBind'

Les deux fichiers sont **deux moitiés du meme changement** :
`aurora-gx-diagnostics.patch` porte le cote consommateur (la logique de
`resolve_sampled_textures` qui lit `tlutObjId` / `tlutDataVersion` /
`copyRevision` sur un `TextureBind`), `aurora-render-fixes.patch` porte le
cote producteur (les trois champs, dans `lib/gfx/texture.hpp`). L'un sans
l'autre ne lie pas.

Pendant ce temps, le hunk `gx.cpp` de `render-fixes` est **deja en amont**
dans Aurora 5143394 - le `copyTextures.find(obj.data)` y est deja present -
donc il est redondant, et c'est la seule raison de l'echec.

D'ou la decision : appliquer le patch moins ce fichier. Correct et attendu,
mais un `for p in patches/aurora-*.patch; do git apply "$p"; done` echoue
toujours sur le dernier, ce qui est un piege pour la personne suivante et pour
la CI. Le vrai correctif est de regenerer `aurora-render-fixes.patch` sans le
hunk `gx.cpp` ; la decision de le faire appartient a celle qui l'a ecrit, pas a
ce script. Ce script rend l'etat actuel reproductible et dit pourquoi.

## Ce que le script ne fait pas

Il ne valide rien et ne touche pas a `generated/`. Il ne committe rien. Il
laisse `extern/aurora` sale : c'est l'etat attendu, et c'est ce que
`git status` doit montrer pour que personne ne le committe par megarde.

Usage :

    python tools/port/apply_aurora_patches.py            # applique
    python tools/port/apply_aurora_patches.py --check    # dry-run
"""
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PATCHES = sorted((ROOT / "patches").glob("aurora-*.patch"))

# Fichiers de render-fixes a ne pas tenter : son hunk gx.cpp est deja en amont.
RENDER_FIXES = "aurora-render-fixes.patch"
RENDER_FIXES_SKIP = "lib/gx/gx.cpp"
RENDER_FIXES_KEEP = ("lib/gfx/texture.hpp", "lib/gfx/tex_copy_conv.cpp")


def run(args, cwd, check=True):
    return subprocess.run(args, cwd=cwd, check=check,
                          capture_output=True, text=True)


def state(patch, aurora, includes):
    """`applied`, `applicable` ou `conflict` pour un patch donne.

    Reverse est la seule maniere fiable de distinguer « deja applique » de « ne
    s'applique plus » : les deux font echouer `git apply --check`, et le traiter
    comme la meme chose fait rapporter les huit patchs comme a appliquer sur un
    sous-module deja complet. `--reverse --check` ne reussit que si le patch est
    actuellement present.
    """
    fwd = ["git", "apply", "--check"]
    rev = ["git", "apply", "--reverse", "--check"]
    if includes:
        fwd += [f"--include={i}" for i in includes]
        rev += [f"--include={i}" for i in includes]
    fwd.append(str(patch))
    rev.append(str(patch))
    if run(rev, aurora, check=False).returncode == 0:
        return "applied"
    if run(fwd, aurora, check=False).returncode == 0:
        return "applicable"
    return "conflict"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="n'applique rien, dit seulement ce qui manque")
    args = ap.parse_args()

    aurora = ROOT / "extern" / "aurora"
    if not (aurora / "CMakeLists.txt").is_file():
        sys.exit(f"{aurora} ne ressemble pas a un sous-module Aurora")

    steps = []
    for patch in PATCHES:
        if patch.name == RENDER_FIXES:
            steps.append((patch, list(RENDER_FIXES_KEEP)))
        else:
            steps.append((patch, None))

    todo = []
    for patch, includes in steps:
        st = state(patch, aurora, includes)
        if st == "conflict":
            print(f"CONFLIT : {patch.name} ne s'applique ni dans un sens ni dans "
                  f"l'autre. L'arbre du sous-module a peut-etre ete edite a la main.")
            return 2
        if st == "applicable":
            todo.append((patch, includes))

    if not todo:
        print("les patchs Aurora sont deja appliques")
        return 0

    print("patchs a appliquer :", ", ".join(p.name for p, _ in todo))
    for patch, includes in todo:
        if includes:
            print(f"  {patch.name}: seulement {' + '.join(includes)} "
                  f"(le hunk {RENDER_FIXES_SKIP} est deja en amont)")

    if args.check:
        return 1

    for patch, includes in todo:
        cmd = ["git", "apply"]
        if includes:
            cmd += [f"--include={i}" for i in includes]
        cmd.append(str(patch))
        run(cmd, aurora)
        suffix = "" if includes is None else f" ({', '.join(includes)} seulement)"
        print(f"applique {patch.name}{suffix}")

    dirty = len(run(["git", "status", "--short"], aurora).stdout.splitlines())
    print(f"\nextern/aurora a maintenant {dirty} fichiers modifies. "
          f"C'est l'etat attendu : ne pas le committer.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
