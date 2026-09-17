#!/usr/bin/env python3
"""Audit de portabilite pour un port PC de SMS.

Mesure, sur l'arbre de la decompilation :
  1. la completude fonctionnelle reelle (souches vides vs code ecrit)
  2. l'usage direct du SDK GameCube depuis le code de gameplay
  3. le routage MTX / PSMTX (paired-singles Gekko vs C portable)
  4. les constructions non portables pour gcc/clang/MSVC

Lecture seule. Usage : python port_audit.py [racine_du_depot]
"""
import os, re, sys, collections

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
GAME = ["Enemy","Player","Camera","Map","MoveBG","NPC","MSound","M3DUtil",
        "MarioUtil","GC2D","Animal","Strategic","System"]

def walk(rel, exts=(".cpp",".c",".hpp",".h")):
    base = os.path.join(ROOT, rel)
    for dp, _, fns in os.walk(base):
        for fn in fns:
            if fn.endswith(exts):
                yield os.path.join(dp, fn)

def read(p):
    with open(p, encoding="utf-8", errors="replace") as f:
        return f.read()

print("=" * 66)
print("1. COMPLETUDE FONCTIONNELLE  (le compteur qui compte pour un port)")
print("=" * 66)
stub_bytes = real_bytes = 0
stubs, total = [], 0
for d in GAME:
    for p in walk(os.path.join("src", d), (".cpp",)):
        total += 1
        n = os.path.getsize(p)
        if n < 200:
            stubs.append((n, os.path.relpath(p, ROOT))); stub_bytes += n
        else:
            real_bytes += n
print(f"  fichiers .cpp de gameplay : {total}")
print(f"  souches vides (<200 o)    : {len(stubs)}  ({100*len(stubs)/total:.1f} %)")
print(f"  code reellement ecrit     : {real_bytes/1024:.0f} Ko")
print("  exemples de souches       :")
for n, p in sorted(stubs)[:6]:
    print(f"      {n:>5} o  {p}")

print()
print("=" * 66)
print("2. USAGE DIRECT DU SDK DEPUIS LE CODE DE GAMEPLAY")
print("=" * 66)
fams = {
    "GX*   (graphismes)": r"\bGX[A-Z]\w*\s*\(",
    "DVD*  (disque)":     r"\bDVD[A-Z]\w*\s*\(",
    "PAD*  (manette)":    r"\bPAD[A-Z]\w*\s*\(",
    "VI*   (video)":      r"\bVI[A-Z]\w*\s*\(",
    "OS*   (systeme)":    r"\bOS[A-Z]\w*\s*\(",
    "CARD* (memoire)":    r"\bCARD[A-Z]\w*\s*\(",
    "AI/AX/DSP (audio)":  r"\b(?:AI|AX|DSP)[A-Z]\w*\s*\(",
}
for label, pat in fams.items():
    rx = re.compile(pat); sites = 0; files = set()
    for d in GAME:
        for p in walk(os.path.join("src", d), (".cpp",)):
            k = len(rx.findall(read(p)))
            if k:
                sites += k; files.add(p)
    print(f"  {label:<20} {sites:>5} appels  dans {len(files):>3} fichiers")

print()
print("=" * 66)
print("3. ROUTAGE MATRICIEL  (paired-singles Gekko vs C portable)")
print("=" * 66)
ps  = re.compile(r"\bPS(?:MTX|VEC)\w*\s*\(")
mtx = re.compile(r"\b(?:MTX|VEC)[A-Z]\w*\s*\(")
c_m = re.compile(r"\bC_(?:MTX|VEC)\w*\s*\(")
tot = collections.Counter(); offenders = collections.Counter()
for d in GAME:
    for p in walk(os.path.join("src", d), (".cpp",)):
        s = read(p)
        a, b, c = len(ps.findall(s)), len(mtx.findall(s)), len(c_m.findall(s))
        tot["PSMTX/PSVEC direct"] += a; tot["MTX/VEC (macro)"] += b; tot["C_MTX/C_VEC direct"] += c
        if a: offenders[os.path.relpath(p, ROOT)] += a
for k, v in tot.items():
    print(f"  {k:<22} {v:>5}")
if offenders:
    print("  fichiers appelant PSMTX/PSVEC en direct (a router via MTX*) :")
    for p, n in offenders.most_common(8):
        print(f"      {n:>4}x  {p}")
else:
    print("  -> aucun appel direct aux paired-singles : bascule triviale")

print()
print("=" * 66)
print("4. CONSTRUCTIONS NON PORTABLES  (gcc / clang / MSVC)")
print("=" * 66)
checks = {
    "#pragma (tous)":      re.compile(r"^\s*#pragma\s+(\w+)", re.M),
    "__declspec":          re.compile(r"__declspec"),
    "asm inline":          re.compile(r"\basm\s*[({]|__asm"),
    "octets non-ASCII":    re.compile(rb"[\x80-\xff]"),
}
prag = collections.Counter(); counts = collections.Counter(); nonascii = []
for rel in ["src", "include"]:
    for p in walk(rel):
        s = read(p)
        for m in checks["#pragma (tous)"].finditer(s): prag[m.group(1)] += 1
        for k in ("__declspec", "asm inline"):
            n = len(checks[k].findall(s))
            if n: counts[k] += n
        with open(p, "rb") as f:
            if checks["octets non-ASCII"].search(f.read()):
                nonascii.append(os.path.relpath(p, ROOT))
print(f"  #pragma        : {sum(prag.values())} occurrences, {len(prag)} directives distinctes")
for k, v in prag.most_common(6): print(f"      {v:>5}x  #pragma {k}")
for k in ("__declspec", "asm inline"):
    print(f"  {k:<15}: {counts[k]}")
print(f"  fichiers contenant des octets non-ASCII (Shift-JIS) : {len(nonascii)}")
for p in nonascii[:5]: print(f"      {p}")
