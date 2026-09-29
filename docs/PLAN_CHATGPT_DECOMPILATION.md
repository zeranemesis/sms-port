# Plan de reprise ChatGPT — décompilation Super Mario Sunshine

## État de référence

- Dépôt local : `C:\sms-aurora-progress`
- Branche : `claude/aurora-port`
- Version suivie : `GMSP01` (édition européenne)
- Dernier build complet vérifié : 29 septembre 2026 ; `build/GMSP01/mario.dol` passe le contrôle SHA1 configuré.
- Progression exacte : **49,74 %** (1 776 312 / 3 571 488 octets ; 10 203 / 12 751 fonctions).
- Similarité fuzzy : **89,57 %**. Ce chiffre sert au diagnostic ; il ne compte pas comme code exact.
- Reste pour atteindre 100 % exact : **1 795 176 octets** et 2 548 fonctions non exactes, selon le rapport actuel.
- Le PC port et l’exécutable du jeu ne sont pas une mesure du matching. Le chiffre est calculé sur le DOL reconstruit pour GMSP01.

## Objectif et faisabilité

Continuer jusqu’à 100 % de code exact, en gardant les modifications lisibles et plausibles pour le code d’origine. C’est un objectif techniquement possible avec le DOL, les outils et la configuration déjà présents, mais la quantité restante est importante : personne ne peut garantir le délai. Les fonctions fuzzy proches de 100 % ne deviennent exactes qu’après correspondance octet par octet ; empiler des fonctions à 98–99 % ne fait pas avancer le total exact.

## Ordre de travail

1. **Préserver une base compilable.** Au début de chaque lot, exécuter `ninja baseline`. Consigner la branche, la version GMSP01 et le total de départ. Ne pas remplacer le DOL ni son hash de référence.
2. **Achever les corrections déjà commencées.** Reprendre d’abord les fichiers Mario et menus touchés récemment : `MarioReceiveMsg.cpp`, `CardLoad.cpp`, `Option.cpp`, `SelectShine2.cpp`, `Talk2D2.cpp`, `CardSave.cpp` et `Guide.cpp`. Comparer les diffs fonction par fonction avec `tools/decomp-diff.py`; rechercher les divergences de contrôle, appels, constantes et accès mémoire avant de régler les registres ou la pile.
3. **Fermer les fonctions proches de l’exact.** Pour chaque TU prioritaire, lister les fonctions non exactes avec `python tools/decomp-diff.py -u mario/<TU> -s nonmatching -t function`. Auditer en premier celles où une différence source est démontrée. Ne pas consacrer un lot entier à des différences de frame, de registres ou à un fuzzy élevé sans progrès reproductible.
4. **Continuer par petits TUs indépendants.** Donner aux agents des fichiers exclusifs, sans modifications concurrentes des mêmes en-têtes. Les zones de jeu déjà reconstruites dans le lot courant incluent plusieurs acteurs ennemis, MoveBG, Animal, Mario et GC2D. Mesurer chacune séparément avant de décider de la suite.
5. **Traiter ensuite les gros blocs restants.** Une fois les corrections sûres épuisées, classer les TUs non exacts par octets récupérables, fonctions encore stubs/missing, dépendances et temps d’analyse. Continuer la décompilation de gameplay et des sous-systèmes par lots cohérents. Ne pas compter une reconstruction fonctionnelle comme un match exact sans preuve du diff.
6. **Consolider chaque lot.** Exécuter `ninja changes_all`, examiner les gains et régressions par symbole, puis `ninja`. Accepter un lot seulement si le build complet réussit et que le contrôle SHA1 de `mario.dol` réussit. Comparer le nouveau total exact au baseline et noter octets, fonctions et TUs devenus exacts.
7. **Committer par lots vérifiés.** Inclure les sources et en-têtes pertinents ainsi que ce plan, jamais les logs temporaires, dumps, scripts ad hoc ou fichiers de travail. Les commits ne doivent pas contenir de modification de middleware/SDK/MSL/JSystem.

## Règles de matching

- Travailler sur le code du jeu (`src/`, `include/`), pas dans les bibliothèques middleware, SDK, MSL ou JSystem.
- Se servir de l’ASM du DOL, du linker map et des diffs objdiff/decomp-diff comme preuves. Lire une fonction entière et isoler les écarts avec `--range` avant de modifier son code.
- Préserver les changements déjà présents dans le worktree. Ne pas faire de reset, stash global ou restauration en bloc pour nettoyer les fichiers d’autres lots.
- Préférer une forme C++ naturelle compatible avec MWCC 1.2.5. Pas de padding artificiel, `volatile`, asm forcé, casts d’offset ou branches sans fonction juste pour gonfler un score.
- Si les preuves ne suffisent pas, conserver la fonction non exacte et passer à un autre symbole.
- Ne pas annoncer de hausse du total basée sur le fuzzy. Le taux exact n’augmente que quand des octets deviennent identiques.

## Compte-rendu à chaque lot

Donner le total exact au départ et à l’arrivée, le nombre d’octets exacts ajoutés ou perdus, les fonctions et fichiers devenus exacts, les résultats de `ninja changes_all` et `ninja`, le statut SHA1, et toute limite rencontrée. Si un build échoue, citer le fichier et l’erreur exacte, puis corriger ou signaler explicitement que le rapport reste celui du dernier build réussi.
