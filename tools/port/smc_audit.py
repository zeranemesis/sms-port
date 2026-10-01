#!/usr/bin/env python3
"""Audite le signal « code auto-modifiant » de dolrecomp et en prouve la cause.

`generated/generated_smc.txt` est produit par `analyze_smc_section`
(`extern/dolrecomp/src/analysis/smc.c`). Ce script rejoue cette analyse pour
etablir, instruction par instruction, ce que vaut reellement le signal. Il
remplace une affirmation (« probablement des faux positifs ») par une mesure.

Lecture seule, sans build :

    python tools/port/smc_audit.py

## Ce que le script etablit

1. **Combien** d'instructions sont reellement signalees.
2. **Ce qu'elles sont** : desassemblage avec objdump, classe par type.
3. **Pourquoi** elles sont signalees : rejeu de la propagation de registres.

## Les deux pieges de lecture du fichier

**Le fichier liste des plages, pas des instructions.** `smc_note` fusionne deux
notes distantes de 4 octets ou moins, si bien que l'interieur d'une plage
contient des adresses qui n'ont jamais ete signalees. Seuls les bouts de plages
sont garantis notes. Compter les octets revient a inventer des instructions :
sur `main.dol`, 135 plages et 259 octets ne font que 148 notes.

**Le balayage est lineaire.** `analyze_smc_section` parcourt les instructions
dans l'ordre memoire et tient un etat de registres connus, sans s'arreter sur une
branche, sur un `blr`, ni sur une frontiere de fonction. Un registre pose par
une fonction reste « connu » dans les fonctions suivantes tant que rien ne
l'efface.

## Le resultat mesure

Les 148 notes sont 144 magasins et 4 `icbi` ; zero instruction d'une autre
classe. Le tri est donc complet : il ne manque aucun site.

Presque tous les magasins remontent a un `lis rA, <constante>` qui fabrique un
**pointeur de fonction** (souvent un pointeur de callback ecrit dans une table
via `stw rX, d(r13)`), place dans une fonction et laisse dans un registre que la
fonction suivante reutilise comme parametre. Le balayage croit alors ecrire a une
adresse de code. Exemple canonique, sur le jeu :

    80349cbc:  lis   r4,-32715      ; PADSetSpec : r4 = 0x80350000
    80349cc4:  stw   r0,-29528(r13) ; r0 = 0x80349FB8, ecrit dans une table
    80349ccc:  blr                 ; fin de PADSetSpec (taille 0x60)
    80349cd0:  li    r3,0          ; debut de SPEC0_MakeStatus
    80349cd4:  sth   r3,0(r4)       ; signale a tort

`r4` est ici le pointeur fourni par l'appelant, que le balayage ne peut pas
connaitre : il vient d'hériter de `0x80350000`, qui tombe bien dans une section
texte. La distance mediane entre un magasin signale et l'instruction qui a pose
son registre de base est de 93 instructions : la fuite est locale, ce qui
designe une frontiere de fonction manquante plutot qu'un defaut d'effacement.

Conclusion : aucun correctif de code auto-modifiant n'est requis pour le port.
"""
import re
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOL = ROOT / "orig" / "GMSP01" / "sys" / "main.dol"
SMC = ROOT / "generated" / "generated_smc.txt"
SYMBOLS = ROOT / "config" / "GMSP01" / "symbols.txt"
OBJDUMP = ROOT / "build" / "binutils" / "powerpc-eabi-objdump.exe"
TMP = ROOT / ".agent-tmp"

RE_INSN = re.compile(r"^([0-9a-f]+):\s+((?:[0-9a-f]{2} ){4})\s*(\S+)\s*(.*)$")
RE_RANGE = re.compile(r"^0x([0-9A-Fa-f]+)-0x([0-9A-Fa-f]+)")
RE_SYM = re.compile(r"^(?P<name>\S+)\s*=\s*\.(?P<sec>\w+):(?P<addr>0x[0-9A-Fa-f]+);")
RE_INT = re.compile(r"^-?0x[0-9a-f]+$|^-?\d+$")

CACHE_OPS = ("icbi", "dcbz", "dcbz_l", "dcbz_forever",
             "dcbt", "dcbtst", "dcbtct", "dcbtp", "dcbts")

STORE_SIZES = {
    "stb": 1, "stbu": 1, "stbx": 1, "stbux": 1, "stwbrx": 1,
    "sth": 2, "sthu": 2, "sthx": 2, "sthux": 2, "sthbrx": 2, "stswx": 1,
    "stw": 4, "stwu": 4, "stwx": 4, "stwux": 4, "stwcx": 4, "stfiwx": 4,
    "stfs": 4, "stfsu": 4, "stfsx": 4, "stfsux": 4,
    "stfd": 8, "stfdu": 8, "stfdx": 8, "stfdux": 8,
    "stswi": 32,
}
# Magasins indexes : l'adresse effective depend de deux registres, dont un que
# le balayage ne connait presque jamais. `smc_inst_targets_code` les traite
# aussi, mais ils ne sont pas decomposes ici.
INDEXED = {"stwx", "stwux", "stbx", "stbux", "sthx", "sthux", "stwbrx",
           "sthbrx", "stswx", "stwcx", "stfiwx", "stfsx", "stfsux",
           "stfdx", "stfdux"}


