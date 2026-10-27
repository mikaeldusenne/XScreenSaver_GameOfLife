# Game of Life — cheap & beautiful

Un petit économiseur d’écran en C99 + Xlib. Naissances menthe, colonies dorées,
traces bleu nuit. Les cellules traversent les bords de l’écran. Monde aléatoire,
figure connue, mélange de figures ou alternance : chaque scène s’efface en douceur
avant la suivante.

![Rendu réel du programme](../img/preview.png)

## Installation complète

Consulter le [guide multiplateforme](INSTALL.md), puis lancer `./install.sh`.
Cette installation ajoute le lanceur `game-of-life`, une entrée dans le menu,
une icône, un manuel et la complétion Bash. `game-of-life doctor` vérifie
la compatibilité de la session. Les légendes suivent la langue du système ;
`--lang fr` force le français, `--lang en` l’anglais.

L’aperçu est accessible via XQuartz sur macOS et WSLg sur Windows ;
les modes fond d’écran et inactivité demandent une session X11 complète.
Le guide ci-dessous conserve aussi les commandes directes du moteur.

## Lancer

Il faut un compilateur C, `make`, `pkg-config` et les en-têtes Xlib et Xss.
Sur Arch : `sudo pacman -S --needed base-devel libx11 libxss`.
Sur Debian/Ubuntu : `sudo apt install build-essential pkg-config libx11-dev libxss-dev`.

```sh
make
./game_of_life
```

**Espace** renouvelle le monde ; **Échap** ou **Q** ferme l’aperçu.
**Clic gauche** passe à la scène suivante, **clic droit** rejoue la précédente.

```sh
./game_of_life -fullscreen          # Une touche pour sortir ; clics pour naviguer
./game_of_life -fps 10 -size 10      # Plus lent, plus gros, moins de calcul
./game_of_life -size 3 -trail 24     # Un tissu plus fin, des traces plus longues
./game_of_life -seed 42             # Reproduire un monde à dimensions identiques
./game_of_life -help
```

## Garder et retrouver un monde

Le fond d’écran et son mode inactivité enregistrent automatiquement une ligne
par état initial dans `~/.local/state/game-of-life/history.tsv`, ou sous
`$XDG_STATE_HOME` si défini. L’aperçu n’écrit rien sans `--history FICHIER`.

```sh
game-of-life replay                      # Dernière ligne complète
game-of-life replay 42                   # Rejouer la ligne 42
game-of-life random --history ~/life.tsv # Journal explicite pour un aperçu
game-of-life replay 1 --history ~/life.tsv
game-of-life screensaver --no-history    # Désactiver l’enregistrement
```

Une ligne fait généralement **60 à 90 octets**, même pour une grille de plusieurs
millions de cellules : version, date, mode, dimensions, graine, traces, nom de
figure et empreinte de vérification. Aucun enregistrement à chaque image.
La relecture conserve exactement la grille initiale, indépendamment de la taille
de la fenêtre ; si elle ne tient pas à un pixel par cellule, la vue est recadrée.
Revenir en arrière n’ajoute pas de doublon aux mêmes dimensions.

