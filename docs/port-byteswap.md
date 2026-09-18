# Byteswap : le vrai point d'entrée, et ce qu'il ne couvre pas

Conception pour la Phase 1d du port (gardes de layout + couche byteswap).
Document de conception, pas de code : le câblage réel touche
`include/JSystem/JSupport/JSUInputStream.hpp` et le chargement d'archives
dans `JSystem/JKernel`, tous deux restreints par `AGENTS.md` — à faire par
un humain. Corrige/précise le plan initial : plutôt que cataloguer chaque
struct J3D/BTI/JPA individuellement (gros chantier, entièrement en zone
restreinte, difficile à vérifier sans assets réels), l'investigation a
trouvé **un point d'entrée unique** qui, corrigé, couvre la quasi-totalité
du chargement piloté par `load(JSUMemoryInputStream&)` — le mécanisme
utilisé par la plupart des acteurs et objets du jeu.

## Le point d'entrée trouvé

`include/JSystem/JSupport/JSUInputStream.hpp:51-106` définit `readS8/readU8/
readS16/readU16/readS32/readU32/readU64/readF32`. Tous suivent exactement le
même patron (exemple, `readU32`, ligne 86-91) :

```cpp
u32 readU32()
{
	u32 i;
	read(&i, sizeof(u32));
	return i;
}
```

`read(void*, s32)` (`src/JSystem/JSupport/JSUInputStream.cpp:7-14`) descend
jusqu'à `readData()` (virtuelle, implémentée par `JSUMemoryInputStream`) qui
fait une copie d'octets brute depuis le buffer. **Aucun de ces maillons ne
fait de byteswap.** Sur le Gekko (big-endian) d'origine, lire 4 octets bruts
dans un `u32` les interprète naturellement en big-endian, ce qui correspond
au flux (lui aussi big-endian) — ça fonctionne par accident d'architecture,
pas par conception explicite. Sur un hôte little-endian (x86/ARM), le même
code lit les mêmes octets mais les interprète à l'envers — silencieusement
faux pour toute valeur multi-octets, sans erreur ni crash.

## Portée réelle de ce point d'entrée

Ce n'est pas un détail interne isolé à JSystem : ce sont les six méthodes
que **tout le système de chargement par nom** (`TNameRef`/`TNameRefAryT` et
ses dérivés, la base de la quasi-totalité des acteurs du jeu) utilise pour
lire ses champs. Preuve trouvée dans du code de gameplay accessible à
l'agent :

```cpp
// include/Strategic/NameRefAry.hpp:20 (TNameRefAryT<T,U>::load)
u32 local_44 = stream.readU32();
```

Et le chargement de paramètres par acteur (`"/enemy/hamukuri.prm"` et les
~200 fichiers `.prm` similaires référencés dans `Enemy/*.cpp`,
`THamuKuriSaveLoadParams` etc.) suit vraisemblablement le même chemin — une
classe de paramètres qui lit ses champs séquentiellement via ces mêmes
méthodes. Corriger `JSUInputStream` corrige donc, en un seul endroit, le
chargement de la plupart des paramètres d'acteurs et de la hiérarchie de
noms de scène, plutôt que de devoir intervenir struct par struct dans
chaque fichier de gameplay qui appelle `load()`.

## Recommandation de conception

Suivre le patron déjà utilisé ailleurs dans ce même dépôt pour un problème
similaire (`include/dolphin/mtx.h`, bascule `PSMTX*`/`C_MTX*` selon
`#ifdef DEBUG`) : gate le comportement derrière une macro de compilation
plutôt que de dupliquer des structs « on-disk » comme le fait
`/home/user/smashmelee/src/port/byteswap.cpp` (patron valable, mais plus
lourd — pertinent surtout pour les formats lus par overlay de struct brut,
voir section suivante). Pour ces six méthodes précisément, il suffit
d'ajouter un swap conditionnel après la lecture :

```cpp
u32 readU32()
{
	u32 i;
	read(&i, sizeof(u32));
#ifndef TARGET_BIG_ENDIAN   // nom indicatif, à aligner sur la macro que la Phase 2 choisira
	i = __builtin_bswap32(i);  // équivalent _byteswap_ulong sous MSVC ; voir
	                           // smashmelee/src/port/byteswap.cpp pour le
	                           // fallback portable sans intrinsèque compilateur
#endif
	return i;
}
```

