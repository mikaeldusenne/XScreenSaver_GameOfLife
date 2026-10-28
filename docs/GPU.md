# An optional GPU engine

[Usage](USAGE.md) · [Installation](INSTALL.md) · [Measurements](../bench/GPU.md)

Version 0.5 adds GPU simulation **and** rendering. The original CPU/Xlib engine
remains the default. It is still possible to build and install the program
without any OpenGL dependency.

```sh
game-of-life doctor --gpu
game-of-life random --gpu --size 1 --fullscreen
game-of-life mix --gpu --size 2
game-of-life figure bunnies --gpu
game-of-life fancy --fullscreen
game-of-life figure pulsar --fancy
game-of-life preview --gpu --random-size 1 --fullscreen
game-of-life replay --gpu
game-of-life replay 42 --gpu
game-of-life preview --cpu
```

GPU rendering is opt-in for every command, including `screensaver` and
`wallpaper`: choose `--gpu`, or `--fancy` for the flame style.
There is no automatic switch to a different engine. If the build or display
cannot support it, the program explains the failure; use `--cpu` to continue
with Xlib. Installing this version does not change existing autostart commands.

## Requirements

- An X11/GLX display with an **OpenGL 4.3 core** driver.
- **libepoxy** development files when compiling GPU support. Epoxy loads the
  OpenGL entry points; there is no CUDA, Vulkan or separate shader compiler.
- An X11 window visual compatible with the driver's GLX framebuffer formats.

Add the appropriate package to the normal build dependencies:

| Platform | Additional development package |
| --- | --- |
| Debian / Ubuntu | `libepoxy-dev` |
| Fedora / openSUSE | `libepoxy-devel` |
| Arch | `libepoxy` |
| Alpine | `libepoxy-dev` |
| FreeBSD | `libepoxy` |

Then rebuild with `./install.sh`. CMake and GNU Make detect libepoxy
automatically. CPU-only builds remain available:

```sh
cmake -S . -B build-cpu -DLIFE_WITH_GPU=OFF
cmake --build build-cpu
# Or with GNU Make:
make clean
make WITH_GPU=0
```

`game-of-life version` reports whether GPU support was compiled in.
`game-of-life doctor --gpu` actually creates a context, compiles the shaders
and names the renderer. **llvmpipe and softpipe mean software rendering on the
CPU**, even though the OpenGL code path is active. Xvfb commonly uses this for
correctness checks; it cannot establish hardware GPU performance.

XQuartz/macOS should use the CPU engine: this GPU engine requires OpenGL 4.3.
BSD, Xwayland and WSLg depend on their GLX driver capabilities; GPU execution
there has not been validated in this release. The existing restrictions on
wallpaper and whole-session idle detection still apply.

## Same worlds and controls

Both engines use the same automatic cell size and frame-rate limit. A higher
definition is explicit: `--size 1` gives roughly two million cells in Full HD;
`--size 2` does the same on a 4K display. The existing allocation limit remains
about four million cells, including the CPU grid's border. Larger worlds need
a larger cell size. A single figure still keeps room around its seed.

For alternation, `--random-size 1` makes random worlds and figure mixes finer
while single figures keep their automatic zoom. Unlike the global `--size`,
this override only affects those two scenes. `--random-size 0` follows the
global setting. Replays always keep their recorded dimensions.

The rules, wrapping edges, cell ages, fading trails, palette, captions,
previous/next navigation, idle presentation and pause when covered are shared.
Scroll zooms around the pointer, and middle click restores the initial view,
without changing or restarting the simulated world. Zoom does not reduce the
simulation cost: cells outside the viewport still evolve.

From version 0.6, GPU cells have rounded, antialiased corners at sizes of five
pixels or more. The fragment shader evaluates a rounded rectangle using the
existing cell read: no extra texture, render pass or CPU work per cell. Smaller
cells remain square. `--square-cells` restores the original GPU appearance,
also useful for pixel comparisons with the CPU renderer. Rounding changes only
the display and never affects journal checksums or the rules of Life.