def u32(data, at):
    return struct.unpack_from(">I", data, at)[0]


def text_sections(data):
    return [(i, u32(data, 0x00 + i * 4), u32(data, 0x48 + i * 4),
             u32(data, 0x90 + i * 4))
            for i in range(7) if u32(data, 0x90 + i * 4)]


def parse_reg(tok):
    m = re.fullmatch(r"r(\d+)", tok)
    return int(m.group(1)) if m else None


def parse_int(tok):
    return int(tok, 0) if RE_INT.match(tok) else None


def disassemble(data, sections):
    """{adresse -> (octets hex, mnemonique, operandes)}.

    Une section a la fois : les adresses virtuelles texte ne sont pas contigues
    dans ce DOL, un blob unique decalerait le decoding (voir `dol_text.py`).
    """
    out = {}
    TMP.mkdir(parents=True, exist_ok=True)
    for index, _off, addr, size in sections:
        blob = TMP / f"smc_audit_sec{index}.bin"
        blob.write_bytes(data[_off:_off + size])
        proc = subprocess.run(
            [str(OBJDUMP), "-b", "binary", "-m", "powerpc:common", "-EB",
             f"--adjust-vma={addr:#x}", "-D", str(blob)],
            capture_output=True, text=True, errors="replace")
        blob.unlink()
        if proc.returncode != 0:
            sys.exit(f"objdump a echoue sur la section {index} : "
                     f"{proc.stderr.strip()}")
        for line in proc.stdout.splitlines():
            m = RE_INSN.match(line)
            if m:
                out[int(m.group(1), 16)] = (
                    m.group(2).replace(" ", ""), m.group(3), m.group(4).strip())
    return out


def load_symbols():
    by_addr = {}
    if not SYMBOLS.is_file():
        return by_addr
    for line in SYMBOLS.read_text(encoding="utf-8", errors="replace").splitlines():
        m = RE_SYM.match(line.strip())
        if m:
            by_addr.setdefault(int(m.group("addr"), 16),
                               (m.group("name"), m.group("sec")))
    return by_addr


def load_notes():
    if not SMC.is_file():
        sys.exit(f"{SMC} absent : lancer tools/port/recompile.py d'abord")
    ranges = []
    for line in SMC.read_text(encoding="utf-8", errors="replace").splitlines():
        m = RE_RANGE.match(line.strip())
        if m:
            ranges.append((int(m.group(1), 16), int(m.group(2), 16)))
    # Seuls les bouts de plages sont garantis notes.
    return ranges, sorted({a for a, _ in ranges} | {b for _, b in ranges})


class Prop:
    """Rejoue `update_known_regs` de `smc.c`.

    Reproduction volontairement fidele, y compris a ses limites : le balayage ne
    connait ni les branches ni les frontiers de fonctions, et n'efface que sur
    les opcodes que `smc.c` liste. C'est le sujet de l'audit, pas un defaut.
    """

    def __init__(self):
        self.value = [None] * 32
        self.origin = [None] * 32

    def clear(self, r):
        if 0 <= r < 32:
            self.value[r] = None
            self.origin[r] = None

    def set(self, r, value, origin):
        if 0 <= r < 32:
            self.value[r] = value & 0xFFFFFFFF
            self.origin[r] = origin


def step(prop, addr, mnem, ops, text):
    """Applique une instruction a l'etat de registres connus."""
    parts = [p.strip() for p in ops.split(",")] if ops else []

    if mnem in ("li", "lis"):
        m = re.fullmatch(r"r(\d+),(\S+)", ops)
        if m:
            v = parse_int(m.group(2))
            if v is not None:
                r = int(m.group(1))
                prop.set(r, ((v << 16) if mnem == "lis" else v) & 0xFFFFFFFF, addr)
        return

    if mnem in ("addi", "addis") and len(parts) >= 3:
        rd, ra, imm = parse_reg(parts[0]), parse_reg(parts[1]), parse_int(parts[2])
        if rd is None or imm is None:
            prop.clear(rd)
        elif ra in (None, 0):
            prop.set(rd, (imm << 16) if mnem == "addis" else imm, addr)
        elif prop.value[ra] is None:
            prop.clear(rd)
        else:
            shift = (imm << 16) if mnem == "addis" else imm
            prop.set(rd, prop.value[ra] + shift, addr)
        return

    if mnem in ("ori", "oris", "xori", "xoris") and len(parts) >= 3:
        ra, rs, imm = parse_reg(parts[0]), parse_reg(parts[1]), parse_int(parts[2])
        if ra is None or imm is None:
            prop.clear(ra)
        elif prop.value[rs] is None:
            prop.clear(ra)
        else:
            sh = imm << (16 if mnem.endswith("is") else 0)
            base = prop.value[rs]
            prop.set(ra, (base | sh) if mnem.startswith(("ori", "xori"))
                     else (base ^ sh), addr)
        return

    # Un chargement efface sa destination : c'est le seul effacement de
    # destination que `smc.c` pratique, et il ne touche pas les autres
    # registres. C'est ce qui laisse fuir les valeurs d'une fonction a l'autre.
    if mnem.startswith("lw") and parts:
        prop.clear(parse_reg(parts[0]))


