# Randomizer classique : état du port natif

## Activer ou désactiver depuis le menu natif

Lancer `Jouer-Aurora.cmd`, ouvrir F1 → Mods → Super Mario Sunshine Randomizer,
choisir une seed, puis cliquer sur « Activer au prochain démarrage ». Le menu
enregistre `randomizer_mode = classic` et la seed dans les réglages. Fermer puis
relancer le jeu applique le choix avant l'initialisation de la carte mémoire.
« Jeu normal au prochain démarrage » enregistre `randomizer_mode = off` et
rétablit la carte habituelle au lancement suivant. La fiche affiche le mode
prévu pour ce prochain lancement.

Le mode classique intégré utilise le générateur original et la progression
expérimentale décrits ci-dessous. Il réutilise
`saves/randomizer-classic-test-v1/<seed>/card-a`, afin de conserver les parties
commencées avec l'ancien raccourci de test. L'alias `classic-test` reste reconnu.
Changer de seed ouvre son profil indépendant ; revenir à une seed retrouve
son manifeste et sa carte. Un échec d'enregistrement rétablit le choix précédent
dans le menu.

Le raccourci `Jouer-Randomizer.cmd` délègue maintenant à `Jouer-Aurora.cmd` et
respecte les réglages du menu. Le raccourci explicite
`Jouer-Randomizer-Classique-Test.cmd` reste réservé aux essais : il force encore
le mode de test. Il n'est plus nécessaire pour jouer au Randomizer intégré.

Validation : 19 cas de runtime et quatre démarrages Aurora lisant directement
les réglages enregistrés (classique seed 1 → désactivé → classique seed 2 →
classique seed 1). Les cartes sélectionnées sont correctes et le manifeste de
la seed 1 reste identique. Les clics de l'interface n'ont pas été vérifiés
visuellement.

## Préversion native jouable : entrées aléatoires

L'ancien réglage `randomizer_mode = entrances` conserve le moteur PC Aurora avec
une seed d'entrées aléatoires et les capacités normales disponibles dès le départ.
Il reste reconnu pour préserver les profils de cette préversion ; le bouton
d'activation du menu sélectionne maintenant le mode classique intégré.
Les buses Hover, Rocket et Turbo se sélectionnent dans cette
fiche ; reprendre la partie applique la buse de secours.
Cette préversion s'utilise avec Sunshine standard ; les combinaisons avec
Eclipse, Archipelago et Android sont refusées.

La carte se trouve dans `saves/randomizer-native-v1/<seed>/card-a`, à côté du
fichier de réglages utilisé. La carte habituelle reste séparée. Un fichier
`entrances.txt` conserve les routes : une modification des données produisant
une autre disposition pour une carte existante est refusée. Mourir ou réessayer
recharge la destination actuelle au lieu de remélanger le niveau.

Cette préversion utilise les règles d'accès récupérées du mod et un mélange
natif distinct, validé par parcours du graphe. Ses seeds ne correspondent pas
aux seeds console. Elle n'implémente pas encore la récupération progressive
des capacités, les Soleils arc-en-ciel ni l'ensemble des réglages du mod original.
Elle conserve les réglages de rendu et la cadence de simulation du port.

