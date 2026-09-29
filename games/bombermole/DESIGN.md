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

### BG palettes (8 x 15 colours)
| Palette | Contents | Chosen |
|---|---|---|
| 0 | font (white, shadow, box) and the 2 weather colours | fixed |
| 1-4 | terrain: the season's row of tiles.png (60 colours) | per season |
| 5 | **props A**: the first colour family of terrain-like props the level uses | per level |
| 6 | explosions | fixed |
| 7 | HUD icons on lines 0-15; **props B** on lines 16-239 | per level |

Terrain-like props (bridge, ice, cover, gates...) have one 15-colour palette per **colour family**
(`tools/sheets.py` `PROP_PALETTES`): *plants* (tall grass, corn, burnt ground, gate, apple tree, beehive,
mushroom), *wood* (crate, tomato splat, well, rails), *stone* (mud, pressure plate, lever, steam vent, pipe)
and *water* (bridge, ice, thin ice). When a level starts, the game lists the families of the props it uses
and of those it can create (cover burns, puddles make mud below, the farmer's tomatoes splat) and loads
them into palettes 5 and 7. Palette 7 is shared with the HUD like an HDMA palette write on the SNES: the
raster callback loads the props-B colours at line 16 and the HUD colours back at line 0. A level may
therefore use **two prop families**; `tools/check_levels.py` (part of `make check`) rejects a level that
needs more, and the game logs it and shows the extra family with palette 5's colours.

Sprite palettes: 0 mole, 1 ferret, 2 cat, 3 boss (loaded per level), 4 bomb and dust, 5 pick-ups,
6 sprite props, 7 critters (guard dog, bees); the enemy variants take 1 and 2, plus 3 without a boss and
7 without a dog (see "Enemies").

### HUD (20 cells of 16x16)
`[heart][hits] [bomb][max bombs] [fire][range] [claws][speed] [grub][grubs left: 2 digits]`
then, for each depth (surface, below, deep), `[marker][depth icon][grubs left there, or a tick]`.
The marker and a blinking icon show the mole's depth. Another depth's icon **blinks with a warning
sign** when something there is dangerous for the mole's depth: a lit bomb or a blast, or an enemy
right next to a hole or ladder that leads to the mole's depth.

### Making the objective obvious (kept light)
- **Level banner** (at the start, never blocking): the level name and "Collect N grubs to open the
  molehill", about 2.5 s, then it slides away while the mole already plays. No "press A", no hint text.
- **HUD**: the grubs left on each depth, a tick when a depth is done (above).
- **Pause screen** (Start): the level name, its **hint as a subtitle** (the only place hints appear), the
  objective, a **map of the three depths** (remaining grubs in gold, holes down in black, ladders and holes
  up in orange, the exit, the mole, the enemies) and a one-line legend: "GOLD GRUB  BLACK HOLE  ORANGE WAY UP".
- **Molehill open**: when the last grub is taken, a jingle, a "MOLEHILL OPEN!" banner for about 1.5 s,
  and an arrow: above the molehill on the surface, or at the top edge with "EXIT: SURFACE" below.
- **Hidden grubs glow**: a dirt, rock, root or frozen block, or an autumn pile of leaves, that hides a golden
  grub shows a slow golden pulse (about 1 s; four corner marks and a centre twinkle drawn with a
  colour-cycled palette entry, so it costs nothing). It is meant to be subtle, not to look like a power-up.
  On **Hard** only blocks within 3 cells of the mole glow. The pause map shows hidden grubs as well.
  Hidden **power-ups do not glow**: finding them stays a reward for digging and blasting, and a second,
  fainter pulse would be read as "a grub, but smaller" and blur the one signal that matters for the goal.
- There are no tutorial prompts: spring 1 teaches by its layout (walk, dig the soft dirt wall, the hole down,
  the grubs below, the ladder back up, the exit). The tutorial flags of older saves are ignored.
- `--opt bot=2` is a test bot that plays a level with only this information (targets: the grubs on
  the map, then the exit the arrow points to); `make check` has it clear spring 1 and 2.

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
- **Digging**: holding a direction toward **soft dirt** digs it in 40 frames (20 in summer, where the
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

Power-ups can lie in the open or hidden inside dirt, rock or leaves (revealed when the cell is dug or blasted;
unlike hidden grubs they do not glow). They last for the current level.

## Enemies and bosses
| Enemy | Where | Behaviour |
|---|---|---|
| Ferret | underground | runs the tunnels; its type (tier) decides its speed, whether it chases and how it treats bombs (see "Enemy tiers"). Cannot dig. May sleep in a nest until a blast wakes it. |
| Cat | surface | patrols straight lines and turns at walls; when the mole is in its line of sight (same row or column, clear, within its vision, not hidden in tall grass or corn) it crouches for half a second, then pounces (length and rest by tier) at 3 px per frame. |
| Guard dog | ally | sleeps until a blast wakes it, then chases the nearest cat on its depth and chases it away on contact; otherwise follows the mole. Blasts only stun it. |

**Seasonal variants**: see the enemy tiers below (each season brings its tier and its palette variants);
enemies stay on their depth unless a vent lifts them or a floor
opens under them.

## Enemy tiers (data-driven)
Every ferret and cat has a **type** (table `ENEMY_TYPES` in `src/world.c`): speed, movement, vision, bomb
awareness with a reaction delay, pounce and cooldown for cats, hits to defeat, and a palette variant. Early
enemies behave like early Super Bomberman: slow, wandering at random, blind to bombs, easy to bomb.

| Type | Tier | Speed (mole = 20) | Movement | Vision | Bombs | Reaction | Pounce / rest | Hits |
|---|---|---|---|---|---|---|---|---|
| sleepy ferret | 1 | 9 | random wander | none | ignores them | | | 1 |
| ginger cat | 1 | 9 | patrol | 4 cells, line of sight | ignores them | | 2 cells / 2 s | 1 |
| brown ferret | 2 | 13 | chases when it smells you (path of 6 cells or less) | 6 | notices bombs late (fuse under 1 s) | 0.5 s | | 1 |
| grey cat | 2 | 12 | patrol | 7, line of sight | late (fuse under 1 s) | 0.5 s | 4 cells / 1.2 s | 1 |
| polecat | 3 | 16 | chases | 8 | avoids blasts (every bomb) | 0.2 s | | 1 |
| black cat | 3 | 14 | patrol | 8, line of sight | avoids blasts | 0.2 s | 6 cells / 1 s | 1 |
| stoat | 4 | 19 | chases | 12 | avoids blasts | 0.1 s | | 2 |
| siamese cat | 4 | 16 | patrol | 9, line of sight | avoids blasts | 0.1 s | 6 cells / 0.75 s | 2 |

- Movement: *random wander* keeps its direction half of the time at each cell; *patrol* walks straight and
  turns at walls; *chase* follows the shortest path to the mole when it is within the vision range
  (and gives up 3 cells beyond it). Moves are decided only at tile centres; the path comes from a
  distance field with a stable tie-break (keep going straight, then the axis where the mole is farther),
  and an enemy turns back only when nothing else keeps the distance or after 3 tiles, so sprites do
  not flip left-right-left. The sprite faces the actual movement and keeps its facing when stopped.
- Bomb awareness: an aware enemy standing in the future blast of a bomb it knows about waits for its
  reaction delay, then runs to the nearest safe cell; it also refuses to step into a known blast.
- Tier-4 enemies take 2 hits (they blink after the first one).

**The curve.** A level's `F` and `C` take the level's default tier: spring 1-4 **tier 1**; spring 5-8 and
summer **tier 2**; autumn **tier 3**; winter **tier 4** (`tier: N` in the header overrides it). Levels mix
tiers with legend words (`sleepy_ferret`, `brown_ferret`, `polecat`, `stoat`, `ginger_cat`, `grey_cat`,
`black_cat`, `siamese_cat`, or `ferret:N` / `cat:N`). `tools/check_levels.py` rejects an enemy above the
level's allowed tier, a spring 1 without enemies to learn on (2 or more), and more variants than free sprite
palettes; `check_levels.py --table` prints the mix of every level.

**Palette-swap variants** (the owner's idea): each type is the base ferret or cat sprite with its own
16-colour palette, so no new art is needed. `tools/palette_variants.py` splits the base palette into ramps
(outline, fur, light parts, accents such as the nose and eyes) and maps the fur ramp onto the variant's colour
by brightness, so the shading survives; it works on the placeholders and on imported AI art alike (the asset
build computes the palettes from whatever art is in the sheets). Variants: sleepy ferret (pale cream), brown
ferret, polecat (dark, light face), stoat (white); ginger tabby, grey, black, siamese (cream with dark points).
Preview: `docs/art-preview/enemy_variants.png` (`tools/art_variants.py`). A level gets sprite palettes 1 and 2
for its variants, plus 3 when there is no boss and 7 when there is no dog.

**Difficulty** (Options, saved; Normal by default):

| | Easy | Normal | Hard |
|---|---|---|---|
| Enemy speed | 85 % | 100 % | 115 % |
| Reaction delay | 150 % | 100 % | 70 % |
| Enemies | one in three removed | as designed | one more on each depth that has enemies |

## Bosses
32x32, loaded with their own palette; the boss holds the last golden grub:

| Arc | Boss | Status | Behaviour |
|---|---|---|---|
| Spring 8 | **The barn cat** | implemented | 5 hits, 1.5 s of invulnerability after each hit; chases, pounces from 6 cells |
| Summer 8 | **The farmer** (owner's idea: tomato season) | implemented; playable test room in `summer-8` | stands in his vegetable garden and lobs rotten tomatoes in an arc; a shadow marks the target cell about 1 s ahead. A hit stuns the mole; a miss leaves a splat that slows it for 4 s. Tomatoes landing in a hole splat on the depth below. He cannot be hurt while his 3-4 tomato crates stand: blow them up and he gets angry (phase 2: 3 hits, he chases and throws faster). |
| Summer 4 or 5 | **The badger** (mini-boss) | behaviour implemented (3 hits, digs through soft dirt), level to design | underground; digs through the soil; will also change depth |
| Autumn 8 | **The fox** | placeholder behaviour (fast chaser) | fast; rides the wind (push fields do not slow it) |
| Winter 8 | **The snowy owl** | placeholder behaviour (flies over obstacles) | swoops from above on the surface and drops icicles on the depth below |

## Level gimmicks ("actuators")
Every level mixes about three gimmicks so it feels unique beyond its layout; each season has a **signature**.

### Generic systems (all implemented)
| System | How it works | Level syntax |
|---|---|---|
| **Push fields** | wind (windmills) or an underground stream current push characters AND bombs one cell at a time on a rhythm; wind blows in gusts on a cycle (`gust: period active step`, in frames), currents always flow; particles show the direction; walking with the wind is faster, against it slower | `<` `>` `n` `u`: floor pushed left, right, up, down; `{` `}`: water with a current; legend words `push_*`, `flow_*` |
| **Water and bridges** | water blocks the way but not blasts; a bridge is blown away by a blast (cut an enemy's path); a floating **log** makes water walkable, drifts with currents and can be pushed along the water | `~` water, `=` bridge, `&` water + log |
| **Ice** | you slide until you hit something (enemies too); a bomb dropped on ice slides away like a kick; **thin ice** cracks after one crossing and turns to water after two | `i` ice, `j` thin ice |
| **Cross-depth** | a bomb tossed into a hole (drop it while facing the hole) or pushed or slid into one falls to the depth below and explodes there; a blast on a thin floor opens a hole and the rubble stuns the enemy below; an enemy standing on a floor that gets blasted open falls, stunned; puddles make mud on the depth below | `_` thin floor; `v`, `^`, `H` |
| **Cover** | tall grass or corn hides the mole from cats (breaks their line of sight); it burns when bombed | `w` (corn in summer) |
| **Switches** | pressure plates (pressed by anyone or a bomb) and levers (bump them) drive gates by channel; timed gates stay open for a while after a trigger. Mine-cart track switching: data hook | `P` plate, `/` lever, the bar character gate; legend words `chan:N`, `timed:N` (tenths of a second) |
| **Noise** | explosions wake sleepers within 6 cells, or 4 cells on the depth right above or below: a guard dog (ally) or a sleeping ferret nest | `D` sleeping dog, `z` sleeping ferret |

**Gate rule (no softlocks)**: every gate the player can walk through must be openable from both sides (a
plate or a lever on each side, or a latching lever gate), and a timed gate never closes while someone
stands in its gateway. Restart (pause menu) always works. `tools/check_levels.py` enforces it with a
softlock search: gates closed, it splits a level into regions, lets a plate or timed gate be entered only
from a region holding a trigger of its channel (lever gates latch: two-way once their lever is reachable),
and checks that from **every** reachable cell every grub and the exit can still be reached; a failure names
the cell and the target (`SOFTLOCK: from depth d, x,y the exit ... can no longer be reached`).
tests/data/levels_bad/softlock-gate.txt (the old spring 4) must fail it.

Also implemented: garden **sprinklers** (they turn every 2 s; their spray defuses bombs; `k`), **steam vents**
(summer signature; `V`), **drain pipes** (teleport to the pipe of the same channel, on any depth; `@`), roots
that regrow (and regrow the dirt dug next to them), **dark levels** (`dark: 1`: the helmet lamp lights only a
circle, made with a window), the **windmill** decoration that spins faster in gusts (`W`), tomato **crates** (`c`).

### Per season (about 3 per level; implemented ones in bold)
| Season | Gimmicks | Signature |
|---|---|---|
| Spring | **river with a log bridge**, **rain puddles (mud below, slows)**, **roots that regrow dug dirt**, **rotating sprinklers whose spray defuses bombs**, **windmill** | **the windmill (gusts)** |
| Summer | **dry soil (digs twice as fast)**, **corn field (cover)**, beehives (a bombed hive releases bees that chase the nearest creature), **steam vents on depth 2 (geyser lift one depth up)**, a harvester sweeping a row on a timer | **steam vents** |
| Autumn | **leaf piles that hide grubs and blow around in the wind**, pumpkins to push (Sokoban-like), apple trees (bomb one: apples fall and stun), bouncy mushrooms (launch you 2 cells), fog (limited vision), **strong wind** | **strong wind** |
| Winter | **frozen river with slippery ice**, **thin ice**, snowdrifts that slow you, snowballs that a blast rolls along (growing, crushing enemies), icicles that fall after nearby blasts, **frozen dirt (bombs only)**, **night (helmet lamp)** | **the frozen river** |
| Underground, any season | mine carts on rails (fast travel, crush enemies, levers switch the track), gas pockets released by blasts (stun ferrets and you), glow-worms lighting dark caves, an ant column carrying grubs away, a well bucket (elevator surface to depth 2, turned with a crank), **drain pipes** | |

Gimmicks not implemented yet are **data hooks**: their art is in `props.png` (placeholders), their TODO rows
exist, unknown header keys in level files are ignored, and the stub levels name the gimmick they are planned for.

### Where the gimmicks are used
| Level | Gimmicks |
|---|---|
| Spring 1 First Dig | digging, holes, puddles with mud below, windmill wind lane |
| Spring 2 Rock Garden | bombs vs rock, ladder, regrowing roots, first ferret |
| Spring 3 River Crossing | river, bombable bridge, drifting log bridge, tall grass cover, first cat, sleeping ferret |
| Spring 4 Thin Floors | thin floors opening holes into sealed chambers on both undergrounds, rubble stuns, plate and timed gate |
| Spring 5 Chain Reaction | rock mazes and chains, remote detonator, sprinklers, bombs tossed into a hole |
| Spring 6 Ferret Warren | warrens on two depths, sleeping ferret nest (noise), lever and gates around the exit |
| Spring 7 Windmill Hill | windmills and gust lanes (they push you and your bombs), two cats, sleeping guard dog, tall grass |
| Spring 8 The Barn Cat | boss, sprinklers, puddles, tall grass, grubs underground |
| Summer 1 Steam Lift | steam vents (signature), dry soil, corn |
| Summer 8 (test room) | the farmer boss, crates, tomato splats, a hole for the cross-depth tomatoes |
| Autumn 1 Gale Force | strong wind lanes (signature), leaves hiding grubs and blowing away |
| Winter 1 Frozen River | ice and thin ice (signature), frozen dirt, snow |

## Seasons (arcs)
4 arcs of 8 levels. Each has its palette row in tiles.png, its music and its twist:

| Arc | Twist | Weather layer |
|---|---|---|
| Spring | puddles: slow the mole to half speed, stop blasts, make mud below | rain |
| Summer | dry soil: digging is twice as fast (20 frames); faster enemies | pollen |
| Autumn | leaves cover the surface and hide items (a pile hiding a grub glows like any block); dig (fast) or blast them; the wind blows them | falling leaves |
| Winter | frozen dirt (bombs only); snow slows the mole on the surface to 3/4; ice | snow |

## Levels
Plain text files in `levels/` (format below), one per level: `<season>-<n>.txt`. They are embedded in
the game at build time; the desktop runner also reads `data/levels/*.txt` next to the executable
first, so levels can be edited without rebuilding.

Arc 1 (spring) difficulty curve: see "Where the gimmicks are used" above. The enemies grow from none (1) to one
ferret (2), a cat (3), sealed chambers (4), chains and sprinklers (5), a ferret warren (6), two cats and gusts
(7) and the boss (8); the starting bombs and range grow from 1/2 to 2/3.

Arcs 2-4: level 1 of each shows the season's signature; `summer-8` is the farmer boss test room; the other
levels are **stubs** (`status: stub`, written by `tools/make_stub_levels.py`, which never overwrites a file):
small playable placeholders with the right season, each naming its planned gimmick, ready to be replaced.

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
boss: barncat           # optional: barncat | farmer | badger | fox | owl (default: the season's boss)
gust: 240 100 16        # optional: wind cycle, gust length, frames between pushes
vent: 240 40            # optional: steam vent cycle and eruption length
dark: 0                 # optional: 1 = night or dark cave (helmet lamp)

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
| `=` | bridge over water | `&` | water with a floating log |
| `i` / `j` | ice / thin ice | `m` | mud |
| `w` | tall grass (corn in summer): cover | `k` | sprinkler |
| `P` | pressure plate (channel 1) | `/` | lever (channel 1) |
| `V` | steam vent | `@` | drain pipe (channel 1) |
| `c` | tomato crate | `W` | windmill |
| `z` | sleeping ferret | `D` | sleeping guard dog |
| `<` `>` `n` `u` | floor pushed left, right, up, down (wind) | `{` `}` | water with a current, left, right |

The gate is the vertical bar character (channel 1). Legend words for custom entries: every terrain name
(`floor stone soft_dirt hard_rock roots frozen_dirt leaves water puddle thin_floor hole_down hole_up ladder exit
bridge ice thin_ice mud tall_grass corn burnt gate plate lever steam_vent pipe crate sprinkler windmill`), items
(`grub bomb fire speed remote heart`), actors (`mole p2 p3 p4 ferret cat boss dog`), `asleep`, `log`,
`push_up/down/left/right`, `flow_up/down/left/right`, `chan:N`, `timed:N`.

Rules checked by the loader and by `tools/check_levels.py` (part of `make check`): 14x20 grids, hints of
at most 76 characters,
one mole start, an exit on the surface, at least one grub, every hole down above a hole up or
ladder (and the reverse), every grub reachable when breakable cells count as passable.

## Game flow
Title (press Start; menu: Play, Options (music, sound, difficulty, erase save), Credits) -> arc select (4 seasons, locked ones greyed) ->
level select (1-8, cleared and locked marks, best time) -> play, with a short level banner (name and objective)
while the iris opens on the mole -> **pause** (map, level hint, one-line legend; Resume / Restart / Quit) -> level clear (time, best time)
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
  pounce, boss hit, menu move, menu confirm, splash, fizzle (defused bomb), switch, steam, woof, splat.
- Music: one 4-channel MOD per season plus the title and the boss, generated by `tools/make_music.py` at
  build time and played by libxmp-lite. Music (4 channels) + up to 4 sound effects = the 8-voice
  guideline. Real modules drop in as `data/music/<name>.xm|.it|.s3m|.mod`.

## Art
- The game builds from four assembled sheets and a logo in `art/` (`characters.png`, `tiles.png`,
  `items_fx.png`, `props.png`, `title_logo.png`), laid out as in docs/art-sheets.md.
- They are written by `tools/art_sync.py sync`: placeholder cells (`tools/make_placeholders.py`) replaced
  cell by cell by the art the owner VALIDATED in `art/incoming/` (one PNG per animation strip, driven by
  `art/incoming/TODO.md`; see docs/art-workflow.md).
- The owner's first AI batch (three whole sheets in `art/incoming/first-batch/`) was cut into 107 strips,
  all GENERATED, waiting for review.
- **Character size**: 16x16 by default; the engine and the levels also take 24x24 or 32x32 characters on the
  16-px grid (bottom-centre anchor, overlapping upwards like SNES Bomberman): `make CHAR_SIZE=24`.
  Comparison images: docs/art-preview/. Recommendation: **24x24**. The AI characters keep their shading and
  expressions at 24 px, they still read as 16-px cells in the maze, and 24 px is SNES Bomberman's character
  height. At 16 px the mole's helmet and nose blur into a brown blob. At 32 px the characters hide the walls
  around them, and the boss (64 px) takes half of the playfield's height. The guard dog is a character
  too (on the characters sheet, CHAR_SIZE); the bosses are 2x2 characters (48 px at CHAR_SIZE 24).
- **Grass below a wall** uses a shadow tile derived from the season's grass when the assets are built (the
  top rows darkened with the grass tile's own colours), so a row of them along a wall repeats seamlessly;
  the `grass_edge` cell of tiles.png is kept for the title screen's hills.
- **Terrain tileset**: `make TILESET=code|ai|ai_v2` (default `code`, see docs/art-workflow.md). The code-drawn
  set won the comparison (docs/art-preview/tiles-compare.png): the AI tiles are noisy at 16x16 (speckles
  everywhere, soil, tunnel and rock close in value), which hides what can be dug. Grass, soil and tunnel
  floor have 3 variants each, placed by a fixed hash of the cell; water shimmers (2 frames, ~0.5 s each) and
  shows a bank where the cell above is not water.
