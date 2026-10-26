<p align="center"><img src="contrib/game-of-life.svg" width="88" alt="A mint and gold glider"></p>

# Game of Life · cheap & beautiful

**810 small worlds. Room to grow. One tiny program.**

Mint births, midnight-blue trails, golden colonies. A tiny C99 + Xlib program
that turns Conway's Life into something worth leaving on your screen.

[Install](docs/INSTALL.md) · [Explore](docs/USAGE.md) · [810 figures](FIGURES.md) · [Français](docs/README.fr.md)

![Game of Life running on a real X11 window](img/preview.png)

## Make yourself a small universe

Install your platform's [build dependencies](docs/INSTALL.md#1-install-build-dependencies), then:

```sh
git clone https://github.com/mikaeldusenne/XScreenSaver_GameOfLife.git
cd XScreenSaver_GameOfLife
./install.sh
~/.local/bin/game-of-life
```

The installer adds a menu launcher, an icon, a manual and Bash completion
under `~/.local`.
No root access, background service or startup changes are made by the installer.
Once `~/.local/bin` is in your PATH, just type `game-of-life`.

```sh
game-of-life figure pulsar       # A perfect rhythm
game-of-life figure bunnies      # A tiny seed, a very long story
game-of-life mix                 # Let classic figures meet
game-of-life figures gun         # Find something new
game-of-life screensaver         # X11 wallpaper + saver after two idle minutes
game-of-life doctor              # What can this session do?
```

**Left click → next. Right click → previous.** In the idle saver, any key brings
you back to work and leaves the wallpaper alive. The 64-scene history replays
the same initial worlds. Controls and the current figure are labeled discreetly.
English and French captions follow the system locale, or `--lang en|fr`.

<table>
<tr><td><img src="img/figure-pulsar.png" alt="Pulsar, a symmetrical oscillator"></td><td><img src="img/figure-koks-galaxy.png" alt="Kok's galaxy"></td></tr>
<tr><td><b>Pulsar</b> · watch a rhythm</td><td><b>Kok's galaxy</b> · watch a rotation</td></tr>
</table>

## Thoughtful defaults

- **Space to grow.** Single figures are centered with margins, with extra room
  for glider guns and expansive seeds.
- **Time to see it.** Still lifes, oscillators, producers and explosions get
  different observation budgets. Settled random worlds give way to fresh ones.
- **Quiet when hidden.** A fully covered wallpaper pauses. Rendering batches
  only changed cells, at 15 fps and roughly 64,000 cells by default.
- **One small executable.** Around 260 KB on the development machine, including
  all 810 figures. No network access, 3D engine or runtime asset downloads.

The library includes 148 still lifes, 387 oscillators, 90 spaceships, 15 guns,
14 puffers, 5 rakes, 25 long-lived seeds and 126 other evolving patterns.
[Browse the catalogue](FIGURES.md), or search it from the terminal.

## Platforms

| Platform | What you get |
| --- | --- |
| Linux / X11 | Preview, wallpaper, interactive idle saver, XScreenSaver integration |
| BSD / X11 | Portable source build; desktop behavior still needs platform validation |
| macOS / XQuartz | X11 preview; Intel and Apple Silicon build configuration |
| Linux / Wayland | Preview through Xwayland |
| Windows / WSLg | Linux GUI preview on Windows |

Wallpaper and whole-session idle handling require an X11 desktop. XQuartz,
Xwayland and WSLg do not make this a native macOS, Wayland or Windows saver.
The built-in idle mode is not a lock screen. [Platform details and dependencies](docs/INSTALL.md).

The core remains deliberately small: CMake for portable builds and packaging,
or the original GNU Makefile for a renderer-only installation. libXss is optional
unless you want native idle detection. A standard XScreenSaver can manage idle
and locking instead. [Autostart and XScreenSaver setup](docs/INSTALL.md#start-with-your-session).

## Build, check, package

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The existing checks compare 4,200 generations to an independent implementation,
verify all 810 pattern seeds and eight symmetries, and exercise adaptive timing
and framing. X11 behavior has also been exercised under Xvfb and i3, including
the real 120-second idle threshold, history replay, focus and hidden-window CPU
pause. See the [historical CPU benchmarks](bench/README.md) for measurements and
their limits; these are not electricity-consumption measurements.

CMake supports staging with `DESTDIR` and building native TGZ, DEB or RPM
packages via CPack. Builds and simulation checks pass on Arch, Debian, Fedora
and Alpine; the DEB and RPM have been installed in clean containers.
A [ready-to-enable CI recipe](ci/README.md) covers Linux
x86-64/ARM64, macOS Intel/Apple Silicon, and Alpine. It is provided as a template,
not an active workflow or a claim that every platform has been tested.
[Build and packaging details](docs/INSTALL.md#cmake-packaging-and-the-small-makefile).

## History & vibe coding

Development began in **2015**. The first commit was made in **2024**.
The first AI assistance arrived in **late October 2026**, beginning the
project's **“vibe coding”** chapter.

## License and pattern sources

The original permissive software notice is preserved in [LICENSE](LICENSE).
Pattern provenance, source RLE links and the scope of the catalogue are
documented in [FIGURES.md](FIGURES.md).
