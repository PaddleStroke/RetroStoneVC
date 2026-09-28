# Bomber Mole: design

All rights reserved, 8BCraft. The first game for the RetroStone virtual console (docs/spec.md).

## Pitch
You are a mole with a miner's helmet and a pocket full of bombs. Each level is a meadow with
**three depths**: the grass **surface**, **underground 1** and **underground 2**. Golden grubs are
spread over the three depths. Collect them all, and the exit molehill on the surface opens. Dig,
bomb, climb and drop between the depths while ferrets hunt you in the tunnels and cats prowl the
grass.

## Screen
- 320x240. HUD: 16 px at the top. Playfield: 20x14 cells of 16x16 px (320x224).
- One depth is shown at a time. Changing depth **slides** the playfield up or down (the earth
  between the depths scrolls past). Starting and leaving a level uses an **iris** (a circle window).
- Layers (see docs/spec.md, Priorities):

| Layer | Use |
|---|---|
| BG1 | HUD and text (menus, messages); top priority |
| BG2 | weather overlay on the surface (rain, pollen, leaves, snow), half-transparent (colour math) |
| BG3 | explosions (metatiles on the cell grid) |
| BG4 | terrain |
| OBJ | mole, enemies, bombs, items, dust (priority 2: above BG3/BG4, below the weather and HUD) |

### HUD (20 cells of 16x16)
`[heart][hits] [bomb][max bombs] [fire][range] [claws][speed] [grub][grubs left: 2 digits]`
then, for each depth, `[marker][depth icon][enemies left]`. The marker shows the mole's depth.
A depth icon **blinks with a warning sign** when something is dangerous there for the mole's
depth: a lit bomb or a blast, or an enemy right next to a hole or ladder that leads to the
mole's depth.

## Controls (SNES pad)
| Button | Action |
|---|---|
| D-pad | move (hold toward soft dirt to dig) |
| B | drop a bomb |
| A | detonate your oldest bomb (with the remote detonator) |
| Start | pause menu |
| Select | (level select) with L+R held: unlock everything (tester cheat) |

## Rules
- **Movement** is cell to cell (smooth, 1.25 px per frame at base speed). The mole can reverse
  mid-way. Moving onto a hole or ladder changes depth.
- **Digging**: holding a direction toward **soft dirt** digs it in 40 frames (24 in summer, where the
  soil is dry). The cell becomes a tunnel. Leaves take 12 frames. Hard rock, frozen dirt and roots
  need bombs; stone is unbreakable.
- **Bombs**: fuse 2.5 s. The blast is a **Bomberman cross** of `range` cells in each direction,
  lasting 0.5 s. A blast stops at the first solid cell and destroys it if breakable (dirt, rock,
  frozen dirt, roots, leaves). Stone stops it. Puddles stop it (spring). Water lets it through.
  A blast reaching a bomb detonates it at once (**chain reaction**). Blasts hurt the mole and
  defeat enemies. Items survive blasts (so a level can never become impossible).
- **Thin floor**: a blast on a thin-floor cell breaks it into a **hole down**; the cell below
  becomes a **hole up** (opened if it was solid). This is how some grubs are reached.
- **Roots** regrow 8 s after being blasted, if the cell is empty.
- **Hits**: the mole has 1 heart (+1 per heart power-up, max 3). A hit costs a heart and gives
  2 s of blinking invulnerability. At 0 the mole is knocked out: one life lost, the level restarts.
  3 lives; game over offers Continue (restart the level) or Quit.
- **Goal**: collect every golden grub on all three depths; the exit opens (sound + glow); walk
  onto it. The level time is saved as a best time.

### Power-ups
| Item | Effect |
|---|---|
| Bomb | +1 bomb at once (max 8) |
| Fire | +1 blast range (max 8) |
| Claws | +1 speed step (max 3) |
| Remote | bombs no longer explode by themselves; A detonates the oldest one |
| Heart | +1 heart (max 3) |

Power-ups can lie in the open or hidden inside dirt, rock or leaves (revealed when the cell is
dug or blasted). They last for the current level.

## Enemies
| Enemy | Where | Behaviour |
|---|---|---|
| Ferret | underground | runs the tunnels; on the mole's depth it chases along the shortest tunnel path (3 times out of 4 at each crossing), else wanders. Cannot dig. |
| Cat | surface | patrols straight lines and turns at walls; when the mole is in its line of sight (same row or column, clear, up to 7 cells) it crouches for half a second, then pounces up to 4 cells at 3 px per frame, then rests. |
| Boss | level 8 of each arc | 32x32, 5 hits, 1.5 s of invulnerability after each hit. Chases the mole and pounces from 6 cells. Holds the last golden grub. |

**Seasonal variants** (palette and tuning): spring as above; summer enemies are faster and the
boss is a **badger** that digs through soft dirt underground; autumn enemies are darker, the boss is
an **owl** that crosses rock; winter ferrets are white **stoats** (fastest) and the boss is a
**wolverine**. Enemies stay on their depth.

## Seasons (arcs)
4 arcs of 8 levels. Each has its palette row in tiles.png, its music and its twist:

