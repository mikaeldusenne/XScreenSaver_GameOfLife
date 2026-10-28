#!/bin/sh
# Build/install as the current user. Never downloads code or invokes sudo.
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
prefix=${PREFIX:-"$HOME/.local"}
build_dir=${BUILD_DIR:-"$source_dir/build-install"}
action=install
usage() {
    cat <<'HELP'
Usage: ./install.sh [--prefix /absolute/path] [--build-dir /absolute/path]
                    [--uninstall | --help]

Build and install Game of Life, its launcher, icon, manual and completion.
Default destination: ~/.local. No sudo, no download, no session changes.
Requires a C compiler, CMake, make or Ninja, and X11 development headers.
Idle screensaver support also needs libXss (optional).
The optional browser duel server needs libwebsockets development files.

Uninstall using the same build directory: ./install.sh --uninstall
For dependencies by platform, see docs/INSTALL.md.
HELP
}
while [ "$#" -gt 0 ]; do
    case $1 in
        --prefix|--build-dir)
            [ "$#" -ge 2 ] || { usage >&2; exit 2; }
            case $2 in /*) ;; *) printf '%s\n' 'Use an absolute path.' >&2; exit 2 ;; esac
            if [ "$1" = --prefix ]; then prefix=$2; else build_dir=$2; fi
            shift 2 ;;
        --uninstall) action=uninstall; shift ;;
        --help|-h) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done
if ! command -v cmake >/dev/null 2>&1; then
    printf '%s\n' 'CMake is missing. Install the dependencies listed in docs/INSTALL.md.' >&2
    exit 1
fi
if [ "$action" = uninstall ]; then
    [ -f "$build_dir/install_manifest.txt" ] || {
        printf 'No installation manifest in %s\n' "$build_dir" >&2; exit 1;
    }
    cmake --build "$build_dir" --target uninstall
    printf '%s\n' 'Installed files removed. Existing startup settings are yours to keep or remove.'
    exit
fi
case $prefix in /*) ;; *) printf '%s\n' 'PREFIX must be absolute.' >&2; exit 2 ;; esac
printf '\n%s\n\n' 'Game of Life — let small things grow.'
cmake -S "$source_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="$prefix"
cmake --build "$build_dir" --parallel 2
cmake --install "$build_dir"
printf '\nInstalled. Try:\n  "%s/bin/game-of-life"\n  "%s/bin/game-of-life" doctor\n' "$prefix" "$prefix"
if [ -x "$prefix/bin/life-duel-server" ]; then
    printf '  "%s/bin/game-of-life" game  # Two-player browser game\n' "$prefix"
fi
case :${PATH:-}: in
    *:"$prefix/bin":*) ;;
    *) printf '\nAdd "%s/bin" to PATH to use the shorter game-of-life command.\n' "$prefix" ;;
esac
printf '%s\n' 'Nothing was added to startup. See docs/INSTALL.md when you are ready.'
