# DolphinJet et port PAL natif

Le frontend `C:/sms-aurora-progress/build-msvc/dolphinjet.exe` possède un bouton de lancement PAL dans ses réglages de démarrage.
Il lance `C:/sms-pal-port/build/windows-64-pal/sms.exe` avec ses réglages de langue et de vidéo, dans un environnement enfant indépendant.
Le moteur natif reste dans son propre processus SDL2/OpenGL.
Le menu du jeu utilise désormais RmlUi SDL2/OpenGL3, les feuilles de style originales et les polices du menu PartyBoard.
Ses onglets horizontaux, panneaux d'options et navigation sont raccordés aux réglages natifs de Sunshine.
Le branchement ne transporte pas le moteur natif dans le moteur recompilé de DolphinJet.

## Fonctionnalités raccordées

| Fonction | Comportement |
| --- | --- |
| Menu en partie | F1 ou bouton Guide ; reprise avec F1, Échap ou Reprendre |
| Pause | Thread du jeu arrêté à la présentation ; image précédente conservée ; son de sortie coupé pendant le menu |
| Fenêtre | Vsync, plein écran, moniteur, taille et capture souris appliqués à la reprise |
| Audio | Gain général appliqué à la reprise ; réactivation d'un périphérique désactivé au démarrage exige un redémarrage |
| Commandes | Clavier réassignable ; bindings et options caméra rechargés à la reprise ; manettes SDL reconnues à chaud |
| Graphismes | Réglages existants conservés ; résolution interne et autres options de rendu prennent effet au prochain lancement |
| Langue | Langue PAL EN/DE/FR/ES/IT persistée ; redémarrage pour la langue du jeu ; principaux libellés du menu EN/FR |
| Sauvegardes | Copie de la carte ; restauration dans une nouvelle carte au prochain lancement ; ancienne carte conservée |
| Mods | Sélection et packs du launcher natif ; modifications activées au prochain lancement |
| Succès | Six succès locaux issus des compteurs de Soleils et pièces bleues du jeu chargé |
| Mises à jour | Recherche release PAL x64, téléchargement SHA256, installation différée après fermeture avec sauvegarde des anciens binaires |

Le lancement force `SMS_NET_MODE=off`.
Le nouveau menu ne propose pas de mode en ligne.

## Distribution des mises à jour

Publier une archive ZIP Windows 64 PAL dans les releases `zeranemesis/sms-port`, avec le digest SHA256 GitHub.
Le ZIP doit contenir un seul `sms.exe` et son `sms-build.json` GMSP01/x64 généré par CMake.
Les binaires et DLL placés à côté sont mis à jour ; les autres données restent intactes.
Une publication sans ces éléments est refusée.
Les détails et chemins des journaux figurent dans `PC_SERVICES.md`.

## État de validation

La compilation DolphinJet a réussi le 7 octobre 2026.
Le journal est `C:/sms-aurora-progress/build-msvc/native-pal-frontend-build.log`.
La compilation PAL a réussi le 7 octobre 2026 après intégration du menu et des services.
Le journal de la dernière recompilation est `C:/sms-pal-port/build/windows-64-pal/dolphinjet-pal-build.log`.
Aucun lancement du jeu ni contrôle visuel n'a été effectué dans ce lot.
La stabilité visuelle, la reprise du menu et les opérations de sauvegarde/mise à jour doivent encore être observées en exécution avant de les qualifier de validées.
