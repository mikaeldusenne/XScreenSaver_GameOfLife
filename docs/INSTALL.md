# Install Game of Life

[Overview](../README.md) · [Using it](USAGE.md) · [Français](README.fr.md)

One C99 renderer, Xlib, and an optional libXss dependency for idle detection.
The default CPU engine needs no GPU API. The optional `--gpu` engine uses
libepoxy and an OpenGL 4.3/GLX driver. No network service, downloaded pattern
pack or Python runtime is needed. [GPU requirements and setup](GPU.md).

## Where it runs

| Session | Preview | Wallpaper + idle screensaver |
| --- | --- | --- |
| Linux / X11, including ARM machines | Yes | Yes; desktop shells may cover the wallpaper |
| FreeBSD / other BSDs with X11 | Source build | Same X11 design; desktop validation still needed |
| macOS + XQuartz, Intel or Apple Silicon | X11 window | No native macOS wallpaper or screensaver |
| Linux / Wayland + Xwayland | X11 window | No; native Wayland windows/input are outside X11's control |
| Windows + WSL 2 + WSLg | Linux GUI window | No native Windows wallpaper or screensaver |
| Native Windows, mobile, browser | No | No |

`game-of-life doctor` checks the current display. It does not change settings.
Known Wayland, WSL and macOS sessions reject `--wallpaper` explicitly. An X11
window existing on a system does not imply control of its whole desktop.
The idle saver is an animation, not a session lock.

### Verified builds

On 26 October 2026, compilation and all existing simulation checks passed on
Arch Linux (GCC and Clang), Debian 12, Fedora 43 and Alpine 3.22/musl. Installation
and removal were exercised in temporary prefixes, including paths with spaces.
The DEB and RPM were also installed in clean Debian/Fedora containers with their
declared runtime dependencies. X11 rendering and input were checked on the local
Linux system using isolated Xvfb sessions. macOS, BSD and ARM desktop execution
have not been verified in this session; the source recipes and optional CI
matrix describe the intended build paths rather than additional test results.

## 1. Install build dependencies

Choose your platform. These commands use your package manager; the project
installer itself never runs `sudo`, installs packages or downloads code.
Git is only needed if you clone rather than download the source archive.

### Debian / Ubuntu / Mint / Raspberry Pi OS

```sh
sudo apt install build-essential cmake pkg-config libx11-dev libxss-dev git
```

### Fedora

```sh
sudo dnf install gcc make cmake pkgconf-pkg-config libX11-devel libXScrnSaver-devel git
```

### Arch / EndeavourOS / Manjaro

```sh
sudo pacman -S --needed base-devel cmake libx11 libxss git
```

### openSUSE

```sh
sudo zypper install gcc make cmake pkg-config libX11-devel libXss-devel git
```

### Alpine

```sh
doas apk add build-base cmake pkgconf libx11-dev libxscrnsaver-dev git
```

### FreeBSD

The base system provides a C compiler and make. In an existing X11 session:

```sh
sudo pkg install cmake pkgconf libX11 libXScrnSaver git
```

On other BSDs, use the base X11 development files or the corresponding packages,
then use CMake below. If X11 is installed outside the standard search paths, pass
`-DCMAKE_PREFIX_PATH=/usr/X11R6` or `/usr/pkg` as appropriate. BSD recipes are
provided for source builds; the CI matrix does not claim BSD desktop coverage.

### macOS

Install Apple's command line tools and [XQuartz](https://www.xquartz.org/).
With Homebrew:

```sh
xcode-select --install
brew install cmake pkgconf libx11 libxscrnsaver
brew install --cask xquartz
```

After the first XQuartz installation, sign out and back in, then open XQuartz.
Use a terminal in that session. The Homebrew libraries build the app; XQuartz
provides its display. If CMake cannot locate the Homebrew libraries:

```sh
CMAKE_PREFIX_PATH="$(brew --prefix)" ./install.sh
```

