# Services du frontend PAL

`pc_services.cpp` expose les sauvegardes, les succès locaux et le téléchargement de mises à jour.
Les appels réseau sont effectués sur un thread séparé.
Les DLL Windows sont résolues dynamiquement ; aucune dépendance de lien supplémentaire n'est requise.

## Sauvegardes

Appeler `activate_pending_save_restore()` avant toute initialisation CARD.
Appeler `backup_saves()` seulement lorsque le thread du jeu est arrêté à une frontière d'image : les écritures CARD sont synchrones, mais aucune exclusion mutuelle n'existe dans le pilote CARD.
Le service copie le fichier d'index et tous les fichiers `.dat` et `.stat` ordinaires.
Il ignore les liens, les sous-dossiers et les autres fichiers.
Une sauvegarde n'est affichée dans la liste qu'après écriture de son marqueur de finalisation.
`prepare_save_restore()` copie la sauvegarde vers une nouvelle carte et prépare sa sélection au prochain démarrage.
La carte précédente reste intacte.
La carte restaurée est sélectionnée durablement via `SMS_SAVE_DIR` ; aucun rechargement de la carte en cours de partie n'est effectué.

## Succès

Appeler `observe_game_progress()` depuis le thread du jeu avec `TFlagManager::getFlag(0x40000)` et `getFlag(0x40001)` après chargement du fichier.
Ces compteurs sont recalculés par `TFlagManager::correctFlag()` à partir des drapeaux de collecte.
Les six succès couvrent les paliers 1, 10, 50 et 120 Soleils ainsi que 1 et 240 pièces bleues.
Charger une ancienne sauvegarde peut débloquer ces succès locaux.
Il n'y a pas de minuterie de déblocage et ces succès ne sont pas présentés comme une intégration RetroAchievements.

## Mises à jour

`check_updates()` lit la dernière release publique de `zeranemesis/sms-port`.
Une archive doit contenir `windows`, `64` et `pal` dans son nom, se terminer par `.zip`, provenir des releases de ce dépôt, et fournir un `digest` GitHub `sha256:`.
Si le dépôt ne publie pas cet artefact, le service affiche une erreur explicite.
Le tag installé doit venir des métadonnées de compilation ; une chaîne vide signifie version installée inconnue.
La comparaison de tags égaux indique la dernière release déjà installée ; des tags différents indiquent une release disponible, pas une preuve d'ordre chronologique des versions.
`download_update()` télécharge dans un dossier distinct, vérifie le SHA-256 et conserve seulement l'archive validée.
Le digest garantit l'intégrité vis-à-vis de l'artefact publié ; il ne constitue pas une signature indépendante du dépôt.
`prepare_update_installation()` revérifie l'archive et démarre un helper PowerShell invisible qui attend la fermeture normale du processus du jeu.
L'interface doit quitter après le succès de cet appel.
Le helper n'arrête jamais lui-même le processus du jeu ; après dix minutes d'attente il annule l'installation.
Il extrait dans un dossier distinct après validation des chemins de chaque entrée ZIP, rejette les liens et borne la taille décompressée à 2 Gio.
La release doit contenir un seul `sms.exe` accompagné de `sms-build.json` déclarant `region: GMSP01` et `architecture: x64`.
Le helper contrôle les en-têtes PE AMD64/PE32+ de cet exécutable.
Les publications sans ce manifeste ne sont pas installées.
Seuls `sms.exe`, `sms-build.json` et les DLL placées à côté sont mis à jour.
Les sous-dossiers, sauvegardes, paramètres, mods et données de jeu restent intacts.
Les fichiers précédents sont copiés dans `installations/<identifiant>/rollback` avant la première modification.
Chaque remplacement utilise un fichier temporaire et le remplacement atomique Windows ; si l'installation échoue les fichiers déjà remplacés sont restaurés dans l'ordre inverse.
Un échec du rollback est consigné avec le chemin de récupération manuelle ; les copies ne sont pas supprimées.
Le journal `installation.log` conserve le résultat.
Le redémarrage après installation est optionnel et utilise le nouveau `sms.exe` depuis son dossier d'installation.
L'installation exige les droits d'écriture actuels, sans demander d'élévation.

Les fichiers du frontend utilisent `SMS_FRONTEND_DIR`, sinon `%APPDATA%/sms-port/frontend`, avec repli XDG/HOME.
Les sauvegardes utilisent exactement l'ordre des variables du pilote CARD : `SMS_SAVE_DIR`, XDG, APPDATA, HOME.
