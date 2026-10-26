# Plan suivi du port mobile

Cibles : Android d'abord, iOS ensuite. Début : 17 octobre 2026.
Ce document suit les résultats obtenus ; une étape n'est cochée qu'avec une
preuve. L'APK diagnostic n'est pas un APK du jeu.

| Étape | État | Critère de passage |
|---|---|---|
| 1. Environnement Android et diagnostic JNI | Terminé sur émulateur | APK signé installé, Activity démarrée, MEM1 64 Mio et pile basse PASS en x86_64 et ARM64 traduit. Preuve : `artifacts/android-arm64-probe/app-validation.json`. |
| 2. Pointeurs natifs SPC | Implémenté et tests isolés réussis ; scripts réels à valider avec le jeu | 133 bindings adaptés, deux dispatchs Android, cinq unités compilées pour Windows et ARM64, tests registre PASS dans l'APK ARM64 traduit. Preuves : `artifacts/android-spc/validation.json`. La compatibilité d'une séquence SPC complète reste à contrôler au lancement du jeu. |
| 3. Compilation ARM64 du jeu | Archive, plateforme, frontend GX et Aurora liés | Le binaire Android `build/android-arm64-game/sms` est lié au vrai renderer. Archive actuelle : 593 objets AArch64 et allocations renommées, preuve `artifacts/android-aurora/current-game-archive/archive-validation.json`. Le chargement du jeu dans une Activity reste à intégrer ; un exécutable lié ne clôt pas l'étape APK. |
| 4. Ressources GX et adresses statiques | Quatre ressources intégrées ; tests de transport réussis dans l'APK | Les deux NullTexData, clear_z_TX et tmp_data sont copiés une fois dans la MEM1 persistante. Le vrai GX API/FIFO passe : adresses hautes, getters, texture immédiate, record/replay texture et tableau BE. Test isolé du registre également réussi. Images/palettes supplémentaires et usage par les scènes réelles restent à valider. Contrôles PTR32 maintenus. |
| 5. Aurora sur Android | Surface Vulkan, audio et arrêt du diagnostic validés ; rendu du jeu à tester | APK SDLActivity avec 49 bibliothèques ARM64 : GX/FIFO PASS, PCM PASS, Vulkan/Lavapipe PASS, 90 images soumises et shutdown retourné. Preuve : `artifacts/android-aurora/surface-validation/validation.json`. Le shader cache est vide (0 pipeline), les pixels des scènes, copies couleur/profondeur et recréation de surface ne sont pas validés. |
| 6. APK du jeu et données | À faire | Import des données utilisateur, stockage privé, écran titre puis entrée dans un niveau, sauvegarde et audio fonctionnels. |
| 7. Commandes et cycle de vie | À faire | Manette, tactile et deux seuils J.E.T., pause/reprise, rotation et recréation de surface sans perte de sauvegarde ni blocage des threads. |
| 8. Fidélité et performances | À faire | HUD Soleil, peintures, pollutions normale/lave/électrique et combat final testés ; mesures CPU/GPU et thermique sur téléphone. Vitesse de simulation conservée ; qualité jamais réduite automatiquement pour masquer une régression. |
| 9. iOS | À faire après Android | Modèle mémoire compatible ARM64 Apple, build et signature dans environnement Apple, backend Metal, mêmes contrôles de fidélité et cycle de vie. |

## Méthode

- Code console original et middleware conservés ; adaptations dans la couche
  plateforme ou patches du miroir PC, avec activation mobile explicite.
- Builds et preuves Android séparés des binaires Windows.
- Un test isolé ne clôt pas une étape d'intégration : les tests de callbacks
  précèdent la compilation de vrais interpréteurs SPC ; le diagnostic mémoire
  précède le chargement d'Aurora et des données du jeu.
- L'émulateur actuel est x86_64 avec traduction ARM64. Ses résultats valident
  ces chemins précis, pas les performances ou pilotes d'un téléphone réel.
- À chaque blocage, conserver la commande, l'erreur et le correctif ciblé, puis
  reprendre l'étape concernée. Ne pas désactiver une fonction pour obtenir un
  démarrage apparemment réussi.

## Point du 25 octobre 2026

L'APK reel est installe et teste sur Samsung SM-G985F : menus PartyBoard,
carte creee puis restauree, montage d'un mod de fichiers et entree dans
Bianco Hills avec HUD et deplacement tactile verifies.
La campagne et les autres stages ne sont pas encore valides dans leur ensemble.
Les anciennes lignes du tableau conservent l'historique initial du port.

La comparaison de fluidite dans le meme APK et avec les memes reglages montre
41,73 -> 34,92 ms en moyenne pour le correctif FIFO, sur 1201 images de
Bianco Hills. Details, limites et commandes : `MOBILE-PERFORMANCES.md`.

