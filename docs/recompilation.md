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


## Backend LLVM et runtime ModernGekko

Le runtime ModernGekko exige le backend LLVM, lequel exige les **fichiers de
développement** LLVM 19 ou 20. Trois obstacles se présentent dans cet ordre, tous
franchis :

1. **L'installateur officiel ne suffit pas.** `LLVM-*-win64.exe` (et ce que
   fournit winget) ne livre que les binaires — ni headers ni fichiers CMake. Il
   faut l'archive complète `clang+llvm-20.1.8-x86_64-pc-windows-msvc.tar.xz`
   (897 Mo).

2. **L'archive référence un chemin qui n'existe pas chez soi.** Son
   `lib/cmake/llvm/LLVMExports.cmake` code en dur, ligne 490, le DIA SDK de
   Visual Studio 2019 Professional :
   `C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/DIA SDK/lib/amd64/diaguids.lib`.
   CMake échoue tant qu'on ne l'a pas remplacé par le chemin local du DIA SDK.

3. **Reconfigurer avec le backend activé** :

```bash
cmake -S . -B build-llvm -DDOLRECOMP_ENABLE_LLVM=ON \
      -DLLVM_DIR=<...>/lib/cmake/llvm
```

**Résultat mesuré** avec ce backend :

```bash
dolrecomp.exe --gamecube --cpu gekko --backend llvm --runtime moderngekko \
              --game-id GMSP01 --map <...>/marioEU.MAP -j12 <...>/sys/main.dol <sortie>
```

16 958 chunks objets LLVM, **50 879 fichiers, 4,0 Go**. Même avertissement de code
auto-modifiant qu'avec le backend C.

## Point ouvert : code auto-modifiant

L'outil avertit que le DOL modifie de la mémoire exécutable à l'exécution et liste
les instructions concernées dans `generated_smc.txt`. Ces sites demanderont des
correctifs ciblés. L'avertissement apparaît avec les deux backends.

## Ce que « traduit » ne veut pas dire — et ce que fournit ModernGekko

Le code est traduit, le jeu n'est pas jouable pour autant : restent le runtime
(mémoire, threads, interruptions) et les couches plateforme.

**Fait à connaître avant de s'engager sur cette route.** ModernGekko se décrit
lui-même comme « built on a Dolphin-derived core for video/audio/HLE », et embarque
Dolphin en dépendance (`lib/ModernGekko/vendor/dolphin`, avec ses Externals
FFmpeg/SDL sur Windows). Deux conséquences :

- **L'audio cesse d'être le poste incertain.** Le reste de ce dépôt dit que
  « l'audio n'a aucune base réutilisable » — c'est vrai pour un port natif écrit
  depuis la décomp, faux pour cette route : Dolphin l'apporte.
- **Ce n'est pas un port natif.** Ce qu'on obtient est un exécutable dédié au jeu
  où le **CPU est recompilé statiquement** et où le reste de la machine vient des
  sous-systèmes de Dolphin. Le gain face à « lancer Dolphin » se limite aux
  performances CPU et à un binaire autonome — pas à une réécriture au niveau
  source. La décision d'emprunter cette voie doit être prise en connaissance de
  cela.

Aurora (`encounter/aurora`) reste l'alternative sur ce point : une vraie couche de
compatibilité source GX → Vulkan/Metal/D3D12, sans cœur d'émulateur, au prix d'un
travail bien plus long et sans réponse pour l'audio.

## Monter la chaîne sur Windows

Quatre points coûtent une tentative chacun si on ne les connaît pas.

**Pas besoin de convertir le RVZ en ISO.** Le `Makefile` du gabarit
`ModernGekko-Template` pilote tout par `ISO=`, mais sa cible d'extraction est un
**fichier réel** (`extracted/<slug>/sys/main.dol`), pas une règle *phony*. Il
suffit donc de pré-remplir `extracted/<slug>/` avec l'extraction faite par `dtk`
— qui lit le RVZ nativement — puis de passer `GAME=<slug>` au lieu de `ISO=` :
l'extraction est sautée. Pour GMSP01 : 1,2 Go, `sys/` complet plus
`files/marioEU.MAP`.

**`make` n'est pas nécessaire.** Git Bash n'en fournit pas, et le `Makefile` ne
fait qu'envelopper CMake. Reproduire ses deux cibles suffit :

```bash
cmake -S lib/DolRecomp   -B lib/DolRecomp/build   -G Ninja -DCMAKE_BUILD_TYPE=Release       -DBUILD_TESTING=OFF -DDOLRECOMP_ENABLE_LLVM=ON -DLLVM_DIR="<...>/lib/cmake/llvm"
cmake --build lib/DolRecomp/build --target dolrecomp -j12
# idem avec lib/ModernGekko, cible moderngekko-port
```

**Le générateur Ninja impose l'environnement MSVC.** Contrairement au générateur
Visual Studio, Ninja exige `cl.exe` dans le `PATH`. Piège connu : lancer
`vcvars64.bat` via `cmd /c` **depuis Bash se bloque**. Il faut importer
l'environnement depuis PowerShell, dans le **même** processus que le build :

```powershell
cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
  if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2] }
}
```

**Le clonage récursif doit être terminé avant de configurer.** ModernGekko
embarque Dolphin, qui a 35 sous-modules `Externals`, lesquels en ont à leur tour.
Configurer pendant que `git submodule update --init --recursive` tourne encore
échoue sur des messages trompeurs — `Externals/libspng/libspng` sans
`CMakeLists.txt`, `cubeb` réclamant `sanitizers-cmake`. Ce ne sont pas des
dépendances manquantes : ce sont des répertoires pas encore remplis. Attendre la
fin du clonage (`Externals` dépasse 600 Mo) avant de lancer CMake.

**État vérifié :** `dolrecomp` se configure et se compile avec
`DOLRECOMP_ENABLE_LLVM=ON` sous Ninja + MSVC 19.51. `pkg-config` est absent de la
machine et n'a jusqu'ici été réclamé ni par le `Makefile` ni par les
`CMakeLists.txt`. La compilation de `moderngekko-port` reste à mener à son terme.

## Ce qui n'est pas versionné ici

Le code produit par la recompilation dérive du disque et n'est pas commité :
seule la recette l'est. Chacun le régénère depuis sa propre copie du jeu.
