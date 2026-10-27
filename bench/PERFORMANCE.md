# Historique compact et efficacité — 27 octobre 2026

Cette campagne décrit la version 0.3. Voir les [mesures de la version 0.4](DENSE.md)
pour le calcul de huit cellules à la fois et la nouvelle définition par défaut.

Le moteur réutilise maintenant les sommes verticales de trois cellules pour
faire glisser son voisinage : trois nouvelles comparaisons de voisinage par
cellule, au lieu de huit. Les règles, âges, traces, empreintes et bords toriques
restent identiques. Le dessin groupe aussi les segments horizontaux de même
couleur quand les cellules sont jointives et envoie jusqu’à 1 024 rectangles
par lot au lieu de 256. Aucun nouveau moteur graphique ni dépendance.

## Calcul seul : environ un tiers de CPU en moins

Core i7-8750H, GCC 16.2, `-O2`, un seul thread. Médianes de six mesures par
taille et version : graines 42 et 2026, trois répétitions, ordre alterné.
Chaque mesure commence après le préchauffage de 32 générations et calcule
exactement 300 générations, sans affichage ni attente. Horloge CPU du processus.
Référence : le moteur de `522a6c9`. Les empreintes et populations finales sont
identiques avant/après dans les 18 paires.

| Grille | Avant, ms/génération | Après | Réduction |
| --- | ---: | ---: | ---: |
| 480 × 263 | 0,757 | 0,513 | 32 % |
| 960 × 527 | 3,099 | 2,054 | 34 % |
| 1920 × 1054 | 12,407 | 8,179 | 34 % |

[36 mesures brutes](compute-2026-10-27.json). Ce gain concerne le calcul,
pas l’intégralité du programme, du serveur X11 ou de la consommation électrique.

```sh
cc -O2 -std=c99 -I. bench/compute.c life.c -o /tmp/life-compute
/tmp/life-compute 960 527 300 42
# Comparer avec le moteur précédent, en gardant le même benchmark :
git show 522a6c9:life.c > /tmp/life-before.c
cc -O2 -std=c99 -I. bench/compute.c /tmp/life-before.c -o /tmp/life-before
/tmp/life-before 960 527 300 42
```

## Avec le dessin : X11 reste une part importante du coût

Xvfb 1920 × 1080 isolé, même machine, `-O2`, deux graines, ordre avant/après
inversé entre les graines, trois secondes de chauffe puis quinze de mesure.
La bande de légende retire 26 pixels à la hauteur de la grille. Moyennes des
deux graines ; CPU programme + Xvfb, en pourcentage d’un cœur logique.
La sonde de `bench/frames.c` attend X11 avec `XSync` à chaque image.

| Réglage | Grille | CPU avant | CPU après | Images/s avant → après |
| --- | --- | ---: | ---: | ---: |
| 4 px, cible 15 fps | 480 × 263 | 13,0 % | 11,1 % | 13,30 → 13,43 |
| 3 px, cible 15 fps | 640 × 351 | 21,7 % | 21,0 % | 12,17 → 12,30 |
| 3 px, cible 10 fps | 640 × 351 | 17,5 % | 13,5 % | 8,53 → 8,83 |

[12 mesures brutes](render-2026-10-27.json). Ces fenêtres temporelles ne finissent
pas exactement à la même génération : la cadence et l’évolution du monde
influencent le travail de dessin. Les résultats varient avec les graines et la
machine ; ils ne démontrent pas qu’une grille deux fois plus fine devient
économe. Xvfb n’est pas un moniteur physique ni un relevé du GPU.

Le budget automatique d’environ 64 000 cellules et les 15 fps restent inchangés.
La pause lorsque le fond est entièrement recouvert reste active. L’extinction
de l’écran reste gérée par le bureau. Les mesures de l’affichage réel du
26 octobre restent consultables dans [le benchmark initial](README.md).

Pour refaire une série avec affichage :

```sh
python3 bench/run.py --cases 1920x1080:4,1920x1080:3 --seconds 15 \
  --seed 42 --output /tmp/life-render.json
python3 bench/run.py --cases 1920x1080:3 --fps 10 --seconds 15 \
  --seed 2026 --output /tmp/life-slower.json
```

## Pistes suivantes, à mesurer avant adoption

1. **Calcul sur des mots de 64 bits.** Un bit par cellule vivante permet de
   traiter plusieurs cellules avec une opération logique. Il faut conserver
   séparément les âges et les traces, puis mesurer le coût de cette conversion.
2. **Blocs actifs.** Éviter les régions stabilisées et leurs dessins, en tenant
   compte des blocs voisins, du bouclage et des traces encore en évolution.
   Cela semble surtout utile après la phase dense d’un monde aléatoire.
3. **Image partagée pour les grilles très fines.** Comparer les rectangles
   différentiels à une image envoyée avec `XShmPutImage` sur un serveur local,
   avec repli Xlib classique. L’extension [MIT-SHM de X11](https://www.x.org/releases/X11R7.5/doc/Xext/mit-shm.pdf)
   permet ce partage mémoire, mais impose de synchroniser les buffers ; son
   existence ne garantit pas un gain pour tous les motifs ou serveurs.

HashLife convient aux motifs très réguliers et aux grands sauts de générations.
Pour la soupe aléatoire chaotique que l’on veut regarder image par image, ce
n’est pas mon premier choix : [la documentation de Golly](https://golly.sourceforge.io/Help/Algorithms/HashLife.html)
signale ses mauvaises performances sur ces motifs. Les trois pistes ci-dessus
sont des propositions, pas des fonctionnalités déjà présentes.

## Jusqu’où compresser l’état initial ?

Un monde quelconque de 1 920 × 1 080 cellules demande **259 200 octets** à un
bit par cellule pour son occupation seule. Les motifs structurés se prêtent
bien au RLE ; du bruit indépendant à 30 % de cellules vivantes a une entropie
d’environ 0,881 bit/cellule, soit environ **228 430 octets** en moyenne idéale.
On ne peut donc pas compresser arbitrairement toutes les grilles aléatoires.

Nos mondes sont générés par un PRNG à état de **32 bits** : la graine tient dans
**4 octets**. La recette permet de recalculer le reste. Le journal texte ajoute
version, date, dimensions, mode, traces, figure et empreinte de compatibilité.
Les 72 lignes de l’essai de navigation font **4 694 octets** au total : 60 à 78
octets par ligne, 65 en moyenne. Gzip les ramène à **1 662 octets**, environ 23
octets par monde sur cet échantillon seulement. Les futurs fichiers peuvent
avoir un autre taux de compression. Le programme conserve le texte simple et
ne compresse pas pendant l’animation.

Cela représente environ **650 ko pour 10 000 scènes**, contre des gigaoctets
de captures de grilles. Ce n’est pas une compression universelle : il faut un
générateur et une bibliothèque compatibles pour reconstruire une recette.
L’empreinte détecte les différences ; elle ne remplace pas une copie autonome
de la grille. [Format, options et relecture](../docs/USAGE.md#keep-a-world).
