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

## Code auto-modifiant : l'avertissement est un faux positif (vérifié)

L'outil avertit que le DOL modifie de la mémoire exécutable à l'exécution et liste
les instructions concernées dans `generated_smc.txt`. L'avertissement apparaît
avec les deux backends. **Ce point n'est plus ouvert : les 148 sites ont été
désassemblés et la cause est identifiée. Aucun correctif ciblé n'est nécessaire.**

### Ce que le fichier contient réellement

`generated/generated_smc.txt` liste des **plages**, pas des instructions.
`smc_note` (`extern/dolrecomp/src/analysis/smc.c`) fusionne deux notes distantes
de 4 octets ou moins, si bien que l'intérieur d'une plage contient des adresses
qui n'ont jamais été signalées. Les bornes des 135 plages totalisent 259
**octets** ; seuls les 148 bouts de plages sont garantis notés. Compter les
octets revient à inventer 111 instructions.

Désassemblés avec `powerpc-eabi-objdump` (`-b binary -m powerpc:common`), les
148 adresses notées se répartissent en :

| classe | nb | détail |
| --- | --- | --- |
| magasins (`st*`) | 144 | `stw`, `stb`, `sth`, `stfs` |
| opérations de cache (`icbi`) | 4 | `0x800034A0`, `0x8000547C`, `0x80337BBC`, `0x8033B8F4` |
| autre | **0** | — |

Zéro instruction hors de ces deux classes. C'est exactement ce que fait
`analyze_smc_section`, qui ne signale qu'un `icbi` ou un magasin
(`smc_inst_targets_code`) : le tri est complet, il ne manque rien.

Le cas le plus net est `0x8033B8F4`, dans `ICInvalidateRange` : les octets sont
`7c 00 1f ac`, soit `icbi 0,r3`, au milieu d'une boucle d'invalidation de cache
(`addi r3,r3,32` / `bdnz`) suivie de `hwsync`. Ce n'est pas un magasin, et ce
n'est pas du code auto-modifiant.

### La cause : un balayage linéaire qui ne s'arrête jamais

`analyze_smc_section` parcourt les instructions **dans l'ordre mémoire** et tient
un état de « registres connus », propagé par `update_known_regs`. Ce balayage ne
s'arrête ni sur une branche, ni sur un `blr`, ni sur une frontière de fonction.
Un registre posé par une fonction reste donc « connu » dans les fonctions
suivantes tant que rien ne l'efface.

Rejouer cette propagation donne la cause exacte. Le site le plus productif est
`0x80349CBC`, dans `PADSetSpec` :

```asm
80349cbc:  3c 80 80 35   lis     r4,-32715     ; r4 = 0x80350000
80349cc0:  38 04 9f b8   addi    r0,r4,-24648   ; r0 = 0x80349FB8  (pointeur de fonction)
80349cc4:  90 0d 8c a8   stw     r0,-29528(r13) ; écrit dans une table de callbacks
80349ccc:  4e 80 00 20   blr                   ; fin de PADSetSpec
80349cd0:  38 60 00 00   li      r3,0          ; début de SPEC0_MakeStatus
80349cd4:  b0 64 00 00   sth     r3,0(r4)      ; <- signalé à tort
```

`PADSetSpec` se termine à `0x80349CCC` (symbole : taille `0x60` à partir de
`0x80349C70`) et `SPEC0_MakeStatus` commence à `0x80349CD0`. Le `blr` n'efface
rien : `r4` vaut toujours `0x80350000` — une adresse **de code** — quand la
fonction suivante écrit `sth r3,0(r4)`. Or `r4` est ici le **pointeur fourni par
l'appelant**, un paramètre que le balayage est incapable de connaître.

C'est le mécanisme général, et il est chiffré :

- 79 des 144 magasins signalés remontent à ce seul `lis r4,-32715` ;
- 50 adresses effectives distinctes seulement, dont 17 Points vers `0x80350000`,
  qui est le début de la zone `CARDCheck`/`DoMount` du SDK ;
- 21 instructions source au total, toutes des `lis`/`addi` servant à fabriquer un
  **pointeur de fonction** (`0x80350000` + un déplacement) stocké ensuite dans
  une table de callbacks — le schéma `PADSetSpec`, `PADControlMotor`, `SPEC*_MakeStatus` ;
