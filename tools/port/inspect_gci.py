#!/usr/bin/env python3
"""Dit si un fichier de sauvegarde .gci contient réellement des données.

Un .gci est un DEntry de 64 octets suivi de `length` blocs de 8192. Le piège
qui a motivé ce script : un fichier peut avoir un en-tête parfaitement valide -
bon game code, bon nom, taille cohérente avec le nombre de blocs annoncé - et
n'être rempli que de zéros, ce que le jeu relit comme une sauvegarde corrompue.
Regarder la taille ne suffit donc pas ; il faut regarder le contenu.

    python tools/port/inspect_gci.py "<chemin du .gci>"
"""
import sys


def main(path: str) -> int:
    with open(path, "rb") as handle:
        blob = handle.read()

    if len(blob) < 64:
        print(f"{path}: {len(blob)} octets - trop court pour un DEntry")
        return 1

    header, data = blob[:64], blob[64:]
    game = header[0x00:0x04].decode("ascii", "replace")
    maker = header[0x04:0x06].decode("ascii", "replace")
    name = header[0x08:0x28].split(b"\0")[0].decode("ascii", "replace")
    blocks = int.from_bytes(header[0x38:0x3A], "big")
    expected = 64 + blocks * 8192

    print(f"{path}")
    print(f"  jeu={game}{maker} nom=\"{name}\" blocs={blocks}")
    print(f"  taille={len(blob)} attendue={expected}"
          f"{'' if len(blob) == expected else '  <-- INCOHERENT'}")

    nonzero = sum(1 for byte in data if byte)
    pct = 100.0 * nonzero / len(data) if data else 0.0
    print(f"  donnees: {len(data)} octets, {nonzero} non nuls ({pct:.2f}%)")
    for index in range(0, len(data), 8192):
        block = data[index:index + 8192]
        print(f"    bloc {index // 8192}: {sum(1 for byte in block if byte)} non nuls")

    if nonzero == 0:
        print("  VERDICT: en-tete valide mais AUCUNE donnee - le jeu lira une sauvegarde corrompue")
        return 1
    print("  VERDICT: contient des donnees")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        raise SystemExit(2)
    raise SystemExit(main(sys.argv[1]))
