# Couche input : ce qui doit vraiment changer (et ce qui ne doit pas)

Conception pour la Phase 7 du port (manette/clavier). Corrige une
simplification excessive de la feuille de route initiale : `TMarioGamePad`
n'est **pas** le bon point de découplage — il n'a besoin d'aucune
modification. Ce document est de la conception, pas du code : la seule action
concrète qu'il implique touche `src/dolphin/pad/*`, restreint par
`AGENTS.md`, donc à faire par un humain.

## Ce que `TMarioGamePad` consomme réellement

`include/System/MarioGamePad.hpp` (accessible à l'agent) hérite publiquement
de `JUTGamePad` (`JSystem/JUtility`, restreint) et utilise directement ses
membres internes : `mButton.mTrigger`/`mButton.mButton`/`mButton.mAnalogRf`,
`mMainStick.mPosX`/`mPosY`, `mPortNum`, les types `EPadPort`/`EButtons` et
les constantes `DPAD_UP`/`Z`/`R`/`L`/etc. Ce n'est pas juste une classe qui
consomme une API publique stable de haut niveau — elle touche la
représentation interne de `JUTGamePad`. La découpler proprement de
`JUTGamePad` demanderait de toucher `JUTGamePad` lui-même, donc c'est hors de
portée pour un agent autonome. Tenter une réécriture partielle ici serait
risqué (comportement de jeu non vérifiable sans build) pour un gain nul :
le vrai point de découplage est plus bas, et plus étroit que prévu.

## Le vrai point de découplage : 7 appels SDK, tous dans `JUTGamePad.cpp`

Lecture de `src/JSystem/JUtility/JUTGamePad.cpp` (restreint, lecture seule
pour l'agent) : c'est le **seul** fichier qui touche le SDK GameCube brut.
Tout le reste de `JUTGamePad` (son `update()`, la construction de `mButton`/
`mMainStick`, `TMarioGamePad` par-dessus) est de la logique C++ pure qui
n'a besoin d'aucun changement :

| ligne | appel SDK |
|---|---|
| 58 | `PADSetSpec(PAD_SPEC_5)` |
| 60 | `PADInit()` |
| 77 | `PADRead(mPadStatus)` |
| 159 | `PADReset(reset_mask)` |
| 416, 424, 432 | `PADControlMotor(port, ...)` (rumble) |
| 566 | `PADRecalibrate(mask)` |

`mPadStatus` est un tableau `PADStatus[4]` (`dolphin/pad.h`, restreint) —
c'est la structure d'échange, déjà le format que `README.port.md` visait
(« structure façon `PADStatus` »).

## Aurora fournit déjà ce remplacement — confirmé par le projet voisin

`docs/recompilation.md` cite Aurora comme fournissant PAD (en plus de GX/DVD/
CARD). Vérifié concrètement dans `/home/user/smashmelee` (même fondation
Aurora, lu en lecture seule) : son `CMakeLists.txt:151` lie
`aurora::pad` dans la liste des bibliothèques du binaire, et son
`files.cmake` **n'inclut jamais `src/dolphin/*`** dans la liste des sources
compilées — c'est-à-dire que le SDK GameCube retail (implémentant
`PADInit`/`PADRead`/etc. par-dessus le vrai matériel) est simplement absent
du build, et Aurora fournit ces mêmes symboles par-dessus SDL2/
GameController en interne. Aucun fichier dédié au pad n'existe côté jeu dans
ce projet voisin : c'est un remplacement au niveau de l'édition de liens,
pas une réécriture.

## Recommandation

1. Ne rien changer dans `include/System/MarioGamePad.hpp` /
   `src/System/MarioGamePad.cpp`, ni dans `JUTGamePad.hpp`/`.cpp` au-delà des
   7 sites listés ci-dessus.
2. Quand le CMake du port existera (Phase 2), exclure `src/dolphin/pad/
   Pad.c` (et `Padclamp.c`) de la liste des sources compilées et lier
   `aurora::pad` à la place — même schéma que `smashmelee/files.cmake` +
   `CMakeLists.txt:151`. Si `aurora::pad` n'expose pas exactement les mêmes
   signatures (`PADSetSpec`, `PADInit`, `PADRead`, `PADReset`,
   `PADControlMotor`, `PADRecalibrate`), un petit fichier de compatibilité
   dans `src/dolphin/pad/` (nouveau, pas une modif du fichier retail) suffit
   à faire le pont — mais d'après le précédent du projet voisin, ce
   remplacement est probablement un lien direct sans code à écrire.
3. Le clavier/manette de secours (fallback quand aucune manette n'est
   connectée) est une politique côté Aurora ou côté ce futur fichier de
   pont, pas côté `TMarioGamePad` — `docs/MELEE_PORT.md` du projet voisin
   documente exactement ce choix pour son propre port (contrôles clavier de
   repli non persistants au port 0), un précédent réutilisable pour la même
   décision ici.

Cette phase n'a donc pas de travail de code supplémentaire côté agent au-delà
de ce document : la modification réelle (exclure `Pad.c`/`Padclamp.c` du
build, lier `aurora::pad`) est indissociable de la Phase 2 (CMake, humain) et
touche potentiellement `src/dolphin/pad/*` (restreint).
