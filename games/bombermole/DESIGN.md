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
| BG1 | HUD and text (menus, messages); top priority. Its low-priority tiles, under the sprites, draw the glow of hidden grubs and the lever-link flash over the playfield cells |
| BG2 | weather overlay on the surface (rain, pollen, leaves, snow), half-transparent (colour math) |
| BG3 | **objects**: everything placed on the ground, with transparent pixels (below); explosions, which replace a burning cell's object for their 0.5 s |
| BG4 | **ground**: only the base ground, full tiles |

**Ground and objects.** Only the base ground is a full tile (BG4): grass (snow in winter) on the surface,
the tunnel floor below, water, ice; underground, the soil blocks and thin floors are the ground too (their
edges are lit and shaded like raised earth). Everything else is an object drawn over it on BG3, so it sits
on the season's ground and a destroyed object leaves that ground: rocks, stone, the soft-dirt blocks of the
surface (drawn as dirt mounds), frozen soil, roots, leaf piles, holes, ladders, the exit mound, puddles,
crates, bridges, gates, plates, levers, vents, pipes, tall grass, mud, burnt ground, tomato splats. Rounded
objects have a soft 1-px dithered contact shadow; the ground under a block's bottom edge is shaded too
(grass and tunnel floor have a derived "shadow" variant). Object art has flat magenta around it; AI art
drawn on its own ground (an opaque cell) gets that ground keyed out when the game is built (the border's
main colour flood-filled from the edges, unless the object's middle has that colour too, like stone
bricks). Sprites (the log, pumpkins, the windmill...) were transparent already. The objects keep their
palettes (terrain palettes 1-4, prop families 5/7): the object layer costs no extra colours.
**Bridges** are drawn according to the crossing: the planks run across the way and the rails along it;
water left or right of a bridge means the river runs sideways, so the bridge is crossed up and down
(`bridge_v`), otherwise left and right (`bridge`). The floating **log** is a short log with both ends cut
(light wood with rings) and bark grain, so it cannot read as an animal.
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
- **Start box** (the owner's playtest: a banner over a running game hid the mole): the GAME WAITS behind a
  centred box, placed away from the mole: the level name, one objective line ("Collect 8 grubs to open the
  molehill") and, in a boss level, one line on how to beat the boss ("Blow up his crates, then bomb him");
  A, Enter or Start dismisses it. A retry of the same level (after a knock-out or a restart) skips it.
- **Boss bar**: in every boss level, the boss's name and health on the top wall; the farmer shows
  "CRATES LEFT n/4" while his crates shield him (and the crates flash), then his health.
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

Keyboard (desktop), player 1: arrows, Z = B, X = A, C = Y, V = X, Q/E = L/R, Enter = Start, Esc or Backspace =
Select (back); player 2 on the same keyboard: W A S D, G = B, H = A, T = Start, R = Select. F11 fullscreen, F12
screenshot, F1-F7 dev keys (dev mode only). Game controllers are pads 1-4 by position; in multiplayer the
players are numbered in the order their pads join (the join screen). The menus take any pad: the one that
confirms on the title is P1. Every prompt names the button of the device in use (one helper, `btn_name()`,
from the SDK's `rs_pad_device()`): "PRESS A (X KEY)", "START (ENTER)", "B (Z KEY)" on the keyboard, "H", "T",
"G" on its second key set, plain "A", "START", "B" on a pad and in the libretro core; the pause screen, the
boxes, the results and the remote banner all use it. Picking up a power-up shows one line for 1.5 s naming
it, and the button when it adds a control: "REMOTE: PRESS A (X KEY) TO BLOW". While the
remote is on, a small A-button glyph sits on the HUD's bomb icon, and remote bombs look different: no fuse,
a short antenna with a blinking light.

**Pause menu**: Left/Right or Up/Down move the cursor, A or Start picks the highlighted item (the cursor
starts on RESUME, so Start still resumes), B or Esc resumes. RESUME, RESTART (the level restarts), QUIT asks
"QUIT TO TITLE? YES / NO" (NO first). In dev mode a second row holds the cheats.

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

## The crocodile
Rivers have a crocodile (`%` = water with a crocodile; header `croc: hits` makes it defeatable, 0 or absent:
only stunned). It never leaves the water: it swims slowly (speed 9, the mole walks at 20) to the water cell
nearest to the mole, its projection on the river, and passes under the bridges. When the mole stands next to
it (a bridge over it included), its eyes and nostrils rise with ripples for 0.6 s, then it snaps: a hit. It
rests 1 s after a snap. A blast on its cell stuns it for 3 s. Crossing a bridge is a timing game: cross while
it is away, or stun it first. Tiers: spring and summer crocodiles cannot be defeated; autumn ones take 2 hits
(`croc: 2`); in winter it swims **under the frozen river's ice** (`R` river ice, `5` river ice with the crocodile):
a dark shape that follows the mole's projection on the river; a mole standing still over it for 1.5 s hears the
ice crack (0.6 s) and falls in when it gives way (a heart; it climbs out at its last safe cell); a bomb on the river
ice opens it (open water, frozen again after 8 s) and exposes the crocodile. Levels: spring 3
(the river), spring 5 and summer 2 (a river across the field, two bridges), autumn 2 (the river and the cellar
pond), autumn 4 (a river with no bridge: the mushrooms hop you over it) and winter 6 (under the frozen pond).

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
ferret, polecat (dark, light face), stoat (white, with grey-blue shading, a dark face mask and a darker,
cooler outline so it reads on snow); ginger tabby, grey, black, siamese (cream with dark points).
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
| Summer 8 | **The farmer** (owner's idea: tomato season) | implemented; playable test room in `summer-8` | stands in his vegetable garden and lobs rotten tomatoes in an arc; a shadow marks the target cell about 1 s ahead. A hit stuns the mole; a miss leaves a splat that slows it for 4 s. Tomatoes landing in a hole splat on the depth below. A tomato hit costs a heart, and touching him is a hit; he walks his field slowly. He cannot be hurt while his tomato crates stand (they flash, and the boss bar counts them): blow them up and he gets angry and vulnerable (he flashes gold; phase 2: 3 hits, he chases and throws faster). |
| Summer 4 or 5 | **The badger** (mini-boss) | behaviour implemented (3 hits, digs through soft dirt), level to design | underground; digs through the soil; will also change depth |
| Autumn 8 | **The fox** | implemented | runs fast (push fields do not slow it) and **dashes** along the gale lanes, or at the mole when it is in line: it cannot be hurt while it runs or dashes. It **hides in the leaf piles** (only the rustling shows). After **3 dashes it stops, panting** (steam puffs, 3 s): only then does a blast hurt it. **4 hits**, health bar; hint "Catch the fox when it stops to rest" |
| Winter 8 | **The snowy owl** | implemented | flies over the surface (over everything); its **shadow marks where it will swoop about 1 s ahead** (0.7 s in phase 2), then it dives: whoever stands on the shadow is hit. While the mole hides underground it hovers over it and **drops icicles through the tunnel's ceiling** (the ceiling cracks 1 s first). After 3 swoops (or 3 icicles) it **lands on a perch** (a dead tree, `7`) to rest 3 s: only then can a blast hurt it (bomb the tree). **5 hits; after 3 it swoops faster** (phase 2). Health bar; hint "Hit the owl when it lands on a perch" |

## Level gimmicks ("actuators")
Every level mixes about three gimmicks so it feels unique beyond its layout; each season has a **signature**.

### Generic systems (all implemented)
| System | How it works | Level syntax |
|---|---|---|
| **Push fields** | wind or an underground stream current push characters AND bombs one cell at a time on a rhythm; wind blows in gusts on a cycle (`gust: period active step`, in frames), currents always flow; streak particles move the way the push goes; walking with the wind is faster, against it slower. A **windmill** blows AWAY from itself, the way its sails face: its lane is the straight line of cells in front of it up to the first solid cell (wall, block, crate, closed gate...); cells behind a block are sheltered, and a dug or blasted block lengthens the lane. A windmill blowing down or up is drawn from the front, one blowing sideways from the side (sails on the side the wind goes). Gales without a windmill (autumn) are push cells of their own | windmill: `W` (blows down) or a legend entry `windmill + blow_up/right/down/left` = **the direction the wind blows**; gale cells: `<` `>` `n` `u` = floor where the wind blows left, right, up, down; `{` `}`: water with a current; legend words `push_*`, `flow_*` (the direction of the push) |
| **Water and bridges** | water blocks the way but not blasts; a bridge is blown away by a blast (cut an enemy's path); a floating **log** makes water walkable, drifts with currents and can be pushed along the water | `~` water, `=` bridge, `&` water + log |
| **Ice** | you slide until you hit something (enemies too); a bomb dropped on ice slides away like a kick; **thin ice** bears two crossings (it cracks, then the red corners blink: the next step breaks it) and the third step breaks it: whoever steps on it falls in (the mole loses a heart and climbs out at its last safe cell; ferrets and cats drown); broken thin ice freezes again after 20 s (never a softlock); a blast breaks it too | `i` ice, `j` thin ice |
| **Cross-depth** | a bomb tossed into a hole (drop it while facing the hole) or pushed or slid into one falls to the depth below and explodes there; a blast on a thin floor opens a hole and the rubble stuns the enemy below; an enemy standing on a floor that gets blasted open falls, stunned; puddles make mud on the depth below | `_` thin floor; `v`, `^`, `H` |
| **Cover** | tall grass or corn hides the mole from cats (breaks their line of sight); it burns when bombed | `w` (corn in summer) |
| **Switches** | pressure plates (step-on: the mole, an enemy, or a bomb sliding onto one) and levers drive gates by channel; a lever flips when the mole walks into it AND when a blast hits it (a click, the two-state lever turns left/right, and the gates it drives and every lever of its channel flash with white corner marks for 1 s, so the link shows); timed gates stay open for a while after a trigger. Levers also switch the mine-cart junctions of their channel | `P` plate, `/` lever, the bar character gate; legend words `chan:N`, `timed:N` (tenths of a second) |
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

### Summer mechanics (all implemented)
| Mechanic | How it works | Level syntax |
|---|---|---|
| **Dry soil** | in summer soft dirt digs twice as fast (20 frames instead of 40) | the season |
| **Corn** | cover: cats cannot see the mole in it (line of sight), and it is drawn OVER the characters (BG1, high priority, transparent between the stalks: a mole or a cat in the corn is half hidden, seen through the gaps; tall grass the same); a blast sets it alight: a cell burns 1 s, hurts whoever stands in it (and sets bombs off), and sets the corn next to it alight after 0.5 s, so a field burns out cell by cell; burnt ground stays (no cover) | `w` in summer |
| **Beehive** | solid; a blast opens it and a swarm comes out: it flies (over everything but stone) to the NEAREST creature it can see (the mole, a ferret, a cat, the dog; not the boss) and stings it ONCE (a hit for the mole, knocked out for an enemy), then flies off and vanishes: one swarm, one target; a creature in corn or tall grass, a puddle, mud or on a bridge cannot be seen: the swarm turns to the next nearest one, or gives up (after 6 s at most) | `e` beehive |
| **Harvester** | parked at one end of its lane (a row or a column, up to the first rock, wall, water or other solid cell); every `harvest` period it warns (the engine rumbles and the lane flashes red for `warn` frames, 1.5 s), then sweeps the lane at 4 cells a second, crushing the mole and the enemies, mowing corn and digging up soft dirt; the next sweep comes back the other way; a rock stops it; a bomb in its way goes off; while parked it is solid | legend `floor + harvester:right` (up/down/left); header `harvest: period warn` (frames, default 480 90) |
| **Puddles dry up** | in summer each puddle evaporates after 25-40 s (a wisp of steam first) | the season |
| **Sprinklers** | as in spring: their spray puts fuses out | `k` |
| **Badger** (summer 5 mini-boss) | digs through soft dirt; charges along a row or column at the mole it can see (nothing but stone or water between) and is stunned for 2.5 s when it runs head first into a rock; when the mole is on another depth for 2 s it digs its own hole (a hole down and a hole up, which stay) and follows; 3 hits; holds the last grub | `boss: badger` + `K` |
| **Drain pipes** | a pipe takes the mole to the other pipe of its channel, on any depth | `@` (channel 1), legend `pipe + chan:N` |
| **Gas pocket** | solid; a blast opens it and a stun cloud spreads 3 cells (not through walls) in 0.3 s; everyone in it (you too) is stunned while it lasts, 3 s | `*` gas pocket |

### Autumn mechanics (all implemented; placeholder art, TODO rows in `art/incoming/TODO.md`)
| Mechanic | How it works | Level syntax |
|---|---|---|
| **Leaf piles** | dig them (fast) or blast them; a pile hiding a grub glows like any block; on a gale lane the gusts blow the piles along (and the grub inside with them) | `l`, `L` (grub inside), legend `leaves + push_right` |
| **Strong wind** | gale lanes (push cells without a windmill) push the mole, the bombs, the ferrets and cats, and blow the leaf piles along; the fox dashes along them | `<` `>` `n` `u`, header `gust:` |
| **Pumpkins** | pushed Sokoban-style: walk into one and hold for a moment; it slides one cell if the cell behind is free. It blocks the enemies' way; pushed onto a hole or into water it fills it (a plug you can walk over); a blast smashes it into mush that slows like mud (a smashed plug reopens the hole or the water) | `0` (floor + pumpkin), legend word `pumpkin` |
| **Apple trees** | solid; a blast next to one shakes 2-3 apples down around it: whoever is under one (the mole too) is stunned 3 s; an apple left on the ground gives +1 heart, once per level | `T` |
| **Bouncy mushrooms** | stepping on one launches the mole 2 cells on (a hop with a shadow), over a wall, a block or water, if the landing cell is free; a bomb dropped while facing a mushroom, or pushed onto one, bounces over the same way | `!` |
| **Fog** | a window around the mole (radius in pixels) keeps the colours; outside, colour math blends the playfield 3/4 towards the fog colour, and the enemies there show only as two eyes | header `fog: 72` (4-5 cells) |
| **Mine carts** | walk into a cart to climb in: it rolls along the rails to the end of the line (fast travel), running over any enemy in its way; at a junction (a rail cell with a channel) the lever of that channel sends it round the bend (on) or straight on (off) | `+` rails, `$` rails + cart, legend `rails + chan:N` for a junction, `/` or `lever + chan:N` |
| **Ants** | a column of ants carries a grub to the nearest ant nest; touch them or blast them and they drop it; a grub carried home is inside the nest until a blast opens the nest (the grub comes out: never a softlock) | `A` (floor + grub + ants), `N` ant nest |
| **Crocodile** | as in spring and summer, but it takes 2 hits in autumn | `%`, header `croc: 2` |

### Winter mechanics (all implemented; placeholder art, the new rows in `art/TODO-pending-winter.md`)
| Mechanic | How it works | Level syntax |
|---|---|---|
| **Ice** | slide until blocked; bombs slide like a kick | `i` |
| **Thin ice** | 2 crossings crack it, the third step breaks it into icy water (drowning: a heart, back at the last safe cell); it freezes again after 20 s | `j` |
| **Frozen river** | ice over a river: the crocodile swims under it (a dark shape), cracks it under a mole standing still over it for 1.5 s, then snaps; a blast opens the ice (open water, frozen again after 8 s) | `R` river ice, `5` with the crocodile, legend word `river`; header `croc: hits` |
| **Snowdrifts** | slow you like mud (enemies too); a blast clears them; in a blizzard they pile up on the gale lanes during the gusts (14 at most) | `,`; gale lanes `<` `>` `n` `u` with `gust:` |
| **Snowballs** | solid; a blast rolls one the way the blast went: it rolls until something stops it, crushing enemies (and hurting the mole) in its way, and grows as it rolls over snow: after 3 cells it is a big ball (drawn 2x2) that blocks a lane; a snowball that cannot roll (against a wall) shatters: never a softlock; a big ball breaks thin ice and sinks | `8` (floor + snowball), legend word `snowball` |
| **Icicles** | hang from the tunnels' ceiling; a blast within 2 cells makes them fall after 0.5 s (a shadow first): a hit for whoever is under them (enemies are stunned 3 s, or knocked out) | `I` (floor + icicle), legend word `icicle` |
| **Frozen dirt** | bombs only, it cannot be dug | `f` |
| **Night** | the helmet lamp: the fog's window and colour math, dark blue outside a circle round the mole (it flickers); the grubs glow from afar, enemies out of the light are only two shining eyes | header `night: 60` (the lamp's radius in pixels) |
| **The well's bucket** | an elevator between the surface and depth 2 (the well's two ends share a cell): step into the bucket and it takes you to the other end (1 s); a crank (walk into it) sends the bucket to the other end, so it can be called from both ends | `U` well (channel 1), `y` crank (channel 1); legend `well + chan:N`, `crank + chan:N` |
| **Perches** | dead trees on the surface where the owl rests; solid, a blast leaves them standing but hits the owl on them | `7` |

### Per season (about 3 per level; implemented ones in bold)
| Season | Gimmicks | Signature |
|---|---|---|
| Spring | **river with a log bridge**, **rain puddles (mud below, slows)**, **roots that regrow dug dirt**, **rotating sprinklers whose spray defuses bombs**, **windmill** | **the windmill (gusts)** |
| Summer | **dry soil (digs twice as fast)**, **corn field (cover; it burns and the fire spreads)**, **beehives (a bombed hive releases bees that chase the nearest creature)**, **steam vents (geyser lift one depth up)**, **a harvester sweeping a row or column on a timer**, **puddles that dry up**, **the badger (mini-boss)** | **steam vents** |
| Autumn | **leaf piles that hide grubs and blow around in the wind**, **pumpkins to push (Sokoban-like)**, **apple trees (bomb one: apples fall and stun)**, **bouncy mushrooms (launch you 2 cells)**, **fog (limited vision)**, **strong wind**, **the crocodile (2 hits)** | **strong wind** |
| Winter | **frozen river with slippery ice (the crocodile under it)**, **thin ice**, **snowdrifts that slow you**, **snowballs that a blast rolls along (growing, crushing enemies)**, **icicles that fall after nearby blasts**, **frozen dirt (bombs only)**, **night (helmet lamp)**, **blizzard (drifts on the gale lanes)**, **the well's bucket** | **the frozen river** |
| Underground, any season | **mine carts on rails (fast travel, crush enemies, levers switch the track)**, **gas pockets released by blasts (stun ferrets and you)**, glow-worms lighting dark caves, **an ant column carrying grubs away**, **a well bucket (elevator surface to depth 2, turned with a crank)**, **drain pipes** | |

Gimmicks not implemented yet are **data hooks**: their art is in `props.png` (placeholders), their TODO rows
exist, unknown header keys in level files are ignored, and the stub levels name the gimmick they are planned for.

### Where the gimmicks are used
| Level | Gimmicks |
|---|---|
| Spring 1 First Dig | digging (walls, hedge gaps, a glowing hidden grub), a hole down and a ladder back, puddles (mud below), tall grass |
| Spring 2 Rock Garden | bombs vs rock (walls, grubs inside rock), regrowing roots, holes down to the root cellar, ladders |
| Spring 3 River Crossing | river, bombable bridge, drifting log, tall grass cover, holes looping the three depths, sleeping ferret |
| Spring 4 Thin Floors | thin floors opening holes into sealed chambers on both undergrounds, rubble stuns, a timed gate with a plate on each side |
| Spring 5 Chain Reaction | rock spiral and bomb chains, remote detonator, sprinklers (spray defuses bombs), bombs tossed into a hole |
| Spring 6 Ferret Warren | warrens on two depths, sleeping ferret nests (noise), a lever two floors down that opens the gates around the exit |
| Spring 7 Windmill Hill | two windmills and their wind lanes (they push you and your bombs), two cats, a sleeping guard dog, tall grass |
| Spring 8 The Barn Cat | the barn cat (boss), sprinklers, puddles, tall grass, grubs in the cellar |
| Summer 1 Steam Lift | steam vents (signature), dry soil, corn |
| Summer 2 Corn Maze | corn maze (cover, fire that spreads), dry soil, a sunken den |
| Summer 3 Busy Bees | beehives (bees chase the nearest creature), puddles that dry up, corn cover |
| Summer 4 Harvest Time | two harvesters on timed lanes, corn in the lanes, rock posts that end them |
| Summer 5 The Badger | the badger (mini-boss): digs, charges, digs its own holes; rock posts stun it |
| Summer 6 Dry Tunnels | drain pipes (two pairs), gas pockets (stun clouds), sealed rooms, dry soil |
| Summer 7 Sprinkler Garden | sprinklers guarding stone-walled beds, steam vents lifting you or a bomb into them, drying puddles |
| Summer 8 The Farmer | the farmer (boss), crates, tomato splats, corn, holes for the cross-depth tomatoes |
| Autumn 1 Gale Force | strong wind lanes (signature), leaves hiding grubs and blowing away |
| Autumn 2 Pumpkin Patch | pumpkins (stepping stones in the river, hole plugs, lane blockers), a river and a cellar pond with crocodiles (2 hits) |
| Autumn 3 Orchard | apple trees on the cats' beats (apples stun, one heals), a mine cart on a bending line through the ferret tunnels |
| Autumn 4 Mushroom Hop | mushrooms over the bridgeless river and over stone walls (bombs bounce too), a crocodile (2 hits), mushroom caverns |
| Autumn 5 Foggy Morning | fog (cats as eyes), leaf piles hiding the path and grubs, tall grass cover |
| Autumn 6 The Ant Road | two ant columns carrying grubs to their nests (surface and tunnels), leaf piles, holes looping the depths |
| Autumn 7 Gale Mine | three gale lanes (you, bombs, leaves, cats drift), a mine-cart network with two junctions and their levers |
| Autumn 8 The Fox | the fox (boss: dashes, hides in the leaves, rests), two gale lanes, leaf piles, stone posts |
| Winter 1 Frozen River | ice and thin ice (signature), frozen dirt, snow |
| Winter 2 Thin Ice | thin-ice bridges over open water between two islands (2 crossings each), an ice cave with a pool |
| Winter 3 Snowball Fight | snowballs in three lanes between frozen hedges, cats in the lanes, drift tunnels below |
| Winter 4 Icicle Caves | icicles over the stoats' beats in two cave levels, frozen dirt |
| Winter 5 Long Night | night (the helmet lamp), glowing grubs, eyes in the dark, a maze of frozen dirt |
| Winter 6 The Old Well | the well's bucket and its two cranks (the only way to the cellar), the crocodile under the frozen pond |
| Winter 7 Blizzard | three gale lanes piling up snowdrifts, ice sheets, an ice-slide tunnel |
| Winter 8 The Snowy Owl | the owl (boss: swoops at its shadow, rests on dead trees, drops icicles below), ice lanes, frozen dirt |

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

Arc 2 (summer) is complete: level 1 shows the signature (steam vents), levels 2-7 each add one summer
mechanic (corn fire, bees, harvesters, the badger, pipes and gas, sprinklers with vents) and level 8 is the
farmer. Arc 3 (autumn) is complete: level 1 shows the signature (strong wind), levels 2-7 each build on one
autumn mechanic (pumpkins, apple trees and a mine cart, mushrooms, fog, ants, carts and junctions under the
gale) and level 8 is the fox. Arc 4 (winter) is complete: level 1 shows the signature (the frozen river),
levels 2-7 each build on one winter mechanic (thin ice, snowballs, icicles, night, the well and the crocodile
under the ice, the blizzard) and level 8 is the snowy owl. `tools/make_stub_levels.py` still writes stubs for
missing levels (it never overwrites a hand-made file); no level is a stub now.

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
fog: 72                 # optional: fog, the radius in pixels of the clear circle around the mole
croc: 2                 # optional: hits to defeat the crocodile (0: it can only be stunned)
night: 60               # optional: winter night, the helmet lamp's radius in pixels (dark blue outside it)

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
| `^` | (obsolete: holes only go down; the way up is a ladder `H`) | `o` / `O` | remote detonator (floor / in dirt) |
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
| `e` | beehive | `*` | gas pocket |
| `%` | water with a crocodile | `0` | floor with a pumpkin |
| `T` | apple tree | `!` | bouncy mushroom |
| `+` | rails | `$` | rails with a mine cart |
| `N` | ant nest | `A` | ants carrying a grub (on floor) |
| `,` | snowdrift | `8` | floor with a snowball |
| `I` | floor with an icicle above (tunnels) | `R` / `5` | river ice / with the crocodile under it |
| `U` | well (channel 1; both ends, surface and depth 2) | `y` | well crank (channel 1) |
| `7` | perch (dead tree, the owl's) | | |

The gate is the vertical bar character (channel 1). Legend words for custom entries: every terrain name
(`floor stone soft_dirt hard_rock roots frozen_dirt leaves water puddle thin_floor hole_down hole_up ladder exit
bridge ice thin_ice mud tall_grass corn burnt gate plate lever steam_vent pipe crate sprinkler windmill beehive
gas_pocket apple_tree mushroom rails ant_nest snowdrift well crank perch`), `harvester:up|right|down|left`, items
(`grub bomb fire speed remote heart apple`), actors (`mole p2 p3 p4 ferret cat boss dog croc ants`), objects
(`pumpkin cart snowball`), `river` (ice over a river), `icicle`, `asleep`, `log`,
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

## Dev mode (for the owner)
- **Turning it on**: `--dev` on the command line of the exe (or `--opt dev=1`), or on the title screen hold
  L+R and press Start ("DEV MODE" shows under the menu). Options then shows "DEV: ALL LEVELS" (on by
  default): every level is unlocked in level select.
- **Cheats**, from the pause menu's second row or with keys: F1 / GOD invincibility, F2 / POWER all
  power-ups (8 bombs, range 8, speed 3, remote, 3 hearts), F3 / SKIP the level (counts as cleared, not
  saved), F4 / DEPTH jump to the next depth (the nearest open cell), F6 / SEE show hidden grubs, hidden
  power-ups and the exit on the pause map (the glow of hidden grubs shows on Hard too), F7 / MS the
  frame-time overlay (update and render ms of the last frame, sprites and the most on one line, palettes in
  use, and the strict-mode guideline warnings seen so far).
- **Level reload**: F5 reloads the current level from its text file and restarts it (no rebuild). The
  desktop builds look for `games/bombermole/levels/` next to the exe (then in the working directory, and in
  `data/`) and fall back to the embedded levels; a level with an error is not loaded (the log says why). With
  `--dev` they also search upwards from the exe's folder (up to 4 levels, e.g. `../../games/bombermole/levels`
  from `dist/windows`), so a desktop shortcut to the dist exe uses the repository's levels. The F7 overlay and the
  log show the folder a level came from.
- **Saves**: dev mode never writes the save's progress (cleared levels, best times); options are still saved.
- The headless runner takes dev keys from input scripts: `<frame> key F5`.

## Later ideas
- **A crocodile** (summer or autumn river levels): it looks like a floating log until you are next to it;
  it snaps at anything that stands on the water next to it and can be stunned with a blast. Not implemented
  (the log is drawn so it cannot be mistaken for one).

## Multiplayer
1 to 4 players on one screen, split by the SDK's viewports (docs/spec.md, "Viewports"). The title menu has
**STORY** (straight to the season select, alone: a lone player never sees the join screen; the season select
has a **2-4 PLAYERS** entry that opens it for co-op) and **BATTLE** (2-4 moles, humans and CPUs). Code: `src/mp.c` (views, cameras, HUD strips, join screen, battle rounds), `src/world.c` (the rules, the
CPUs, sudden death).

**Join screen.** The pad that opened it is P1, already in. A or Start from any pad not in yet joins it:
players are P1..P4 in join order (a pad's port can be any of the four: the keyboard is pad 1, WASD pad 2, then
the game controllers); when nobody is in, Start joins that pad as P1 and starts at once, so nobody is ever
stuck. Left/Right choose the helmet colour (unique; red, blue, green, yellow in P order by default), B leaves.
In battle P1 adds a CPU with X, removes one with Y and sets their skill with L/R (easy, normal, hard). Start
from any player begins: STORY goes back to the season select (alone: solo, 2-4: co-op); BATTLE to the arena
menu (arena, wins needed 1-5, round time, bombs into holes on/off), a lone battler getting a CPU mole.

**Colours.** The moles differ by their **miner's helmet only**: red, blue, green, yellow. The fur, nose, claws
and lamp keep the art's colours. `tools/palette_variants.py` finds the helmet in the mole's frames (the true
reds connected to its mid and light reds; a red also drawn elsewhere, like a dark red shading the AI mole's
face, becomes two palette entries) and the asset build exports the mole palette four times with only those
entries recoloured, keeping their shading (3 shades, hue-shifted: cooler shadows, warmer lights):
`bm_mole_helmet_pals`. The placeholder mole wears a small red helmet with a yellow lamp. A **P1..P4 marker**
above each mole and on its HUD strip is drawn from the font in the helmet's lightest shade on the outline
colour. Preview: `docs/art-preview/mole-helmets.png` (`tools/mole_helmets.py`, the placeholder and the AI art,
4x). Each extra mole needs a sprite palette: the boss's or the critters' when the level has none, then
an enemy one while enough stay for the enemy variants; when none is left a mole shares P1's (the marker tells
them apart). Battle arenas have no enemies: every mole has its own palette.

**Views and cameras.** One view per human (per mole when only CPUs play: a spectator screen). Layouts: 1 = full
screen; 2 = left/right halves of 159x240 (option: top/bottom 320x119); 3 = four quadrants with a **live map** in
the 4th (option: a top half and two bottom quarters); 4 = quadrants of 159x119. Each view has a 16-px HUD strip
(its mole, hearts, bombs, flame, and the grubs left or the wins) and a play area with its own camera: a dead
zone of 1/8 of the view, then a smooth catch-up (1/6 of the distance per frame), clamped to the level (a level
smaller than the view is centred; the code works for bigger levels). A view shows **its mole's depth**: in
multiplayer the three depths are drawn at once in their own 256-px slots of the BG3/BG4 maps (64x128), and a
depth change slides that view alone from one slot to the other (0.6 s, the earth between them passing by).
Shared information: in co-op a goal bar over the divider shows the grubs left on each depth; with 3 views the
map shows the three depths, the moles and the counts; in battle a clock bar shows the round and the time left.
Option (co-op, 2 players): **shared view** - while the two moles are on one depth and close together they
share one full-screen view (two HUD strips side by side), and it splits again when they move apart (with
hysteresis; the cameras glide). Night and fog work per view: each view's raster callback puts the lamp circle
round its own mole.

**Co-op story (2-4 players).** The same 32 levels. The grubs count for everyone; the molehill opens when all
are found, and **the first mole to enter it clears the level for the team** (the others are taken along: no
waiting at the door). Hearts and power-ups are per player. A knocked-out mole comes back after 5 s, with 1
heart and a short invulnerability, next to a teammate who is still up (or at its start); the level is lost
only when every mole is down at once (one shared life is lost, as in solo). Friendly fire is an option, off by
default: a teammate's blast then only stuns for 1 s (your own bombs always hurt). More moles, more enemies:
one more on each depth that has some per extra mole (like Hard), and the bosses get half their health again
per extra mole. Progress is the story's save (shared). Pause: any player's Start pauses; the pause box has
RESUME, RESTART and QUIT. Options (title menu): friendly fire, shared view, 2-player split.

**Battle (2-4 moles).** Bomberman rules: last mole standing wins the round; first to N wins (3 by default)
takes the battle, then a results screen with the tally. One heart each; soft blocks (dirt, leaves) drop a
power-up 40% of the time when a blast breaks them (bomb 30%, fire 30%, speed 20%, heart 10%, remote 10%).
Moles do not hurt each other by touch. A round lasts 90 s (menu: 30 s to 5 min), then **sudden death**: every
hole, ladder, pipe and well closes (each mole stays on its depth) and stone blocks fall on all three depths,
from the walls inwards along a spiral, the same cell in the four quarters at once (so no start is safer),
one step every 0.2 s with a warning mark on the next ones: whatever they fall on is knocked out. A double
knock-out within a second is a draw. **Bombs into holes**: a bomb dropped while facing a hole (or pushed,
slid or blown into one) falls to the depth below, a cross-depth attack (menu option, on by default).
**CPU moles** fill empty slots: they read the blasts they know about (fuses, chains, sudden-death marks) and
never step into one; they bomb a mole in line or a soft block only when they can then walk out of the blast,
hunt power-ups, chase the nearest mole (through holes and ladders to its depth), and dig or push through when
stuck. Easy reacts every 0.25 s and misses half its chances, normal every 0.1 s, hard every frame and also
tosses bombs down holes onto moles below.

**Arenas** (`games/bombermole/arenas/*.txt`, the level format with `mode: battle`, the starts `M 2 3 4`, no
grubs, no exit). Each is drawn as a top-left quarter mirrored left-right and top-bottom by `tools/make_arenas.py`
(the files are plain text), so the four starts are the same place turned round:

| Arena | Season | Identity |
|---|---|---|
| Molehill Maze | spring | the classic grid of stone posts and soft dirt, holes everywhere between the three depths |
| River Duel | summer | a river across the middle, two bridges each side, the crocodile in the middle (it starts in one of the 4 mirrored cells, by luck); tunnels under the riverbed |
| Windmill Wars | spring | gale lanes blowing towards the middle: a bomb dropped in a lane drifts into the other half |
| Ice Rink | winter | ice sheets ringed by snowdrifts: slide, kick bombs across the ice; rocks are the brakes |
| Mine Cart Mayhem | autumn | runaway carts (legend `rails + runaway`) race round a loop and flatten any mole on the rails |
| Pumpkin Fort | autumn | each start is a fort of pumpkins: push them out as cover, or into a hole to seal it |

`tools/check_levels.py` checks the arenas too: the 4 starts on the surface, holes that land on ladders, at
least 4 holes on the surface and 2 on depth 1, no softlock (every start reaches the others and every depth,
and from every cell it can reach, all the starts again), **fair starts** (the same cost to the nearest soft
block and to the nearest opponent from every start, within 1) and **safe starts** (no other start within 10
cells on foot without digging: 3 s or more; no enemy within 6 cells). `tests/data/levels_bad/arena-unfair.txt`
must fail. The smoke test runs every arena with 4 CPUs in 4 views, then 200 rounds of 4 CPUs on each arena
without the picture (`--opt battlesim=200`, about 1.5 s each): no start may win more than 40% of the rounds
won.

**Headless options** (tests, screenshots): `--opt mp=coop --opt players=N --opt level=spring-3` (co-op),
`--opt mp=battle --opt players=4 --opt cpus=K --opt arena=NAME [--opt skill=1..3] [--opt sd=N: sudden death after N frames]`,
`--opt battlesim=N --opt arena=NAME`, `--opt screen=join` (nobody in; `--opt story=1`: the story's), `--opt ff=1`, `--opt merge=1`, `--opt splith=1`,
`--opt map3=0`. Input scripts address the pads with `P1`..`P4`. The dump adds an `mp:` line (views, cameras,
each mole's depth, cell, hearts and state).

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
- **Character size**: 24x24 in the default art set (`art-ai/`), 16x16 in `art/`; the engine and the levels take 16, 24 or 32-px characters on the
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

## Level ideas
Every level has one clear idea (below). `tools/check_levels.py` (in `make check`) enforces the design rules on
every level, stubs included, on top of solvability and the softlock search:
- **An enemy on every playable depth** (a depth with grubs, or with open floor the mole can reach), within the
  level's tier curve, and none closer than 6 cells to the mole's start (a quiet start).
- **Holes only go down, ladders only go up**: no hole up (`^`) anywhere; a hole down lands on a ladder.
- **Busy surfaces** (the owner's playtest: open lawns were too easy): no rectangle of plain floor larger than
  12 cells on the surface, at least 30% of the surface's inside is structure (walls, blocks, water, cover,
  items, enemies), and at least 3 cats on the surface from spring 3 on (1 in spring 1-2, which stay easy);
  boss levels as they are.
- **Every depth is unique**: a playable depth may not share more than 60% of its layout with a depth of
  another level (the Jaccard index of its rarer kind of cell: the open cells of a tunnel level, the structure
  of a lawn), so a template cannot be reused.
- **No big empty areas**: on a playable depth, no rectangle of plain floor larger than 20 cells (both sides 3
  or more), and each 6x6 sector with 12+ open cells has at least 25% of something other than plain floor
  (blocks, items, enemies, gimmicks, push fields; a windmill's or a harvester's lane counts as a push field).
- **Every gimmick matters**: a point gimmick (plate, lever, gate, pipe, vent, bridge, sprinkler, crate,
  windmill, thin floor, beehive, gas pocket, apple tree, mushroom, ant nest, well, crank, icicle) lies within 2 cells of a required path, i.e. a shortest path (digging costs more than
  walking) from the start to a grub, the boss or the exit, or from a grub to the exit; an area gimmick (ice,
  thin ice, tall grass, puddles, mud, push fields, logs) touches one; plates and levers matter when their gate
  does; the farmer's crates are required targets; a pumpkin, a mine cart or a snowball lies within 3 cells of
  one; snowdrifts are area gimmicks.
- **Autumn in the solver**: a mushroom is an edge 2 cells on (the hop); pumpkins do not count as plain floor;
  the ant nests of a depth with ants are required targets (a grub may end up inside); apple trees are solid.
- **Winter in the solver**: **thin ice bears 2 crossings**: some order of the grubs (then the exit) must get
  through with each thin-ice cell crossed twice at most, the legs being cheapest paths that avoid the thin ice
  already crossed twice (a level may not rely on waiting for the refreeze); snowballs are movable blockers (a
  blast rolls or shatters them: like a block); the well is an edge between the surface and depth 2, and each
  well needs a crank of its channel on both depths (else the bucket could be left at the wrong end); the owl
  needs 2 or more perches on the surface with a reachable cell next to them; the crocodile may be under river
  ice; the blizzard's drifts count in the prop families of a winter level with gale lanes.
- **At most 2 prop colour families**: the props a level uses (or makes: a pumpkin makes mush, a puddle mud)
  must fit the 2 BG palettes for props.
- Known-bad levels in `tests/data/levels_bad/` must fail each rule (smoke test).

| Level | Idea |
|---|---|
| Spring 1 First Dig | Out of the burrow, down the hole, along the tunnel and up the ladder into the hedged garden: teaches walking, digging, holes, a glowing hidden grub, the ladder, the exit (tier 1). |
| Spring 2 Rock Garden | Every grub is walled in: bomb the rock beds on the grass, then two floors down to the root cellar where blasted roots grow back behind you. |
| Spring 3 River Crossing | Three ways across the river: the bridge (lure the cat over, then blow it up), the drifting log, or the long way under the riverbed. |
| Spring 4 Thin Floors | Trapdoors: bomb a thin floor to open a hole into a sealed chamber (and drop the rubble on the ferret waiting below), two floors deep. |
| Spring 5 Chain Reaction | A rock spiral too long for one bomb: chain bombs, toss one down a hole to clear the landing, keep bombs out of the sprinklers' spray. |
| Spring 6 Ferret Warren | The molehill is fenced in and its lever lies at the bottom of the warren between sleeping ferrets: sneak in without a single blast. |
| Spring 7 Windmill Hill | Ride and fight the gusts: time the crossings of two windmill lanes, and let a gust carry a bomb to the sheltered rock that holds a grub. |
| Spring 8 The Barn Cat | Boss arena: a Bomberman barn floor of posts, hay and puddles; bait the barn cat into bomb lanes after taking the grubs hidden under the barn. |
| Summer 1 Steam Lift | Geysers lift you into sealed rooms no tunnel reaches; fire a bomb up the deep vent first to clear the nest above. |
| Summer 2 Corn Maze | Sneak along the rows of corn past the cats; a bomb burns a shortcut through a field, but the fire spreads and the cover is gone for good. |
| Summer 3 Busy Bees | Hives stand by the enemies' dens: bomb one when an enemy is nearer than you, then run for a puddle or the corn (and the puddles dry up). |
| Summer 4 Harvest Time | Two harvesters sweep the field on a timer: time your crossings of the flashing lanes, and lure the cats into them. |
| Summer 5 The Badger | Mini-boss arena on three depths: lead the badger's charges into the rock posts, bomb it while it is dazed; it digs after you. |
| Summer 6 Dry Tunnels | Far-apart rooms linked by drain pipes; blast the gas pockets by the ferret nests to stun them, then walk past. |
| Summer 7 Sprinkler Garden | Grubs in stone-walled beds guarded by sprinklers; ride a vent up into a bed, or send a bomb up it, and time the spray. |
| Summer 8 The Farmer | Boss arena: his four crates stand in the corners of the corn field, so every crate run crosses his line of fire; weave through the corn and dive down holes. |
| Autumn 1 Gale Force | Gale lanes push you and your bombs; the gusts blow the leaf piles away and uncover the grubs. |
| Autumn 2 Pumpkin Patch | Sokoban in the patch: push pumpkins into the river as stepping stones (the only other way over is the bridge the crocodile watches), onto a hole to plug it, or into a lane to shut a cat out; a blast turns one into slowing mush. |
| Autumn 3 Orchard | Apple trees along the cats' beats: bomb next to a tree when a cat walks under it (the apples stun it, and you, for 3 s; the first apple you pick up is a heart); below, ride the mine cart to run the ferrets over. |
| Autumn 4 Mushroom Hop | No bridge: mushrooms on the banks hop you over the river (and the crocodile), others over stone walls; a bomb dropped facing a mushroom bounces over the wall onto what hides behind it. |
| Autumn 5 Foggy Morning | You see only a circle around you; the cats are two eyes in the fog until they are close; the grubs' glow shows through the fog, the leaf piles hide the way, the tall grass hides you. |
| Autumn 6 The Ant Road | A race: two ant columns carry grubs to their nests; catch them on the way (touch them or a blast), or blast the nest open afterwards. |
| Autumn 7 Gale Mine | Gale lanes rake the surface (you, your bombs, the leaves and the cats drift); below, set the two junction levers to send the mine cart to the far rooms, running the ferrets over. |
| Autumn 8 The Fox | Boss arena: two gale lanes and leaf piles; the fox dashes along the lanes and hides in the leaves; after 3 dashes it stops to pant: that is the moment to hit it (4 hits). |
| Winter 1 Frozen River | Cross the frozen river in one slide; only a bomb kicked across the ice cracks the frozen island in the middle. |
| Winter 2 Thin Ice | A frozen lake of open water crossed by thin-ice bridges between two islands: each bridge bears two crossings, so plan the order (and lure the cats onto a cracked one); below, an ice pool whose two islets each take one bridge in and out. |
| Winter 3 Snowball Fight | Three lanes between frozen hedges, a snowball at the head of each and a cat in it: bomb behind a ball and it rolls down the lane, grows and flattens the cat, then blocks the lane's end (blast it on, or shatter it off the grub it stopped on). |
| Winter 4 Icicle Caves | The tunnels are hung with icicles over the stoats' beats: bait a stoat under them and blast within 2 cells; their shadow gives you 0.5 s to get out from under them yourself. |
| Winter 5 Long Night | Night: the helmet lamp's flickering circle is all you see; follow the grubs' glow through a maze of frozen dirt and watch for shining eyes. |
| Winter 6 The Old Well | The well's bucket is the only way down to the cellar (a crank at each end calls it); above, a grub lies on a frozen pond with the crocodile under the ice: slide, stop, go, and never stand still. |
| Winter 7 Blizzard | Three gale lanes rake the field and pile up snowdrifts during the gusts; between them, sheets of ice you cannot stop on; bombs drift in the wind. |
| Winter 8 The Snowy Owl | Boss arena: a clearing with three dead trees; sidestep the owl's shadow, then bomb it on its perch (kick bombs along the ice lanes to get there in time); underground it drops icicles through the ceiling. 5 hits, faster after 3. |
