# Game of Life — cheap & beautiful

Un petit économiseur d’écran en C99 + Xlib. Naissances menthe, colonies dorées,
traces bleu nuit. Les cellules traversent les bords de l’écran. Monde aléatoire,
figure connue, mélange de figures ou alternance : chaque scène s’efface en douceur
avant la suivante.

![Rendu réel du programme](img/preview.png)

## Lancer

Il faut un compilateur C, `make`, `pkg-config` et les en-têtes Xlib.
Sur Arch : `sudo pacman -S --needed base-devel libx11`.
Sur Debian/Ubuntu : `sudo apt install build-essential pkg-config libx11-dev`.

```sh
make
./game_of_life
```

**Espace** renouvelle le monde ; **Échap** ou **Q** ferme l’aperçu.

```sh
./game_of_life -fullscreen          # Une touche ou un clic pour sortir
./game_of_life -fps 10 -size 10      # Plus lent, plus gros, moins de calcul
./game_of_life -size 3 -trail 24     # Un tissu plus fin, des traces plus longues
./game_of_life -seed 42             # Reproduire un monde à dimensions identiques
./game_of_life -help
```

## Choisir un mode

```sh
./game_of_life --random                            # Le monde aléatoire d’origine
./game_of_life --figure pulsar                     # Une seule figure, centrée
./game_of_life --figure gosper-glider-gun -size 8   # Laisser de la place aux gliders
./game_of_life --random-figures                    # Mélange aéré de motifs connus
./game_of_life --alternate -seconds 30             # Alterner les trois scènes
./game_of_life --wallpaper                        # Fond animé, alternance adaptative
./game_of_life --list-figures                      # Les 810 noms disponibles
```

Le mode par défaut reste **`--random`**. `--mode random`, `--mode random-figures`
et `--mode alternate` sont également acceptés. Les modes sont exclusifs.
Les anciennes options à un tiret acceptent aussi deux tirets : `--fps`, `--size`,
`--fullscreen`, etc.

- **`--figure NOM`** dépose exactement une instance, sans bruit autour et sans
  préchauffage de la simulation. Le zoom automatique laisse au moins douze
  cellules libres de chaque côté quand la fenêtre le permet, et beaucoup plus
  pour les canons et les graines expansives. `-size` choisit un zoom explicite,
  réduit si nécessaire pour préserver cette marge.
  À chaque renouvellement, c’est cette même figure qui repart de sa forme initiale.
- **`--random-figures`** répartit des motifs tirés de la bibliothèque, avec des
  positions, rotations et symétries aléatoires. Six cellules d’espace les séparent
  au départ, avec environ un motif pour 1 200 cellules et au plus 128 motifs.
  La population initiale est plafonnée à 5 % de la grille ; ce plafond ne descend
  pas sous quatre cellules pour les minuscules aperçus.
  Ils évoluent ensuite librement et peuvent se rencontrer.
- **`--alternate`** enchaîne **aléatoire → figure seule tirée au sort → mélange
  de figures**, puis recommence. La durée s’adapte à leur développement ;
  `-seconds N` impose une durée fixe, hors effacement. **Espace** passe à la
  suivante dans l’aperçu en fenêtre.
  Redimensionner la fenêtre conserve le mode et la figure en cours.

Une petite légende en bas à droite indique la scène et, le cas échéant, le nom
de la figure. Sa bande réservée reste lisible sans recouvrir la simulation.
Une scène entièrement éteinte est renouvelée après deux secondes, afin de ne pas
laisser un écran vide, même avec une durée fixe. Une cadence effective plus basse
allonge ces durées ; une fenêtre entièrement masquée met l’animation et son
compteur en pause.
`-seed` rend aussi le choix et le placement des figures reproductibles à dimensions
identiques. La bibliothèque est intégrée au binaire, sans fichier à télécharger.

[Catalogue des figures et exemples visuels](FIGURES.md).

La bibliothèque comprend 810 motifs : 148 structures stables, 387 oscillateurs,
90 vaisseaux, 15 canons, 14 générateurs de débris, 5 râteaux à gliders,
25 graines à longue évolution et 126 autres motifs évolutifs. Essayer
`simkin-glider-gun`, `pufferfish`, `space-rake`, `lidka` ou `canada-goose`.
Ce catalogue figé de motifs compacts (au plus 128 × 128 cellules et 512 cellules
vivantes au départ) est documenté dans `FIGURES.md` ; il ne prétend pas couvrir
toutes les constructions possibles ni toutes les découvertes récentes.