Même chose pour `readU16`/`readS16`/`readS32`/`readU64` (et `readF32`, qui
doit swapper les 4 octets bruts avant de les réinterpréter comme flottant,
pas after — attention à l'ordre exact). Sur la cible GameCube big-endian
d'origine (matching decomp), la macro doit rester définie pour que rien ne
change dans ce build — même logique de garde que `PSMTX*`/`C_MTX*`.

## Ce que ça ne couvre PAS

Le chargement par flux (`load()`) n'est qu'une partie du problème. Les
formats binaires lus par **overlay direct de struct sur un pointeur brut**
(sans passer par `JSUInputStream`) restent un chantier séparé, entièrement
en zone restreinte :
- `include/JSystem/JKernel/JKRArchive.hpp:13-18` définit déjà
  `read_big_endian_u32()` — le seul primitif de swap qui existe dans tout le
  dépôt, et il est correct par construction (reconstruction octet par octet,
  fonctionne sur n'importe quel hôte) mais n'est visiblement pas appliqué
  partout où l'en-tête RARC est lu.
- Les formats J3D (modèles/matériaux/joints/animations,
  `src/JSystem/J3D/*`), BTI (`ResTIMG`/`JUTTexture`), JPA (particules,
  `src/JSystem/JParticle/*`) restent à auditer un par un pour savoir s'ils
  lisent via `JSUInputStream` (couverts par le correctif ci-dessus une fois
  fait) ou via overlay direct (à traiter au cas par cas, patron
  `smashmelee/src/port/byteswap.cpp` : structs « on-disk » séparées +
  fonctions `bswap()` explicites vers des structs natives).

## Gardes de layout (`static_assert`)

Aucun struct de `sizeof` fixe qui mappe des données DVD n'a été trouvé dans
les répertoires accessibles à l'agent (`Enemy, Player, Camera, Map, MoveBG,
NPC, MSound, M3DUtil, MarioUtil, GC2D, Animal, Strategic, System`) — c'est
plutôt rassurant : le code de gameplay accessible suit déjà le patron « lire
champ par champ via `JSUInputStream` » plutôt que l'overlay de struct brut,
donc le correctif ci-dessus le couvre sans qu'aucun fichier accessible n'ait
besoin d'être modifié. Le seul struct explicitement packé trouvé,
`TDLTexQuad::DL` (`include/MarioUtil/DLUtil.hpp:31-35`, `#pragma pack(push,
1)`), est une liste d'affichage GX construite par le jeu lui-même au
runtime pour la carte graphique — pas une donnée lue depuis le disque — donc
hors du périmètre de ce risque spécifique ; ajouter un `static_assert` dessus
n'apporterait rien pour ce risque précis.

Les structs qui mappent réellement des données DVD (J3D/BTI/JPA/RARC) vivent
tous dans `JSystem`/`dolphin`, restreints — c'est là qu'un humain devra
ajouter les `static_assert(sizeof(...) == N)` documentaires une fois
l'inventaire de la section précédente fait.

## Harnais de test à octets synthétiques

Une fois le correctif ci-dessus écrit (humain, dans `JSUInputStream.hpp`),
un test de non-régression ne nécessite aucun asset réel : construire un
buffer d'octets à la main correspondant à une valeur connue en big-endian,
appeler `readU32()`/`readF32()` dessus via une petite instance de test de
`JSUMemoryInputStream`, et vérifier que le résultat correspond à la valeur
native attendue. Exemple minimal (à adapter à l'API réelle de
`JSUMemoryInputStream`, non modifiée par ce document) :

```cpp
u8 buf[4] = { 0x3F, 0x80, 0x00, 0x00 }; // 1.0f en IEEE754 big-endian
JSUMemoryInputStream stream(buf, sizeof(buf));
assert(stream.readF32() == 1.0f);
```

Ce test vit naturellement à côté du reste du code, pas dans un outil séparé
puisqu'il exerce directement l'API publique de la classe corrigée.