| Arc | Twist | Weather layer |
|---|---|---|
| Spring | puddles: slow the mole to half speed and stop blasts | rain |
| Summer | dry soil: digging is faster (24 frames); hotter, faster enemies | pollen |
| Autumn | leaves cover the surface and hide items; dig (fast) or blast them | falling leaves |
| Winter | frozen dirt (bombs only) replaces most soft dirt; snow slows the mole on the surface to 3/4 | snow |

## Levels
Plain text files in `levels/` (format below), one per level: `<season>-<n>.txt`. They are embedded in
the game at build time; the desktop runner also reads `data/levels/*.txt` next to the executable
first, so levels can be edited without rebuilding.

Arc 1 (spring) difficulty curve:
1. **First Dig**: surface + one tunnel depth mostly open; digging, one hole down, 3 grubs, no enemies.
2. **Rock Garden**: bombs vs rocks, a ladder, first ferret underground.
3. **Puddle Lane**: puddles stop blasts; first cat on the surface; power-ups.
4. **Thin Ice... er, Floor**: thin floors that open holes to reach closed pockets.
5. **Chain Reaction**: bomb chains through rock mazes; remote detonator.
6. **Ferret Warren**: three ferrets in a looping warren on both undergrounds.
7. **Cat's Garden**: two cats on the surface, grubs spread on all depths, roots.
8. **Big Cat** (boss): an arena on the surface; the big cat holds the last grub; grubs underground.

Arcs 2-4: level 1 of each is built to show the season; levels 2-8 are **stubs** (`status: stub`),
small playable placeholders with the right season, ready to be replaced.

### Level file format
```
# comments start with '#'
name: First Dig
season: spring          # spring | summer | autumn | winter
arc: 1
level: 1
bombs: 1                # starting bombs (default 1)
range: 2                # starting blast range (default 2)
speed: 0                # starting speed step (default 0)
music: spring           # optional, defaults to the season
hint: Hold a direction to dig through soft dirt.   # shown when the level starts
status: done            # done | stub

legend:                 # optional: extra or overridden characters
Z = soft_dirt + heart

surface:
<14 lines of 20 characters>
under1:
<14 lines of 20 characters>
under2:
<14 lines of 20 characters>
```
Default legend (terrain `+` item `+` actor):

| Char | Meaning | Char | Meaning |
|---|---|---|---|
| `.` | floor (grass on the surface, tunnel below) | `M` | mole start (on floor) |
| `#` | stone (unbreakable) | `F` | ferret |
| `d` | soft dirt | `C` | cat |
| `r` | hard rock | `K` | boss (holds the last grub) |
| `t` | roots | `g` | golden grub |
| `f` | frozen dirt | `G` | grub hidden in soft dirt |
| `l` | leaves | `L` | grub hidden in leaves |
| `~` | water | `Q` | grub hidden in hard rock |
| `p` | puddle | `b` / `B` | bomb power-up (floor / in dirt) |
| `_` | thin floor | `x` / `X` | fire power-up (floor / in dirt) |
| `v` | hole down | `s` / `S` | claws (speed) (floor / in dirt) |
| `^` | hole up | `o` / `O` | remote detonator (floor / in dirt) |
| `H` | ladder (up) | `h` / `Y` | heart (floor / in dirt) |
| `E` | exit molehill (surface) | `2`-`4` | start of players 2-4 (future) |

Rules checked by the loader and by `tools/check_levels.py` (part of `make check`): 14x20 grids,
one mole start, an exit on the surface, at least one grub, every hole down above a hole up or
ladder (and the reverse), every grub reachable when breakable cells count as passable.

## Game flow
Title (press Start; menu: Play, Options, Credits) -> arc select (4 seasons, locked ones greyed) ->
level select (1-8, cleared and locked marks, best time) -> level intro (iris opens on the mole,
level name and hint) -> play -> **pause** (Resume / Restart / Quit) -> level clear (time, best time)
-> next level. After level 8: the **arc final screen** (a rotating seasonal emblem on the affine
layer, "Spring complete!") -> back to the arc select, next arc unlocked.
Knocked out: lives left -> the level restarts; no lives -> **Game over** (Continue / Quit).

Save RAM (32 KiB, only 82 bytes used): magic `BMSV`, version, levels cleared per arc, options
(music, sound, all unlocked), last arc/level, best times (seconds) per level, checksum.

## Multiplayer hooks
The game keeps an array of 4 players and reads pad N for player N. Levels may place `2`, `3`,
`4` starts. Only 1 player is enabled now; `mode` (solo / coop / battle) is stored with the level
state so co-op (shared grubs, shared exit) and battle (last mole standing, no grubs) can be added.

## Audio
- Sound effects are synthesised at start-up (square waves, noise, sweeps): bomb drop, fuse tick,
  blast, rock break, dig, grub, power-up, hurt, knock-out, exit open, depth whoosh, enemy defeated,
  pounce, boss hit, menu move, menu confirm.
- Music: one 4-channel MOD per season plus the title, generated by `tools/make_music.py` at
  build time and played by libxmp-lite. Music (4 channels) + up to 4 sound effects = the 8-voice
  guideline. Real modules drop in as `data/music/<name>.xm|.it|.s3m|.mod`.

## Art
Placeholder sheets from `tools/make_placeholders.py` in `art/`; the owner's AI art cut by
`tools/cut_ai_sheet.py` in `art/ai/`. `make ART=ai` builds with the AI art. Sheet layout:
docs/art-sheets.md.
