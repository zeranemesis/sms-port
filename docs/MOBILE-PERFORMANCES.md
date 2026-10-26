# Fluidite Android â€” validation du 25 octobre 2026

Appareil : Samsung SM-G985F, Android 13, ARM64, Vulkan Mali.
Resolution native de surface, presentation avec proportions conservees, audio actif,
VSync active, cadence originale de 30 images/s.
Aucun retrait de textures, effets, geometrie ou calculs de gameplay.

## Changements

- Patch Android du miroir : le flag existant 0x2 de TDrawSyncManager desactive
  les messages des points d'arret FIFO. Les fonctions natives GXEnableBreakPt et
  GXDisableBreakPt sont deja vides. Les callbacks des tokens restent appeles dans
  leur ordre, ainsi que les lectures GPU, copies et mesures de pollution.
- Compilation -O3 pour le jeu, sa plateforme, GX et le traitement des commandes
  Aurora sur Android. Les options flottantes existantes sont conservees ; aucun
  fast-math ajoute. Les options CMake SMS_ANDROID_OPTIMIZE_CPU et
  SMS_ANDROID_OPTIMIZE_RENDERER permettent de comparer les builds.
- APK avec 417 descriptions de pipelines : 404 GX, 2 clear et 11 RmlUi.
  Aurora les compile pour le GPU du telephone avant le jeu. Aucun cache binaire
  de pilote n'est distribue. Couverture limitee au demarrage, aux menus et a
  Bianco Hills ; les autres scenes enrichissent le cache a leur premier passage.
- Profilage active seulement par un lancement diagnostic explicite.

## Comparaison controlee du FIFO

Meme APK, meme settings.txt, Bianco Hills 2,0 sans deplacement pendant la mesure.
Fenetre de 1201 images, numeros 300 a 1500. Le mode de reference restaure les
messages FIFO ; le mode normal utilise le correctif. Le materiel n'est pas
verrouille en frequence : des repetitions sont utiles pour les faibles ecarts.

| Mesure | Reference | FIFO optimise |
|---|---:|---:|
| Temps moyen | 41,73 ms | 34,92 ms |
| Mediane | 40,52 ms | 33,40 ms |
| P90 | 50,53 ms | 36,10 ms |
| P99 | 59,80 ms | 51,15 ms |
| Images au-dessus de 40 ms | 50,54 % | 8,66 % |

APK commun : 44645229fff453fb2134e9e56cb2ac532dfeee04d3deacdf3fa8f6bf52de3498.
Resultats : artifacts/android-bench-o3-reference et android-bench-o3-optimized.
Le profil initial montrait environ 33 bascules vers le thread DrawSync par image,
pour une attente mediane de 4,86 ms ; ces bascules disparaissent apres le patch.

## Rendu optimise et cache neuf

Resultat du build avec Aurora optimise, dans un dossier de cache de test neuf
separe du cache habituel : moyenne 34,78 ms, mediane 33,41 ms, P90 35,25 ms,
P99 65,84 ms, 6,66 % d'images au-dessus de 40 ms sur la meme fenetre.
La preparation des 417 pipelines a pris 7,617 s avant le jeu ; aucune attente
lookup de compilation observee ensuite pendant le passage.
Les rares pics restants correspondent surtout aux attentes de debut de rendu.
Ce resultat valide une entree et une courte sequence dans Bianco Hills,
pas toute la campagne, tous les telephones ou une session thermique longue.

Le HUD a ete observe et les compteurs de pollution continuent de changer dans
le journal apres le patch. Cela ne remplace pas une validation visuelle complete
pour les pollutions lave/electrique et le combat final.

## Reproduire

Installer l'APK, puis lancer depuis la racine du depot :

```powershell
python tools/mobile/benchmark-phone.py --serial SERIAL_ADB --label reference --reference
python tools/mobile/benchmark-phone.py --serial SERIAL_ADB --label optimise
python tools/mobile/benchmark-phone.py --serial SERIAL_ADB --label cache-neuf --cold-shaders
```

Le diagnostic utilise Bianco Hills et quitte apres 1800 updates.
Il ne change ni les reglages persistants ni les donnees de sauvegarde.
Les fichiers et le SHA256 de l'APK sont conserves sous artifacts/android-bench-LABEL.
Pour exporter de nouvelles descriptions, attendre la fin du processus, copier
pipeline_cache.db et son WAL, puis utiliser tools/mobile/export-pipeline-seed.py.
Ne pas remplacer le seed par dawn_cache.db, qui depend du GPU/pilote.

## Tampon supplementaire evalue

Un essai avec trois images en vol a donne 34,98 ms en moyenne, P90 36,85 ms,
P99 62,16 ms et 8,66 % d'images au-dessus de 40 ms. Il n'ameliore pas la
regularite globale. Le produit conserve deux images en vol, sans ajouter cette
latence ; le code du commutateur temporaire a ete retire apres la comparaison.
Preuve : artifacts/android-bench-triple-buffer/result.json.
