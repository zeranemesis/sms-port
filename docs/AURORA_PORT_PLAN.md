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

## Avancement vérifié le 2026-09-20

- La sortie audio hôte est active : les buffers AI DMA invités sont envoyés à SDL en PCM stéréo S16BE, à 32 ou 48 kHz, avec une file limitée à 250 ms pour éviter toute dérive lorsque le jeu s'exécute plus vite que le temps réel.
- Le décodeur THP progresse avec vidéo et audio (`v/a=238/236` pendant le run contrôlé). La corruption horizontale ne venait ni de GX, ni de la DMA du cache verrouillé : l'émulation sautait le changement d'état FPU paresseux opéré par l'exception GameCube `FP unavailable` lors des bascules de threads.
- Le port reproduit maintenant ce changement d'état dans son gestionnaire d'exception : FPR, valeurs paired-single, FPSCR et indicateur `FPSAVED` sont sauvegardés/restaurés par `OSContext`. Le run RVZ passe d'une rugosité YUV moyenne d'environ 16 avec des pics à 140 (bruit) à environ 0,8 avec des pics à 2,9. Les plans U et V sont entièrement non nuls.
- Des auto-tests couvrent le cache verrouillé, son chemin `psq_st` avec GQR6, et le transfert FPU entre deux contextes invités. Tous les tests du runtime passent avec la build `RelWithDebInfo`.
- La validation reste de niveau `SCRIPTED` : une inspection humaine de l'image, du son, des menus, de la création/relecture de sauvegarde et du gameplay reste nécessaire avant de déclarer un jalon jouable.

## État fonctionnel vérifié le 2026-09-20 (suite)

- L'entrée clavier atteint réellement le jeu : `Z` est mappé à `PAD_BUTTON_A`,
  permet de sauter l'intro et le journal invité confirme l'état
  `APP_STATE_GAMEPLAY`. Ce résultat dépend d'une fenêtre SDL au premier plan ;
  il prouve le trajet clavier → SDL → Aurora → `PADRead` → jeu, mais ne
  remplace pas un test à la manette.
- Le blocage initial de gameplay était `GXWaitDrawDone` : la tâche principale
  attendait indéfiniment le signal PE_FINISH absent. Le pont `GXSetDrawDone`
  marque maintenant le drapeau invité terminé après le rendu synchrone Aurora.
  Les callbacks PE et le modèle complet d'interruption restent à faire.
- Le FIFO GX exécute désormais les listes d'affichage invitées et résout les
  bases de tableaux CP dans la RAM recompilée. Les désynchronisations de FIFO,
  les rejets de bases de tableaux et le crash d'upload des tableaux indexés ne
  se reproduisent plus dans un run de gameplay de 70 s.
- Le menu de fichiers/options est visiblement rendu. Après validation du menu,
  la boucle de jeu soumet environ 80 000 draws et 640 000 sommets par seconde,
  sans crash pendant 70 s, mais la scène 3D demeure noire. C'est le bloqueur
  actuel : les compteurs montrent que la géométrie arrive ; l'état de matériaux
  (TEV/BP) des listes J3D doit être mesuré puis corrigé, pas remplacé par un
  état artificiel.
- Le pont de textures couvre également les matériaux J3D qui écrivent
  `TEXIMAGE0`/`TEXIMAGE3` directement dans une liste d'affichage au lieu
  d'appeler `GXLoadTexObj`. Aurora reçoit maintenant le pointeur hôte de ces
  pixels avant l'exécution de la liste. La validation visuelle atteint une
  scène de gameplay 3D : Mario, FLUDD, HUD, eau, ombres, terrain et textures
  sont visibles. Ce jalon valide le rendu de base ; il ne valide pas encore
  toutes les zones, effets, palettes ni le parcours complet.
- `GXLoadTlut` est maintenant observé par le port : la palette GameCube est
  traduite en pointeur hôte et en révision Aurora, tout en laissant le SDK
  recompilé émettre ses registres BP. Cette correction cible les artéfacts
  d'objets et d'effets indexés vus en gameplay. La build, l'intro THP et une
  cinématique en moteur (Peach, Toads et sous-titres) sont validées avec ce
  chemin. Les plans très saturés suivants sont l'effet de goop de la scène,
  pas une palette globalement corrompue.
