# Life Duel

A two-player game alongside the screensaver. One small C server simulates and
arbitrates the match; both players open its browser client. The existing CPU
and GPU screensaver modes retain their rules, controls and startup settings.
The browser interface is currently in French.

## Start a game

Add the optional WebSocket development dependency, then run the normal installer:

```sh
# Arch
sudo pacman -S --needed libwebsockets
# Debian / Ubuntu
sudo apt install libwebsockets-dev

./install.sh
~/.local/bin/game-of-life game
```

Open **http://localhost:7681**, create a room and share its invitation. Two
separate tabs can play locally; each tab keeps its own seat in session storage.
No accounts, external assets, npm installation or GPU are required. The server
defaults to loopback and records match journals in the user's state directory.
Ctrl+C stops it. Installing does not start a server or enable a background service.

To play with another device on your local network:

```sh
~/.local/bin/game-of-life game --bind 0.0.0.0
```

Both players should open `http://SERVER_IP:7681`, replacing `SERVER_IP` with the
host's LAN address, then share the room link. A `localhost` invitation only works
on the host itself. Any existing firewall must allow the chosen port; the program
does not change firewall rules. `--port 8080` chooses another port.

For Internet play, run the server on a reachable host behind an HTTPS reverse
proxy that forwards WebSocket upgrades and preserves the original `Host` header.
The client automatically uses `wss://` on HTTPS pages. A room code does not make a
home machine reachable through NAT; there is no hosted matchmaking or relay service.
Use the same origin for the page and its WebSocket connection.

The browser client works independently of X11, including on Wayland, Windows,
macOS and phones. The POSIX server can be built without X11 or OpenGL:

```sh
cmake -S . -B build-server -DLIFE_WITH_X11=OFF \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build-server --parallel
cmake --install build-server
~/.local/bin/life-duel-server
```

This path needs a C compiler, CMake, pkg-config and libwebsockets development
files. `-DLIFE_WITH_DUEL=OFF` omits the server entirely from a normal screensaver
build. GNU Make continues to build only the original renderer.

## Play

The 128 × 80 arena has two construction zones and three objectives in the middle.
Its edges are bounded, unlike the wrapping screensaver worlds. Both players
start with a four-cell block and 40 energy. Mint starts on the left, gold on the right.

1. **Plan together.** Place figures or individual cells on empty cells in your
   own camp. Rotate with **R**, mirror with **F**, undo with **Ctrl+Z**. The arrows
   and Enter also place figures when the arena has keyboard focus. Drafts remain
   private and do not evolve. Their energy is reserved, and undo refunds it.
2. **Commit together.** Click the ready button or press Space. Both plans enter
   the grid together once both players are ready, or after the 20-second deadline.
   Unsubmitted drafts are included when time expires. The world then advances
   **16 generations at 10 generations/second**.
3. **Control the centre.** A zone belongs to the player with a strict majority
   of its live cells and at least three cells there. Controlling at least two
   zones gives one point per generation. The first player to **160 points** wins.
   After **30 rounds**, the higher score wins; equal scores produce a draw.

Every following round gives each player **16 energy**, capped at **100**, plus
**1 XP** and up to **2 extra XP** for objectives held at the end of the round.
Basic income does not depend on population or births. An oscillator cannot
generate XP just by cycling. These are initial balance settings, not a claim
of competitive balance.

| Construction | Energy | Unlock |
| --- | ---: | ---: |
| Single cell | 2 | Start |
| Block | 4 | Start |
| Eater 1 | 8 | Start |
| Glider | 7 | Start |
| Lightweight spaceship | 14 | 3 XP |
| Gosper glider gun | 60 | 8 XP |

All figures come from the existing library. Each player can draft up to 64
placements per round. Building zones are disjoint, so network arrival order
cannot decide a contested placement. A placement never overwrites a live cell.
Unlocks grant ready-made blueprints at a lower cost than drawing their cells
individually. Freehand construction remains possible, including shapes whose
blueprint is still locked; the physics never forbids a shape from emerging.

The simulation uses **Immigration Life**: ordinary B3/S23 survival and birth,
with each newborn taking the majority color of its three living neighbors.
Surviving cells retain their owner. There is no temporary invulnerability:
atomic placement gives manual constructions time to take shape before evolution.
Collisions can destroy a projectile, leave a colony or help the opponent.

