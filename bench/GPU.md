# Calcul et affichage GPU — version 0.5, 27 octobre 2026

Le moteur OpenGL est optionnel (`--gpu`). Le moteur CPU/Xlib reste le choix
par défaut, et les compilations sans OpenGL restent disponibles.
[Utilisation et architecture](../docs/GPU.md).

## Protocole

Machine locale : Core i7-8750H, NVIDIA Quadro P1000, pilote 580.178.04,
OpenGL 4.6, écran physique 1920 × 1080 sous Xorg/i3. Compilation GCC 16.2,
`-O3`, cible de 15 images/s. Le pilote sélectionne ici la carte NVIDIA ; ces
mesures ne portent pas sur le GPU Intel intégré.

Chaque essai conserve un monde pendant 30 secondes, après 0,5 seconde de
préchauffage. L'initialisation et le préchauffage du monde aléatoire sur CPU
sont donc exclus de la fenêtre principale. Les graines 42 et 2026 servent aux
mondes aléatoires ; la graine 42 sert au mélange. Les tailles sont lues dans
les recettes produites, avec une légende de 26 pixels.

Le CPU indiqué additionne le programme et Xorg, en pourcentage d'un cœur
logique. Xorg inclut aussi l'activité éventuelle des autres clients ; les
relevés de référence graphique sont conservés sans être soustraits. La cadence
réelle diffère entre moteurs : les mondes n'atteignent pas exactement la même
génération après 30 secondes. Il s'agit du coût d'une animation à regarder,
pas d'une comparaison à nombre de générations identique.

La sonde CPU attend `XSync`. La sonde GPU place une clôture OpenGL après le
dessin et ne consigne une image qu'après signalement de cette clôture, lors
d'un passage ultérieur. Les horodatages GPU correspondent à la soumission ;
les dernières images encore en attente à l'arrêt ne sont pas comptées. La
file est limitée à huit images, au-delà le benchmark échoue. Cela évite
l'attente forcée d'un `glFinish` après chaque image, qui fausserait le coût
CPU du programme livré. Un essai supplémentaire utilise le binaire livré
sans aucune sonde de cadence.

## Résultats

| Monde | Grille | CPU avec moteur CPU | CPU avec moteur GPU | Cadence CPU → GPU |
| --- | --- | ---: | ---: | ---: |
| Aléatoire, automatique | 640 × 351 | 10,9 % | 2,5 % | 13,77 → 14,65 fps |
| Aléatoire, cellules de 1 px | 1 920 × 1 054 | 51,1 % | 2,2 % | 9,62 → 14,68 fps |
| Mélange, cellules de 1 px | 1 920 × 1 054 | 15,8 % | 2,5 % | 12,87 → 14,67 fps |

À 2 023 680 cellules, le binaire GPU **sans instrumentation** consomme 2,1 %
d'un cœur au total : 0,77 % pour le programme et 1,30 % pour Xorg, sur un
essai de 30 secondes avec la graine 42. La cadence n'est pas mesurée dans cet
essai. Les relevés avec sonde situent l'animation autour de 14,7 images/s.

Le contexte et le pilote ajoutent de la mémoire : environ **57 à 115 Mio de
RAM résidente** pour le processus GPU pendant ces essais, contre **6 à 11 Mio**
en mode CPU. Ces chiffres n'incluent pas la mémoire du serveur Xorg et ne sont
pas des mesures de VRAM.

**Aucun gain énergétique n'est démontré.** Le compteur de puissance de cette
Quadro renvoie « N/A ». Les chiffres CPU ne mesurent ni la charge du GPU ni les
watts de la machine. La taille automatique reste volontairement la même dans
les deux moteurs ; la grille plus fine demande `--size 1` explicitement.
Il s'agit de fenêtres courtes sur un seul modèle de carte, pas d'une promesse
pour toutes les machines ou pour une alternance de longue durée.

[Relevés bruts, empreintes et paramètres](gpu-render-2026-10-27.json).


## Vérifications

- Le moteur GPU sur la Quadro a été comparé au CPU sur 27 mondes et
  2 700 générations : tous les états, âges, traces et fondus coïncident.
  Cela couvre les grilles de 1 × 1 à 1 920 × 1 054, les dimensions non multiples
  de huit, les bords toriques et les traces de longueur 0, 18 et 31. Les
  populations, changements, découvertes et empreintes GPU sont vérifiés.
- 1 080 images sous OpenGL logiciel/Xvfb correspondent pixel par pixel au
  rendu CPU, dans les trois modes : couleurs, espacements, légendes et
  frontières de fondu/renouvellement des scènes compris.
- Les contrôles existants du journal, de la navigation sur 64 scènes,
  des entrées et de l'inactivité ont été exercés avec OpenGL sous
  AddressSanitizer/UBSan. La détection des fuites du pilote est désactivée
  pour ces exécutions ; aucune conclusion LeakSanitizer n'en est tirée.
- Les trois suites existantes passent avec GCC et avec Clang, y compris une
  compilation sans libepoxy ni libXss. Le code de simulation CPU reste intact.

## Refaire les mesures

Trouver le PID de Xorg et vérifier la carte effectivement utilisée avec
`game-of-life doctor --gpu`. Les commandes suivantes ouvrent temporairement
un aperçu en plein écran ; `--graphics-pids` attend le PID de votre serveur.

```sh
python3 bench/run.py --desktop --graphics-pids PID_XORG --engine gpu \
  --cases 1920x1080:0,1920x1080:1 --warmup 0.5 --phases 15,15 \
  --seed 42 --output /tmp/life-gpu.json
# Refaire avec --engine cpu, puis avec --seed 2026.
# Pour les mélanges : --mode random-figures.
# Pour le binaire installé sans sonde de cadence :
python3 bench/run.py --desktop --graphics-pids PID_XORG --engine gpu \
  --binary "$HOME/.local/bin/game_of_life" --no-frame-probe \
  --cases 1920x1080:1 --warmup 0.5 --phases 15,15 \
  --seed 42 --output /tmp/life-gpu-installed.json
```

Sans `--desktop`, le benchmark utilise Xvfb. Celui-ci peut exécuter les shaders
avec llvmpipe sur le CPU : cela permet de vérifier les résultats, mais ne
mesure pas la performance d'une carte graphique.
