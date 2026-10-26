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