## Fond d’écran animé

```sh
make install
~/.local/bin/game_of_life --wallpaper
```

`--wallpaper` choisit l’alternance adaptative par défaut. Sa fenêtre X11 reste
sous les applications, sans prendre le focus ni quitter sur une touche ou un
clic. Elle suit la taille de l’écran et s’arrête de calculer lorsqu’une autre
fenêtre la masque entièrement. Le fond précédent réapparaît à l’arrêt.

Pour une installation utilisateur persistante avec systemd :

```sh
mkdir -p ~/.config/systemd/user
cp contrib/game-of-life-wallpaper.service ~/.config/systemd/user/
systemctl --user import-environment DISPLAY XAUTHORITY
systemctl --user daemon-reload
systemctl --user enable --now game-of-life-wallpaper.service
```

Avec i3 lancé depuis `.xinitrc`, ajouter aussi cette ligne à `~/.config/i3/config`
après avoir importé `DISPLAY` et `XAUTHORITY` dans le service utilisateur :

```text
exec --no-startup-id systemctl --user restart game-of-life-wallpaper.service
```

Pour arrêter : `systemctl --user stop game-of-life-wallpaper.service`.
Pour retirer le démarrage automatique, supprimer cette ligne i3 et exécuter
`systemctl --user disable --now game-of-life-wallpaper.service`. Le fichier
d’unité fourni suppose l’installation par défaut dans `~/.local/bin`.

## Utiliser dans XScreenSaver

Le programme est autonome : aucune compilation des sources de XScreenSaver
n’est nécessaire.

```sh
make install                       # Installe dans ~/.local/bin
```

Ajouter cette ligne à la liste **`programs:`** dans `~/.xscreensaver`, en
remplaçant `/home/vous` par votre répertoire personnel :

```text
        "Game of Life"  /home/vous/.local/bin/game_of_life -root --alternate \n\
```

Puis ouvrir `xscreensaver-settings` et sélectionner **Game of Life**.
Si la liste `programs:` n’existe pas encore, enregistrer d’abord un réglage
dans cette interface pour créer le fichier. Conserver la liste existante.

XScreenSaver fournit la fenêtre via `XSCREENSAVER_WINDOW` : le programme dessine
dedans et lui laisse la gestion de l’inactivité, des entrées et du verrouillage.
`-window-id 0x…` permet aussi l’intégration dans une fenêtre existante.
Sans fenêtre fournie, `-root` dessine sur la fenêtre racine X11.
L’aperçu plein écran est un aperçu, pas un verrouillage de session.

Cette version cible **X11 / XScreenSaver** ; elle n’est pas un économiseur natif
Wayland. `make uninstall` retire le binaire installé. `PREFIX`, `BINDIR` et
`DESTDIR` sont configurables pour l’installation et le packaging.

## Pourquoi c’est léger

- **15 images/s**, avec attente bloquante entre les images et aucune rafale de
  rattrapage. La simulation s’arrête quand la fenêtre est entièrement masquée
  ou démappée.
- **Environ 64 000 cellules maximum par défaut**, grâce à une taille automatique
  adaptée à la résolution. En modes aléatoire et mélange de figures, à
  1920 × 1080, la grille contient 320 × 175 cellules avec la légende ;
  à 3840 × 2160, elle contient 320 × 177 cellules.
  `-size` permet de choisir explicitement une autre densité.
- **Deux grilles d’un octet par cellule** : état, âge et trace sont encodés ensemble.
  Une troisième marque les emplacements déjà explorés, une fois par seconde.
  Aucun tableau de hachage par pixel, aucune allocation pendant une génération.
- **Dessin différentiel**, par lots de rectangles de même couleur. Une cellule
  inchangée ne produit aucune commande de dessin. Aucun effacement global à
  chaque image, aucun moteur 3D, aucune texture plein écran.
- **Un halo d’une cellule** fait boucler la grille sans modulo dans la boucle
  de simulation. Les règles restent celles de Conway, B3/S23 ; les traces ne
  comptent jamais comme des cellules vivantes.

Par défaut, les durées suivent le type de scène :

| Scène | Temps d’observation avant renouvellement |
| --- | --- |
| Structure stable | 10 secondes |
| Oscillateur | Au moins 12 secondes et quatre périodes complètes |
| Vaisseau | 20 à 60 secondes, plafond prolongé si quatre périodes l’exigent |
| Canon, générateur de débris, râteau | 90 secondes à 5 minutes, selon l’activité |
| Graine expansive et autres formes évolutives | Au moins 30 secondes et 512 générations, puis jusqu’à 20 minutes tant que le monde se développe |
| Aléatoire et figures aléatoires | 15 secondes à 5 minutes, arrêt anticipé quand le monde devient inactif |