This installs an X11 program, not a `.app` or `.saver` bundle. Start it from the
terminal with `~/.local/bin/game-of-life`. Fullscreen behavior is managed by
XQuartz and may differ from a Linux window manager.

### Windows / WSLg

Follow Microsoft's [WSL GUI setup](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps).
In an Ubuntu WSL 2 terminal, use the Debian/Ubuntu dependencies above and the
installation below. Keep the `DISPLAY` provided by WSLg. The installed desktop
entry can be exposed by WSLg in the Windows Start menu. There is no native `.scr`
integration or Windows-wide idle detection.

## 2. Install as yourself

```sh
git clone https://github.com/mikaeldusenne/XScreenSaver_GameOfLife.git
cd XScreenSaver_GameOfLife
./install.sh
~/.local/bin/game-of-life
```

The installer builds into `build-install` and installs into `~/.local`:

- `bin/game_of_life`: the renderer and its original CLI.
- `bin/game-of-life`: the friendly command entry point.
- An application-menu entry with Pulsar, Bunnies and fullscreen actions.
- An SVG icon, a manual, Bash completion and reference documentation.

It does not edit your shell, change monitor power settings, enable autostart or
restart a running animation. Add `~/.local/bin` to your shell's PATH if needed.
On freedesktop desktops, `~/.local/share/applications` is normally discovered
automatically; a menu refresh or a new session may be needed.

Custom destination and build directory:

```sh
./install.sh --prefix "$HOME/Apps/Life" --build-dir "$HOME/.cache/life-build"
```

A custom data directory may need to be added to `XDG_DATA_DIRS` for menu/icon
discovery. Configure the final prefix before building: generated desktop entries
contain its absolute executable path. Do not relocate an installed tree or use
`cmake --install --prefix` to override it; reconfigure with the new prefix instead.

## CMake, packaging and the small Makefile

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build --parallel
cmake --install build
```

libXss is detected automatically. Without it, previews, wallpaper and integration
with an external XScreenSaver still build; only `--idle` is unavailable.
Use `-DLIFE_WITH_XSS=OFF` to omit it explicitly. `game-of-life version` reports
the feature. To run the existing simulation checks, configure with
`-DBUILD_TESTING=ON`, build, then run `ctest --test-dir build --output-on-failure`.
The checks remain enabled even in a Release build.

libepoxy is also detected automatically. Without it, the CPU engine builds as
before. Use `-DLIFE_WITH_GPU=OFF` for a build without OpenGL dependencies, or
`make WITH_GPU=0` with GNU Make. Compiling GPU support does not enable it at
runtime: the default stays CPU, and `--gpu` explicitly selects OpenGL.

The original GNU Make route remains available: `make`, `make check`,
`make install PREFIX="$HOME/.local"`. It installs **only the renderer**.
For a minimal build use `make clean && make WITH_XSS=0`. On BSD use `gmake` for
this Makefile, or prefer CMake. Rebuild after changing Make feature flags.

Packagers can stage the full installation without touching the host:

```sh
cmake -S . -B build-package -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-package --parallel
DESTDIR="$PWD/stage" cmake --install build-package
cd build-package
cpack -G TGZ
# On a Debian build host with dpkg-shlibdeps:
cpack -G DEB
# On an RPM build host with rpmbuild:
cpack -G RPM
```

Packages/archives are specific to the build OS, architecture and linked library
versions. They are not static universal binaries. Build on the oldest system
you intend to support. CPack derives DEB/RPM runtime dependencies on its build
host. Packaging does not start or enable any service.

## Start with your session

Try `game-of-life screensaver` first. It uses the wallpaper plus a two-minute
idle timeout. Quit the foreground command with Ctrl+C when finished trying it.
Do not run two startup mechanisms for the same animation.

This is the built-in interactive saver: the wallpaper pauses when fully hidden,
rises above applications when idle, and returns behind them on any key. It
supports next/previous clicks and scroll zoom, and does not lock the session.
The separate [XScreenSaver integration](#xscreensaver) starts the animation only
when idle and delegates input and optional locking to XScreenSaver.

### i3 and other minimal window managers

Without systemd, add this to i3's config, replacing the home path:

```text
exec --no-startup-id /home/you/.local/bin/game_of_life --wallpaper --alternate --idle 120
```

With a systemd user session and the default `~/.local` installation, run from
the repository:

```sh
mkdir -p ~/.config/systemd/user
cp contrib/game-of-life-wallpaper.service ~/.config/systemd/user/
```

The supplied service uses the CPU engine. To reproduce the development i3
machine's GPU setup, first install the [GPU dependencies](GPU.md#requirements),
rebuild with `./install.sh`, and check `~/.local/bin/game-of-life doctor --gpu`.
Then replace `ExecStart` in `~/.config/systemd/user/game-of-life-wallpaper.service`
with:

```ini
ExecStart=%h/.local/bin/game_of_life --wallpaper --alternate --idle 120 --gpu --random-size 1 --classic
```

This keeps the two-minute timeout, enables GPU calculation and rendering,
and uses the classic mint/blue/gold palette. Use `--random-size 2` on a 4K
display; `1` is suitable for Full HD. Single figures retain automatic framing.

Enable the service from a terminal in your graphical session:

```sh
systemctl --user import-environment DISPLAY
if [ -n "${XAUTHORITY:-}" ]; then
    systemctl --user import-environment XAUTHORITY
