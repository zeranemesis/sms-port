# Plan — fonctions non finies, tri corrigé

Réécrit le 30/09/2026 après mesures. La version précédente reposait sur un
tri erroné et est remplacée.

## Pourquoi la première liste des 50 était fausse

Deux bugs d'analyse, tous deux corrigés :

1. La regex d'appel exigeait un symbole **guillemeté** (`bl "sym"`). Or dtk
   écrit aussi `bl sym`. 63 symboles classés « sans appelant » étaient en
   fait appelés.
2. La comparaison de chemins oubliait le préfixe `src/`, rendant le filtre de
   zone-parallèle inopérant.

Chiffres corrigés sur les 157 fonctions `missing` du jeu :

| | avant (faux) | correct |
|---|---|---|
| sans appelant | 115 | **60** |
| appelées | 52 | **96** |

## Le palier « petite fonction facile » est épuisé

Sur les 60 restantes sans appelant : 9 sont dans ShadowUtil, 4 en JSystem/MSL,
et le reste se répartit sur des accesseurs dont le corps inline est **déjà
correct** et qui ne manquent que parce que l'appelant diffère.

Le gain réel est venu d'un seul fichier : `include/MoveBG/MapObjBase.hpp`, où
neuf méthodes étaient déclarées `virtual` **sans corps**. MWCC n'émettait donc
pas le corps faible que la vtable réclame — voir `Enemy/bosstelesa.s:10784`,
`dead__11TMapObjBaseFv, weak` = `blr`, référencé par la vtable. Écrire les corps
a donné **12 fonctions à 100 %** (9 directes + 3 gagnées par propagation des
weak), +1 100 octets, 51,26 % → 51,31 %.

## Trois murs, à ne pas recommencer

1. **Padding de frame.** `tools/why-not-100.py` classe chaque fonction selon
   que le diff contient un vrai opcode ou seulement des décalages de
   registres/frame. Sur 787 fonctions non exactes : **226 sont `frame_only`,
   soit 18 % des octets résiduels, non exprimables en source.** Vérifié sur six
   candidats (99,8–100 % d'opcodes identiques, seul `stwu r1, -0x18` vs
   `-0x20` diffère) : aucun n'est gagnable.
2. **ShadowUtil.** Les six `makeDL`/`~TSetupN` ont le bon corps, octet pour
   octet ; seul le compteur `$NNNN` diffère (+1284). Le fichier explique qu'il
   faudrait reconstruire ~2 728 B de code non observable et interdit le
   bourrage de locaux factices.
3. **JSystem / JUT / MSL.** Adjustor thunks `@80@`/`@104@`, `J3DShapeMtx`,
   `std::fmodf`. Interdits aux agents.

## Ce qui reste réellement faisable

Deux bassins, tous deux en conflit avec la session parallèle (307 fichiers
modifiés au 30/09) :

- **40 fonctions de jeu absentes du source** (18 300 B) : hanasambo (11),
  bosstelesa (5), Kazekun (3), igaiga (1), amiNoko, cameragc, killer,
  lensflare, bosswanwan. Chacune exige de résoudre des appels virtuels par
  slot de vtable, des tables rodata et la sémantique brute des conteneurs.
- **1124 fonctions non exactes dans 196 TUs intacts** (856 kB), dont 328 kB en
  `wrong_opcode` — la seule classe réellement gagnable.

Meilleure cible intacte au 30/09 : `Player/WaterGun`, 13 480 B en
`wrong_opcode` pour seulement 20,9 % de padding. Mais sur ses 23 fonctions non
exactes, **six n'ont aucune instruction divergente** et deux n'ont que 2–3
écarts ; le gros du travail est dans `movement` (110 instructions divergentes)
et `animation` (37).

## Règles de méthode qui ont tenu

- Mesurer avant d'ouvrir un fichier : `objdiff-cli report generate`, puis
  `decomp-diff.py`, puis `why-not-100.py` pour classer.
