# Shift-JIS : ce que le port natif doit préserver

Inventaire et recommandations pour lever le blocage « Shift-JIS » listé dans
`README.port.md`. Ceci est un document de conception (pas de code) : le
câblage réel touche `src/JSystem/JDrama` et les chargeurs d'assets, tous deux
hors de portée pour un agent autonome selon `AGENTS.md` — à faire réviser et
implémenter par un humain.

## Le mécanisme confirmé

`configure.py`/`tools/project.py` traitent le Shift-JIS comme un détail
d'invocation de MWCC, rien de plus :
- Chaque objet porte un flag `obj.options["shift_jis"]` explicite (pas de
  détection automatique) qui sélectionne la règle ninja `mwcc_sjis`/
  `mwcc_sjis_extab` au lieu de `mwcc` (`tools/project.py:1038-1046`).
- Cette règle fait passer le fichier source par `sjiswrap` avant MWCC
  (`tools/project.py:673`) ; `-multibyte` (`configure.py:196`) dit à MWCC
  d'interpréter les octets ainsi transcodés comme du Shift-JIS multi-octets.
- Les sources elles-mêmes sont de l'UTF-8 valide (lisible en éditeur/diff) ;
  `sjiswrap` réencode à la volée les littéraux de chaîne en Shift-JIS, pour
  que les constantes de chaîne dans l'objet compilé reproduisent exactement
  les octets du binaire retail (qui, lui, a été compilé par les développeurs
  originaux directement en Shift-JIS).

**Confirmé** : un futur CMake natif n'a aucune raison d'appeler `sjiswrap`
(c'est une règle ninja spécifique au pipeline MWCC), donc par défaut, GCC/
Clang/MSVC compileront ces mêmes littéraux comme de l'UTF-8 — tant que le
CMake ne définit jamais de charset source/exécution qui les retranscoderait
(`-finput-charset`/`-fexec-charset` sous GCC, `/utf-8` sous MSVC). C'est une
règle simple à respecter et à documenter dans le futur `CMakeLists.txt`.

## Le vrai risque (plus précis que ce que le plan initial disait)

Ne rien faire de plus que « ne pas toucher les octets » **ne suffit pas**.
Le problème n'est pas la lisibilité ou l'encodage du fichier source — c'est
que certains de ces littéraux servent de **clé de comparaison à l'exécution**
contre des chaînes qui, elles, proviennent des assets chargés depuis l'image
disque et qui restent, telles quelles, en Shift-JIS (elles ne passent par
aucun `sjiswrap` au runtime, seulement à la compilation du code source).

Preuve concrète trouvée dans le code de gameplay (accessible à l'agent) :

```cpp
// src/Strategic/MirrorActor.cpp:110
JDrama::TNameRefGen::search("鏡シーン"));
```

`TNameRefGen::search` (`JSystem/JDrama`, définition dans le JSystem
restreint) fait une recherche par nom dans un arbre `TNameRef` — le genre de
structure alimentée par les noms de nœuds J3D chargés depuis le disque. Si ce
littéral est compilé en UTF-8 par un compilateur natif au lieu du Shift-JIS
qu'utilise le nœud chargé depuis l'asset, la comparaison échoue silencieusement
(pas de crash, juste un nœud introuvable) — exactement le type de « corruption
silencieuse » que `README.port.md` redoute. `src/Strategic/liveactor.cpp:174`
fait un appel `search(buffer)` avec un buffer construit dynamiquement, à
vérifier de la même façon.

À l'inverse, tous les littéraux japonais ne sont pas des clés de lookup :
`src/System/MarNameRefGen.cpp:217-233` construit des `TCubeManagerBase("?",
"カメラキューブテーブル")` où le second argument est un nom de debug humain
(affichage console), pas une clé comparée à des données d'asset — un mauvais
encodage y serait cosmétique, pas fonctionnel. Une revue humaine doit trier
au cas par cas plutôt que traiter les 116 fichiers de gameplay concernés
(liste ci-dessous) comme uniformément à risque.

## Inventaire (fichiers de gameplay avec octets non-ASCII, agent-accessible)

116 fichiers sous `src/{Enemy,Player,Camera,Map,MoveBG,NPC,MSound,M3DUtil,
MarioUtil,GC2D,Animal,Strategic,System}` portent des octets ≥0x80 (UTF-8
japonais). Concentration notable dans `System/MarNameRefGen*.cpp` (le système
de génération de références par nom lui-même) et dans les fichiers `Enemy/*`,
`Map/*`, `MoveBG/*` qui instancient des acteurs avec des noms. La liste
complète est reproductible avec :

```
python3 -c "
import os
GAME = ['Enemy','Player','Camera','Map','MoveBG','NPC','MSound','M3DUtil',
        'MarioUtil','GC2D','Animal','Strategic','System']
for dp, dn, fns in os.walk('src'):
    top = dp.split(os.sep)[1] if len(dp.split(os.sep)) > 1 else ''
    if top not in GAME: continue
    for fn in fns:
        if not fn.endswith(('.cpp','.c','.hpp','.h')): continue
        p = os.path.join(dp, fn)
        if any(b >= 0x80 for b in open(p,'rb').read()):
            print(p)
"
```

(89 fichiers supplémentaires portent des octets non-ASCII dans `include/` et
dans les dossiers restreints JSystem/dolphin — hors de portée pour un agent,
mais à couvrir par la même revue humaine puisque le même risque de lookup
s'y applique potentiellement, notamment dans `JSystem/JDrama` où vit
`TNameRefGen` lui-même.)

## Recommandation pour un humain (deux options, pas encore tranché)

1. **Garder `sjiswrap` (ou équivalent) dans le pipeline natif**, au moins pour
   les fichiers/littéraux identifiés comme clés de lookup, pour que leurs
   octets compilés restent Shift-JIS et continuent à matcher les données
   d'assets telles quelles. Rapide à mettre en place mais fait perdurer la
   contrainte Shift-JIS dans du code par ailleurs portable, et ne couvre que
   les littéraux du code — pas les buffers construits dynamiquement
   (`liveactor.cpp:174`).
2. **Décoder les chaînes issues des assets en UTF-8 au chargement** (dans le
   chargeur JSystem/JDrama restreint), pour que toutes les comparaisons de
   noms se fassent en UTF-8 des deux côtés. Plus de travail initial (touche
   le chargeur de `TNameRef`/J3D, donc humain), mais élimine la dépendance à
   Shift-JIS dans tout le reste du code, y compris pour un futur usage des
   chaînes dans une UI native (fenêtre, débogueur) qui attend de l'UTF-8.
   Recommandé comme direction cible, la 1. pouvant servir de solution
   temporaire pendant la transition.

Aucune de ces deux options n'est actionnable par un agent : la clé de
recherche (`TNameRefGen::search`) et son chargeur de données vivent tous
deux dans `JSystem`, restreint par `AGENTS.md`.
