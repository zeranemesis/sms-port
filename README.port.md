# sms-port — port PC de Super Mario Sunshine

Ce dépôt part de la décompilation [`doldecomp/sms`](https://github.com/doldecomp/sms)
et vise un exécutable natif. Il est **séparé** de la décomp parce que les deux ont
des contraintes opposées : la décomp exige un code qui se recompile à l'octet près
en PowerPC, le port exige un code portable. Ici on casse le matching librement.

Version ciblée : **PAL (`GMSP01`)**.

## Dépôts liés

| remote | dépôt | rôle |
|---|---|---|
| `origin` | `zeranemesis/sms-port` | ce port |
| `decomp` | `doldecomp/sms` | amont, à récupérer périodiquement |

Le travail de matching sur la PAL continue dans `zeranemesis/sms` et remonte ici
via `decomp`.

## État mesuré

Chiffres produits par `tools/port/audit.py` (lecture seule, reproductible) et par
`ninja changes_all` côté décomp. Ils datent de la mise en place du dépôt.

**Décompilation PAL** : 38,79 % de code matché, 49,98 % de données,
**18,23 % d'unités complètes**.

Le chiffre qui compte pour un port est le dernier : la décomp comble aujourd'hui
ses trous avec l'**assembleur PowerPC d'origine** (mode `--non-matching`), ce qui
produit un DOL complet mais reste inutilisable ici.

**Code de gameplay** : 380 fichiers `.cpp`, **3892 Ko écrits**. 60 fichiers sont
des souches de 2 octets — vides, pas ébauchées — et très concentrées : `Enemy`
en compte 40 sur 84 (48 % du répertoire), surtout les boss.

**Dépendance matérielle** (appels directs depuis le gameplay) :

| famille | appels | fichiers |
|---|---|---|
| `GX*` (graphismes) | 2400 | 45 |
| `OS*` | 104 | 17 |
| `CARD*` | 32 | 2 |
| `VI*` | 13 | 4 |
| `DVD*` | 11 | 2 |
| `PAD*` | 5 | 1 |
| `AI*`/`AX*`/`DSP*` | **0** | 0 |

Aucun appel audio direct : tout passe par JAudio2, donc l'audio se remplace en
bloc plutôt que site par site.

## Ce qui bloque un build natif

- **Matrices — déjà résolu pour l'essentiel.** `include/dolphin/mtx.h` bascule
  entre `PSMTX*` (paired-singles Gekko) et `C_MTX*` (portable) selon `#ifdef DEBUG`.
  Reste ~46 appels directs à router via les macros `MTX*`, dans ~14 fichiers.
- **Assembleur inline** : un seul fichier de gameplay (`src/MarioUtil/MathUtil.cpp`),
  mais 3 headers en contiennent et contaminent leurs includeurs
  (`dolphin/os.h`, `J3DTransform.hpp`, `JGMatrix34.hpp`).
- **125 `#pragma`**, dont 104 `dont_inline` — neutralisables par `#define`.
- **Shift-JIS** : les sources sont en UTF-8, `sjiswrap` transcode à la compilation.
  Le piège n'est pas l'encodage : ces 205 fichiers portent des **clés d'objets
  utilisées à l'exécution**, donc un build natif doit émettre les mêmes octets.
- **Aucun garde-fou de layout** : pas un `static_assert` dans l'arbre. Les
  structures sont figées implicitement par `-align powerpc -enum int -char signed`,
  et le parsing d'assets suppose le big-endian sans vérification. **C'est le risque
  de corruption silencieuse le plus sérieux du port.**
- **PCH** : 300 des 739 objets dépendent du `.mch` MWCC, sans équivalent natif.

## Route retenue

Hybride. La voie « décomp à 100 % d'abord » est celle de Twilight Princess et
Pikmin 1, mais elle suppose des années avant le moindre exécutable. La
recompilation statique produit déjà des builds SMS jouables sans passer par la
décomp. On combine : une base exécutable, remplacée progressivement par du C
décompilé.

## Ce qui existe et évite de réécrire

- [`encounter/aurora`](https://github.com/encounter/aurora) — couche GX →
  Vulkan/Metal/D3D12, plus PAD, DVD, CARD. Motorise Dusklight (port de Twilight
  Princess), Metaforce et Party Board.
- **Pas de réimplémentation partagée de JSystem/J3D**, et **pas de runtime audio
  DSP réutilisable** : Aurora ne fournit pas d'audio. C'est le poste le plus
  incertain — pour cette route-ci. La recompilation statique y échappe, mais en
  reprenant un cœur dérivé de Dolphin : voir `docs/recompilation.md`.

## Outils

```bash
python tools/port/audit.py .
```

Produit le rapport de portabilité chiffré ci-dessus. À relancer pour suivre la
progression — le compteur de souches vides est le vrai indicateur d'avancement.

## Construire la décomp (inchangé pour l'instant)

Tant que les couches de plateforme ne sont pas activées, l'arbre se construit
comme la décomp : placer un dump du disque PAL dans `orig/GMSP01/`, puis

```bash
python configure.py --version GMSP01
ninja
```

Le `.dol` produit doit rester identique à celui de la décomp : c'est le garde-fou
de non-régression tant que le port n'a pas divergé.
