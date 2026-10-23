# Archipelago dans Sunshine Aurora

Ce mode est une option de **Super Mario Sunshine**, indépendante du mod Eclipse.
La compilation dédiée utilise les données PAL déjà configurées pour `Jouer-Aurora.cmd`.
Le monde réseau garde le nom officiel `Super Mario Sunshine` et les identifiants du
monde Joshark ; aucun nouveau monde réseau PAL n'est créé.

## Jouer

1. Compiler avec `Construire-Archipelago.cmd`.
2. Lancer `Jouer-Archipelago.cmd` pour la partie solo Archipelago fournie.
   Le client est installé automatiquement au premier lancement, après vérification
   du SHA-256 du paquet publié. La connexion est vérifiée avant de lancer le jeu.
3. Sélectionner le fichier de sauvegarde **A** et sauvegarder normalement une fois
   pour que le client puisse associer cette sauvegarde à la partie.

Pour rejoindre une salle :

```bat
Jouer-Archipelago.cmd --server wss://HOTE:PORT --slot MonJoueur --profile ma-partie
```

Utiliser un profil différent pour chaque seed/joueur. Conserver le dossier complet
`saves/archipelago/<profil>` : carte, journal des objets reçus et, pour le solo,
sauvegarde du serveur sont liés. Les cartes habituelles `saves/card-a` sont séparées.
Un mot de passe peut être transmis par `--password-env NOM_VARIABLE`.
`--plan` affiche les chemins sans installer ni lancer ; `--verify-server` vérifie
la connexion réelle sans ouvrir le jeu ni l'interface du client.

Le mode demande le client et le contrat APWorld correspondant au paquet Peliarch
`wip-2026-10-15`, qui utilise le monde Joshark à la révision
`20cc30b58613ee28a7fe3dd4143f96c33fdadf72`. Ce contrat diffère de l'APWorld publié
0.6.2 : une salle utilisant des options incompatibles est refusée. Voir
https://git.peliarch.ca/alaric/sunshine-archipelago pour les sources et exemples YAML.

## Comportement

Les pickups deviennent des checks ; les objets reçus déterminent la quantité de
Soleils, les buses, Yoshi et les tickets de niveaux. Le client garde les reçus
pour éviter de doubler les récompenses après une reconnexion. Les pièces bleues,
les échanges et DeathLink suivent les options de la seed.
Après réception d'une buse, gauche du D-pad équipe Hover, bas Rocket, droite Turbo.
Un ticket ou Yoshi reçu sur la place peut demander une sauvegarde puis un
redémarrage pour recréer les objets d'entrée.

Le mode ne change pas la vitesse de simulation ni la qualité graphique.
Le lanceur habituel reste sur la compilation Aurora sans Archipelago.
L'association Eclipse + Archipelago et le port Android ne sont pas pris en charge
par cette première intégration. Une campagne complète rendue n'est pas encore validée.

## Provenance

Adaptateur natif MIT : https://git.peliarch.ca/alaric/sunshine-pc-port,
révision documentée dans `platform/ap/UPSTREAM.md`, licence conservée.
Le point de synchronisation a été adapté à la boucle PAL sans dépendance aux save states.
Les patches sont appliqués à des copies de compilation ; les sources de
décompilation originales ne sont pas modifiées.

## Vérifications effectuées

La compilation PAL Aurora passe. Les tests exécutés couvrent 144 combinaisons
d'options réseau avec contrôles négatifs, 72 combinaisons de méthodes natives,
11 cas de sauvegarde/pièces bleues et les reçus de vies, doublons et conflits.
Trois tests du lanceur vérifient l'isolation des profils et le refus d'un
téléchargement altéré. Une authentification sur le serveur solo réel passe.
Un démarrage de 25 secondes produit un heartbeat du pont sans erreur, mais la
sauvegarde n'est pas encore enrôlée : cela ne valide pas une collecte physique
en multiworld ni une campagne complète. Le rapport local est dans
`artifacts/archipelago-research/validation.json`.

Pour rejouer les matrices après compilation :

```powershell
python tools/archipelago/test_launch.py
python platform/ap/tests/options_bridge_test.py build/windows-64-pal-archipelago/sms_ap_bridge_test.exe platform/ap/tests/original-world-contract.json
python platform/ap/tests/native_options_test.py build/windows-64-pal-archipelago/sms.exe artifacts/ap-options-nouvel-essai
python platform/ap/tests/native_blue_methods_test.py build/windows-64-pal-archipelago/sms.exe artifacts/ap-blue-nouvel-essai
```

Compiler d'abord la cible de test avec
`cmake --build build/windows-64-pal-archipelago --target sms_ap_bridge_test`.
Les dossiers de rapports des méthodes natives doivent être nouveaux.
