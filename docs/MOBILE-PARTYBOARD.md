# Menu PartyBoard sur Android

Le port Android rÃ©utilise le menu RmlUi de Sunshine, ses fenÃªtres, onglets,
panneaux et styles provenant du port Mario Party 4 de l'utilisateur :
https://github.com/zeranemesis/Marioparty4.
La rÃ©fÃ©rence GitHub consultÃ©e est `audio-local`, rÃ©vision
`5ee7005440bc642f2121965f3763a9d02bd8cf4d`.
Les crÃ©dits des composants et polices restent dans `res/partyboard/CREDITS.md`.
La police propriÃ©taire optionnelle NewRodin n'est pas incluse dans l'APK ;
Inter est enregistrÃ©e comme police de remplacement pour les mÃªmes styles.

Il ne s'agit plus d'un menu de rÃ©glages Java sÃ©parÃ©. Android emploie les mÃªmes
composants RmlUi, la mÃªme interface Aurora et le mÃªme fichier `settings.txt`
que les rÃ©glages PC de Sunshine. Le pont et le menu utilisent exactement la
mÃªme version ARM64 de RmlUi ; SDL3 remplace la dÃ©pendance SDL2 du frontend PC.

## Utilisation

- Depuis l'accueil, Â« RÃ©glages, commandes, sauvegardes et mods Â» dÃ©marre le
  moteur avec le menu ouvert.
- En jeu, **MENU** ou Retour Android ouvre/ferme ce menu. Le jeu et son audio
  sont suspendus ; le tactile du jeu est masquÃ© et les touches relÃ¢chÃ©es.
- Six onglets utiles sur tÃ©lÃ©phone : vidÃ©o/graphismes, audio, manettes,
  jeu, sauvegardes et mods. Les options de fenÃªtre/clavier de bureau et les
  mises Ã  jour Windows ne sont pas proposÃ©es.
- Â« Enregistrer les rÃ©glages Â» conserve les choix ; Â« Reprendre Â» enregistre
  Ã©galement et revient au jeu. Les rÃ©glages applicables en direct utilisent
  les callbacks existants du moteur. La synchronisation verticale est appliquÃ©e
  au prochain lancement : recrÃ©er la surface pendant le clic causait un ANR
  sur le Samsung de test.
- Le tactile reprend les modes automatique/toujours/jamais de Mario Party 4,
  avec opacitÃ© et taille. En automatique, une manette connectÃ©e le masque.
  Sunshine conserve son stick camÃ©ra et les deux pressions J.E.T.

## Proportions et rÃ©solution

Sans format imposÃ©, Android transmet le rapport rÃ©el de sa surface Ã  la
camÃ©ra et au correctif GX grand Ã©cran. La prÃ©sentation conserve les proportions
et les menus d'origine utilisent les transformations 2D existantes.
La rÃ©solution par dÃ©faut reste celle de la surface (`resolution = 0`) ;
corriger le format ne force pas une rÃ©solution GameCube infÃ©rieure.
`widescreen = off` et les formats fixes restent disponibles.

Le tactile du menu est calculÃ© dans la surface d'interface entiÃ¨re. Avant le
premier XFB, utiliser le viewport 4:3 du jeu dÃ©calait les boutons du menu.

## Validation du 24 octobre 2026

APK installÃ© sur Samsung **SM-G985F**, Android 13, ARM64, Vulkan Mali.
VÃ©rifications sur l'appareil : affichage des onglets, navigation tactile vers
Audio, Ã©criture du rÃ©glage audio dans `settings.txt`, message Â« RÃ©glages
enregistrÃ©s Â», reprise de l'introduction et rÃ©ouverture avec MENU.
La carte crÃ©Ã©e au cours du test est conservÃ©e ; aucune sauvegarde PC n'a Ã©tÃ©
copiÃ©e. Les donnÃ©es PAL du joueur sont dans le stockage propre Ã  l'application.

La restauration de carte, le chargement des mods et une campagne complÃ¨te ne
sont pas encore validÃ©s sur ce tÃ©lÃ©phone. Ces limites ne sont pas couvertes
par le test de navigation du menu.

### Sauvegardes Android : raccordement et test

Les services de copie, liste et sélection différée de carte disposent désormais
 d’une implémentation hors Windows. Les copies excluent liens symboliques,
 sous-dossiers et fichiers étrangers à la carte ; chaque opération reçoit un
 dossier distinct. La restauration conserve la carte initiale.

Test ARM64 exécuté directement sur le Samsung : deux copies distinctes,
 énumération, exclusion des liens, refus d’un chemin avec remontée, restauration
 différée et sélection persistante au démarrage suivant : PASS. Source du test :
 `tools/mobile/card_services_test.cpp` (exécutable Android autonome).

Le bouton « Créer une copie de la carte » du menu installé a également créé
 les fichiers index, dat et stat et le marqueur de fin dans le stockage privé.
 La restauration via toute la séquence du menu et le chargement des mods restent
 à vérifier. APK reconstruit et installé en conservant les données existantes.

### Validation en niveau et ergonomie tactile

Restauration effectuée depuis le menu, puis redémarrage : le journal CARD
confirme le montage de la nouvelle carte. La carte initiale est conservée.
Le mod de validation contenant une copie identique de PerformLists.bin a été
activé dans le menu ; le journal DVD confirme un fichier remplacé. Le mod a
ensuite été désactivé. Cela valide le montage de fichiers, pas les mods de code.

Le diagnostic Android ouvre maintenant Bianco Hills (2,0), sans modifier la
cadence ni les réglages de cinématiques du lancement normal. Entrée en niveau,
affichage du HUD et déplacement tactile vérifiés sur Samsung SM-G985F.
Résolution native conservée. Mesures de cette courte session : médianes par
fenêtre de 33,5 à 44 ms, p90 jusqu’à 71,1 ms et blocage initial de 2236,3 ms.
Le niveau est accessible ; la fluidité stable et la campagne restent à valider.

Le glissement tactile fait désormais défiler les panneaux, sans clic sur la
ligne au départ du geste. Testé dans la liste des mods, avec activation puis
désactivation par toucher. Les cartes de mods affichent titre et état sur des
lignes distinctes. La création de copie actualise aussi la liste en mémoire.

## Fluidite du 25 octobre 2026

Optimisations et comparaisons reproductibles : voir `MOBILE-PERFORMANCES.md`.
Le paquet ARM64 optimise comprend le correctif FIFO et un seed de 417 pipelines.
Le benchmark Bianco Hills conserve la resolution native et la cadence originale.
