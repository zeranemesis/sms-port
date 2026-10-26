# Android ARM64: premier jalon

Ces outils construisent un **diagnostic mémoire**, en exécutable shell ou APK.
Le jeu n'est pas encore inclus.
Ils ne lisent aucune donnée du jeu et ne modifient aucune sauvegarde.

Depuis PowerShell à la racine du dépôt :

```powershell
.\tools\mobile\build-probe.ps1
# Téléphone ARM64 connecté, débogage USB activé et ordinateur autorisé :
.\tools\mobile\run-probe.ps1
# Si plusieurs appareils sont connectés : ajouter -Serial <identifiant adb>.
```

Le script utilise le SDK local, le NDK 29.0.14206865 et CMake 3.22.1 ; leurs
chemins/versions sont configurables. Il cible Android API 28 pour ce diagnostic
uniquement ; la version minimale du jeu complet n'est pas encore fixée.

Le programme vérifie :

- le placement de 64 Mio à `0x80000000` sans écraser de mapping existant ;
- l'écriture et la relecture d'un octet par page de cette mémoire ;
- la création d'un thread sur une pile située sous 2 Gio ;
- la présence d'une variable locale à l'intérieur de cette pile.

Il rapporte aussi la taille des pages et les adresses du code et d'une donnée
statique. Un succès ne résout pas les pointeurs du jeu tronqués en 32 bits :
le code et les données du programme Android peuvent se trouver au-dessus de
4 Gio. Le jeu PC les maintient bas via les options de lien, alors que ce
diagnostic est un ELF PIE Android. Cet écart reste à traiter.

Le script d'exécution choisit un appareil autorisé, vérifie l'ABI, transfère le
diagnostic dans `/data/local/tmp` sous un nom contenant son empreinte, puis
conserve le résultat sous `artifacts/android-arm64-probe/`. Le diagnostic reste
dans ce répertoire temporaire du téléphone. Aucun root n'est demandé.

La réussite en processus `adb shell` doit être distinguée de celle dans
l'application Android. Les deux contextes ont maintenant passé les contrôles
mémoire, mais le moteur graphique n'est pas chargé dans ce diagnostic et aucun
test sur téléphone physique n'a été réalisé.

iOS reste la seconde cible. Ces scripts sont propres à Android ; la politique
mémoire d'iOS et la construction/signature de l'application devront être
traitées séparément.

## Émulateur Windows

L'AVD dédié `Sunshine_Android_Test` utilise l'image déjà installée Android API 36,
Google APIs, x86_64, profil Pixel 7. Il est séparé de `PartyBoard_Test`.

```powershell
.\tools\mobile\start-emulator.ps1
.\tools\mobile\build-probe.ps1 -Abi x86_64
# Une fois Android démarré :
.\tools\mobile\run-probe.ps1 -Abi x86_64 -Serial emulator-5580
# L'image API 36 installée annonce aussi arm64-v8a via libndk_translation.so :
.\tools\mobile\run-probe.ps1 -Abi arm64-v8a -Serial emulator-5580
```

