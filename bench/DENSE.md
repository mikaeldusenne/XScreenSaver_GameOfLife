# Grilles aléatoires plus fines — moteur 0.4, 27 octobre 2026

Le mode aléatoire utilise maintenant **256 000 cellules au maximum en taille
automatique**, contre 64 000 auparavant, avec des cellules d’au moins deux
pixels. En Full HD, cela donne des cellules de **3 pixels au lieu de 6**, soit
environ **quatre fois plus de cellules**. Les scènes aléatoires de l’alternance
en bénéficient aussi. Depuis la version **0.4.1**, les mélanges de figures
utilisent également ce budget ; les figures seules conservent leur zoom.
Les mesures du mode aléatoire ci-dessous restent celles de la version 0.4.0.

## Ce qui accélère le programme

Le moteur traite **huit cellules en parallèle dans un entier de 64 bits**.
Chaque octet est une cellule ; les sommes restent inférieures à 256 et ne
débordent pas dans l’octet voisin. Les états, âges et traces restent identiques.
Le code utilise des opérations C99 et `memcpy`, sans instruction CPU spécifique,
nouvelle dépendance, thread supplémentaire ou calcul GPU.

Pendant le calcul, chaque ligne mémorise les bornes des cellules qui ont changé.
Le dessin saute les lignes intactes, leurs marges et les groupes de huit
cellules inchangées. Un expose ou un changement de scène redessine toujours
toute la grille. Les couleurs, traces et générations intermédiaires sont
conservées, de même que les empreintes et la relecture des anciens journaux.

## Calcul seul, à grille identique

Core i7-8750H, GCC 16.2, compilation `-O3`, un seul thread. Référence : version
0.3.0, commit `dab32e9`. Chaque mesure calcule 300 générations après un
préchauffage de 32 ou 1 024 générations. Médianes de deux graines (42, 2026) et
deux répétitions avec ordre avant/après inversé. Toutes les populations et
empreintes finales coïncident dans les **16 paires**.

| Grille | Génération de départ | 0.3, ms/génération | 0.4 | Accélération |
| --- | ---: | ---: | ---: | ---: |
| 640 × 351 | 32 | 0,913 | 0,376 | ×2,43 |
| 640 × 351 | 1 024 | 0,754 | 0,292 | ×2,58 |
| 960 × 527 | 32 | 2,055 | 0,861 | ×2,39 |
| 960 × 527 | 1 024 | 1,704 | 0,687 | ×2,48 |

[Mesures brutes du calcul](dense-compute-2026-10-27.json) et
[compilateur, empreintes des sources et binaires](dense-metadata-2026-10-27.json).
Ce résultat concerne le calcul seul ; le serveur X11 reste une part importante
du coût total.

## Coût des nouveaux réglages par défaut

Les mesures avec affichage suivent un même monde dans des fenêtres successives :
0–15, 15–45, 45–90 et 90–180 secondes environ. Le benchmark impose `--seconds 3600`
pour observer son vieillissement sans changement de scène et commence le relevé
0,1 seconde après le lancement. En usage normal, l’heuristique d’inactivité peut
passer plus tôt au monde suivant.

Xvfb isolé ; CPU du programme **et** du serveur graphique, en pourcentage d’un
cœur logique. La sonde attend `XSync` après chaque dessin, ce qui compte les
images traitées par le serveur et évite de mesurer une file qui s’allonge.
La cadence maximale reste 15 fps. Le budget CPU initial est plus élevé parce
que beaucoup de cellules et de traces changent simultanément.

Full HD, moyennes des graines 42 et 2026. L’ancien réglage compte **320 × 175**
cellules ; le nouveau **640 × 351** (avec la police Xvfb utilisée).

| Âge du monde | CPU 0.3, ancien défaut | CPU 0.4, nouveau défaut | Cadence 0.4 |
| --- | ---: | ---: | ---: |
| 0–15 s | 8,4 % | 22,1 % | 12,33 fps |
| 15–45 s | 5,0 % | 11,9 % | 13,42 fps |
| 45–90 s | 3,7 % | 8,3 % | 13,77 fps |
| 90–180 s | Non mesuré | 5,7 % | 14,09 fps |

La finesse supérieure coûte donc davantage, surtout au démarrage. Ce n’est pas
une promesse de CPU identique pour quatre fois plus de cellules. Le coût descend
sous 10 % d’un cœur dans les fenêtres 45–90 s et 90–180 s des deux mondes mesurés.
La moyenne sur les trois minutes vaut 8,8 %, **en maintenant le même monde** ;
elle ne prédit pas celle d’une alternance qui renouvelle ses scènes plus tôt.

En **4K**, la nouvelle taille automatique est de 6 pixels, soit **640 × 355**
cellules. Un essai avec la graine 42 donne :

| Âge du monde | Programme | Xvfb | Total | Cadence |
| --- | ---: | ---: | ---: | ---: |
| 0–15 s | 7,8 % | 18,5 % | 26,3 % | 11,73 fps |
| 15–45 s | 5,0 % | 8,9 % | 13,9 % | 13,13 fps |
| 45–90 s | 4,0 % | 5,7 % | 9,7 % | 13,60 fps |

