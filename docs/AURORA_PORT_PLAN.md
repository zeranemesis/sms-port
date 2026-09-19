# Plan Aurora pour un Super Mario Sunshine jouable à 100 %

## But et définition de « 100 % »

La cible est un port PC de la version PAL `GMSP01`.

Le code PowerPC original est recompilé statiquement par DolRecomp et exécuté dans DolphinJet.

Aurora est la couche de plateforme : fenêtre, rendu GX, disque, carte mémoire, vidéo, entrées et services système.

L'audio hôte sera un module de DolphinJet intégré à cette architecture, car Aurora ne fournit pas de runtime audio GameCube partagé.

Le port ne distribue ni ISO, ni DOL, ni données extraites.

Un jalon n'est validé que sur un dump personnel PAL correspondant au SHA-1 de `config/GMSP01/build.sha1`.

« 100 % » signifie qu'une partie neuve peut obtenir les 120 soleils, sauvegarder, être rechargée, et atteindre l'écran de fin sans utiliser Dolphin, ModernGekko ou des stubs qui changent la logique du jeu.

La cadence cible est 50 Hz PAL avec VSync activée.

## État de départ constaté le 2026-09-19

- `dolphinjet` compile avec Aurora, DolRecomp et le module de jeu généré.
- Un run local avec `Super-Mario-Sunshine-Europe-En-Fr-De-Es-It.rvz` atteint l'état applicatif `DONE`, maintient les retraces et émet environ 144 lots GX/s ; la console est donc vivante au-delà du boot.
- Ce même run ne produit pour l'instant qu'un écran vert uniforme : les primitives arrivent dans Aurora, mais la sortie GX (état/TEV/textures/EFB) reste incorrecte.
- Le pont `GXTexObj` transmet désormais les pointeurs de RAM invités, `GXLoadTexObjPreLoaded`, l'invalidation de cache et une révision de contenu pour les buffers mutables. Il est exercé par le RVZ, mais n'a pas encore corrigé l'écran vert.
- La scène problématique est le rendu THP YUV : le jeu charge les plans I8 Y (640×448), U et V (320×224) dans les maps 0, 1 et 2, puis les combine en quatre étapes TEV. Le prochain diagnostic doit comparer cet état TEV et le contenu des plans décodés, plutôt que supposer que le disque ou le boot est en faute.
- Mesure du 2026-09-19 : les trois plans THP sont zéro et leurs threads de lecture/décodage restent en attente ; `dvdError` et `videoError` restent nuls. Le blocage est donc en amont de TEV, dans l'ordonnancement/les files de messages THP et leurs déclencheurs VI/audio, pas dans le combinateur de couleurs.
- Les contextes, interruptions PI/VI et le transfert DVD de base sont déjà pontés.
- Les tableaux de sommets GX ont un pont hôte ; la validation visuelle de géométrie et de textures reste à faire.
- Les interruptions de fin de DMA audio et l'audio restent des blocages fonctionnels connus.
- Le décrémenteur/alarmes est maintenant ponté et possède un test ciblé ; sa validation pendant un run réel reste requise.
- `PADInit`, `PADRead` et les opérations PAD scalaires sont maintenant reliés à Aurora. Le pas par canal du `PADStatus` valait 11 octets alors que la table de symboles du jeu le fixe à 12 (`mPadStatus__10JUTGamePad … size:0x30`, soit 4 × 12) : les manettes 2 à 4 étaient écrites de travers. Corrigé, et l'auto-test vérifie désormais que le canal 1 tombe bien à +12 — la version précédente ne testait qu'un seul canal et validait donc la constante fausse. **Un playtest avec une entrée réelle reste requis**, et aucun clavier n'est encore activé (`PADSetKeyboardActive` n'est pas appelé), donc sans manette physique tous les ports rapportent `PAD_ERR_NO_CONTROLLER`.

Les observations détaillées et les commandes de build restent dans `docs/port_bootstrap.md` et `docs/recompilation.md`.

## Ordre d'exécution