fi
systemctl --user daemon-reload
systemctl --user enable --now game-of-life-wallpaper.service
```

For i3 launched by `startx`, put the display environment import commands above
in `.xinitrc` before `exec i3`, and add this to i3's config:

```text
exec --no-startup-id systemctl --user restart game-of-life-wallpaper.service
```

This covers sessions that do not start `graphical-session.target`.
The example service assumes `~/.local/bin`; edit `ExecStart` for another prefix.
Do not use the native idle service in a Wayland session.

If you edit a service that is already running, apply the changes with
`systemctl --user daemon-reload`, then
`systemctl --user restart game-of-life-wallpaper.service`.

Stop: `systemctl --user stop game-of-life-wallpaper.service`. Disable: remove
the i3 startup line and run `systemctl --user disable --now game-of-life-wallpaper.service`.

### Other X11 desktops

Add `game_of_life --wallpaper --alternate --idle 120` using the desktop's Startup
Applications settings, with the full installed path if necessary. A desktop
shell that paints its own background may cover the wallpaper. Use the preview
or XScreenSaver integration on such desktops instead of assuming identical
wallpaper behavior everywhere.

### XScreenSaver

Add to the `programs:` list in `~/.xscreensaver`, substituting your home path:

```text
        "Game of Life"  /home/you/.local/bin/game_of_life -root --alternate \n\
```

Select it in `xscreensaver-settings`. Keep existing entries. XScreenSaver owns
idle detection, input and locking here; the program only renders in the supplied
window. This works without libXss in the renderer. The next/previous mouse
controls belong to standalone windows, not windows managed by XScreenSaver.

## Uninstall or troubleshoot

Disable any autostart you enabled, then run `./install.sh --uninstall` using the
same build directory (`--build-dir` if customized). Only files from that build's
installation manifest are removed; parent directories and your settings remain.
For a manual CMake installation: `cmake --build build --target uninstall`.
Do not delete the build directory until you no longer need its manifest.

Run `game-of-life doctor` if the window does not appear. An unavailable display
usually means the command is running outside the graphical session. Missing
core fonts can be fixed by installing the platform's X11 misc/fixed-font package
(e.g. `xfonts-base` on Debian). For package installation, prefer your package
manager's normal uninstall command.

References: [XQuartz installation](https://www.xquartz.org/releases/),
[Fedora libXss](https://packages.fedoraproject.org/pkgs/libXScrnSaver/libXScrnSaver-devel/),
[FreeBSD X11 handbook](https://docs.freebsd.org/en/books/handbook/x11/),
[freedesktop launchers](https://xdg.pages.freedesktop.org/xdg-specs/desktop-entry/latest-single/).
