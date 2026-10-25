# Batched incremental rendering

The renderer now visits the board once, skips unchanged cells, coalesces adjacent
dirty cells of the same colour into horizontal runs, and submits rectangles in
bounded batches of 256 per colour. It no longer builds a growing cell queue or
scans that queue once for each of the 50 trail colours. Rectangle storage is
fixed at 100 KiB per instance; drawing requires no per-frame allocation.

First draw, Expose events and the existing reshape/restart paths invalidate the
screen so incremental updates cannot leave stale pixels. A full repaint also
clears margins. Comparison uses the final next board, after extinction handling,
so fading trails and the intentional cycle-breaking changes are drawn.

## Reproduce the renderer benchmark

Requires Python 3 and a C99 compiler, with no Python dependencies or X server:

```sh
python3 benchmarks/render.py
python3 benchmarks/render.py --sanitize
```

The script extracts the actual optimized rendering functions from game_of_life.c.
It compares pixels across 80 frames, including trails, exposure/full redraws,
unchanged cells, erasure, colour runs, batch boundaries and non-unit cell sizes.
It then times 60 frames per workload, with 640 x 360 cells and 50 colours.

One optimized-build run in the development environment (2026-10-25):

| Workload | Reference ms/frame | Optimized ms/frame | Ratio | Drawing calls/frame |
| --- | ---: | ---: | ---: | ---: |
| Unchanged | 16.454 | 0.195 | 84.22x | 230400 -> 0 |
| About 10% dirty | 17.074 | 0.693 | 24.62x | 230400 -> 100 |
| All dirty | 16.750 | 1.684 | 9.95x | 230400 -> 903 |

These are headless renderer measurements, not whole-screensaver FPS. X11 calls
are local recording/counter functions. The reference implements the previous
50-pass cell rendering algorithm without its queue-management and coordinate
division overhead. Simulation, transport to an X server, compositor and display
are excluded. Timings vary with the machine and workload; ratios are not a
guarantee of a 10x improvement for the complete application.

The full source was also compiled against a temporary X11 shim and exercised
for 500 generations with framebuffer comparisons, exposure, hashing and board
restarts under ASan/UBSan. No error was reported in those scenarios. Leak
detection is disabled in the benchmark because the development environment
cannot provide the process metadata it requires.

## Real-display validation still required

Build with real XScreenSaver headers and compare frame times on an X server at
the same resolution, cell size, trail count and delay. Include first paint,
Expose events, restart, and resizing. The existing global simulation state and
resize/allocation handling still need a separate lifecycle review; this change
does not establish multi-screen or arbitrary-resize safety.

The default delay remains 100000 microseconds (roughly 10 FPS before rendering
cost). For a fluidity experiment, use `-delay 16667`; this changes the generation
rate and is not a fair before/after performance comparison. Keep the delay
identical when measuring speed or CPU usage.

## Low-CPU operation (the default goal)

The default `-cpu-percent 5` asks the draw callback to use at most approximately
5% of one CPU core over its work/sleep cycle. It measures callback CPU time with
`clock()` and returns a longer timer delay when needed. It never busy-waits.
The original `-delay` is still a minimum pause, so a cheap generation keeps the
existing cadence. Expensive generations slow down rather than chase a frame rate.

Examples (options passed to the built screensaver executable):

```sh
-cpu-percent 5
-cpu-percent 2 -delay 200000 -size 4
```

The second example favours low CPU with a smaller simulation grid and a slower
cadence. `-cpu-percent 100` disables the additional CPU-based pacing for fair
benchmarks. The default minimum delay remains 100000 microseconds.

This is a soft per-instance callback budget, not an OS-enforced process limit.
It excludes X server/compositor work and other callbacks; scheduling and clock
resolution also affect observed utilisation. Real-display process CPU usage and
power consumption still need measurement. The default does not target 60 FPS.

## Less work per generation

A sliding three-column sum reuses neighbour observations, reducing eight live
neighbour comparisons to three fresh observations per cell plus the centre
classification. A transition lookup preserves births, survival and fading. The
cycle hash is accumulated during evolution when needed, avoiding a second board
scan. If a generation is unchanged, drawing is skipped altogether unless an
exposure/restart or cycle-breaking fade requires it.

```sh
python3 benchmarks/evolve.py
python3 benchmarks/evolve.py --sanitize
```

The benchmark compares the actual new evolution function to the previous
8-neighbour loop on 3000 deterministic boards (trail counts 2, 3 and 50, narrow
grids, both hash modes). It checks every cell, occupied counts, change flags and
hashes. Optimized-build runs measured roughly 1.8–6.6x faster generation kernels,
depending on density and hash mode; these ratios do not multiply directly with
the renderer ratios above.

A separate full-source comparison over 400 frames with X11 recording shims,
starting at 640 x 360 cells, produced identical board digests and cycle counters
on every frame. Callback CPU time averaged 2.936 ms/frame before the evolution
change and 2.100 ms/frame after (about 28% less CPU work). Hashing, cycle breaking,
restarts and incremental rendering were included; real X11 costs and sleeps were
excluded. Full-source ASan/UBSan framebuffer checks also pass for 500 generations.
The pacing arithmetic was checked for 5%, 50%, 100%, minimum-delay and unavailable
clock cases. Timings remain environment-specific.