| Jalon | Résultat livrable | Critère de sortie mesurable |
|---|---|---|
| P0 — Reproductibilité | une configuration CMake propre, un dump vérifié, un test de fumée automatisé et des journaux bornés | build `RelWithDebInfo` réussie ; le test de fumée démarre et maintient le jeu en état `running` |
| P1 — Démarrage | titre, intro, menu de fichiers et transition vers la partie sans blocage | 10 démarrages consécutifs, aucun crash ni boucle de journal ; lecture DVD et création de carte mémoire confirmées |
| P2 — Boucle temps/interruptions | décrémenteur, alarmes, PI et sources DMA livrent leurs interruptions avec le contexte invité correct | les alarmes du SDK déclenchent ; les threads THP/audio ne restent plus en attente d'une interruption absente |
| P3 — Rendu GX | sommets indexés, textures/palettes, EFB/XFB, copies et états TEV sont transmis correctement à Aurora | captures de référence : intro, sélection de sauvegarde, aéroport et place Delfino avec géométrie, HUD et textures visibles |
| P4 — Contrôles et persistance | SI/PAD convertit manette et clavier ; CARD lit, crée, met à jour et recharge une sauvegarde | un humain crée un fichier, relance DolphinJet puis retrouve ce fichier ; marche, caméra, FLUDD et menus sont jouables |
| P5 — Audio et THP | DMA AI/DSP, mixage hôte et décodage/horloge THP synchronisent son et vidéo | intro complète avec son ; transitions, voix et musique de trois niveaux sans dérive ni silence prolongé |
| P6 — Parcours complet | toutes les zones, boss, cinématiques, morts, transitions et retours menu fonctionnent | matrice de playtests couvrant les 120 soleils et les 7 épisodes de chaque monde, avec sauvegarde/rechargement |
| P7 — Finition | performances, stabilité, UX et paquet utilisateur | 50 Hz dans les scènes représentatives ; 2 h de playtest humain sans crash ; installation ne contenant aucun asset Nintendo |

## Travail prioritaire immédiat

1. Valider le décrémenteur pendant un démarrage réel : l'alarme doit entrer dans `DecrementerExceptionHandler` puis reprendre le contexte sans boucle.

2. Faire un playtest avec une manette réelle et une image PAL configurée : vérifier marche, caméra, FLUDD et vibration via le pont PAD.

3. Émuler les complétions AI DMA nécessaires à la sortie de veille des threads audio et THP.

4. Débloquer les files THP à partir du run RVZ : les trois plans YUV arrivent mais sont nuls ; les workers lecture/décodage attendent. Examiner l'ordonnancement OS, les files de messages, les retraces VI et le déclencheur audio qui doit promouvoir un frame décodé vers `dispTextureSet`. Les traces par image de `GXCopyDisp`/`GXFlush`/`GXDrawDone` ont été supprimées car elles masquaient les diagnostics utiles.

5. Construire un playtest reproductible du titre jusqu'à l'aéroport, puis remplacer progressivement l'automatisation par une manette réelle.

Ces actions sont ordonnées par dépendance : sans horloge, interruptions et entrée, une image fixe ne permet pas de conclure sur le rendu ni sur la jouabilité.

## Discipline de validation

- Ne jamais simuler une réussite de gameplay par une valeur arbitraire : tout registre, interruption ou protocole doit être relié à la spécification GameCube ou à une implémentation de référence vérifiable.
- Ajouter un test ciblé chaque fois qu'un pont Aurora reçoit une nouvelle responsabilité.
- Pour chaque correction, conserver un log court, une capture ou un état invité mesurable qui prouve le changement.
- Séparer strictement les données générées depuis le disque des sources du port et des correctifs aux sous-modules.
- Avant toute modification de JSystem, du SDK Dolphin, de THPPlayer ou du runtime, obtenir une revue humaine explicite conformément à `AGENTS.md`.

## Indicateurs à publier à chaque jalon

- version de l'ISO et hachage vérifié ;
- build, test de fumée et durée du run ;
- scène précise, type de test (`SCRIPTED` ou `HUMAN`) et preuve visuelle ;
- vitesse PAL (`speed`), images/seconde et résolution interne ;
- état de lecture/écriture de la carte mémoire ;
- blocages ouverts, avec PC invité, thread concerné et hypothèse marquée comme telle.
