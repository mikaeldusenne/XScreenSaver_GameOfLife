

# XScreenSaver Game of Life hack

The incremental renderer batches X11 rectangles by colour and avoids repainting
unchanged cells. See [PERFORMANCE.md](PERFORMANCE.md) for measured results,
the reproducible benchmark and the remaining real-display validation.

![](img/xscreensaver_wide.gif)

![](img/xscreensaver_zoom.gif)


Low consumption is the default: `-cpu-percent 5` adjusts the pause to the measured
CPU cost of each frame. For a more economical setting, use
`-cpu-percent 2 -delay 200000 -size 4`. This is a soft callback budget; see
[PERFORMANCE.md](PERFORMANCE.md) for its scope and the real-display checks still
required.