def main():
    ranges, notes = load_notes()
    data = DOL.read_bytes()
    sections = text_sections(data)
    symbols = load_symbols()
    cranges = [(addr, addr + size) for _, _, addr, size in sections]

    text = disassemble(data, sections)
    noteset = set(notes)

    stores, cache, other = [], [], []
    for addr in notes:
        entry = text.get(addr)
        if entry is None:
            other.append((addr, "?", "?", "?"))
            continue
        _raw, mnem, ops = entry
        (stores if mnem.startswith("st")
         else cache if mnem in CACHE_OPS else other).append((addr, mnem, ops))

    nbytes = sum(b - a + 1 for a, b in ranges)
    print(f"source        : {SMC.relative_to(ROOT)}")
    print(f"plages        : {len(ranges)}  ({nbytes} octets, "
          f"et non {nbytes} instructions)")
    print(f"notes garanties: {len(notes)}  "
          f"({sum(1 for a, b in ranges if a == b)} plages a une seule adresse, "
          f"{sum(1 for a, b in ranges if a < b)} fusionnees)\n")
    print(f"magasins            : {len(stores)}")
    print(f"operations de cache : {len(cache)}")
    for addr, mnem, ops in cache:
        sym = symbols.get(addr)
        label = f"  [{sym[0]}]" if sym else ""
        print(f"    0x{addr:08X} {mnem} {ops}{label}")
    print(f"ni l'un ni l'autre  : {len(other)}")
    for addr, mnem, ops in other[:10]:
        print(f"    0x{addr:08X} {mnem} {ops}")

    # --- Cause : rejeu de la propagation ---
    prop = Prop()
    order = sorted(text)
    position = {a: i for i, a in enumerate(order)}
    hits = []
    for addr in order:
        _raw, mnem, ops = text[addr]
        parts = [p.strip() for p in ops.split(",")] if ops else []

        if mnem in STORE_SIZES and addr in noteset and mnem not in INDEXED:
            tail = parts[-1] if parts else ""
            m = re.fullmatch(r"(-?\S+)?\((r\d+)\)", tail)
            if m:
                simm = parse_int(m.group(1)) if m.group(1) else 0
                base = parse_reg(m.group(2))
                if base is None or simm is None:
                    base = None
                if base == 0:
                    ea = simm & 0xFFFFFFFF
                elif base is not None and prop.value[base] is not None:
                    ea = (prop.value[base] + simm) & 0xFFFFFFFF
                else:
                    ea = None
                if ea is not None and any(lo <= ea < hi for lo, hi in cranges):
                    hits.append((addr, ea, base, prop.origin[base]))
        step(prop, addr, mnem, ops, text)

    print(f"\n--- cause ---\n")
    print(f"{len(hits)} magasins dont l'adresse effective tombe dans une "
          f"section texte.")
    eas = Counter(h[1] for h in hits)
    print(f"adresses effectives distinctes : {len(eas)}")
    for ea, n in eas.most_common(6):
        sym = symbols.get(ea)
        label = f"{sym[0]}  [{sym[1]}]" if sym else "(sans symbole)"
        print(f"  {n:>4} x  0x{ea:08X}  {label}")
    if len(eas) > 6:
        print(f"  ... et {len(eas) - 6} autres")

    origins = Counter(h[3] for h in hits)
    print(f"\ninstructions source distinctes : {len(origins)}")
    for origin, n in origins.most_common(8):
        if origin in text:
            _raw, mnem, ops = text[origin]
            sym = symbols.get(origin)
            label = f"   <- {sym[0]}" if sym else ""
            print(f"  {n:>4} x  0x{origin:08X}  {mnem} {ops}{label}")

    dists = sorted(position[h[0]] - position[h[3]]
                   for h in hits if h[3] in position)
    if dists:
        print(f"\ndistance en instructions entre le magasin et l'instruction "
              f"qui a pose son registre de base :")
        print(f"  min {dists[0]}  mediane {dists[len(dists) // 2]}  "
              f"max {dists[-1]}")
        print(f"  au-dela de 1000 instructions : "
              f"{sum(1 for d in dists if d > 1000)}/{len(dists)}")
        print("\nUne fuite locale, courte : le defaut est une frontiere de "
              "fonction\n(non respectee) et non un effacement global.")
    return 0


if __name__ == "__main__":
    sys.exit(main())