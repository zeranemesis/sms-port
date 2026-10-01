#!/usr/bin/env python3
"""Extrait les sections texte d'un DOL en binaire brut, pour objdump.

Un `.dol` n'est pas un ELF : objdump refuse de le desassembler tel quel, alors
qu'il n'y a rien de difficile. L'en-tete DOL (0x100 octets, big-endian) donne
pour chaque section un offset de fichier, une adresse virtuelle et une taille ;
on peut donc sortir la section telle quelle et la rebaser.

Deux pieges, tous deux reels sur `main.dol` de Super Mario Sunshine :

1. **Le magic est a 0x100, pas a 0.** Les trois tables (offsets, adresses,
   tailles) occupent les 0x100 premiers octets ; c'est ensuite que vient le
   magic `msa\\0`. Lire un magic en 0 tombe sur la taille de la premiere section.

   Disposition des tables (`extern/dolrecomp/src/frontend/container/dol.c`) :

       0x00  7 offsets de fichier  (texte)
       0x1C  7 offsets de fichier  (donnees)
       0x48  7 adresses virtuelles (texte)
       0x64 11 adresses virtuelles (donnees)
       0x90  7 tailles             (texte)
       0xAC 11 tailles             (donnees)
       0xD8  adresse / taille du BSS
       0xE0  point d'entree

   Noter 0x48 pour les adresses, pas 0x1C : a 0x1C on y trouve les offsets de
   donnees, qui ressemblent a des adresses.

2. **Les sections texte ne sont pas contigues en adresse virtuelle.** Sur
   `main.dol`, la section 0 finit a `0x80005540` et la section 1 commence a
   `0x80005600` : 0xC0 de bourre, alors que leurs offsets de fichier sont
   jointifs. Concatener les sections en un seul blob et rebaser sur l'adresse
   de la premiere decale donc de 0xC0 le decoding de tout ce qui suit. D'ou
   l'usage d'un fichier par section, avec son propre `--adjust-vma`.

Lecture seule, sans build :

    python tools/port/dol_text.py orig/GMSP01/sys/main.dol all .agent-tmp/text.bin

Le script affiche la commande objdump exacte a lancer ensuite.
"""
import struct
import sys
from pathlib import Path

HEADER_SIZE = 0x100
MAGIC = 0x6D736100  # 'msa\\0'

OFF_TEXT, OFF_DATA, OFF_TEXTA, OFF_DATAA = 0x00, 0x1C, 0x48, 0x64
SZ_TEXT, SZ_DATA, OFF_ENTRY = 0x90, 0xAC, 0xE0


def u32(data, at):
    return struct.unpack_from(">I", data, at)[0]


def text_sections(data):
    """[(index, offset, adresse, taille)] pour les sections texte non vides."""
    out = []
    for i in range(7):
        size = u32(data, SZ_TEXT + i * 4)
        if size:
            out.append((i, u32(data, OFF_TEXT + i * 4),
                        u32(data, OFF_TEXTA + i * 4), size))
    return out


def data_sections(data):
    """[(index, offset, adresse, taille)] pour les sections donnees non vides."""
    out = []
    for i in range(11):
        size = u32(data, SZ_DATA + i * 4)
        if size:
            out.append((i, u32(data, OFF_DATA + i * 4),
                        u32(data, OFF_DATAA + i * 4), size))
    return out


def main(argv):
    if len(argv) < 4:
        sys.exit(__doc__)
    src, which, dst = Path(argv[1]), argv[2], Path(argv[3])
    data = src.read_bytes()
    if len(data) < HEADER_SIZE:
        sys.exit(f"{src} trop court pour un en-tete DOL")

    sections = text_sections(data)
    if not sections:
        sys.exit(f"{src} : aucune section texte")

    if which == "all":
        chosen = sections
    else:
        try:
            index = int(which)
        except ValueError:
            sys.exit(f"section inconnue : {which!r} (attendu : un index ou 'all')")
        chosen = [s for s in sections if s[0] == index]
        if not chosen:
            sys.exit(f"section texte {index} vide")

    dst.parent.mkdir(parents=True, exist_ok=True)
    for index, offset, address, size in chosen:
        blob = data[offset:offset + size]
        if len(blob) != size:
            sys.exit(f"section texte {index} tronquee dans le fichier")
        if len(chosen) == 1:
            out = dst
        else:
            # Un fichier par section : les adresses virtuelles ne sont pas
            # contigues, un blob unique decalerait le decoding (voir module).
            out = dst.with_name(f"{dst.stem}.{index}{dst.suffix}")
        out.write_bytes(blob)
        print(f"texte[{index}] fichier {offset:#x}..{offset + size:#x} "
              f"-> vaddr {address:#010x}..{address + size:#010x} "
              f"({size} octets) -> {out}")
        print(f"  powerpc-eabi-objdump -b binary -m powerpc:common -EB "
              f"--adjust-vma={address:#010x} -D {out}")

    if len(sections) > 1:
        print("\nRappel : une section par fichier, les adresses virtuelles "
              "texte ne sont pas contigues.")
    print(f"\npoint d'entree du DOL : {u32(data, OFF_ENTRY):#010x}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))