- distance médiane entre le magasin et l'instruction qui a posé le registre :
  **93 instructions**, maximum 443. Rien ne traîne sur des millions
  d'instructions ; la fuite est strictement locale, ce qui exclut un défaut
  d'effacement global au profit d'un défaut de **frontière de fonction**.

Aucun de ces magasins n'écrit du code. Le port n'a donc **rien à corriger** pour
le code auto-modifiant, et ce point n'est plus un « point ouvert ».

### Reproduire

`tools/port/triage_smc.py` rattache les plages à `config/GMSP01/symbols.txt`.
Pour la preuve par désassemblage, il faut tenir compte de deux pièges que ce
fichier de sortie ne montre pas :

- les sections texte d'un DOL ne sont **pas** contiguës en adresse virtuelle
  (ici la section 0 finit à `0x80005540` et la section 1 commence à `0x80005600`,
  soit 0xC0 de bourre) : concatener les sections décale le décodage ;
- l'en-tête DOL place le magic à `0x100`, pas à `0`, et les adresses virtuelles
  à `0x48` — les offsets de sections sont à `0x00`, pas à `0x1C`.

Voir `tools/port/dol_text.py` et `tools/port/smc_audit.py`.

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

**Mise à jour du 2026-09-18 : corrigé localement.** L'écart tenait en deux
parties bien identifiées une fois le code réel lu (pas deviné) des trois
couches concernées (`GXRuntime/include/core/cpu.h`, `GXRuntime/src/core/cpu.c`,
`Source/Core/Core/PowerPC/StaticRecomp/StaticRecompCore_Hooks.cpp`) :

