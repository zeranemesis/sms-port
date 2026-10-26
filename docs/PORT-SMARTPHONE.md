# Port smartphone — reconnaissance du 17 octobre 2026

> Mise à jour du 24 octobre : un APK du jeu ARM64 est maintenant installé et
> testé sur Samsung SM-G985F (Android 13, Vulkan Mali). Le démarrage, les vidéos,
> la création de carte et le menu PartyBoard partagé avec le PC sont vérifiés.
> Les proportions automatiques de la surface sont raccordées au correctif
> grand écran. Voir `MOBILE-PARTYBOARD.md` pour les menus et leurs limites de
> validation. Le texte ci-dessous conserve la reconnaissance initiale.

État : inspection du code local uniquement. Aucun APK ni binaire iOS produit.
Choix utilisateur : Android en premier, puis iOS. Le modèle Android est demandé.

Premier jalon construit : `tools/mobile/` produit un diagnostic ELF ARM64 PIE
avec le NDK local. Compilation et inspection ELF réussies ; aucune exécution
sur téléphone à ce stade. Les diagnostics mémoire passent dans l'émulateur
API 36, en x86_64 et en ARM64 traduit via `libndk_translation.so`. Le code et
les données statiques restent au-dessus de 4 Gio ; le port complet n'est pas
validé. Voir `tools/mobile/README.md` pour la reproduction et les preuves.

Un premier APK de diagnostic JNI est maintenant construit, installé et lancé
dans l'émulateur. Les tests MEM1/pile passent dans l'Activity en x86_64 et ARM64
traduit. Il ne contient pas encore le moteur Aurora ni le jeu. Les blocages
pointeurs sont détaillés dans `artifacts/android-arm64-probe/pointer-audit.md`.

## Fondations présentes

- Aurora comporte des chemins Android de gestion de surface et de cycle de vie
  (`third_party/aurora/lib/window.cpp`).
- Son fournisseur Dawn sélectionne Vulkan/OpenGLES pour Android et Metal pour
  Apple (`third_party/aurora/cmake/AuroraDawnProvider.cmake`). Cela ne prouve pas
  que les fonctions GPU employées par Sunshine passent sur un appareil donné.
- Le runtime possède une allocation POSIX à adresse contrôlée
  (`platform/port_runtime.cpp`) et les threads utilisent des piles dédiées
  (`platform/os/os_thread.cpp`). Leur fonctionnement Android ARM64 reste à tester.
- Le SDK Android local contient le NDK 29.0.14206865. `adb devices` ne signale
  aucun appareil connecté à la reconnaissance.

## Obstacles constatés

1. `aurora-bridge/CMakeLists.txt` force des fournisseurs de dépendances pensés
   pour la construction PC et lie `dbghelp` sans condition de plateforme.
   Il faut une construction ARM64 cohérente du jeu, du pont et des dépendances.
2. Le jeu conserve des pointeurs dans des entiers 32 bits. Le placement de la
   mémoire, des données et des piles doit être prouvé sur l'appareil cible avant
   de considérer une compilation réussie comme un port fonctionnel. Le CMake
   actuel bloque explicitement Apple ARM64 ; supprimer ce garde ne résout pas
   le problème.
   Le lien PC maintient aussi le code et les données statiques bas. La preuve
   d'une allocation MEM1 basse ne suffira donc pas à valider le jeu Android PIE.
3. L'entrée actuelle est un exécutable de bureau (`src/port_main.cpp`). Il manque
   une application mobile avec démarrage natif, import des données du jeu,
   stockage privé des sauvegardes et des caches, et paquet installable.
4. Il faut raccorder les pauses/reprises et pertes de surface à l'audio, au VI,
   au worker de rendu et aux copies GPU en attente.
5. Les commandes tactiles et leur raccordement aux entrées GameCube restent à
   construire : sticks, caméra, boutons et deux seuils de gâchette pour J.E.T.
6. La performance et les fonctions GPU doivent être mesurées sur téléphone.
   Les résultats D3D12 du PC ne prédisent pas les résultats Vulkan/Metal.

## Ordre de réalisation

1. Fixer la cible et tester dans un petit programme ARM64 les allocations,
   troncatures de pointeurs et piles utilisées par le runtime. Ce test décide
   si le modèle mémoire actuel est utilisable ou doit être adapté.
2. Construire Aurora et un écran de diagnostic sur le backend de la cible,
   tester les copies couleur/profondeur, les formats et la recréation de surface.
3. Construire l'application native et atteindre l'écran titre depuis les données
   importées, avec sauvegarde et audio fonctionnels.
4. Ajouter tactile et manette ; vérifier reprise après mise en arrière-plan.
5. Contrôler sélection du Soleil, peintures, pollution normale/lave/électrique,
   Noki et combat final. Préserver les protections des copies CPU/GPU.
6. Mesurer les durées CPU/GPU et la stabilité thermique en partie prolongée,
   puis optimiser sans modifier la vitesse de simulation. Aucun changement
   automatique de qualité n'est prévu.

Le premier livrable utile est la preuve du runtime ARM64 et du rendu mobile,
puis un paquet de démarrage testable. Aucun délai ni débit d'images n'est établi
avant ces validations. La configuration Windows reste indépendante.