On the development Quadro P1000, an isolated Full HD fragment draw took about
0.47–0.49 ms with square cells and 0.54–0.56 ms with rounded cells at sizes
8, 32 and 128: roughly 0.07–0.08 ms extra. Sizes 1 and 4 stayed around
0.47–0.49 ms in either mode. These are GPU draw times, not whole-frame, CPU
or power measurements. The offscreen RGBA8 target was 1920 × 1080, with every
cell occupied; 20 square/rounded pairs were timed in alternating order after
four warmup pairs, using `GL_TIME_ELAPSED` and 66.7 ms between draws.
[Raw measurements](../bench/rounded-2026-10-27.json).

Version 0.7 adds `--fancy`: hot cream-colored births, older amber cells,
cooling ember trails and subtle rising flames. It selects the GPU engine;
`--classic` disables the effect while retaining that engine. `--cpu` selects
the classic CPU renderer. The last applicable option wins.

Fancy uses the existing age/trail byte, three extra bounded world samples and
two smooth oscillations in the same fragment pass. It allocates no intermediate
texture and does no CPU work per cell. The original cell silhouettes stay crisp,
including rounded corners; the caption stays outside the effect. A periodic
clock advances with the simulation, so covering the wallpaper pauses the effect,
and zooming or exposing the window does not advance it. These are display-only
flames: no extra simulation, particles or changes to replay recipes.

![Pulsar with warm cells, ember trails and subtle rising flames](../img/fancy-pulsar.png)

On the same Quadro P1000, a 20-second fullscreen run at 1920 × 1054 cells
with fancy enabled reached **14.7 fps**, with **2.0% of one CPU core** for the
program and Xorg combined (about 1.0% each). Seed 42, 15 fps target, 0.5-second
warmup, Release/O3 build and the same asynchronous frame probe as the original
GPU benchmarks. Startup is excluded, and Xorg includes other desktop activity.
[Full run](../bench/fancy-render-2026-10-28.json).

An offscreen Full HD draw comparison measured 0.50 ms classic / 0.89 ms fancy
at one pixel per cell. At cell sizes 8, 32 and 128, fancy took 2.15, 3.90 and
5.26 ms respectively. GPU clocks remained automatic, so these times include
clock-state variation; they are not normalized shader throughput. Each size
used 20 alternating classic/fancy pairs after four warmup pairs, 66.7 ms apart,
on a fixed seed-42 world after 32 warmup generations. `GL_TIME_ELAPSED` covered
only the fragment draw to an RGBA8 target, not simulation or presentation.
These short measurements establish no electricity saving.
[Draw measurements](../bench/fancy-draw-2026-10-28.json).

To repeat the fullscreen run (it temporarily covers the desktop):

```sh
python3 bench/run.py --desktop --graphics-pids PID_XORG --engine gpu \
  --style fancy --cases 1920x1080:1 --warmup 0.5 --phases 10,10 \
  --seed 42 --output /tmp/life-fancy.json
```

The scene's initial state is generated on the CPU, including the 32-generation
random warmup, so existing `life1` recipes and their checksums remain valid.
After upload, every animation generation is calculated on the GPU.

The two evolving grids stay in one-byte integer textures on the GPU. A third
texture remembers explored cells. An OpenGL compute shader applies Conway's
rules; a fragment shader draws cells and colors. The text is rasterized with
the existing X11 font only when its caption changes and uploaded as a small
mask. No full-grid download or CPU simulation runs during the animation.

Twenty bytes of statistics return to the CPU each generation: population,
changes, exploration and an occupation fingerprint. They are read before
the next generation, after the regular frame-rate sleep, to avoid immediately
waiting for a just-issued GPU calculation. A very slow GPU can still make this
read wait for completion.
Exploration is sampled once per simulated second, as in the CPU engine.
The GPU uses a parallel 64-bit occupation fingerprint for cycle detection;
this is separate from the unchanged journal checksum. Observation budgets,
extinction detection and the inactivity heuristic remain shared.

## What “cheap” means here

A GPU can make much finer grids practical, but lower CPU use does not establish
lower electricity consumption. Driver/context memory also adds overhead.
A dedicated GPU that would otherwise sleep can cost more to keep awake. Keep
15 fps, retain the hidden-window pause, and compare on your own machine.
This version does not change GPU selection, driver power settings or screen
blanking. A driver can select a dedicated GPU even on a laptop with an iGPU.

The [benchmark notes](../bench/GPU.md) distinguish hardware measurements from
software OpenGL checks and document the tested grids and limitations.
