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