- La build `RelWithDebInfo` est reproductible et les auto-tests host-call,
  MMIO, SDK, décrémenteur, PAD, cache verrouillé et contexte FPU passent.
- La carte mémoire dispose des ponts synchrones init/mount/open/create/
  read/write/statut, mais la création puis le rechargement d'une sauvegarde
  par un humain ne sont pas encore validés. Les API CARD asynchrones restent
  volontairement observées par le SDK recompilé, sans callback invité forgé.
- La création est néanmoins confirmée au niveau du support hôte : le jeu a
  produit `EUR/Card A/01-GMSP-super_mario_sunshine.gci` (57 408 octets) dans
  le profil DolphinJet. La relecture après redémarrage est maintenant
  confirmée par un run séparé : `CARDOpen`, `CARDGetStatus`, `CARDRead` (8
  192 octets) et `CARDClose` retournent tous `CARD_RESULT_READY`.
- Le pont ne détourne plus `GXCopyDisp` vers l'entrée publique Aurora, qui est
  un stub. Le corps recompilé du SDK est à nouveau exécuté : il émet les
  registres BP de copie EFB → XFB directement dans le FIFO, y compris le
  déclencheur de copie. Une exécution reconstruite affiche l'intro THP, le
  plan de vol et la carte animée avec ce chemin. Les écrans noirs initiaux
  restent des transitions à mesurer ; ils ne sont plus masqués par un appel
  de copie vide.
- Une séquence propre atteint ensuite un vrai gameplay 3D : niveau, Mario,
  FLUDD, HUD, pièces, transparences, géométrie dense et éclairage sont
  visibles. L'écran noir vu juste avant s'est résorbé durant la transition ;
  il est désormais classé comme chargement à mesurer (durée bornée), et non
  comme une panne de rendu permanente.
- Le crash de pipeline qui interrompait certaines cinématiques et matériaux a
  été corrigé dans Aurora. `GX_TEV_COMP_A8_*` partage les valeurs numériques
  de `GX_TEV_COMP_RGB8_*`, mais les opérandes alpha sont scalaires : le
  générateur WGSL produisait donc à tort un accès `.r` sur un `f32`. Le chemin
  alpha génère désormais sa comparaison scalaire et la version du cache de
  pipelines est passée à 14 afin qu'aucun shader WGSL défectueux préexistant
  ne soit réutilisé.
- Validation graphique manuelle du correctif : un run Direct3D 12 a dépassé
  30 000 draws/s sans erreur WGSL ni arrêt du processus ; un second run Vulkan
  a franchi le boot, la cinématique avec Mario/Peach/Toad, les panoramas 3D
  d'Isle Delfino et les séquences suivantes avec modèles, textures et couleurs
  cohérents. Le réglage utilisateur a été remis sur `Auto` après ces essais.

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

1. Atteindre la sélection de fichier et lancer explicitement le GCI existant,
   puis couvrir une entrée et une sortie de niveau. Mesurer la durée des
   écrans noirs de chargement ; ne les traiter comme un défaut que si le jeu
   cesse de progresser ou si le délai est anormalement long.
   La première cinématique ne peut pas être court-circuitée par une simple
   touche A : le code de `TMovieDirector` ne l'accepte qu'après que le drapeau
   « déjà vue » a été enregistré. Le test doit donc attendre la fin de la
   démo, ou repartir d'un état de titre réel ; il ne doit pas confondre cette
   règle du jeu avec une perte d'entrée clavier.

2. Étendre la validation graphique à la sélection de sauvegarde, l'aéroport
   jouable et la place Delfino. Les cinématiques et panoramas d'Isle Delfino
   sont désormais validés sur Direct3D 12 et Vulkan ; les textures à palette,
   les effets EFB/XFB et la caméra de jeu restent à contrôler dans une scène
   interactive.

3. Une fois la scène 3D visible, faire un playtest à la manette : marche,
   caméra, FLUDD, pause, mort et changement de zone.

4. Vérifier au casque la synchronisation et la qualité de la sortie AI DMA dans
   l'intro puis dans trois niveaux ; le transport est actif, l'écoute humaine
   manque.

5. La création puis la relecture du GCI sont validées. Vérifier maintenant
   une mise à jour de progression et sa relecture, sans formater ni écraser la
   carte. Les API asynchrones CARD ne seront bridgées que lorsqu'un appel réel
   le nécessitera et avec un chemin de callback invité vérifiable.

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
