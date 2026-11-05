# Note de développement — v0.4.0-rc.2 — 5 novembre 2026

Cette release reconstruit l'application Android ARM64 depuis les sources
actuelles. L'APK annonce `versionName=0.4.0-rc.2` et `versionCode=2`, avec le
même identifiant `org.smsunified.aurora` et la même clé de développement locale
que les versions précédentes pour permettre une mise à jour sur place.

Le jeu Android utilise Aurora/Vulkan et le menu PartyBoard partagé, avec import
du disque PAL personnel, commandes tactiles et manettes. Le Randomizer natif
reste réservé au PC ; la reconstruction Android n'en active pas le portage.
Le guide `tools/mobile/README.md` décrit maintenant la construction et
l'installation de l'APK du jeu, avant l'historique des diagnostics.

Validation du 5 novembre : compilation ARM64 du runtime complet ; packaging
des 49 bibliothèques natives ; signature APK et alignement de 16 Kio contrôlés ;
certificat identique à celui de l'APK précédent ; installation et démarrage
du launcher réussis sur l'AYN Thor connecté. L'application annonce bien
`versionCode=2` et `versionName=0.4.0-rc.2`. Aucun disque n'a été importé pendant
ce contrôle : le démarrage du moteur et une partie complète n'ont pas été
retestés sur cet appareil pour cette release.

Les binaires PC Sunshine sont réemballés avec cette note et le guide actuel,
sans nouvelle modification du moteur PC. Le runtime Eclipse reste celui de
v0.3.3. Aucun disque ni donnée extraite du jeu n'est distribué.

## Historique : v0.4.0-rc.1 — 28 octobre 2026

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
