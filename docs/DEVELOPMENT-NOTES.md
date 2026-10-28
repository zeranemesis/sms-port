# Note de développement — v0.4.0-rc.1 — 28 octobre 2026

Cette préversion apporte le Randomizer classique au runtime PC PAL Aurora et
au menu natif. Elle ne constitue pas encore un port complet du mod original.

## Changements

- F1 → Mods → Super Mario Sunshine Randomizer : seed, activation ou
  désactivation au prochain démarrage, et liste des capacités débloquées.
- Profils séparés par seed, conservation des anciens profils `classic-test`,
  validation des manifestes et protection de la carte du jeu normal.
- Générateur et progression issus de l'analyse du mod de TheAzack9 bêta 0.3.6.
  Contrôles natifs des actions de Mario et des buses J.E.T., restauration des
  capacités à partir des Soleils récupérés et message de récompense.
- Textes du Randomizer en français, anglais, allemand, espagnol et italien,
  suivant la langue effective du jeu. Les autres textes du menu ne sont pas
  tous traduits par cette modification.
- Logo Sunshine fourni par l'utilisateur et crédits des créateurs de mods.
- Le paquet public exclut la police locale propriétaire NewRodin et utilise
  Inter en repli.

## Validation effectuée

Compilation Windows x64 Aurora et contrôle des différences Git ; 19 cas de
runtime ; quatre démarrages lisant les réglages enregistrés, avec alternance
jeu normal/Randomizer et changement de seed ; 140 textes de menu et 115
messages HUD ; comparaison du générateur sur 72 cas, plus huit cas PAL ;
1 025 comparaisons du calcul des couleurs avec le module PowerPC original.
Ces comparaisons vérifient les composants concernés, pas une campagne entière.

## Limites et suite du développement

Le Randomizer natif reste réservé au Sunshine standard sur PC. Il refuse
Android, Eclipse et Archipelago. Des interactions, scripts de missions,
objectifs/options et éléments HUD restent à porter ou à valider. Une campagne
complète et le cycle sauvegarde/rechargement en jeu restent à vérifier.
La dernière présentation visuelle du HUD et les clics du menu n'ont pas pu
être contrôlés avec l'outil d'interface ; les démarrages et journaux ont été
contrôlés.

Le modèle spécial `unlockshine.bmd` n'est pas distribué : il doit provenir
de la copie patchée de l'utilisateur, à l'emplacement et avec l'empreinte
décrits dans [le guide Randomizer](RANDOMIZER-CLASSIQUE.md). Sans ce fichier,
le jeu conserve le modèle de Soleil normal ; le message et la liste des
capacités restent disponibles. Aucun disque ni fichier extrait du jeu n'est
inclus dans le paquet.

Les téléchargements Eclipse et Android de cette release sont les fichiers
inchangés de `cubeshelf-v0.3.3`, fournis pour conserver l'accès aux autres
plateformes. Ils ne contiennent pas ce nouveau port natif du Randomizer.

Crédits : TheAzack9 (SMS Randomizer), contributeurs BetterSunshineEngine ;
JoshuaMK, CyrusLoS et Eclipse Team (Super Mario Eclipse).
