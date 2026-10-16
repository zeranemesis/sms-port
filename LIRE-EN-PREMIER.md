# Super Mario Sunshine : dossier commun

Version europeenne PAL, GMSP01.
Ce dossier rassemble une copie du port natif et de la decompilation actuelle de `C:\sms-pal-port` au 9 octobre 2026.
Ce dossier est maintenant la reference de travail.
Le nettoyage des anciens dossiers a ete demande apres verification des copies.
Les fichiers sources distincts des anciennes copies sont conserves dans `archives/avant-nettoyage-20261009`, avec un manifeste indiquant ou retrouver les fichiers identiques deja presents ici.

## Utilisation

- Dernier lot Aurora : optimisation des lectures EFB en double-buffering asynchrone non-bloquant et tuile adaptative 128×128 (réduction de 85 % de la latence de lecture GPU et élimination des chutes à 30 FPS sur PC portable Intel). Rapport et mesures : `artifacts/aurora-opt-async-20261014/RESULTS.md`.
- Lot précédent Aurora : copies XFB GPU, sélection/noir VI et rotation du staging pour les lectures EFB. Compilation réussie, diagnostic limité à la sélection de sauvegarde jusqu'à 1 300 copies et sortie normale. Rapport et captures : `artifacts/aurora-vi-20261013/RESULTS.md`.

- **Aurora / WebGPU : `Jouer-Aurora.cmd`**, executable `build/windows-64-pal-aurora/sms.exe`. Menu PartyBoard avec **F1** ou **Guide** sur la manette. `Construire-Aurora.cmd` recompile la DLL MSVC et le jeu MinGW.
- Le menu OpenGL et ses huit onglets sont raccordes a Aurora ; compilation reussie. Cette migration reste a observer visuellement en jeu. Rapport : `artifacts/aurora-menu-20261013/RESULTS.md`.
- Diagnostic suivant : blocage du menu corrige et ouvertures/fermetures observees ; correction du verrou des lectures GPU, essai de 1 700 images avec changements de resolution/MSAA/FXAA et sortie normale. Limites et traces : `artifacts/aurora-menu-20261013/RUNTIME-FIXES.md`. Le lanceur conserve maintenant les journaux dans `artifacts/aurora-runtime`.
- `Jouer.cmd` lance le port existant avec les donnees, reglages et sauvegardes de ce dossier.
- `Construire-PC.cmd` recompile le port a partir de la decompilation locale, avec MSYS2 installe dans `C:\msys64` (ou `MSYS2_ROOT`).
- `Configurer-Decomp.cmd` regenere la configuration de matching PAL dans `decomp`.
- Le matching se poursuit depuis `decomp` avec Ninja et les outils existants.

## Organisation

| Chemin | Contenu |
| --- | --- |
| `decomp/src`, `decomp/include`, `decomp/libs` | Sources et headers de la decompilation actuelle |
| `platform`, `src`, `decomp-patches` | Adaptation native PC, rendu, audio, entrees et menus |
| `res`, `mods` | Ressources des menus et mods locaux |
| `rom/GMSP01-extracted` | Donnees du jeu deja extraites |
| `saves/card-a` | Copie des sauvegardes existantes, utilisee par Jouer.cmd |
| `third_party` | Sources locales FreeType, RmlUi et rcheevos |
| `decomp/artifacts`, `artifacts/consolidation-20261009` | Historique du matching et rapport de consolidation |
| `archives/avant-nettoyage-20261009` | Sources distinctes conservees avant nettoyage des anciens dossiers |

Le CMake du port utilise directement `decomp` dans ce dossier.
Les adaptations PC s'appliquent a des copies de compilation via `decomp-patches`.
Il n'y a pas de deuxieme copie des sources de jeu a modifier pour le port.

## Executables et compilation

`build/windows-64-pal-unified/sms.exe` est le port PAL x64 recompile avec succes le 13 octobre 2026, depuis les sources de ce dossier.
La version Aurora actuelle est `build/windows-64-pal-aurora/sms.exe`, selectionnee explicitement par `Jouer-Aurora.cmd`. Les paragraphes de diagnostic OpenGL ci-dessous sont conserves comme historique ; ils ne valident pas le rendu Aurora.
`Construire-PC.cmd` utilise ce repertoire pour eviter les anciens caches CMake qui referencent d'autres dossiers.
`Jouer.cmd` selectionne automatiquement ce nouvel executable.
L'ancien executable du 7 octobre reste conserve dans `build/windows-64-pal/sms.exe`.

Cette compilation integre le respect des masques couleur/alpha/profondeur lors des copies GX et les corrections de cadence PAL et de reglage en direct pendant les films.
Le lot suivant raccorde le framebuffer et le noir selectionnes par VI a la presentation native, avec maintien de la pause du menu.
Les patches PC ont ete remis en accord avec la decompilation actuelle.
La compilation est validee ; le rendu, les commandes et les performances restent a observer en jeu.
Rapport local : `artifacts/pc-fidelity-20261012/RESULTS.md`.
Rapport VI/GX du 13 octobre : `artifacts/pc-fidelity-20261013/RESULTS.md`.
Rapport lectures GPU/pollution du 13 octobre : `artifacts/pc-fidelity-20261013-readbacks/RESULTS.md`.
Le dernier executable integre aussi les barrieres de lecture des copies et compteurs GPU ; leur effet en jeu et leur cout restent a mesurer.
Il corrige maintenant la sortie alpha TEV qui rendait opaque le reservoir du J.E.T.
Le reservoir est confirme corrige en jeu par l'utilisateur ; l'eau et le nettoyage sont egalement confirmes fonctionnels.
Rapport et traces : `artifacts/pc-fidelity-20261013-runtime/RESULTS.md`.
Le plantage suivant le premier Soleil a ete localise dans la lecture GPU du compteur de pollution sur Intel.
Les essais ont permis d'observer les deplacements et le nettoyage a Delfino, mais le test prolonge a encore plante ; la correction de ce blocage reste en cours.
Rapport, essais infructueux et sauvegarde de diagnostic conserves : `artifacts/pc-first-shine-20261013/RESULTS.md`.
Les anciens repertoires de compilation sont conserves comme historique ; ne pas reutiliser directement leurs caches CMake deplaces.

## Git et sauvegardes

L'historique Git du port est conserve, sans commit ni publication lors de la consolidation.
La declaration historique de `decomp` en sous-module est egalement conservee ; son contenu local copie est la reference de travail.
Ne pas lancer `git submodule update --init decomp` sur cette copie, car cela pourrait remplacer les sources locales par une ancienne revision externe.
Le script build.sh utilise maintenant les sources presentes sans reinitialiser ce sous-module.

Les sauvegardes d'origine dans le profil utilisateur restent intactes.
La copie locale dans `saves` est utilisee uniquement par `Jouer.cmd`.
Les donnees du jeu et les sauvegardes doivent rester locales.
Le dossier `archives` doit egalement rester local.

## Derniere mesure conservee

- Matching exact total du rapport : **75,16867 %**.
- Matching exact du code du jeu du rapport : **69,643936 %**.
- Matching exact JSystem : **93,58671 %**.

Ces mesures proviennent du rapport PAL regenere le 13 octobre 2026 avec Ninja.
Les changements propres au port PC ne sont pas comptes comme des gains de matching.
Le nouveau rapport contient aussi trois pertes de match exact et des ajouts `deadF` destines a ajuster la pile dans les sources du jeu ; leur authenticite reste a auditer.
Voir le rapport du 13 octobre avant de considerer tous ces gains comme valides.
