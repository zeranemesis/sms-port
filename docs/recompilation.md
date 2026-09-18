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

## Contrat d'exécution de DolRecomp — et pourquoi ModernGekko n'est pas nécessaire

Le blocage consigné plus haut (« le backend qui donnerait la performance est
celui qui ne peut pas être lié » — LLVM, requis par le runtime ModernGekko) est
spécifique à la route ModernGekko, pas à DolRecomp lui-même. Lu dans
`extern/dolrecomp/src/cpu/cpu.h`, le contrat du backend simple
(`--backend c`, sans LLVM) est indépendant de tout runtime externe :

- Chaque fonction recompilée devient `void func_<adresse>(CPUState* ctx)` où
  `CPUState` est un simple fichier de registres PowerPC (gpr/fpr/ps1/pc/lr/cr/
  msr/…) — vérifié en construisant `extern/dolrecomp` (backend C, sans LLVM)
  dans ce bac à sable : `cmake -S extern/dolrecomp -B build && cmake --build
  build --target dolrecomp` réussit sans réseau, sans zlib, sans LLVM.
- `CPUState::host_call` est un simple pointeur de fonction
  (`bool(*)(CPUState*, u32 adresse)`) appelé quand le code recompilé saute
  vers une adresse que DolRecomp n'a pas traduite nativement — typiquement un
  appel à une fonction du SDK Dolphin (`GXSetVtxDesc`, `PADRead`, `OSReport`…)
  identifiée par son adresse dans la MAP du jeu.

C'est exactement le point d'accroche pour rediriger ces appels vers les
implémentations d'Aurora plutôt que vers le runtime « Dolphin-dérivé » de
ModernGekko : `src/port/recomp_host.cpp` installe ce hook et tient la table de
correspondance adresse → trampoline natif. **Aucune entrée n'y est encore
enregistrée** — la table est indexée par adresse, et une adresse n'existe
qu'une fois `dolrecomp` lancé sur un vrai DOL/MAP GMSP01 (voir
`tools/port/recompile.py`) — mais le mécanisme est vérifié de bout en bout,
sans code de jeu ni image disque, par `sms::recomp::run_self_test()`
(`--recomp-hostcall-self-test`) : chemin de correspondance et chemin
d'échec (adresse non enregistrée, journalisée pour savoir quoi ajouter à la
table une fois la MAP en main) fonctionnent tous les deux.

Ceci répond à la question laissée ouverte plus haut : la route Aurora n'a
plus besoin d'être « plus longue et sans réponse pour l'audio » sur le plan de
l'intégration CPU — reste effectivement l'audio (JAudio2 n'a aucun appel
matériel direct, donc aucun crochet naturel côté DolRecomp pour cette
raison), qui demandera son propre travail quel que soit le chemin retenu.

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

## Construire le module du jeu

`moderngekko-port build` enchaîne trois choses : il relance `dolrecomp` sur le
DOL, configure le *module-template* et compile une DLL par jeu.

```bash
lib/ModernGekko/build/moderngekko-port.exe build extracted/<slug> \
    --backend c --toolchain auto --output C:\mgm
```

**Résultat sur GMSP01 :** `gGMSP01_recomp.dll`, **264 Mo**, 219 chunks,
2 plages de code et 135 plages de code auto-modifiant. La chaîne complète
fonctionne — outils, extraction, traduction, compilation, édition de liens.

### Le backend LLVM ne passe pas l'édition de liens

Symptôme : `LNK2001: ppc_native_region_available` sur chaque chunk, puis
`LNK1120` sur la DLL.

Cause : il existe **deux copies** du runtime CPU dans le Dolphin vendorisé
(`ExpansionPak/RecompCore`), et le *module-template* compile la mauvaise :

| copie | taille | définit le symbole |
|---|---|---|
| `DolRecomp/src/cpu/cpu.c` | 51,5 Ko | oui |
| `GXRuntime/src/core/cpu.c` (celle que le module compile) | 12,9 Ko | **non** |

Le symbole n'est émis que par `DolRecomp/src/backend/llvm/exits.cpp`, donc le
backend C n'est pas concerné. Repointer le sous-module ne corrige rien : l'amont
n'a qu'un seul commit d'avance, sans rapport. C'est un décalage à signaler au
mainteneur, ou à corriger localement.

**Ce n'est pas qu'un symbole manquant, et il n'y a pas de correctif local.**
Ajouter la fonction à GXRuntime compile, mais produit un module inerte.