C’est une recette déterministe : conserver une version compatible du programme
et de sa bibliothèque pour les archives durables. Une empreinte différente fait
refuser la relecture, au lieu de montrer silencieusement un autre monde.
Le fichier reste un texte ordinaire, sans rotation ni effacement automatique.
[Format et détails](USAGE.md#keep-a-world).

## Choisir un mode

```sh
./game_of_life --random                            # Le monde aléatoire d’origine
./game_of_life --figure pulsar                     # Une seule figure, centrée
./game_of_life --figure gosper-glider-gun -size 8   # Laisser de la place aux gliders
./game_of_life --random-figures                    # Mélange aéré de motifs connus
./game_of_life --alternate -seconds 30             # Alterner les trois scènes
./game_of_life --wallpaper                        # Fond animé, alternance adaptative
./game_of_life --wallpaper --idle 120              # Aussi en screensaver après 2 minutes
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
À gauche, si la largeur le permet, un rappel indique les clics de navigation.
Les 64 dernières scènes sont mémorisées par leur graine aléatoire : revenir en
arrière rejoue la même figure ou le même monde depuis son début, à dimensions
identiques. Un clic gauche après un retour retrouve la scène suivante déjà vue.
À la première scène conservée, le clic droit ne fait rien. Le mode `--figure NOM`
continue de montrer uniquement la figure demandée.
Une scène entièrement éteinte est renouvelée après deux secondes, afin de ne pas
laisser un écran vide, même avec une durée fixe. Une cadence effective plus basse
allonge ces durées ; une fenêtre entièrement masquée met l’animation et son
compteur en pause.
`-seed` rend aussi le choix et le placement des figures reproductibles à dimensions
identiques. La bibliothèque est intégrée au binaire, sans fichier à télécharger.

[Catalogue des figures et exemples visuels](../FIGURES.md).

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

Ajouter **`--idle 120`** pour faire passer cette même animation devant les
applications après deux minutes sans activité clavier ou souris, y compris
devant les fenêtres plein écran d’i3. Il n’y a qu’un processus et une simulation.
Les clics gauche/droit changent de scène sans quitter le screensaver ; déplacer
la souris le laisse affiché. **Une touche, par exemple Échap, ramène au bureau**
et laisse le fond animé actif. Un nouveau délai complet précède la réactivation.
Les clics sur le fond visible fonctionnent aussi hors du screensaver.

Le compteur d’inactivité X11 est interrogé au plus une fois par seconde ; le
déclenchement a donc une précision d’environ une seconde. Les touches et clics
du screensaver sont consommés, sans les transmettre à l’application dessous.
Le focus est conservé, et une capture d’entrée déjà détenue par un autre programme
(par exemple un verrouilleur) empêche son activation. Les délais de mise en veille
de l’écran restent ceux de la session. Ce mode n’est pas un verrouillage : il
ne demande aucun mot de passe. Pour utiliser la gestion d’inactivité et de
verrouillage de XScreenSaver, suivre plutôt la section suivante.
L’extension utilisée est documentée dans le [manuel Xss de X.Org](https://xorg.freedesktop.org/releases/X11R6.9.0/doc/html/XScreenSaverQueryInfo.3.html).

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
Il active aussi le screensaver après deux minutes ; retirer `--idle 120` de
`ExecStart` pour garder uniquement le fond animé, puis exécuter
`systemctl --user daemon-reload` et `systemctl --user restart game-of-life-wallpaper.service`.

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
Les clics de navigation concernent les fenêtres autonomes et le fond animé ;
dans XScreenSaver, les entrées restent gérées par XScreenSaver.
`-window-id 0x…` permet aussi l’intégration dans une fenêtre existante.
Sans fenêtre fournie, `-root` dessine sur la fenêtre racine X11.
L’aperçu plein écran est un aperçu, pas un verrouillage de session.

Cette version cible **X11 / XScreenSaver** ; elle n’est pas un économiseur natif
Wayland. `make uninstall` retire le binaire installé. `PREFIX`, `BINDIR` et
`DESTDIR` sont configurables pour l’installation et le packaging.

## Un moteur GPU en option

Le moteur CPU reste le choix par défaut. `--gpu` active le calcul et le dessin
OpenGL 4.3, avec les mêmes figures, couleurs, traces, commandes et historiques.
La taille automatique reste identique ; `--size 1` permet d’essayer un monde
plus fin. Le programme se compile aussi sans aucune dépendance OpenGL.

```sh
game-of-life doctor --gpu
game-of-life random --gpu --size 1 --fullscreen
game-of-life mix --gpu
game-of-life preview --cpu
```

[Installation GPU et limites](GPU.md). La baisse du CPU ne suffit pas à prouver
une baisse de consommation électrique. Une carte dédiée peut coûter plus cher
à maintenir active ; le fond masqué continue de suspendre toute simulation.

## Pourquoi c’est léger

- **15 images/s**, avec attente bloquante entre les images et aucune rafale de
  rattrapage. La simulation s’arrête quand la fenêtre est entièrement masquée
  ou démappée.
- **Budget de 256 000 cellules pour les mondes aléatoires et les mélanges de
  figures**, grâce au moteur optimisé de la version 0.4. En Full HD, environ
  640 × 350 cellules de 3 pixels remplacent les 320 × 175 cellules de 6 pixels :
  quatre fois plus de cellules.
  La hauteur exacte dépend de la police de légende. Cela s’applique aussi dans
  l’alternance ; les figures seules gardent leur zoom adapté. Les mélanges
  restent aérés : au plus 128 figures, six cellules vides entre les figures,
  et au plus 5 % de cellules vivantes au départ sur une grille de taille
  normale. Les anciens historiques gardent leurs dimensions.
  `-size 6` retrouve l’ancienne densité en Full HD.
- **Deux grilles d’un octet par cellule** : état, âge et trace sont encodés ensemble.
  Une troisième marque les emplacements déjà explorés, une fois par seconde.
  Aucun tableau de hachage par pixel, aucune allocation pendant une génération.
- **Dessin différentiel sur CPU**, par lots de rectangles de même couleur. Une cellule
  inchangée ne produit aucune commande de dessin. Aucun effacement global à
  chaque image, aucun moteur 3D, aucune texture plein écran.
- **Huit cellules calculées ensemble**, dans les huit octets d’un entier 64 bits,
  en C99 portable. Les portions de lignes inchangées sont sautées lors du dessin.
  Le démarrage d’un monde aléatoire reste plus coûteux que sa phase stabilisée.
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
| `--idle` | `0` : désactivé | 0–86400 secondes, avec `--wallpaper` |

Une grille explicitement trop grande est refusée au-delà de 4 Mio par tampon.
La cadence est un plafond ; elle baisse si le dessin prend du temps.

Les [mesures du moteur 0.4](../bench/DENSE.md) suivent le CPU au démarrage puis
pendant le développement du monde. Le
[benchmark historique sur l’affichage réel et sous Xvfb, jusqu’en 8K](../bench/README.md)
compare les résolutions et les densités du mode aléatoire sur un **Core i7-8750H**,
avant l’ajout de la bibliothèque de figures.
Sur l’ancienne version Full HD testée, `-size 4` atteignait environ **10 % d’un cœur logique**
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
La navigation est vérifiée avec de vrais événements de souris, dont le retour
exact sur les 64 scènes conservées. Les essais d’inactivité couvrent le retour
au bureau, le nouveau délai, les captures d’entrée concurrentes et leur libération
à l’arrêt. Une session i3 isolée vérifie aussi les 120 secondes réelles devant
une application plein écran.

`life.c` contient uniquement la simulation ; `figures_data.inc` les graines ;
`figures.c` leur placement ; `scene.c` adapte le zoom et les durées ;
`game_of_life.c` gère X11, la légende, la palette et la cadence. Les références
de vérification sont dans `tests/figures_expected.inc`.
Les anciennes animations restent dans `img/` pour
mémoire ; `preview.png` montre le rendu actuel.

## Histoire et « vibe coding »

Les premiers développements ont commencé en **2015**, et le premier commit
a été créé en **2024**. La première assistance IA est arrivée **fin octobre 2026** :
c’est le début du chapitre « vibe coding » de ce projet.
