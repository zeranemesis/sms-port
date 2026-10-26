# Commandes Android

`TouchControls` est une couche d'entrée pour l'Activity du jeu, indépendante de la Surface SDL/Aurora.
Elle publie les commandes dans `platform/pad/mobile_input.cpp` ; seul `PADRead` fusionne ces snapshots dans le statut GameCube, sous `__ANDROID__`.
Les événements SDL déjà pris en charge restent disponibles et le code desktop n'appelle pas cette fusion.
Aucune vitesse ni logique du jeu n'est changée.

## Intégration de l'Activity

Charger `libsms.so` avant de créer les commandes, puis installer une seule fois sur le thread UI :

```java
controls = TouchControls.install(this);
```

Dans `dispatchKeyEvent`, retourner true si `controls.handleKeyEvent(event)` retourne true ; sinon appeler `super.dispatchKeyEvent(event)`.
Procéder de même avec `dispatchGenericMotionEvent` et `controls.handleGenericMotionEvent(event)`.
Dans `onPause`, appeler `controls.releaseInputs()` **avant** `super.onPause()`.
Les champs peuvent être null pendant la construction SDL ; protéger les trois délégations.
L'overlay traite lui-même la perte de focus, l'annulation tactile, son détachement et le retrait d'une manette.
Il ne ferme ni l'Activity ni la Surface.

Les deux symboles JNI à conserver dans la carte d'exports de `libsms.so` sont :

```text
Java_org_smsunified_mobile_TouchControls_nativeSubmit
Java_org_smsunified_mobile_TouchControls_nativeClear
```

Le préfixe `Java_org_smsunified_mobile_*` les couvre également.
Le nouveau source `platform/pad/mobile_input.cpp` doit être inclus par la configuration du port, comme les autres sources plateforme.
Cette adaptation ne modifie pas le packaging ni les fichiers CMake.

## Tactile

Deux joysticks indépendants contrôlent Mario et la caméra ; leur doigt reste identifié jusqu'à son relâchement.
Le multitouch permet déplacement + JET + saut en même temps.
Les boutons A/B/X/Y, L/Z, Start et croix directionnelle utilisent les bits GameCube habituels.
La disposition s'adapte à la taille de la vue et évite les marges système signalées par Android.

- **JET** : pression analogique R brute 160, sans clic numérique R, pour arroser en mouvement.
- **JET +** : pression brute 255 avec clic numérique R, pour l'appui complet/visée.

Le `PADClamp` original transforme respectivement ces valeurs en 130 et 150 ; le clic R reste distinct.
Le seuil numérique reste 250 avant clamp, exactement comme dans le chemin manette SDL existant.
Le C-stick tactile passe ensuite par l'option d'inversion caméra existante, une seule fois.

## Manette Android

Le premier périphérique qui produit une commande reconnue est sélectionné ; les autres périphériques de jeu reconnus ne remplacent pas ses axes.
Le retrait de ce périphérique libère son état et permet d'en sélectionner un autre.
Les axes gauche X/Y et caméra Z/RZ (ou RX/RY en repli) restent analogiques.
L2/R2 utilisent LTRIGGER/RTRIGGER, avec BRAKE/GAS en repli.
Les événements de touche L2/R2 sont ignorés lorsqu'un axe analogique correspondant existe : Android peut envoyer cette touche à mi-course, ce qui ne doit pas provoquer un appui complet prématuré.
Une gâchette sans axe dispose seulement du repli numérique complet ; le bouton tactile JET reste disponible pour la pression partielle.
R1 correspond à Z, L1 à l'appui complet L ; A/B/X/Y, D-pad, Start et Select sont mappés directement.

## Contrôles effectués et limites

Les sources PAD et JNI compilent avec le NDK 29 pour ARM64/API 28.
La bibliothèque JNI isolée lie sans symbole indéfini ; les deux exports JNI sont présents.
La classe Java compile contre `android.jar` API 35 en source/target 8.
Les huit tests natifs passent, avec le vrai `PADClamp`, la coexistence SDL, les deux appuis JET, les resets et des publications/lectures concurrentes.
Les preuves et la sauvegarde du PAD précédent sont dans `artifacts/android-input-20261018/`.

Cela ne prouve pas encore l'usage sur téléphone : le multitouch, le pilote manette, les insets, le rendu de l'overlay et pause/reprise doivent être essayés dans l'APK intégré.
Le changement n'ajoute pas de vibration et ne règle pas les éventuels problèmes de rendu, mémoire, performances ou packaging de l'APK.