Le programme occupe **4,4 à 4,6 Mio de RAM résidente** dans ces trois essais,
hors serveur graphique. La 4K est mesurée sous Xvfb, pas sur un moniteur 4K réel.
Ce sont des moyennes par fenêtre, pas des pics instantanés ni des limites
universelles. La pause lorsqu’une autre fenêtre recouvre le fond reste active.

[Relevés complets 0.4](dense-render-2026-10-27.json) et
[référence 0.3](dense-baseline-2026-10-27.json). Les essais de référence démarrent
leur premier relevé au lancement, ceux de 0.4 après 0,1 seconde. Les durées
mesurées et les cadences réelles sont conservées dans les fichiers.

## Mélanges de figures — version 0.4.1

`--random-figures`, `game-of-life mix` et les mélanges de l’alternance utilisent
désormais le même budget de 256 000 cellules. Le moteur optimisé sert déjà aux
deux modes. Les figures seules gardent leur zoom ; `--size` reste prioritaire.
Le générateur de mélanges reste identique : au plus 128 figures, six cellules
vides entre elles, et au plus 5 % de cellules vivantes au départ sur les grilles
de taille normale. Les anciennes recettes restent rejouables à leurs dimensions
d’origine.

Même machine et protocole Xvfb que ci-dessus, compilation GCC `-O3`, cible de
15 fps, un mélange conservé pendant 90 secondes. En Full HD : moyenne des graines
42 et 2026 sur **640 × 351** cellules de 3 pixels. En 4K : graine 42 sur
**640 × 355** cellules de 6 pixels. CPU total du programme et de Xvfb, en
pourcentage d’un cœur logique :

| Âge du mélange | CPU Full HD | Cadence Full HD | CPU 4K | Cadence 4K |
| --- | ---: | ---: | ---: | ---: |
| 0–15 s | 5,4 % | 14,10 fps | 7,9 % | 13,80 fps |
| 15–45 s | 5,4 % | 14,12 fps | 7,1 % | 13,93 fps |
| 45–90 s | 5,6 % | 14,09 fps | 6,4 % | 13,96 fps |

Sur les 90 secondes, les deux graines Full HD donnent respectivement **6,6 %**
et **4,4 %**, soit **5,5 % en moyenne** ; l’essai 4K donne **6,9 %**. Le programme
occupe environ **4,5 Mio** de RAM résidente, hors serveur graphique.
Les mélanges peuvent garder des oscillateurs ou des producteurs actifs : leur
coût ne diminue pas nécessairement avec le temps. Ces trois essais ne couvrent
pas toutes les combinaisons de figures ni le renouvellement adaptatif des scènes.

[Mesures brutes et empreintes des sources](dense-mix-2026-10-27.json).
Les vérifications existantes passent avec GCC et Clang. La relecture des trois
modes a été vérifiée sous AddressSanitizer/UBSan, ainsi que le recadrage des
anciennes recettes. Une ancienne recette de mélange conserve son empreinte ;
l’alternance prend bien la nouvelle grille, et `--size 6` conserve l’ancienne
définition Full HD.

## Vérifications et reproduction

Les vérifications existantes comparent les états à une implémentation
indépendante, valident les 810 figures et la temporisation des scènes.
Elles passent avec GCC et Clang, avec et sans libXss. La relecture, la navigation
sur 64 scènes, les légendes, la pause lorsque le fond est couvert et les captures
d’entrée ont aussi été exercées sous Xvfb, avec AddressSanitizer/UBSan.

Le programme conserve les réglages d’extinction de l’écran. Ces mesures ne sont
pas une mesure de consommation électrique ni une garantie de performance pour
tous les processeurs, motifs ou serveurs X11.

```sh
# Calcul, sans affichage : 300 générations après 1 024 générations de chauffe.
cc -O3 -std=c99 -I. bench/compute.c life.c -o /tmp/life-compute
/tmp/life-compute 640 351 300 42 1024

# Affichage isolé, suivi du vieillissement avec les réglages automatiques.
python3 bench/run.py --cases 1920x1080:0 --warmup 0.1 \
  --phases 15,30,45,90 --seed 42 --output /tmp/life-aging.json
# Répéter avec --seed 2026, puis --cases 3840x2160:0 pour la 4K.

# Mélanges de figures, avec le même budget de grille depuis la version 0.4.1.
python3 bench/run.py --mode random-figures --cases 1920x1080:0 --warmup 0.1 \
  --phases 15,30,45 --seed 42 --output /tmp/life-mix.json
```

`bench/run.py` compile désormais en `-O3`, comme l’installation CMake Release,
et lit les dimensions réelles dans une recette temporaire. `--binary CHEMIN`
permet de mesurer un autre binaire instrumenté ; ajouter `--no-frame-probe` pour
un binaire normal (la cadence n’est alors pas mesurée). Les journaux de mesure
restent dans le dossier temporaire du benchmark, pas dans l’historique personnel.

Pour privilégier un coût encore plus bas sur une machine lente, `--size 6`
retrouve l’ancienne densité en Full HD et `--fps 10` réduit la cadence demandée.
