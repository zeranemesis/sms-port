# À faire — état au 20/09/2026, après trois sessions de 12-13 minutes

Cette liste sort de trois sessions longues lancées le même jour, pas d'une
revue de code. Chaque point porte la mesure qui l'établit, ou dit explicitement
qu'il n'est pas établi.

**Avertissement de méthode, à lire avant le reste.** Toutes les entrées de ces
sessions sont **scriptées** : des touches envoyées par `keybd_event`. Personne
n'a joué. Cela démontre que le jeu survit à des appuis plausibles, pas qu'il
est jouable, et la distinction a compté : le scintillement d'image avait été
repéré à l'œil en quelques secondes là où des heures de compteurs l'avaient
manqué.

---

## Corrigé pendant ces sessions

Trois défauts qui tuaient le port, tous invisibles pour les compteurs existants.

1. **Gel total à 38 secondes** — le saut d'inactivité sautait l'instruction
   `bl OSEnableInterrupts` de `SelectThread`, donc `MSR[EE]` restait à zéro,
   donc le retrace VI n'était jamais délivré, donc rien ne redevenait
   exécutable. Le port annonçait 60 images/s et « budget de trame 100 % » sur
   un invité exécutant zéro instruction. Commit `36aff60f`.
2. **Invité tué à la première image 3D** — `DCStoreRange` se termine par `sc`,
   `J3DModel::viewCalc` l'appelle pour chaque modèle, et la boucle de trappes
   abandonnait après 64 essais. Commit `d36dc9e2`.
3. **Crash à 272 secondes** — `std::bitset<8>::set` avec un identifiant de
   texture hors bornes lève une exception que personne n'attrape. Commit
   `55c9de2b`.

Les deux premiers ont la même forme, et c'est la leçon à retenir : **un
garde-fou dimensionné contre une panne imaginaire, qui se déclenche sur du
travail réel.** Tous deux avaient été écrits avant que le port puisse atteindre
la charge qui les aurait testés.

---

## 1. Bloquant pour jouer

### 1.1 Trouver le décodage qui produit un identifiant de texture hors bornes

Le correctif `55c9de2b` empêche le crash et **nomme** la valeur fautive, mais
sa provenance n'est pas établie : le décodage BP prend trois bits, et le
`GXSetTevOrder` d'Aurora masque `0x100` comme le fait le SDK. La prochaine
session qui déclenche le cas imprimera la ligne `… id 0x… is outside the 8 …`
avec le site. C'est cette ligne qu'il faut lire, pas un sixième décodage à
deviner.

### 1.2 `GXCopyTex` n'est pas ponté

Le rendu vers texture est mort, et Sunshine s'en sert beaucoup (eau, reflets,
miroirs). Tout est en place sauf le pont :

- Aurora a l'implémentation : `extern/aurora/lib/dolphin/gx/GXFrameBuffer.cpp:150`,
  avec la conversion de format (`gfx/tex_copy_conv`).
- Le symbole invité existe : `GXCopyTex = .text:0x8035707C`.
- Le port installe douze ponts GX ; `GXCopyTex` n'en fait pas partie.
- Le pont a besoin de traduire le pointeur `dest` invité vers l'hôte, ce que
  `g_guestMemoryResolver` fait déjà pour les listes d'affichage et les bases de
  tableaux.

### 1.3 Des passes 512×512 et 1024×1024 sont dessinées à l'écran

`draw passes 1s: onscreen=9627 offscreen=0 … scissor=(0,0 1024x960) viewport=(4,4 1024x1024)`
— `offscreen` vaut **zéro sur toute la session**. Ce qui devrait être une cible
de rendu intermédiaire part à l'écran. Probablement la même racine que 1.2.

### 1.4 Le contournement `GXSetTevAlphaOp` est trop large

`host_call_gx_set_tev_alpha_op` remplace par `ADD` **toute** opération `>= 8`.
Or le header du SDK donne `GX_TEV_COMP_A8_GT = GX_TEV_COMP_RGB8_GT = 14`, et
l'émetteur d'Aurora pour 14/15 n'utilise aucun swizzle — il produit du WGSL
valide pour un scalaire. Seules les opérations **8 à 13** sont cassées : elles
écrivent `.r`, `.rg`, `.rgb` sur le résultat scalaire de `tev_overflow_f32()`.
Et le journal montre que **l'op 14 est la plus fréquente**, donc le
contournement abîme surtout le cas qui marchait.

À restreindre à `8..13`. *Ce fichier appartient à la session parallèle
(non committé) — à coordonner avec elle plutôt qu'à modifier.*

### 1.5 Le vrai défaut derrière 1.4

`extern/aurora/lib/gx/shader.cpp` : `tev_op()` est partagé entre le combineur
couleur (arguments `vec3`) et le combineur alpha (arguments `f32`), et les cas
8 à 13 supposent un vecteur. Pour l'alpha, ces modes comparent les entrées
**couleur** de l'étage, que `tev_alpha_op` n'a pas — d'où un vrai
réaménagement, pas un swizzle à retirer.

---

## 2. Qualité d'image

### 2.1 La moitié de la résolution horizontale manque

C'est la cause de l'illisibilité. La mesure porte sur les pixels à fort
gradient uniquement, sans quoi les zones plates gonflent le score :

| axe, dans **la même zone client** de la capture | paires adjacentes identiques |
|---|---|
| colonnes | **100,0 %** (32 800 échantillons) |
| lignes | 23,0 % (58 100 échantillons) |