The page and arena have a subtle, smoothly changing tint based on the overall
population balance. This is visual feedback; victory depends on the three
central objectives. The initial palette remains legible at either extreme.

## Disconnections and replays

A dropped connection pauses an ongoing match for **30 seconds**. Reopening
the room in the same tab resumes its seat with a private token and receives
the full current state, including its own drafts. If only one player returns,
that player wins; if neither returns, the match is a draw. Tokens are not put in
invitation links or replay files. Matches are held in memory: a server restart
ends live sessions. Rooms empty for five minutes are reclaimed (16 rooms maximum).

Completed matches can be downloaded with **Télécharger la partie**. On the
home screen, **Ouvrir une partie enregistrée** plays a `.duel` file with pause
and a timeline. The client obtains the versioned figure catalogue from the
server, then performs playback locally. The server can also verify every recorded
checkpoint against its authoritative C rules:

```sh
game-of-life game --replay path/to/MATCH.duel
```

By default the journals are stored in
`~/.local/state/game-of-life/duels/ROOM.duel`, or under `$XDG_STATE_HOME`.
`--replay-dir DIRECTORY` changes the location; `--no-replays` disables recording.
Journals contain the rules version, accepted placements, generation boundaries,
checksums and the result. They contain neither credentials nor animation frames.
Incomplete recordings with a valid checkpoint can be verified as unfinished
matches. The existing `life1` screensaver history format is unchanged.

## Implementation and protocol

`duel.c` contains the bounded, integer-only rules and reuses `figures.c` to load
the six construction seeds. `duel_server.c` uses libwebsockets' event loop for
HTTP, WebSockets and match timing. No X11, OpenGL or display is needed by the server.
`web/` contains plain HTML, CSS and JavaScript; no external resources are loaded.

WebSocket subprotocol: **`life-duel-v1`**. Client commands are bounded text frames:

```text
create
join ROOMCODE
resume ROOMCODE PRIVATE_TOKEN
place SEQUENCE ROUND PIECE_INDEX X Y TRANSFORM
undo SEQUENCE ROUND
clear SEQUENCE ROUND
ready SEQUENCE ROUND
replay
```

Coordinates are zero-based; transforms 0–3 are clockwise quarter-turns, 4–7
reflect horizontally before rotating. The connection determines player identity.
Increasing sequence numbers prevent duplicate commands from spending energy twice;
round numbers reject stale actions. Reconnection starts a new sequence at 1.
The server validates ownership, phase, unlocks, energy, bounds and occupancy.
Frames are limited to 511 bytes and connections to 60 commands per second.

Responses are JSON `catalog`, `welcome`, `state` or `error` messages. The grid in
`state.grid` packs four cells per byte, two bits each, least-significant cell
first, then encodes the bytes as Base64. Empty = 0, mint = 1, gold = 2. State
updates include scores, resources and only the recipient's own draft. Slow
connections receive the latest state rather than an unbounded snapshot queue.
`replay` returns the journal as a binary WebSocket message after the match ends.

The 2.5 KiB packed grid takes about 3.4 KiB in Base64, plus JSON metadata and
drafts. Clients draw authoritative states; there is no live client prediction or
lockstep requirement. Browser playback uses the same integer rules independently.

## Validation and measurement

A complete two-client scripted match on the development machine finished after
189 generations with a 160–0 objective victory. All 189 transitions matched an
independent simulator. Illegal territory, locked figures, numeric overflow and
a third player were rejected. Both clients received the same final world. The
1,060-byte journal replayed with all 12 checkpoints matching in C; browser
playback reached the same generation and score.

Two browser tabs also exercised private drafts and reconnection. AddressSanitizer
and UndefinedBehaviorSanitizer runs covered duplicate commands, undo, planning
timeouts, oversized frames and the 30-second disconnect result. A separate rules
check covered all figure transforms, majority births, bounded edges and the
30-round draw. The repository's three existing simulation checks passed.

That Release server used **0.20 CPU seconds over 20.17 wall seconds**, about
**1% of one CPU core**, with **8,296 KiB RSS**. This was one synthetic match
(one player launched gliders, the other passed), with clients outside the
measured process. It is not a many-room capacity test or a power measurement.
[Recorded measurements](../bench/duel-2026-10-28.json).