L’inactivité repose sur des empreintes d’occupation répétées (historique de 256
générations), ou sur vingt secondes sans exploration significative et avec peu
de naissances/morts. Le délai passe à trente secondes pour les graines expansives.
Le nombre de changements est moyenné sur chaque seconde pour ne pas dépendre
de la phase d’un oscillateur. Les traces colorées ne comptent pas comme activité.
Ces garde-fous visuels restent des heuristiques sur un monde torique fini ; ils
ne prouvent pas qu’une construction ne changera plus jamais. Les durées sont
des budgets de générations à la cadence demandée, hors pause et effacement.

| Option | Défaut | Valeurs |
| --- | --- | --- |
| `-size` | `0` : automatique | 0–128 pixels |
| `-fps` | 15 | 1–60 |
| `-trail` | 18 | 0–31 générations |
| `-seconds` | `0` : adaptatif | 0 ou 10–3600 secondes d’animation |
| `-seed` | variable | 0–4294967295 |

Une grille explicitement trop grande est refusée au-delà de 4 Mio par tampon.
La cadence est un plafond ; elle baisse si le dessin prend du temps.

Le [benchmark détaillé sur l’affichage réel et sous Xvfb, jusqu’en 8K](bench/README.md)
compare les résolutions et les densités du mode aléatoire sur un **Core i7-8750H**,
avant l’ajout de la bibliothèque de figures.
Sur l’écran Full HD testé, `-size 4` atteint environ **10 % d’un cœur logique**
pour le programme et le serveur graphique réunis ; `-size 5` garde une marge
autour de **7 %**, avec **3,3 Mo de RAM résidente** pour le programme.
Le binaire avec sa bibliothèque fait environ 260 Ko. En 4K automatique sous Xvfb,
le CPU total mesuré en mode aléatoire reste
entre **8,25 et 9,80 %**. Ces chiffres ne mesurent pas la consommation électrique.

## Vérifier

```sh
make check
make clean
make CFLAGS='-O1 -g -std=c99 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer' check all
```

Les tests couvrent les blocs stables, les oscillateurs, un planeur qui traverse
les deux bords, le vieillissement, l’effacement et les redimensionnements
invalides. Ils comparent également 4 200 générations à une implémentation de
référence sur des grilles allant de 1 × 1 à 127 × 83, avec trois longueurs de trace.
La bibliothèque est vérifiée pour ses 810 populations initiales et ses huit
symétries. Les seize premières générations de chaque motif sont comparées à une
simulation indépendante ; les cycles complets et déplacements des 625 motifs
périodiques sont également vérifiés, ainsi que l’émission du canon de Gosper,
la disparition de Diehard et le placement aéré reproductible sous le plafond
de densité.
Les tests de scènes couvrent également les cycles longs, la production continue,
l’expansion de Bunnies, les mélanges d’oscillateurs, les durées explicites et les
marges laissées autour des figures.

Le rendu a aussi été vérifié dans un serveur Xvfb isolé : intégration dans une
fenêtre fournie, animation, redimensionnements de 1 × 1 à 4K, pause lorsque la
fenêtre est cachée, renouvellement, touches de l’aperçu et arrêt par SIGTERM.
Douze destructions supplémentaires de la fenêtre hôte à 60 images/s vérifient
la fermeture pendant un dessin. Ces essais passent avec AddressSanitizer,
UndefinedBehaviorSanitizer et LeakSanitizer.
Les nouveaux modes ont également été vérifiés sous Xvfb avec ces outils :
810 figures seules, redimensionnements, transitions automatiques et passage à la
scène suivante avec Espace.
Le fond animé est vérifié pour son ordre d’empilement, l’absence de prise de
focus, la pause CPU lorsqu’il est masqué et la persistance des légendes.

`life.c` contient uniquement la simulation ; `figures_data.inc` les graines ;
`figures.c` leur placement ; `scene.c` adapte le zoom et les durées ;
`game_of_life.c` gère X11, la légende, la palette et la cadence. Les références
de vérification sont dans `tests/figures_expected.inc`.
Les anciennes animations restent dans `img/` pour
mémoire ; `preview.png` montre le rendu actuel.