Le protocole entier manque au runtime : `grep` sur les sources de ModernGekko et
sur l'ABI StaticRecomp ne trouve **ni** `PPC_HOST_CALL_NATIVE_REGION_QUERY`
(`0xFFFFFFFC`) **ni** `NATIVE_REGION`. L'hôte ne sait pas répondre à la requête.

Or voici ce que `src/backend/llvm/exits.cpp` émet à chaque entrée de région :

```
aucun host_call installé -> exécution native
sinon                    -> appeler ppc_native_region_available
    vrai                 -> exécution native
    faux                 -> interception_exit
```

ModernGekko **installe** un `host_call` : c'est un runtime dérivé de Dolphin, il
intercepte. Chaque région passerait donc par la requête, une implémentation
bouchon renverrait faux (`handled` ne pouvant jamais valoir
`PPC_NATIVE_REGION_QUERY_HANDLED`), et toutes les régions sortiraient par
`interception_exit`. Le module se lierait et n'exécuterait **rien** nativement —
l'inverse exact du gain recherché.

**Conclusion : le backend LLVM n'est pas utilisable avec ce runtime aujourd'hui.**
C'est un écart d'intégration à corriger en amont, pas une dépendance à ajouter.

**Conséquence pratique : utiliser `--backend c`.** C'est de toute façon le défaut
du gabarit.

### MAX_PATH bloque la compilation du module

Symptôme trompeur, côté ninja et non côté code :

```
ninja: error: WriteFile(...chunk_0000_text0_80003100.c.obj.rsp): Unable to create file.
```

Le chemin faisait **261 caractères**, un de plus que la limite Windows. Le
répertoire de cache combine une empreinte de 64 caractères et une de 16, ce qui
suffit à déborder depuis un répertoire de travail ordinaire.

Correctif sans toucher au système : sortir vers un chemin court avec `--output`
(`C:\mgm` ramène les chemins à environ 210 caractères). Activer
`LongPathsEnabled` marcherait aussi mais c'est un réglage machine.

## Lancer le jeu

```bash
lib/ModernGekko/build/moderngekko-run.exe     --game extracted/<slug>     --module C:/mgm/GMSP01/<hash>/gGMSP01_recomp.dll     --allow-interpreter
```

Sous Windows PowerShell 5.1, enchainer avec `;` : `&&` y est une erreur de
syntaxe.

**Resultat verifie sur GMSP01 : le jeu tourne et s'affiche.** Le module se charge
(`entry=0x8000522C`, le point d'entree reel du jeu), le backend audio Cubeb
s'initialise, et le jeu atteint l'ecran de selection de fichier -- decor rendu,
Mario affiche, dialogue de carte memoire fonctionnel. Fenetre titree
`ModernGekko - Super Mario Sunshine [GMSP01]`.

### Performance mesuree

Le compteur de FPS s'affiche dans le titre de la fenetre, donc il se lit sans
capture d'ecran :

```powershell
$p = Start-Process -FilePath <moderngekko-run.exe> -ArgumentList $a -PassThru
$p.Refresh(); $p.MainWindowTitle
```

| configuration | regime etabli |
|---|---|
| sans `--allow-interpreter` | 22 - 27 FPS |
| avec `--allow-interpreter` | 29 - 32 FPS |

Le compteur atteint **exactement 50,0** pendant les scenes legeres, ce qui est la
cadence nominale PAL. Le jeu tourne donc **autour de la moitie de sa vitesse**.

**Attention a ne pas surinterpreter ce tableau.** Les deux mesures ne sont pas
comparables : rien ne garantit que les deux executions etaient au meme point du
jeu au meme instant, et la charge varie enormement d'une scene a l'autre.
L'hypothese que `--allow-interpreter` coutait les images manquantes n'est **pas**
confirmee -- la mesure suggere meme l'inverse. Une comparaison valable demande
d'atteindre la meme scene dans les deux cas.

**Performance : en deca de la vitesse nominale.** C'est attendu avec le
backend C, qui produit du C portable et non du code optimise. Le backend qui
donnerait la performance est justement celui qui ne peut pas etre lie (voir
ci-dessous) : le decalage amont cesse d'etre un desagrement pour devenir le
verrou de performance du projet.

### `--headless` plante

En mode `--headless` le meme module part en `SIGSEGV` (code 139) juste apres le
chargement. Le mode fenetre, lui, tient. Le chemin sans fenetre est donc a eviter
pour valider un jeu : il donne un faux negatif spectaculaire.

## Ce qui n'est pas versionné ici

Le code produit par la recompilation dérive du disque et n'est pas commité :
seule la recette l'est. Chacun le régénère depuis sa propre copie du jeu.