Validation : 64 dispositions reproductibles et bijectives, avec contrôle de
l'accessibilité des sept Anti-Mario dans le graphe des règles ; neuf tests du
runtime (dont isolation des cartes, refus des seeds invalides, conservation des
routes, refus d'un manifeste modifié et demandes de buse). Un test PC Aurora
avec les archives PAL a généré 98 routes et chargé plusieurs destinations.
Mario, J.E.T. et le HUD ont été observés dans la destination initiale Sirena 6,5.
Une campagne complète et tous les scripts de mission restent à vérifier.

Le randomizer de TheAzack9, bêta 0.3.6, est distribué en patch xdelta :
https://github.com/TheAzack9/SMS_Randomizer/releases/tag/beta-0.3.6
Son dépôt public ne contient pas le code source du mod.

## Intégration classique expérimentale dans Aurora

`Jouer-Randomizer-Classique-Test.cmd` lance le même exécutable Aurora, avec
`SMS_RANDOMIZER_MODE=classic-test`. Ce mode utilise le générateur original
traduit et une carte dédiée : `saves/randomizer-classic-test-v1/<seed>/card-a`.
Le manifeste conserve les 110 routes et les 120 récompenses ; une différence
avec un manifeste existant provoque un refus de chargement.

La collecte d'un Soleil passe par `TFlagManager::setShineFlag` et recalcule les
capacités. À chaque chargement de niveau, les 120 drapeaux de Soleils du jeu
restaurent le masque ; le processus démarre avec le masque original `0x400`.
Les hooks raccordent les décisions de changements d'état de Mario, la prise
d'objet, la rotation en saut, les autorisations des buses, leur changement et
la présence de J.E.T. Le chargement applique aussi les récompenses de tenue,
lunettes et casque. Le pointeur `mHolder` est effacé par son champ C++ lorsque
le mod bloque certains états d'escalade ; ce champ GameCube à l'offset `0x68`
ne représente pas une vitesse et doit rester un pointeur sur le port 64 bits.

Le menu F1 indique le mode de test actif, le nombre et les noms des capacités obtenues. Ses boutons
de buses proposent uniquement les buses autorisées. Le bouton normal d'activation
sélectionne désormais le mode `classic`, qui utilise ce générateur et ce profil.

Ce mode reste incomplet : les autres contrôles de capacités, les transitions
spéciales, les autres éléments du HUD du mod, les objectifs et les options restent
à terminer. Le recalcul depuis les drapeaux est testé dans le runtime ; un
cycle réel collecte → sauvegarde → fermeture → reprise reste à valider en jeu.

Validation : 14 cas de runtime passent, couvrant notamment le mode désactivé,
les entrées invalides, l'isolation des profils, la stabilité des manifestes,
leur modification refusée, la collecte et la restauration des capacités, les
restrictions et la conservation de fichiers témoins des autres cartes. Un
lancement par `Jouer-Aurora.cmd` avec les archives PAL a généré les 110 routes
originales et exécuté 900 frames de gameplay dans la première destination de
la seed 1 (`2f,00`), sans erreur fatale dans le journal. Un second parcours
a chargé `2f,00`, `07,00` et `04,07`. Les trois chargements de Mario ont confirmé
le masque initial `000400` et J.E.T. verrouillé dans les journaux. Cela ne valide pas
visuellement le niveau : Computer Use ne parvient pas à joindre son service
Windows pendant ces essais.

## Récompenses identifiables en jeu

Les Soleils qui donnent une capacité utilisent désormais le modèle original
`unlockshine.bmd` et une couleur changeante. Le modèle a été extrait localement
du `common` de l'ISO patché de l'utilisateur dans
`rom/randomizer-beta-0.3.6/unlockshine.bmd` (18 176 octets, SHA-256
`21b682f5b4d79883f3c9adc5088c17b7802e8902ef39ef1042fd88c56b21b6ca`).
Ce fichier reste dans `rom/`, ignoré par Git, sans redistribution dans les sources.
Le code vérifie son en-tête et sa taille avant chargement, puis copie la ressource
dans le tas du jeu avec l'alignement requis avant conversion de son endianness.

Le calcul HSL du mod original est repris avec saturation 1 et luminosité 0,55.
1 025 comparaisons RGB correspondent aux instructions PowerPC originales. Le
cycle de teinte utilise le temps écoulé (0,15 tour par seconde), et non une
accélération de la simulation du jeu. Les Soleils ordinaires et les modèles
vides des Soleils déjà récupérés gardent leur comportement habituel.

Pendant l'animation de récupération, le HUD natif affiche un message de
déblocage dans la langue du jeu. La fiche F1 → Mods → Randomizer conserve une
liste des capacités obtenues consultable après l'animation.

### Langue des textes du Randomizer

Les cinq langues du port PAL sont couvertes : anglais, allemand, français,
espagnol et italien. `localization.h` contient les 23 noms de capacités et les
28 textes de la fiche Randomizer dans chaque langue : description, crédits,
statut, boutons, aide, erreurs et message de récompense.

Le choix suit l'option réelle `0xA0001` de `TFlagManager`, y compris les options
chargées depuis la carte et les changements de langue dans F1. Le réglage
`SMS_LANGUAGE` fournit la langue avant l'initialisation des options du jeu.
Une valeur de langue inconnue utilise l'anglais. Les textes RmlUi restent en
UTF-8 ; le message du HUD est converti en texte occidental simple octet pour
`standard_fontEx.bfn` (encodage 0, vérifié dans les archives PAL). Cette conversion
préserve les accents et contrôle la taille du tampon.

Les tests vérifient les 140 libellés de menu, les 115 messages de capacités,
les changements de langue en cours d'exécution, le repli en anglais et les
tampons trop petits. Les noms propres, les liens et les noms de fichiers des
lanceurs restent identiques. Les autres rubriques du menu partagé ne sont pas
couvertes par cette traduction du Randomizer.

Cinq démarrages Aurora séparés avec les archives PAL ont également confirmé
les indices de langue réels au chargement de Mario : `en=0`, `de=1`, `fr=2`,
`es=3`, `it=4`. Ces essais utilisent des profils isolés et ne modifient pas les
options ou la progression utilisateur. Le placement visuel du texte traduit
pendant une récupération de Soleil reste à contrôler dans chaque langue.

Dans la partie interactive du 27 octobre, le journal a confirmé la collecte du
Soleil 2, le passage du masque `000400` à `080400`, puis le retour à Delfino avec
J.E.T. disponible. Le secteur 2 de la carte enregistrée contient bien ce Soleil.
Cela confirme la collecte et son enregistrement ; la reprise après redémarrage
n'est pas encore validée. Une seed 2 indépendante a ensuite chargé le modèle
original et créé les Soleils de récompense 43 et 49, avec 600 frames de gameplay
sans erreur fatale. Le rendu des couleurs et du texte reste à vérifier visuellement.

## Fidélité au générateur original : comparaison complète

La cible de fidélité est désormais `platform/mods/randomizer/classic.h`, qui
traduit la routine de génération originale `0xcc` de la bêta 0.3.6. Elle conserve
l'ordre des listes, les doublons de candidats, les tirages MSL, les seuils de
Soleils et l'héritage des restrictions entre niveaux. Elle conserve aussi le
chevauchement des derniers octets de trois tableaux du module : le corriger
changerait les seeds après les sept Anti-Mario.

La comparaison passe sur 64 seeds avec capacités progressives et sur huit seeds
avec l'option originale « Start with all unlocks ». Pour chaque cas, les
110 routes dans leur ordre original et les 120 cases de récompenses sont
identiques. Les tests comprennent la seed zéro et les limites 32 bits.
Trois empreintes de référence permettent aussi une régression C++ autonome ;
quatre listes d'archives invalides sont refusées.

Les archives PAL réellement énumérées sont au nombre de 111 : `0c,01` est une
archive supplémentaire sans contrainte du Randomizer. Huit comparaisons
supplémentaires avec cette liste correspondent également au code original.
La capacité originale de 128 entrées reste contrôlée ; les 110 routes et les
récompenses restent identiques.

L'oracle `tests/original_seed.py` exécute le code PowerPC original et le RNG du
disque américain. La liste ordonnée de 110 archives est fournie comme fixture ;
les allocations et les services de journalisation sont simulés. Le générateur
et ses opérations sur les listes restent ceux du module. Cette validation ne
couvre pas les scripts de missions, les hooks de collecte, le rendu ou la carte
mémoire. Elle ne prouve donc pas une fidélité complète du mod en jeu.

Le runtime l'utilise dans le mode `classic-test` décrit plus haut. La préversion
normale conserve son générateur antérieur et son profil indépendant.

## Prototype de placement progressif distinct

Le prototype antérieur `platform/mods/randomizer/progression.h` construit un placement natif des
22 capacités restantes, avec le masque initial `0x400`. Les récompenses sont
placées sur des Soleils accessibles, sans doublon de capacité. Le parcours
recalcule les capacités à chaque collecte et contrôle l'accès aux sept Anti-Mario.
Ce prototype ne reproduit pas les seeds console ; il ne constitue plus le
générateur cible du port fidèle.

Les tests passent sur 256 seeds avec les 102 scènes de la table et sur 256 seeds
avec les 99 scènes constituées des destinations du manifeste PAL et du retour
fixe à la place Delfino. Ils contrôlent la reproductibilité, les 22 récompenses,
la progression logique et le recalcul incrémental depuis les drapeaux des Soleils.
Cela ne valide pas encore les scripts réels des missions ou une campagne jouée.

`actions.h` porte les décisions de restriction de la routine originale `0x5704` :
sauts, prise d'objet, glissade murale, attaque au sol, Blooper et escalade.
45 066 cas correspondent aux instructions PowerPC originales, y compris le
contournement des restrictions pour les acteurs autres que Mario. Le test isole
les restrictions ; les appels ordinaires du moteur sont simulés.

`nozzles.h` porte également les autorisations des six buses et du bouton de
changement de buse. 768 cas correspondent aux deux routines originales avant
leurs mises à jour physiques et visuelles. Les identifiants de buse hors limites
sont refusés par le code natif.

Les décisions de mouvements et de buses sont raccordées au mode de test. La
réflexion murale, les autres contrôles de caméra, certaines interactions Yoshi
et les contrôles indirects restent à examiner. Le prototype de placement
progressif décrit au début de cette section reste séparé du mode de test,
qui utilise le générateur classique.

## Test du 27 octobre 2026

- Le SHA-256 du patch téléchargé correspond à celui indiqué par GitHub :
  `3d0dad279db6063b4f5e36c65f9edeb7c0a58711113d5291bab9849f5013d81d`.
- La copie américaine RVZ a été convertie en ISO puis le patch appliqué par le
  décodeur VCDIFF de CubeShelf, avec vérification des sommes des blocs. Le disque
  obtenu porte l'identifiant `GMSE67`, contre `GMSE01` pour le disque d'origine.
- Le `main.dol` a changé : son SHA-256 passe de
  `13934c863d649b1ddca1ca4d7748f49d28a571685cbee5fb1542545c32869955` à
  `0689fae19d31ff88992201c8e85413aac60c28c2dde3d61de75ed4ae14ba683e`.
- Un lancement du port Windows natif avec ce disque et une carte isolée atteint
  l'initialisation graphique et audio. Le test a été arrêté après 30 secondes ;
  aucune entrée de niveau ni fonctionnalité du randomizer n'a été validée.
- Un lancement séparé dans Dolphin avec un répertoire utilisateur de test affiche
  l'écran titre « Super Mario Sunshine Randomizer », puis une scène de plateforme
  avec Mario et son HUD. La génération et la progression d'une seed restent à
  vérifier ; cette observation ne valide pas le port natif.

## Portage commencé

L'analyse du disque a identifié deux modules : `Randomizer.kxe` (73 920 octets)
et `BetterSunshineEngine.kxe` (575 648 octets), ainsi qu'une table `levels.rlv`.
L'utilisation de cette dernière par la version publiée reste à confirmer.

`tools/mods/inspect_kxe.py` lit les sections et les relocalisations KXER v0 et
résout les appels vers la carte de symboles américaine existante. Le module
Randomizer contient 50 080 octets dans sa section code/données, 1 457
relocalisations, 239 sites d'appel vers 57 fonctions du jeu et cinq imports BSE.
Tous ces appels vers le jeu ont été résolus dans la carte américaine. Les
fonctions appelées ne sont pas nécessairement les points modifiés par le mod.

Les imports BSE sont l'indicateur de chargement, les callbacks de mise à jour
du niveau et de fin de chargement du joueur, le nom du groupe de réglages et
l'enregistrement du module. La logique appelle aussi la sauvegarde des flags,
les changements de niveau et les états de Mario.

Une première règle a été traduite en C++ natif dans
`platform/mods/randomizer/nozzles.h`, à partir des instructions du callback
joueur aux offsets module `0x6b64..0x6bf8` : sélectionner la buse de secours
(priorité Hover, Rocket, Turbo), équiper Spray si disponible et retirer J.E.T.
si aucune buse n'est débloquée. Les 16 combinaisons passent dans un exécutable
de test Windows x64. Cette règle n'est pas encore raccordée au jeu.

Les recherches de transitions (`transitions.h`) et la restauration des capacités
par les Soleils collectés (`rewards.h`) sont maintenant traduites. Le test
`platform/mods/randomizer/tests/compare_original.py` exécute directement les
instructions PowerPC du module officiel avec Unicorn et les compare à deux
exécutables C++ x64. Les 512 comparaisons passent : 384 recherches sur des tables
vides, simples ou de 128 entrées (doublons et destinations absentes compris),
puis 128 restaurations couvrant chacun des 120 Soleils et plusieurs collectes.
Le test borne l'exécution et vérifie le retour effectif de chaque fonction.
Pour la restauration, seul l'accès aux flags de Soleils est simulé ; la boucle
et l'accumulation des capacités sont celles du module original.

Ces composants du générateur console ne génèrent pas encore une seed complète.
La préversion d'entrées décrite plus haut utilise un générateur natif séparé.

La table native `level_rules.inc` contient désormais les 207 contraintes de
101 scènes produites par la fonction originale `0x1fd4`. La comparaison
`tests/compare_rules.py` couvre exhaustivement les 65 536 combinaisons possibles
des deux octets niveau/épisode. Les listes, leur ordre, leurs flags et leurs
masques de capacités concordent exactement avec la sortie PowerPC. Seuls
l'allocation et la libération de mémoire sont fournis par le banc de test.
Ces règles sont utilisées par le générateur de la préversion ; leur traduction
ne remplace pas encore tout le placement progressif du générateur console.

Les étapes suivantes du générateur sont également traduites :

- `access.h` transforme les capacités débloquées en conditions d'accès, en tenant
  compte des sept combats contre Anti-Mario : 30 779 comparaisons PowerPC passent.
- `candidates.h` retrouve les capacités à placer pour débloquer un accès :
  8 264 comparaisons passent. L'ordre et les doublons sont conservés, car ils
  influencent le tirage de la seed.
- `eligibility.h` décide si un niveau possède un Soleil ou une sortie accessible,
  selon les filtres du générateur : 9 792 comparaisons passent.
- `start.h` sélectionne une première destination dans les scènes fournies et
  réutilise les fonctions MSL du port. Trois listes de destinations (capacités
  vides, initiales ou complètes) et 768 premiers tirages concordent avec le module
  et les instructions `srand`/`rand` du disque américain original. Le test utilise
  les 101 scènes ayant des contraintes ; la liste réelle des archives du jeu
  devra être fournie lors du raccordement. Une liste sans destination valide
  renvoie une erreur vide au lieu de tirer modulo zéro.

Ce premier tirage ne reproduit pas une seed console complète. Le placement
progressif natif décrit plus haut est validé séparément ; la traduction du
générateur console complet et son raccordement restent à terminer.

Le menu PartyBoard partagé affiche le logo Sunshine fourni par l'utilisateur,
une fiche Eclipse (JoshuaMK, CyrusLoS, Eclipse Team) et une fiche Randomizer
(TheAzack9). La fiche Randomizer distingue la préversion jouable du portage
encore incomplet des capacités progressives.

Trois tests Python de l'inventaire passent également, dont les fichiers tronqués
et les références hors limites. Les binaires extraits et le désassemblage sont
locaux dans `artifacts/randomizer-research`, sans redistribution dans les sources.

## Travail restant

Le disque modifié contient du code GameCube ; le port exécute son propre code
compilé et ne reprend pas automatiquement les changements du `main.dol`. Les
fichiers du disque seuls ne prouvent donc pas que le randomizer fonctionne.
La traduction depuis le module est possible, mais l'ensemble du gameplay n'est
pas encore porté. Les étapes restantes sont :

1. Étendre les essais du mode classique raccordé : plusieurs entrées, collecte
   effective, sauvegarde et reprise après redémarrage.
2. Compléter les redirections du mod et raccorder les restrictions de mouvements
   déjà comparées, avec les autres points de contrôle des capacités.
3. Adapter les récompenses, les conditions de victoire, le HUD et les sauvegardes.
4. Étendre les hooks, l'isolation des sauvegardes et les réglages déjà présents
   dans la préversion pour accueillir le générateur classique traduit.
5. Vérifier plusieurs seeds jusqu'à leur objectif final.

Le launcher active le mode classique expérimental. L'ancienne préversion
d'entrées reste reconnue pour les profils existants.

La fidélité complète sera validée par les mêmes entrées, récompenses et
verrouillages pour une seed donnée, l'affichage des récompenses du mod, ses
objectifs de victoire et ses options, puis une collecte et une reprise après
fermeture du jeu. Les transitions spéciales, les accès à Corona et les scripts
des missions doivent être contrôlés dans le moteur natif. Les comparaisons
isolées du générateur ne remplacent pas ces essais.

## Rejouer les comparaisons

Compiler séparément les tests C++17 `tests/nozzles_test.cpp`,
`tests/transitions_test.cpp`, `tests/transitions_probe.cpp` et
`tests/rewards_probe.cpp` du dossier `platform/mods/randomizer`.
Exécuter les deux premiers, puis :

```text
python platform/mods/randomizer/tests/compare_original.py Randomizer.kxe transitions_probe.exe rewards_probe.exe
```

Compiler également `tests/rules_probe.cpp`, puis comparer la table de contraintes :

```text
python platform/mods/randomizer/tests/compare_rules.py Randomizer.kxe rules_probe.exe
```

Compiler `tests/access_probe.cpp`, `tests/candidates_probe.cpp`,
`tests/eligibility_probe.cpp` et `tests/start_probe.cpp`. Ce dernier se lie avec
`platform/misc/msl_rand.c`, compilé en C. Comparer ensuite :

```text
python platform/mods/randomizer/tests/compare_access.py Randomizer.kxe access_probe.exe
python platform/mods/randomizer/tests/compare_candidates.py Randomizer.kxe candidates_probe.exe eligibility_probe.exe rules_probe.exe
python platform/mods/randomizer/tests/compare_start.py Randomizer.kxe start_probe.exe rules_probe.exe Sunshine-US.iso
```

Compiler `tests/actions_probe.cpp` et `tests/nozzle_gate_probe.cpp`, puis :

```text
python platform/mods/randomizer/tests/compare_actions.py Randomizer.kxe actions_probe.exe
python platform/mods/randomizer/tests/compare_nozzle_gates.py Randomizer.kxe nozzle_gate_probe.exe
```

Compiler `tests/progression_test.cpp` avec `platform/misc/msl_rand.c` compilé en C.
L'exécuter sans argument teste la table complète. Le chemin optionnel d'un
manifeste `entrances.txt` issu d'une session native limite le test à ses scènes :

```text
progression_test.exe saves/randomizer-native-v1/1/entrances.txt
```

Compiler `tests/classic_probe.cpp` et `tests/classic_test.cpp` avec le RNG MSL,
puis lancer le test autonome et la comparaison complète :

```text
classic_test.exe
python platform/mods/randomizer/tests/compare_classic.py Randomizer.kxe Sunshine-US.iso rules_probe.exe classic_probe.exe
```

Compiler `tests/runtime_test.cpp` et `tests/classic_runtime_test.cpp` avec
`platform/mods/randomizer/runtime.cpp` et le RNG MSL. Exécuter ensuite :

```text
python platform/mods/randomizer/tests/runtime_suite.py runtime_test.exe classic_runtime_test.exe NOUVEAU_DOSSIER_DE_TEST
```

Ce dossier doit être nouveau. Il conserve ses petits profils et son rapport
`results.json` pour inspection, et ne modifie pas les sauvegardes utilisateur.

Compiler `tests/presentation_probe.cpp`, puis comparer les couleurs :

```text
python platform/mods/randomizer/tests/compare_presentation.py Randomizer.kxe presentation_probe.exe
```

Compiler et exécuter `tests/localization_test.cpp` en C++17 pour vérifier les
cinq catalogues, le format du message et son encodage pour la police PAL.

Unicorn 2.1.4 est utilisé uniquement pour ce test de comparaison. Il n'est pas
une dépendance du jeu. Le module original doit être extrait de la copie patchée
de l'utilisateur ; son SHA-256 exact est vérifié avant toute exécution de test.
