<div align="center">

# Super Mario Sunshine — port PC natif (PAL)

**Super Mario Sunshine recompilé pour Windows depuis sa décompilation, avec le moteur de rendu Aurora (Direct3D 12, Vulkan).**
Version européenne **GMSP01**, et **Super Mario Eclipse** en version jouable à part.

[![Dernière version](https://img.shields.io/github/v/release/zeranemesis/sms-port?label=version&color=ffc93c)](https://github.com/zeranemesis/sms-port/releases/latest)
![Windows x64](https://img.shields.io/badge/Windows-x64-1e88e5)

[**Télécharger**](https://github.com/zeranemesis/sms-port/releases/latest)

<sub>Aucune donnée du jeu n'est fournie : il faut sa propre copie du disque.</sub>

</div>

## Ce que propose le port

- **Rendu Aurora** (WebGPU via Dawn) : Direct3D 12 par défaut, Vulkan ou Direct3D 11 au choix.
- **60 images/s** en jeu, à la vitesse d'origine ; repli automatique sur 30 i/s si le PC ne suit plus.
- **Écran large** 16:9, 16:10 ou 21:9, avec le HUD au centre ou sur les bords.
- **Résolution interne** jusqu'à 8×, MSAA, FXAA, filtrage anisotrope, netteté, luminosité.
- **Menu en jeu** (touche **F1** ou bouton **Guide** de la manette) : vidéo, audio, manettes, jeu, sauvegardes, mods, succès, mises à jour, réglés en direct.
- **Manettes** Xbox, PlayStation, Switch Pro et autres (SDL), clavier entièrement reconfigurable, caméra à la souris.
- **Mods** : packs de textures HD (format Dolphin), mods GameBanana via CubeShelf.
- **Super Mario Eclipse** : le mod tourne nativement, avec son propre exécutable.
- **Archipelago** (optionnel) : Sunshine en multimonde randomisé.

## Jouer

### Avec CubeShelf (recommandé)

[CubeShelf](https://github.com/zeranemesis/CubeShelf-Launcher) installe et met à jour ce port tout seul : il prend la dernière version publiée ici, prépare le jeu depuis ton disque (ISO, GCM ou RVZ) et gère les mods.
Ajoute ton disque de Super Mario Sunshine dans CubeShelf, puis **Jouer**.

### Sans CubeShelf

1. Télécharge `sms-pal-win-x64.zip` dans la [dernière version](https://github.com/zeranemesis/sms-port/releases/latest) et décompresse-le où tu veux.
2. Lance `sms.exe`.
3. Au premier lancement, une fenêtre demande l'image disque du jeu (**ISO, GCM ou CISO**). Le chemin est retenu dans `settings.txt` (`disc_image`) : il ne sera plus demandé.

Un fichier **RVZ** doit d'abord être converti en ISO avec Dolphin (clic droit sur le jeu → *Convertir le fichier* → ISO).

Pour **Super Mario Eclipse**, utilise `sms-eclipse-win-x64.zip` avec le disque Eclipse (`GMSE04`), obtenu en appliquant le patch officiel du mod à l'ISO nord-américaine. CubeShelf s'en charge pour toi.

**Configuration :** Windows 10 ou 11 64 bits, carte graphique compatible Direct3D 12 ou Vulkan.
Au tout premier lancement, la préparation des shaders peut prendre quelques minutes (barre de progression dans la fenêtre) ; les lancements suivants sont immédiats.

## Commandes

Les touches sont celles d'un clavier AZERTY (elles suivent la position physique : WASD en QWERTY).

| Action | Clavier / souris | Manette |
| --- | --- | --- |
| Se déplacer | Z Q S D ou flèches | stick gauche |
| Caméra | I J K L, ou la souris | stick droit |
| A / B | Espace / Maj | A / B |
| X / Y | V / F | X / Y |
| L / R | A / E, clic droit / clic gauche | gâchettes |
| Z | W | bouton d'épaule droit |
| X (souris) | clic du milieu | |
| Start | Entrée | Start |
| Menu du port | F1 | Guide |
| Plein écran | F11 ou Alt+Entrée | |
| Quitter | Échap | |

Toutes les touches se changent dans le menu F1 ou dans `bindings.txt`.

## Réglages

Tout se règle dans le menu F1. Les réglages sont enregistrés dans `settings.txt` à côté de `sms.exe`, un fichier texte commenté qui peut aussi se modifier à la main (`widescreen = 16:9`, `frame_rate = 60`, `resolution = 2`…).
Chaque option a aussi sa variable d'environnement (`SMS_WIDESCREEN`, `SMS_FRAME_RATE`…) qui prend le pas sur le fichier.

Les sauvegardes sont dans `%APPDATA%\sms-port\card-a` : une mise à jour du port ne les touche pas.

## Compiler depuis les sources

Le port se compile sous Windows avec deux chaînes :

- **MSVC** (Visual Studio 2022 ou plus récent, CMake, Ninja) pour `sms_aurora.dll`, le pont vers Aurora (`aurora-bridge/`) ;
- **MSYS2 MINGW64** (`C:\msys64`) pour le jeu lui-même.

```bat
Construire-Aurora.cmd
Jouer-Aurora.cmd
```

`Construire-Aurora.cmd` compile la DLL puis le jeu dans `build/windows-64-pal-aurora/`.
`Jouer-Aurora.cmd` le lance avec les données extraites dans `rom/GMSP01-extracted/files` et les sauvegardes de `saves/`.

Les paquets publiés se construisent avec `tools/package-cubeshelf.ps1 -Version cubeshelf-vX.Y.Z` (puis `-Eclipse` pour Eclipse) ; le script refuse tout exécutable qui contiendrait des fichiers du jeu.

Pour aller plus loin :

| Document | Contenu |
| --- | --- |
| [BUILD.md](BUILD.md) | prérequis et compilation manuelle |
| [aurora-bridge/README.md](aurora-bridge/README.md) | le pont Aurora et la série de patches `aurora-sms.patch` |
| [docs/CUBESHELF.md](docs/CUBESHELF.md) | le contrat avec CubeShelf : paquets, lancement, liste de mods |
| [docs/ECLIPSE.md](docs/ECLIPSE.md) | Super Mario Eclipse en natif |
| [docs/ARCHIPELAGO.md](docs/ARCHIPELAGO.md) | le mode Archipelago |
| [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) | organisation du code, patches de la décompilation, variables d'environnement |
| [docs/64-BIT.md](docs/64-BIT.md) | comment le jeu, écrit pour 32 bits, tourne en 64 bits |

## Organisation du dépôt

```
aurora-bridge/    sms_aurora.dll : le pont MSVC vers Aurora, et ses patches
platform/         remplacement du SDK GameCube : rendu GX, disque, audio, manettes, OS, menus
decomp/           la décompilation du jeu
decomp-patches/   changements propres au PC, appliqués à une copie des sources à la compilation
src/              point d'entrée et en-têtes de compatibilité
res/              ressources du menu
tools/            compilation, paquets et outils de développement
docs/             documentation
```

## Remerciements

- La décompilation de Super Mario Sunshine ([doldecomp/sms](https://github.com/doldecomp/sms)) et tous ses contributeurs.
- Le port PC dont ce dépôt est issu : [chasem-dev/sms-pc-port](https://github.com/chasem-dev/sms-pc-port).
- [Aurora](https://github.com/encounter/aurora), la réimplémentation de GX sur WebGPU.
- [Super Mario Eclipse](https://github.com/JoshuaMKW/Super-Mario-Eclipse) et [BetterSunshineEngine](https://github.com/DotKuribo/BetterSunshineEngine).
- [RmlUi](https://github.com/mikke89/RmlUi) pour le menu, [SDL](https://github.com/libsdl-org/SDL) pour les fenêtres et les manettes.

<sub>Super Mario Sunshine est © Nintendo. Ce projet n'est ni affilié à Nintendo ni approuvé par Nintendo, ne contient aucune donnée du jeu et doit être utilisé avec une copie du jeu que l'on possède.</sub>
