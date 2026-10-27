# Small worlds, room to grow

[Install](INSTALL.md) · [All 810 figures](../FIGURES.md) · [Français](README.fr.md)

Start with `game-of-life`. It alternates a random world, a known figure and a
sparse mix of figures. No configuration file is needed.

## Pick a mood

| Want to watch… | Run |
| --- | --- |
| A little of everything | `game-of-life` |
| A perfectly repeating pulsar | `game-of-life figure pulsar` |
| A continuous stream of gliders | `game-of-life figure gosper-glider-gun` |
| A small seed become an explosion | `game-of-life figure bunnies` |
| Classic figures meeting each other | `game-of-life mix` |
| Random cells finding their own rhythm | `game-of-life random` |
| A quieter, slower world | `game-of-life preview --fps 10` |
| A screen-filling view | `game-of-life preview --fullscreen` |

The menu launcher also offers Pulsar, Bunnies and fullscreen actions where
the desktop supports them.

## Explore

```sh
game-of-life figures gun
game-of-life figures spaceship
game-of-life figure copperhead
```

The search is a case-sensitive substring match on the canonical name or family.
Bash completion offers the full figure library after `game-of-life figure`.
If your shell does not automatically load local completions, source
`~/.local/share/bash-completion/completions/game-of-life` from your Bash config.

**Left click** advances. **Right click** returns to the previous scene and
replays it from the start. The last 64 seeds are kept, using only a small history
buffer. The same scene is reproduced at the same window dimensions. Returning
forward retraces scenes already seen. At the oldest stored scene, right click
does nothing. A fixed `figure NAME` command keeps replaying that one figure.

In windowed preview, **Space** advances gently; **Escape** or **Q** closes it.
In fullscreen preview, any key exits. The caption shows the current mode and
figure; controls sit in the opposite corner when there is enough room.
Use `--lang fr` or `--lang en` to choose the caption language explicitly.

## Keep a world

Wallpaper and idle-saver modes automatically append initial-world recipes to
`$XDG_STATE_HOME/game-of-life/history.tsv`, falling back to
`~/.local/state/game-of-life/history.tsv`. Previews do not write a journal unless
you request one. A line is typically 60–90 bytes, independent of grid area.

```sh
game-of-life replay                     # Last complete line (same as replay 0)
game-of-life replay 42                  # Physical line 42, counting from 1
game-of-life random --history ~/life.tsv
game-of-life replay 1 --history ~/life.tsv
game-of-life screensaver --no-history
nl -ba ~/.local/state/game-of-life/history.tsv | tail
```

Replay keeps the recorded dimensions, seed and trail length. It fits the world
to the window with whole-pixel cells; a world larger than the window at one
pixel per cell is centered and clipped, never resized logically. `--size`,
`--seed` and `--trail` do not override a recording. You can change playback speed
with `--fps`. A replay keeps restarting the selected world and never appends to
the journal. Copy a favorite line into another file to keep or share it.

New scenes are appended once. Back/forward navigation at unchanged dimensions
does not write again; resizing a live scene to a different grid records that new
initial world. Exposes and simulation steps do not write. Existing journals are
never truncated or automatically rotated. New files are private (0600), new
directories are 0700; existing permissions are preserved. A write failure emits
one warning and disables logging for that process without stopping animation.
An interrupted final line is ignored by `replay 0`. Writes are not forced to disk
with `fsync`, so the very latest entry is not guaranteed across a power failure.

### A recipe, one tab-separated line

There is no header. Each line contains these nine fields:

| Field | Meaning |
| --- | --- |
| `life1` | Version of the reconstruction recipe |
| timestamp | Unix seconds when the initial world was recorded |
| mode | `random`, `figure`, or `mix` |
| columns, rows | Exact toroidal grid dimensions (two fields) |
| seed | Eight hexadecimal digits, before generating the world |
| trail | Trail length, 0–31 |
| figure | Canonical figure name, or `-` |
| checksum | 64-bit FNV-1a of the initial cell bytes, 16 hexadecimal digits |

`life1` uses the existing xorshift32 generator and 30% random soup, warmed up
for 32 generations; figures and mixes begin at generation zero. The checksum
covers ages and trails as well as occupancy, row by row without the scratch
halo. It is a compatibility check, not a cryptographic integrity guarantee.
Replay refuses a reconstructed world that differs from the recorded checksum.

Recipes rely on a compatible generator and, for figures/mixes, figure library.
They are not standalone grid snapshots. Keep the matching program version for
long-term archives; a future catalogue change can require that older version
to replay a mix. Gzip can compress archived journals further; decompress before
replay. No compression library is needed by the application.

## Make it part of your desktop

```sh
game-of-life wallpaper
game-of-life screensaver                # Also show above apps after 2 idle minutes
game-of-life screensaver --idle 300     # Five minutes instead
```

These modes need an X11 desktop. The same process stays behind applications,
pauses its simulation when fully covered, then rises above them after the idle
threshold. In the saver, mouse movement lets you aim; left/right clicks navigate;
**any key returns to work** while leaving the wallpaper alive. A full idle
interval is required before it appears again.

Monitor blanking and power-off remain controlled by your existing desktop
settings. This animation does not lock the session. See [installation](INSTALL.md)
for autostart and integration with an external XScreenSaver.

## Let the scene choose its pace

Still lifes stay for 10 seconds; oscillators show at least four full periods.
Glider guns and other producers get 90 seconds to five minutes. Expansive seeds
can run for up to 20 minutes while developing. Random worlds move on once their
occupancy repeats or sustained low activity suggests they have settled.
An empty world is replaced after two seconds.

These are visual heuristics on a finite, wrapping grid. `--seconds 60` fixes the
observation budget to a minute, except for extinction. Durations count simulation
steps at the selected frame rate; a hidden or slowed window takes longer in wall
time. Single figures start centered with breathing room, especially producers
and expansive seeds.

## Tune without fuss

| Option | Default | Meaning |
| --- | --- | --- |
| `--fps N` | 15 | Maximum frame rate, 1–60 |
| `--size N` | 0 | Cell size in pixels; 0 chooses an economical grid |
| `--trail N` | 18 | Fading trail, 0–31 generations |
| `--seconds N` | 0 | 0 = adaptive; otherwise 10–3600 seconds |
| `--seed N` | Varies | Repeat a world at the same dimensions |
| `--idle N` | 120 in `screensaver` | Idle seconds; 0 disables presentation |
| `--lang en\|fr` | System locale | Caption language |

The underlying `game_of_life` command remains available for existing scripts,
including `-root`, `--window-id` and `--display`. Its default mode is the original
random world; the friendly `game-of-life` launcher defaults to alternation.
For all flags: `game_of_life --help`. For build features: `game-of-life version`.