Pour recréer cet AVD si nécessaire, avec JAVA_HOME pointant vers le JBR Android
Studio, utiliser `avdmanager create avd -n Sunshine_Android_Test -k
"system-images;android-36;google_apis;x86_64" -d pixel_7` (répondre non à la
création d'un profil matériel personnalisé). Le launcher utilise le port 5580.

L'image Android API 30 ARM64 a également été installée et l'AVD
`Sunshine_ARM64_Test` créé. Le lancement avec Emulator 36.5.11 échoue sur cet
hôte avec : `Avd's CPU Architecture 'arm64' is not supported by the QEMU2
emulator on x86_64 host. System image must match the host architecture.`
La preuve est conservée dans `artifacts/android-arm64-probe/emulator-arm64.stdout.log`.
L'image ARM64 reste installée mais ne démarre pas directement sur cet hôte.

Le 17 octobre, le diagnostic a cependant été exécuté avec succès en x86_64
**et en ARM64 traduit** sur l'image x86_64 API 36. Celle-ci annonce les ABI
`x86_64,arm64-v8a` et utilise `libndk_translation.so`. Dans les deux exécutions,
les 64 Mio MEM1 et la pile basse du thread passent les contrôles. Les adresses
du code et des données statiques restent supérieures à 4 Gio : le jeu complet
n'est donc pas validé. Les résultats sont conservés sous
`artifacts/android-arm64-probe/run-20261017-204058.json` (x86_64) et
`run-20261017-204137.json` (ARM64 traduit).

Le démarrage initial avec GPU hôte n'a pas atteint ADB pendant l'essai ; la
relance avec SwiftShader, 4 Gio RAM et quatre cœurs a permis les diagnostics.
Le script utilise ce réglage de l'émulateur par défaut ; cela ne change pas
la qualité graphique du jeu PC. L'accès ADB ne prouve pas à lui seul que
l'interface Android a fini son démarrage.

Le diagnostic x86_64 contrôle les prérequis POSIX Android dans l'émulateur.
Il ne mesure ni les performances d'un téléphone, ni le fonctionnement du jeu
complet ARM64, ni les capacités Vulkan du GPU mobile. L'exécution ARM64 traduite
ne remplace pas la validation du runtime dans une application sur téléphone.

## Application Android installable

```powershell
python -X utf8 tools/mobile/build-app.py
python -X utf8 tools/mobile/build-app.py --abi arm64-v8a
```

Sorties sous `build/android-diagnostic-app/` : `Sunshine-Diagnostic.apk`
(deux ABI) et `Sunshine-Diagnostic-arm64-v8a.apk` (ARM64 seul). Le script utilise
le JBR Android Studio, SDK API 35, Build Tools 36.1.0 et les compilations NDK
ci-dessus. Il n'a pas besoin de Gradle et ne télécharge pas de dépendance.
Les APK sont signés avec une clé de développement locale dans le répertoire
de build. Cette clé n'est pas destinée à la distribution finale.

Pour installer la variante ARM64 dans l'émulateur démarré :

```powershell
$adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
& $adb -s emulator-5580 install -r build/android-diagnostic-app/Sunshine-Diagnostic-arm64-v8a.apk
& $adb -s emulator-5580 shell am start -W -n org.smsunified.diagnostic/.ProbeActivity
& $adb -s emulator-5580 shell run-as org.smsunified.diagnostic cat files/last-result.txt
```

Le diagnostic s'exécute sur un thread de l'application, affiche le résultat
et le conserve dans son stockage privé. Il ne demande aucune permission ni
connexion réseau. Chaque allocation de test respecte les mappings existants
et est libérée après un test réussi. Aucun asset commercial n'est embarqué.

Validation du 17 octobre : lancement Activity confirmé, code résultat PASS
en x86_64 puis ARM64 traduit, `primaryCpuAbi=arm64-v8a`, boot Android terminé.
Preuve détaillée et empreinte APK : `artifacts/android-arm64-probe/app-validation.json`.
L'APK ARM64 de diagnostic reste installé et l'activité ouverte.

## Compilation de l'archive du jeu

```powershell
.\tools\mobile\build-game.ps1 -Jobs 2
```

Cette étape utilise un build séparé `build/android-arm64-game`, compile les
sources du jeu en ARM64 PIC contre les interfaces GX natives et vise seulement
`sms_game`. Elle ne construit pas encore le rendu Aurora ni un APK du jeu.
Les sources locales de rcheevos déjà préparées par le build PC sont réutilisées,
sans réutiliser ses bibliothèques Windows. Le CMake 3.22.1 du SDK ne comprend
pas l'option de téléchargement récente du projet : fournir ces sources évite
ce chemin de téléchargement incompatible.

Le plan suivi se trouve dans `docs/PLAN-PORT-MOBILE.md`.

## Aurora et surface Android (18 octobre)

```powershell
.\tools\mobile\build-aurora.ps1
.\tools\mobile\build-game.ps1 -Target sms -Jobs 2
.\tools\mobile\build-game.ps1 -Target sms_mobile_surface_probe -Jobs 2
python .\tools\mobile\build-surface-app.py
```

Le script du jeu utilise maintenant CMake 4.4 et le runtime C++ partagé du
bridge, avec le frontend Aurora actif. Sa cible par défaut reste `sms_game`.
La cible `sms` produit un exécutable Android lié ; elle ne produit pas l'APK
jouable et ne doit pas être confondue avec l'entrée SDLActivity.

Le nouvel APK `build/android-surface-app/Sunshine-Aurora-Surface-arm64-v8a.apk`
contient le vrai bridge Aurora et la fermeture de ses 49 dépendances ARM64.
Il teste uniquement GX/FIFO, callbacks PCM et surface Vulkan, sans données du
jeu. Le packaging refuse une dépendance manquante ou d'une autre architecture,
et ne retire les symboles de debug que des copies destinées à l'APK.

SwiftShader de l'AVD a échoué sur les limites WebGPU Core requises par Dawn.
Pour le diagnostic GPU, démarrer l'AVD avec `start-emulator.ps1 -Gpu lavapipe`.
Si le port 5580 est déjà occupé par cet AVD, le script le réutilise : arrêter
explicitement cet émulateur avant de changer son pilote. Le diagnostic seul
autorise les adaptateurs CPU par `SMS_AURORA_ALLOW_CPU_ADAPTER=1` ; le bridge
conserve son refus par défaut. Aucune limite de rendu n'est diminuée.

```powershell
$adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
& $adb -s emulator-5580 install -r .\build\android-surface-app\Sunshine-Aurora-Surface-arm64-v8a.apk
& $adb -s emulator-5580 shell am start -S -W -n org.smsunified.surface/.SurfaceActivity
# Après la fin du diagnostic :
python .\tools\mobile\verify-surface-run.py
```

Résultat vérifié : GX/FIFO PASS, PCM PASS, périphérique Vulkan Lavapipe,
90 images soumises sur la surface 2400×1080 et retour de shutdown.
`artifacts/android-aurora/surface-validation/validation.json` associe cette
preuve à l'APK installé par son empreinte. Le cache shaders était vide et
aucune scène du jeu, copie GPU, recréation de surface ou performance sur
téléphone n'est validée par ce test. L'Activity se ferme à la fin normale.

`tools/mobile/android/org/smsunified/mobile/AuroraActivity.java` relie les vrais
callbacks SDLSurface à Aurora, sans modifier les sources Java SDL3 du vendor.
L'entrée JNI correspondante est exportée explicitement ; les GX internes du
bridge restent privés.