1. **Le lien** : `GXRuntime/src/core/cpu.c` n'implémentait pas
   `ppc_native_region_available`, ni les macros `PPC_HOST_CALL_NATIVE_REGION_QUERY`
   / `PPC_NATIVE_REGION_QUERY_PENDING` / `PPC_NATIVE_REGION_QUERY_HANDLED` que
   son propre `cpu.h` ne déclarait pas non plus — contrairement à la copie de
   DolRecomp. Le layout de `CPUState` est vérifié identique entre les deux
   copies (mêmes champs `external_addr`/`external_value`/`external_rid`,
   même `host_call`), donc porter uniquement les trois macros + la fonction
   (28 lignes, vérifiées ligne à ligne contre l'original) dans la copie de
   GXRuntime est un ajout sûr, pas un remplacement de fichier.
2. **Le protocole côté hôte** (la partie qui manquait vraiment et que le
   `grep` précédent confirmait absente) : `StaticRecompCore::HookHostCall`
   (`StaticRecompCore_Hooks.cpp`) transmettait tout appel, y compris
   l'adresse sentinelle `0xFFFFFFFC`, tel quel à `ModManager::HostCall`, qui
   ne la reconnaît pas — `external_rid` restait donc à `PENDING`, et
   `ppc_native_region_available` renvoyait toujours "indisponible". Le
   correctif intercepte cette adresse **avant** de transmettre, et répond
   avec l'oracle déjà câblé mais jusque-là seulement utilisé au chargement du
   module : `m_module_source.host_call_range_contains` (lui-même
   `ModManager::HandlesRange`, qui sait déjà quelles plages contiennent un
   point d'interception).

Patch complet (3 fichiers, 92 lignes) : [`patches/moderngekko-native-region-query.patch`](../patches/moderngekko-native-region-query.patch).
À appliquer depuis `lib/ModernGekko/vendor/dolphin` (le fork Dolphin vendorisé
sous `ModernGekko`) :

```bash
git -C lib/ModernGekko/vendor/dolphin apply /path/to/sms-port/patches/moderngekko-native-region-query.patch
```

**Résultat mesuré sur GMSP01**, après avoir recompilé `moderngekko-port`/
`moderngekko-run` (le correctif touche le runtime hôte, pas seulement le
module par jeu) puis reconstruit le module avec `--backend llvm` : le lien
**réussit sans erreur** (6978 chunks, `gGMSP01_recomp.dll` produite), et le
module n'est **pas inerte** — capture d'écran à l'appui, le jeu affiche la
même cinématique d'ouverture (avion, vue de l'archipel) qu'avec le backend C,
preuve que l'exécution native est bien empruntée et pas juste que
l'interception de secours tourne en boucle. FPS observé en fenêtré avec
`--allow-interpreter` : pics à ~31-33 sur les scènes légères contre ~22-27
avec `--backend c` au même point de la cinématique — un gain réel mais qui
n'atteint pas encore les 50 FPS nominaux PAL ; d'autres goulots
(vraisemblablement les sites d'interception encore fréquents dans le code de
boot, et le thread audio) restent à investiguer séparément.

**Preuve de niveau SCRIPTED uniquement** : lien vérifié par un build réel,
rendu vérifié par capture d'écran automatisée sur ~2 minutes d'exécution
stable (pas de crash), FPS lu depuis le titre de fenêtre. Aucune partie
humaine jouée sur ce backend pour l'instant — la comparaison de performance
ressenties en jeu réel reste à faire.

**Mise à jour du 2026-09-19 : la mesure ci-dessus vient de la cinématique
d'intro, pas du gameplay — ne pas la généraliser.** Valentin a fait remarquer,
à juste titre, qu'une cinématique n'est pas représentative : elle enchaîne des
plans très divers (avion, foule de Pianta, gros plans figés) dont la charge
GPU n'a aucun rapport avec le jeu réellement joué. Mesurer là-dessus et
conclure sur « la vitesse du jeu » aurait été trompeur.

Pour corriger ça, le protocole d'automatisation intégré à ModernGekko
(`--automation-dir`, fichiers `commands/*.txt` en clé=valeur, `status.txt`
avec `fps`/`vps`/`speed` lus directement depuis `Core::System::GetPerfMetrics()`
côté moteur — pas depuis le titre de fenêtre) a servi à naviguer les menus
jusqu'à une vraie partie : création de fichier sur la carte mémoire, sélection
de données, `START` sur le fichier, traversée de la cinématique d'arrivée,
jusqu'au HUD de jeu complet (pièces, soleils, vies) avec Mario réellement
déplaçable au stick. Vérifié positivement : `main_y`/`main_x` font marcher
et nager Mario (ondulations d'eau en temps réel autour de lui, capture à
l'appui), donc c'est bien de l'interaction, pas une seconde cinématique.

**Mesuré en gameplay réel (plage/eau autour de l'aéroport de Delfino,
backend LLVM + résolution interne 6x)** : `speed` = 0.997, 1.002, 1.006,
0.9996, 1.008 sur cinq échantillons pris pendant un déplacement effectif du
personnage (pas à l'arrêt) — c'est-à-dire la cadence nominale PAL **atteinte
et même très légèrement dépassée** sur cette zone, contre 0.4-0.55 relevé
plus tôt pendant les passages les plus chargés de l'intro. La cinématique
d'intro est donc le pire cas du jeu côté performance, pas une référence pour
le reste.

**Toujours SCRIPTED, et une seule zone testée.** Ce sont des entrées
automatisées (fichiers de commande), pas une manette tenue par un humain, et
la zone testée est une plage ouverte peu chargée (peu de géométrie, aucun
PNJ) — pas la place de Delfino elle-même, plus dense (façades, Pianta,
eau/particules), qui reste le vrai test de charge à faire avant de conclure
que « le jeu tourne à 50 FPS » sans qualificatif.

**Conséquence pratique : `--backend llvm` est utilisable**, mais `--backend c`
reste le défaut du gabarit et le choix le plus sûr tant que ce correctif n'est
pas remonté en amont (`ExpansionPak/ModernGekko`) ni éprouvé au-delà de la
cinématique d'intro.

## Préparer une vraie manette (clavier) pour un playtest humain

Le générateur de config de ModernGekko (`GenerateControllerConfig`,
`tools/frontend_config.cpp`) ne produit que des profils manette SDL
(`Buttons/A = `Button A``) — inutilisable pour jouer au clavier, dont les
touches n'ont pas ces noms. Il faut écrire à la main
`<user-dir>/Config/GCPadNew.ini` (le fichier existant n'est jamais régénéré,
`EnsureControllerConfig` vérifie juste sa présence) et ajouter
`controller=<device>` sous `[Input]` dans `config.ini` pour que ce profil soit
effectivement chargé au lancement (sinon `EnsureControllerConfig` n'est même
pas appelé, `moderngekko_run.cpp:258`).

**Deux pièges, trouvés en lisant `GXRuntime/vendor/dolphin/Source/Core/InputCommon/ControllerInterface/DInput/`
après qu'un premier essai n'ait eu aucun effet** :
- Le nom de périphérique clavier sous Windows est `DInput/0/Keyboard Mouse`
  (`DInputKeyboardMouse.cpp` : `GetSource()` renvoie `"DInput"`, `GetName()`
  renvoie `"Keyboard Mouse"`) — pas `Keyboard Mouse/0/Keyboard Mouse`.
- Les touches spéciales sont nommées en **MAJUSCULES** dans la table
  `NamedKeys.h` de Dolphin (`RETURN`, `UP`, `DOWN`, `LEFT`, `RIGHT`) — `Return`
  ou `Up` ne correspondent à rien et sont silencieusement ignorés. Les lettres
  seules (`A`-`Z`) ne sont pas affectées par ce piège.

Profil clavier vérifié fonctionnel (testé : une pression sur `RETURN`
interrompt immédiatement le mode démo automatique du titre, alors que la
version fautive plus haut ne faisait jamais rien) :

```ini
[GCPad1]
Device = DInput/0/Keyboard Mouse
Buttons/A = X
Buttons/B = Z
Buttons/X = C
Buttons/Y = S
Buttons/Z = D
Buttons/Start = RETURN
D-Pad/Up = T
D-Pad/Down = G
D-Pad/Left = F
D-Pad/Right = H
Main Stick/Up = UP
Main Stick/Down = DOWN
Main Stick/Left = LEFT
Main Stick/Right = RIGHT
Main Stick/Modifier = Shift
C-Stick/Up = I
C-Stick/Down = K
C-Stick/Left = J
C-Stick/Right = L
Triggers/L = Q
Triggers/R = W
```

**Piège méthodologique attenant :** l'écran-titre de SMS ne reste pas figé sur
« PRESS START! » — il enchaîne d'office sur un mode démo (extraits de gameplay
auto-joués façon FLUDD). Presser une touche pendant l'intro ou la démo ne
prouve donc rien par simple observation d'un changement de scène : ça peut
avancer tout seul. Le test fiable est d'envoyer l'entrée et vérifier une
transition **immédiate** (moins d'une seconde), pas juste « ça a changé après
quelques secondes ».

**`--automation-dir` et une vraie manette/clavier sont mutuellement
exclusifs sur un même port** : l'automatisation installe un input overrider
(`ciface::Touch::RegisterGameCubeInputOverrider`) qui prend la main sur
l'entrée normale. Lancer avec `--automation-dir` pendant qu'un humain joue au
clavier bloquerait ses touches — relancer sans ce flag pour un vrai playtest.

## Bug ouvert : l'écran « Select data » affiche « New » malgré une sauvegarde réelle sur disque

Constaté en relançant une session fraîche sur un profil qui avait déjà une
partie créée : le fichier `.gci` existe bel et bien sur la carte mémoire
virtuelle (`<user-dir>/GC/EUR/Card A/01-GMSP-super_mario_sunshine.gci`,
57 Ko, horodaté de la session précédente — pas un fichier vide), mais l'écran
« Select data » affiche quand même « New » sur les trois emplacements au lieu
du nom/aperçu attendu pour un fichier existant. L'écriture sur la carte
mémoire fonctionne donc, mais quelque chose dans la relecture/l'affichage au
menu ne la reconnaît pas. Pas encore diagnostiqué plus loin (pas bloquant :
sélectionner le bloc recrée simplement un fichier par-dessus), mais à
creuser avant de considérer la persistance de sauvegarde comme fiable.

## Mesure de charge au-delà de la plage : la zone de gunk hostile de l'aéroport

En poussant plus loin après l'aéroport (zone recouverte du gunk
rose/orange hostile de l'histoire, avec effet de teinte sur Mario et rendu de
liquide), la vitesse mesurée est descendue à **0.90** (backend LLVM,
résolution 6x) — la première zone où un vrai coût de rendu se voit, contre
0.997-1.008 sur la plage ouverte et un retour à ~1.00-1.03 juste après, une
fois sorti du gunk et de retour dans l'eau. Toujours loin des 0.40-0.55 de la
cinématique d'intro, et toujours SCRIPTED. La place de Delfino elle-même
(façades, Pianta, plus de géométrie) n'a pas été atteinte par navigation à
l'aveugle cette session — reste le vrai test de charge à faire.

## Test de fumée automatisé

`tools/port/moderngekko_smoke_test.py` (dans ce dépôt) rejoue le strict
minimum vérifiable de façon fiable : lancer `moderngekko-run.exe` avec
`--automation-dir`, attendre `booted=1`, laisser tourner quelques secondes,
et vérifier que `state=running` et que `speed` (lu depuis
`Core::System::GetPerfMetrics`, pas le titre de fenêtre) dépasse un seuil bas.
Volontairement **ne rejoue pas** la navigation de menus jusqu'au gameplay :
cette séquence dépend du minutage exact des scènes de l'intro (voir plus haut)
et serait un test fragile, pas un test fiable. Ça suffit en revanche à
détecter un lien cassé, un module qui retombe silencieusement sur
l'interpréteur, ou un crash au boot — exactement la classe de régression que
le bug native-region-query aurait dû déclencher s'il avait existé un test
avant cette session :

```bash
python tools/port/moderngekko_smoke_test.py \
    --moderngekko-run <ModernGekko-Template>/lib/ModernGekko/build/moderngekko-run.exe \
    --game <ModernGekko-Template>/extracted/GMSP01 \
    --module <output>/GMSP01/<hash>/gGMSP01_recomp.dll
```

Vérifié dans les deux sens le 2026-09-19 : `PASS` avec le vrai module LLVM
(`speed=0.516` pendant l'intro, cohérent avec les mesures ci-dessus), et
`exit code 2` propre sur un chemin de module inexistant.

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

## Reproduction locale du 2026-09-18 — recette confirmée reproductible

La recette ci-dessus a été rejouée intégralement sur une machine Windows 11
distincte, avec le vrai dump GMSP01 PAL déjà extrait pour la route Aurora
(`sms-port/orig/GMSP01/`, réutilisé tel quel dans `extracted/GMSP01/` du
template, sans re-extraction). Chaque étape a été exécutée séparément et
vérifiée avant de passer à la suivante :

- MSVC 19.51 (VS 18 Insiders) + Ninja embarqué + LLVM 20.1.8 (archive
  officielle extraite localement, `LLVMExports.cmake` patché sur le chemin
  DIA SDK réel de cette machine) : `dolrecomp` se configure et compile sans
  erreur avec `DOLRECOMP_ENABLE_LLVM=ON`.
- `lib/ModernGekko` (qui embarque Dolphin, 35 sous-modules `Externals`,
  ~881 Mo une fois clonés) se configure et compile sans erreur, cible
  `moderngekko-port`.
- `moderngekko-port.exe build extracted/GMSP01 --backend c --toolchain auto
  --output C:\mgm` produit `gGMSP01_recomp.dll` sans erreur.
- `moderngekko-run.exe --game extracted/GMSP01 --module <dll>
  --allow-interpreter` (fenêtré, pas `--headless`) : le jeu démarre
  (`entry=0x8000522C`, identique à la session précédente), le titre de
  fenêtre affiche un FPS qui monte de 0 à ~15-24 en quelques secondes, et
  **deux captures d'écran réelles** confirment un rendu correct au-delà du
  point déjà documenté : la cinématique d'intro (avion, vue de l'archipel
  depuis le hublot) puis la scène d'accueil sur la place Delfino avec les
  Pianta et le sous-titre "We're so pleased to welcome you to our beautiful
  home!" rendu net. Aucune erreur dans les logs (`stderr` ne contient que les
  deux lignes de boot attendues).

**Preuve de niveau SCRIPTED uniquement** : ce run a été lancé et observé par
capture d'écran automatisée, sans manette ni entrée humaine. Il valide que la
chaîne d'outils est reproductible sur une machine neuve et que le rendu/l'audio
progressent bien au-delà du point déjà atteint précédemment, mais **ne
constitue pas** une validation de jouabilité : rester jusqu'à l'écran de
sélection de fichier puis manipuler réellement une manette (niveau HUMAN)
reste l'étape suivante avant de considérer la question de la jouabilité comme
avancée.
