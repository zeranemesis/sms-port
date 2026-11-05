# Sunshine Aurora — v0.4.0-rc.2 — Android ARM64

Nouvel APK Android reconstruit depuis les sources actuelles, avec le runtime Aurora/Vulkan et le menu PartyBoard partagé.

## Android

- `Sunshine-Aurora-arm64-v8a.apk` : nouvelle compilation ARM64, version `0.4.0-rc.2`, code Android `2`.
- Même identifiant et même certificat que l'APK précédent : installation possible par-dessus l'ancienne application, sans désinstallation.
- Android 9/API 28 minimum, appareil ARM64 avec Vulkan. Importer sa propre copie européenne PAL GMSP01 en ISO/GCM depuis l'application, puis choisir « Jouer ».
- Le bouton MENU donne accès aux réglages ; commandes tactiles et manettes Android sont intégrées.
- APK de développement signé, distribué en préversion. Aucun disque ni donnée extraite du jeu n'est inclus.

Validation : compilation du runtime ARM64, contrôle des 49 bibliothèques natives, signature et alignement APK ; installation et démarrage du launcher réussis sur AYN Thor. Aucun disque n'a été importé pendant ce contrôle : le moteur en partie et les performances ne sont pas revalidés par ce test.

## PC et autres téléchargements

- `sms-pal-win-x64.zip` : runtime PC PAL Aurora, réemballé avec la nouvelle documentation. Le code du moteur PC est inchangé depuis rc.1.
- `sms-eclipse-win-x64.zip` : fichier Eclipse inchangé, repris à l'identique de v0.3.3.
- `DEVELOPMENT-NOTES.md` : note de développement, validations et limites.
- `checksums.txt` et `Sunshine-Aurora-arm64-v8a.apk.sha256` : empreintes SHA-256 pour vérifier les téléchargements.

Le Randomizer classique natif reste expérimental et réservé à Sunshine standard sur PC. Il s'active dans F1 → Mods, puis au redémarrage. Il n'est pas pris en charge sur Android, Eclipse ou Archipelago. Sa campagne complète et certains scripts/interactions restent à valider ; le modèle spécial `unlockshine.bmd`, issu de la copie patchée personnelle, n'est pas distribué.

Crédits : TheAzack9 (SMS Randomizer), contributeurs BetterSunshineEngine ; JoshuaMK, CyrusLoS et Eclipse Team (Super Mario Eclipse).

[Note de développement](https://github.com/zeranemesis/sms-port/blob/cubeshelf-v0.4.0-rc.2/docs/DEVELOPMENT-NOTES.md) · [Guide Android et construction](https://github.com/zeranemesis/sms-port/blob/cubeshelf-v0.4.0-rc.2/tools/mobile/README.md)
