# Sunshine Aurora — v0.4.0-rc.1

Préversion du Randomizer classique intégré au menu natif PC PAL.

- **F1 → Mods → Super Mario Sunshine Randomizer** : choisir une seed et activer ou désactiver le mod. Fermer puis relancer le jeu pour appliquer le choix.
- Sauvegardes séparées par seed ; anciens profils classiques conservés ; retour au jeu normal avec sa carte habituelle.
- Générateur original, premiers contrôles de progression de Mario et de J.E.T., annonce de capacité et liste des déblocages dans le menu.
- Textes du Randomizer suivant la langue du jeu : français, anglais, allemand, espagnol et italien.
- Menu Sunshine avec logo et crédits : TheAzack9, contributeurs BetterSunshineEngine ; JoshuaMK, CyrusLoS et Eclipse Team pour Eclipse.

**Le portage du Randomizer reste expérimental et incomplet.** Les scripts, interactions et objectifs restants, ainsi qu'une campagne complète, doivent encore être validés. Il est réservé au Sunshine standard PC, sans combinaison avec Eclipse, Android ou Archipelago. La référence analysée est le mod original bêta 0.3.6.

Le modèle spécial des Soleils de capacité doit être fourni depuis la copie patchée de l'utilisateur (`rom/randomizer-beta-0.3.6/unlockshine.bmd`). Il n'est pas distribué. Sans lui, le modèle normal reste affiché ; le message de capacité et la liste restent disponibles. Voir le guide inclus dans le paquet.

## Téléchargements

- `sms-pal-win-x64.zip` : **nouveau** runtime PC PAL Aurora. Extraire puis lancer `sms.exe`, ou utiliser CubeShelf. F1 ouvre les réglages et les mods. Une copie personnelle européenne GMSP01 du jeu est nécessaire.
- `sms-eclipse-win-x64.zip` : runtime Eclipse **inchangé**, repris à l'identique de v0.3.3.
- `Sunshine-Aurora-arm64-v8a.apk` : application Android ARM64 **inchangée**, reprise à l'identique de v0.3.3. Elle ne contient pas le nouveau Randomizer natif PC.
- `checksums.txt` : empreintes SHA-256 des trois téléchargements.

Aucun disque ni fichier extrait du jeu n'est distribué. La police propriétaire locale NewRodin est exclue du nouveau paquet PC ; Inter assure le repli.

## Note de développement

La [note de développement](https://github.com/zeranemesis/sms-port/blob/cubeshelf-v0.4.0-rc.1/docs/DEVELOPMENT-NOTES.md) détaille les changements, validations et limites. Elle est aussi incluse dans le paquet PC avec le guide Randomizer.

Validation : compilation Aurora Windows x64, 19 cas de runtime, quatre démarrages depuis les réglages enregistrés, 255 textes/messages traduits, 72 cas du générateur original plus huit cas PAL, et 1 025 comparaisons de couleurs. La présentation finale du HUD et les clics du menu ne sont pas visuellement confirmés.