L'argument tient sans témoin externe, et c'est ce qui le rend solide : les deux
chiffres viennent de la même image capturée par le même chemin. Un chemin de
capture qui diviserait la résolution diviserait **les deux** axes. Ici la
verticale porte tout son détail et l'horizontale n'en a aucun : la sortie est
donc **rendue en 640 de large puis doublée**, tandis que la hauteur est
réellement rendue à 896.

*(Un témoin sur la barre de titre donne 72,7 %, cohérent, mais il ne prouve rien
à lui seul : le cadre non client et la zone client ne transitent pas forcément
par le même chemin dans `PrintWindow`.)*

Aurora annonce pourtant `renderViewport 1280x896 logicalViewport 640x448`,
`Using framebuffer size 1280x960`, et `map_logical_viewport` (`lib/gx/gx.cpp`)
applique bien `scaleX` et `scaleY` symétriquement. **Prochaine mesure** :
journaliser `gfx::get_render_target_size()` et la largeur réelle du descripteur
de texture de la cible, une ligne bornée, pour voir lequel des trois ment.

### 2.2 35 % des dessins n'ont aucune texture liée

`issued=9739 of which without_texture=2448`, stable sur toute la session. Une
partie est légitime (géométrie colorée par sommet), 35 % demande vérification —
une liaison non résolue échantillonne zéro, ce qui « ressemble exactement à un
shader correct et produit du noir ».

### 2.3 Les couleurs de la vidéo THP sont délavées

Pendant la cinématique d'intro (un seul quad plein écran par image), tout est
surexposé. Écarté : les uniformes atteignant le shader sont **exactement** les
valeurs attendues de `THPDraw.c` (`colorRegs[0] = (-90, 0, -114, 135)`,
`kcolors` conformes). Le moteur 3D, lui, sort des couleurs justes. Reste à
examiner l'arithmétique des étages TEV du combine YUV, ou les plans Y/U/V
eux-mêmes.

---

## 3. Dettes consignées, à solder

### 3.1 L'interruption PE
Le pont force `DrawDone` (`.sbss:0x804060F8`) à 1 ; le drapeau ne cycle donc
jamais, un GP réellement en retard passerait inaperçu, et
`GXSetDrawDoneCallback`/`TokenCB` ne se déclenchent jamais. Demande le bloc PE
à `0xCC001000` et la cause PI `0x400`.

### 3.2 Le masque d'opcode
`cmd & CP_OPCODE_MASK` transforme un octet invalide en commande valide
(`command_processor.cpp:520`). Il a déjà fait avaler 135 Ko. Un non-opcode doit
être signalé.

### 3.3 Le disque est PAL, la machine est déclarée NTSC
`__OSTVMode` est laissé à zéro, ce qui se lit `VI_NTSC` pour un disque GMSP01
(PAL) — c'est écrit noir sur blanc dans `recomp_boot.cpp:967`. `kGuestFrameSeconds`
est figé à 1/60, ce qui est **cohérent avec ce choix**, donc les deux doivent
changer ensemble ou pas du tout. À dériver des registres VI.

---

## 4. Les instruments eux-mêmes

Cette session a été freinée trois fois par ses propres sondes plus que par le
jeu. Elles méritent d'être réparées.

- **16 050 avertissements « unresolved host call » au démarrage**, 2,9 Mo, un par
  adresse distincte. Or une absence de pont **n'est pas un échec** : le corps
  invité s'exécute normalement. Le message dit « needs a HostCallEntry » pour
  ~9 000 adresses dont presque aucune n'en a besoin, et enterre les vrais
  messages de boot. À remplacer par une ligne de résumé.
- **L'histogramme des primitives ne tient que 8 types** ; la 3D réelle en a plus
  et 30 604 dessins par seconde ne sont pas classés (`overflow=30604`).
- **La sonde `genMode` s'arrête à 32 changements**, tous pendant le boot : elle
  ne voit jamais le jeu. Elle m'a fait conclure à tort que le nombre d'étages
  TEV ne dépassait jamais 1 (la vraie distribution observée est 1, 2, 3 et 5).
- **`analyse_captures.py` doit rester le contrôle systématique** : c'est la
  différence image-à-image qui a nommé le gel, et le témoin de la barre de titre
  qui a tranché la question de résolution. Une sonde sans témoin ment.

---

## 5. Non vérifié

- **Carte mémoire.** Le fichier `.gci` actuel contient **4 octets non nuls sur
  57 344**, daté d'avant ces sessions. Une sauvegarde réussie a déjà été
  démontrée (4 236 octets non nuls), mais elle n'est **pas** revérifiée
  aujourd'hui, et aucune de ces sessions n'a déclenché de sauvegarde.
- **Audio sous charge.** Le flux SDL 32 kHz s'ouvre et reçoit ses tampons, mais
  rien ne mesure une sous-alimentation quand la 3D charge la machine.
- **Profil de jeu réel.** Le profil de blocs actuel mesure le décodeur THP, pas
  le jeu.
- **Campagnes de 15 et 30 minutes.**

---

## 6. Ce qui manque le plus

**Que quelqu'un y joue.** Les trois défauts corrigés ici l'ont été parce que le
jeu a enfin tourné assez longtemps pour les atteindre, pas parce qu'une sonde
les avait prévus. Les questions qui restent — est-ce que Mario répond, est-ce
que la caméra suit, est-ce que la vitesse est juste — ne se mesurent pas depuis
un script. En AZERTY : **A est sur Z**, B sur X, le stick sur ZQSD/WASD selon la
disposition physique, la caméra sur IJKL.