## Journal

- Diagnostic JNI : deux ABI testées ; Android termine son démarrage.
- Audit pointeurs : cinq familles documentées dans
  `artifacts/android-arm64-probe/pointer-audit.md`.
- Premier lot lancé : callbacks SPC. Le format `TSpcSymbol` reste à 20 octets ;
  le champ 32 bits devient un identifiant de callback sur Android uniquement.
- Lot SPC : tests de callbacks hauts/typés/nuls/inconnus, déduplication et
  réentrance réussis. Les cinq unités adaptées compilent sur les deux cibles.
- Configuration du jeu ARM64 lancée dans `build/android-arm64-game`, sans GX
  à cette première étape. Aucune installation des binaires de test sur le PC.
- Configuration NDK réussie. Correctifs de compatibilité Android : macros
  Bionic, compteur du journal audio et tailles explicites des alias de tables.
  Compilation de l'archive en cours avec les interfaces GX natives.
- Diagnostic de traces natives API 28 implémenté et testé dans l'émulateur ;
  audit de l'entrée JNI conservé dans `artifacts/android-spc/runtime-entry-audit.md`.
- Archive ARM64 construite : 594 étapes Ninja terminées, 593 objets vérifiés.
  La couche plateforme compile aussi ; l'essai de lien exhaustif ne trouve que
  31 références GX manquantes, sans autre erreur de liaison. Aucun stub ajouté.
- 18 octobre : Aurora/Dawn/SDL3/RmlUI ARM64 construits puis liés au jeu et
  au frontend GX. Les 31 références GX de l'essai antérieur sont résolues.
  Le bridge exporte son ABI sab_* et l'entrée JNI explicite de surface ;
  les symboles GX internes restent masqués pour éviter l'interposition.
- Adaptation SDL3 Android : les vrais verrous d'Activity utilisés par Aurora
  sont exportés explicitement depuis SDL3 ; aucun faux verrou ajouté.
- Quatre ressources statiques immuables enregistrées dans la MEM1 du rootheap,
  et accès textures GX adapté uniquement sur Android. Le test du registre passe
  sur l'émulateur ; le vrai GX API/FIFO passe ensuite dans une SDLActivity ARM64.
- Nouvel APK `Sunshine-Aurora-Surface-arm64-v8a.apk`, distinct du jeu et du
  diagnostic mémoire. Il teste le frontend réel, l'audio et la surface Vulkan.
  Le PCM 32 kHz S16 stéréo passe les tests de démarrage différé, callbacks
  bornés, arrêt avec jointure et fermeture globale. Cela ne valide pas encore
  la musique, le mixeur DSP ni la synchronisation audio du jeu.
- Premier échec GPU conservé dans `artifacts/android-aurora/surface-first-run.log`.
  Le pilote SwiftShader de cet émulateur est insuffisant pour les limites
  WebGPU Core requises. Lavapipe fournit ensuite un périphérique compatible.
- Liaison du cycle de vie réel de SDLSurface vers la porte de pause Aurora,
  avec réveil de SDL_WaitEvent lors des transitions. Le blocage initial sur
  « surface non prête » est corrigé sans forcer cet état à vrai en permanence.
- APK vérifié installé par empreinte SHA256 ; processus ARM64 traduit confirmé.
  90 images soumises via Vulkan puis retour de sab_shutdown. Aucune validation
  des pixels du jeu ni de la recréation d'une surface n'est déduite de ce test.

## Prochain lot : intégrer le jeu dans l'Activity

1. Remplacer l'entrée exécutable par une bibliothèque d'application ; garder
   les arguments et chemins dans un stockage possédé, utiliser une pile basse
   vérifiée et démarrer hors du thread UI. Refuser un second démarrage tant que
   les threads et allocations globaux ne savent pas être réinitialisés.
2. Ajouter une initialisation embedded qui renvoie ses erreurs : réservation
   MEM1 exacte, disque explicite, paramètres et sauvegardes privés. Désactiver
   le launcher et les handlers de signaux desktop dans le processus Android.
3. Traiter reset/quit et fin de session avec arrêt/jointure des producteurs VI,
   audio et threads ; résoudre le recyclage des piles Bionic avant les essais
   répétés de chargement de scènes. Ne pas détourner globalement exit.
4. Importer les données utilisateur par le sélecteur Android dans un fichier
   accessible aux lecteurs DVD, puis vérifier écran titre et premier niveau.
5. Tester GPU sur des pixels connus (couleur/profondeur), perte/recréation de
   surface, puis les régressions historiques : HUD Soleil, peintures et pollutions.

Rapports associés : `docs/PORT-SMARTPHONE.md`, `tools/mobile/README.md`.