- Vérifier `ninja` + delta exact après chaque lot ; une session qui n'ajoute
  rien doit le dire et laisser le tree propre.
- Si les preuves ne suffisent pas, laisser la fonction non exacte avec un TODO
  factuel. Deux tentatives sur `TWaterGun::isEmitting` (95,9 % → 92,6 %) ont
  été revertées plutôt que laissées en régression.

## Handoff — fonctions déjà décodées, à écrire (30/09)

Trois corps sont entièrement élucidés depuis l'ASM target. Il ne manque
qu'un nom de champ à chacune. **Bloquant commun : la hiérarchie
`TLiveActor`/`THitActor` a des trous de nomenclature dans nos headers** —
les listes de champs de `smallEnemy.hpp` sautent de 0x10 à 0xA8, et celles
de `HitActor.hpp` finissent à 0x68. Lever ce blocage débloque les trois, plus
une partie du gisement de 415 kB.

### 1. `TTelesa::load` — 96 B — [src/Enemy/telesa.cpp](src/Enemy/telesa.cpp)

```cpp
void TTelesa::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	reset();                                    // appel virtuel, vtable slot 0xfc
	mDampenedGroundHeight = <champ à 0x14>;     // lfs 0x14(r31) -> stfs 0x1b0(r31)
	mTelesaType = 0;                            // 0x1BC, confirmé Telesa.hpp:113
	mFadeState = 0;                             // 0x1DC, confirmé Telesa.hpp:131
	unkF0 &= 0x1FFFFFFF;                        // rlwinm r0,r0,0,31,29
}
```

Noms confirmés : `mDampenedGroundHeight` (0x1B0), `mTelesaType` (0x1BC),
`mFadeState` (0x1DC). Manque : le nom du champ à 0x14.

### 2. `TSmallEnemy::decHpByWater` — 76 B — [src/Enemy/smallEnemy.cpp](src/Enemy/smallEnemy.cpp)

```cpp
s16 v = gpModelWaterManager->mParticleAttackSOA[other->unk68];  // 0x614, tableau s16
if (v < 1) v = 1;
u8 cur = <champ à 0x13C>;
if (cur < v) { <champ à 0x13C> = 0; return; }
<champ à 0x13C> = v - cur;
```

`mParticleAttackSOA` confirmé à [ModelWaterManager.hpp:162](include/Player/ModelWaterManager.hpp#L162).
Deux manques : le type à 0x68 de la classe dérivée de `THitActor` (que le
target traite comme un index de slot), et le champ à 0x13C, qui tombe
**à l'intérieur** de `TParamRT<s32> mSLAttackWait` (0x134, taille 0x14).

### 3. `TMenuBase::perform` — 152 B — [src/GC2D/Menu.cpp](src/GC2D/Menu.cpp)

Le target teste `cue & 8` (`rlwinm. r0,r4,0,28,28`), puis construit un
`J2DOrthoGraph` depuis `graphics+0x54`, appelle `setup2D`, puis
`J2DScreen::draw(0,0,…,&graph)`, puis
`GXSetScissor(+0x64, +0x6C, +0x68, +0x70)`. **Attention : l'ordre des
arguments de `GXSetScissor` est contre-intuitif** (x=0x64, x0=0x6C,
y=0x68, y0=0x70) — à vérifier avant d'écrire.

## Gisement vérifié

**580 fonctions / 415 868 octets** dans les TUs dont le `.cpp` **et** le header
sont tous deux intacts. Plus grands : `GC2D/CardSave` (30 kB),
`Enemy/bosspakkun` (24 kB), `System/EventWatcher` (18 kB),
`System/MarNameRefGen_MapObj` (14 kB), `Player/WaterGun` (13 kB).

Limite connue du scan : un corps **inline dans une classe** s'écrit
`isHitValid(u32)` sans répéter le nom de la classe, donc la détection
`Classe::methode` le manque. Corriger en détectant aussi les déclarations
virtuelles sans définition dans le .cpp.
