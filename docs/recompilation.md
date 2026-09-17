# Recompilation statique — recette et résultats

La route retenue pour obtenir un jeu jouable est la **recompilation statique**,
pas la décompilation. Ce document consigne ce qui a été fait et mesuré, pour
pouvoir le refaire sans repartir de zéro.

Rappel du raisonnement : un port issu de la décomp suppose ~100 % de complétude
fonctionnelle (on est à 18,23 %, avec ~2489 fonctions à écrire). La
recompilation traduit mécaniquement le PowerPC d'origine en code natif et ne
demande aucune décompilation.

## Outils

| projet | rôle |
|---|---|
| [`ExpansionPak/DolRecomp`](https://github.com/ExpansionPak/DolRecomp) | recompilateur statique GameCube/Wii (CPU uniquement) |
| [`ExpansionPak/ModernGekko`](https://github.com/ExpansionPak/ModernGekko) | runtime pour les recompilations |
| [`encounter/aurora`](https://github.com/encounter/aurora) | couche GX → Vulkan/Metal/D3D12 |

Précédent direct : [`chrissotraidis/sunpad`](https://github.com/chrissotraidis/sunpad)
fait tourner ce jeu sur plateformes Apple par cette voie.

## Extraction du disque

`dtk` lit le RVZ nativement :

```bash
dtk disc extract <image.rvz> <dossier_sortie>
```

Fournit `sys/main.dol` (le binaire, 4 094 112 o) et `files/marioEU.MAP`
(7,9 Mo, la table des symboles). La map n'est pas optionnelle en pratique :
elle donne des **fonctions nommées** dans le code produit au lieu d'adresses.

## Construction de DolRecomp

CMake plus un compilateur C11 suffisent ; le générateur Visual Studio trouve
MSVC seul, sans passer par `vcvars`.

```bash
cmake -S . -B build
cmake --build build --config Release
```

## Recompilation

```bash
dolrecomp.exe --gamecube --cpu gekko --backend c --runtime recompcore \
              --map <...>/marioEU.MAP -j12 <...>/sys/main.dol <sortie>
```

**Résultat mesuré sur GMSP01 :**

- **890 680 instructions décodées**
- 890 608 reconnues, 72 données intégrées, **0 inconnue**
- 219 fichiers C produits dans `generated/chunks`, plus `generated_symbols.h`

Zéro instruction non décodée : le binaire entier est traduit.

## Deux points ouverts

**Code auto-modifiant.** L'outil avertit que le DOL modifie de la mémoire
exécutable à l'exécution et liste les instructions concernées dans
`generated_smc.txt`. Ces sites demanderont des correctifs ciblés.

**Runtime ModernGekko.** Il exige le backend LLVM, lequel exige les fichiers de
développement LLVM 19 ou 20. Attention : l'installateur Windows officiel
(`LLVM-*-win64.exe`, également ce que fournit winget) ne livre que les
binaires — ni headers ni fichiers CMake. Il faut l'archive complète
`clang+llvm-20.1.8-x86_64-pc-windows-msvc.tar.xz` (897 Mo), puis reconfigurer :

```bash
cmake -S . -B build-llvm -DDOLRECOMP_ENABLE_LLVM=ON \
      -DLLVM_DIR=<...>/lib/cmake/llvm
```

## Ce que « traduit » ne veut pas dire

Le code est traduit, le jeu n'est pas jouable pour autant. Restent le runtime
(mémoire, threads, interruptions) et surtout les couches plateforme. Aurora
couvre GX ; **l'audio n'a aucune base réutilisable** et demeure le poste le plus
incertain du projet.

## Ce qui n'est pas versionné ici

Le code produit par la recompilation dérive du disque et n'est pas commité :
seule la recette l'est. Chacun le régénère depuis sa propre copie du jeu.
