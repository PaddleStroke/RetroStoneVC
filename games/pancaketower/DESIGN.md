# Pancake Tower: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/pancaketower/LICENSE). A RetroStone VC game
(docs/spec.md), in the house style of the family (docs/art-direction.md, Leady Squid's look).

## Pitch
A chubby chef is making the tallest stack of pancakes the world has ever seen. A pancake slides left and
right above the tower; press A to drop it. Whatever hangs over the pancake below is cut off and flops down
the side with a drip of syrup, and the next pancake is that much narrower. Drop it spot on and it keeps its
width, and a chain of perfect drops makes the pancakes grow back. Every tenth pancake a topping lands on
the tower; syrup makes the next pancake slide a little. The camera climbs with the tower: through the
kitchen ceiling, out of the roof, past the birds and the clouds, up to space, where a cow jumps over the moon.
One button, instant retry, a best height to beat.

It is a "Stack"-style timing game (Ketchapp, 2016). Its mechanics follow the genre's reference feel (a
ping-pong slide, the cut of the overhang, a perfect-drop window, the regrow after a chain of perfects, a
pace that quickens with the height). Nothing else is taken from it: no name, art, sound, layout or text.
Stack is 3D; ours is a clean 2D side view (below). The pancakes, the chef, the toppings, the syrup, the
journey upward, the screens, the medals, the sounds and the music are ours.

## Legal and originality
- Stack is proprietary. We use only its **mechanics** (not protected), as described by published guides
  and as estimated by permissively licensed clones.
- Only MIT-licensed clones were read, only for their numbers and rules; the code of Pancake Tower is our own
  (C11 on the RetroStone SDK, integer arithmetic, no floating point in the game).
- All sources and their licences: "Feel sources" below.

## Screen and layers
320x240 at 60 Hz, fixed step. The world is a vertical column: y = 0 is the top of the plate (the tower's
base), up is positive; a layer of the tower (a pancake or a topping) is **8 px** tall. The camera keeps the
top of the tower at screen y = 120 once it has climbed that high; it eases toward it (at most 2 px per frame).

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the sky: one colour per line (raster callback, HDMA-like), a gradient that follows the altitude (a garden morning through the kitchen window, blue sky, a paler band in the clouds, deep blue then violet in the stratosphere, black in space) | - |
| BG4 | far: the garden and the neighbour's house through the kitchen window, the town's roofs seen from above, far clouds, the curve of the Earth, far stars | 1/4 of the camera |
| BG3 | near: the kitchen (tiled wall, window, shelves, pans, the counter), the ceiling slab, the attic, the roof, near clouds, cirrus streaks, stars | 1 (the tower's world) |
| BG2 | the tower: every layer but the top one, drawn per row into the map (the edges of each row are tiles made at run time, pixel-exact); the **wobble** is a per-line scroll | 1 |
| BG1 | text and panels (the house UI: title logo, banners, the game-over panel, pause) | fixed |
| OBJ | the sliding pancake, the top pancake (it squashes on landing), the cut-off pieces, the falling toppings, butter pats, sparkles, syrup drips, crumbs and plaster debris, birds, the cow and the moon, the chef, the score digits and medals | |

Map sizes are kept small for the VRAM guideline: BG2 32x32 (the tower's columns only; 2 players: a half
each), BG3 64x32, BG4 32x32 (256 px wide, it repeats); rows are streamed as the camera climbs (a ring of 32
rows is more than the 31 rows on screen).
The near scenery is made of five **segments** of 256 px (32 rows) that share one tile set (at most 704
tiles, all loaded at the start: in 2 players each viewport may show any segment); their map rows are streamed
into the 32-row ring as the camera climbs.

| Segment | World y | Contents |
|---|---|---|
| 0 kitchen | -64 .. 192 | the floor and the counter (the plate sits on it), the tiled wall with a window, shelves, hanging pans; the **ceiling slab** at 96..120 (the tower breaks through at its 13th layer: a comic crash); the attic (rafters, boxes, a round window, a cobweb); the **roof** at 168..192 (tiles fly off when the tower pokes out) |
| 1 sky | 192 .. 448 | the open sky; the town's roofs far below (BG4); birds fly across (sprites) |
| 2 clouds | 448 .. 704 | puffy clouds near and far (two layers of parallax) |
| 3 stratosphere | 704 .. 960 | thin streaks, a weather balloon, the sky turns deep blue and violet; the first stars (BG4) |
| 4 space | 960 .. | stars (repeating every 256 px), the moon, and **a cow jumping over the moon** now and then |

## Palettes
The house layout (docs/art-direction.md). BG: 0 the UI (the house kit; entry 0 the sky gradient), 1 the
tower (pancakes, syrup, butter, the plate), 2 the toppings layers, 3, 4 and 6 the near scenery, 5 the title
logo, 7 the far layer. OBJ: 0 the chef, 1 the chef of player 2 (his neckerchief and trousers recoloured), 2
effects (plaster, dust, roof tiles, a star burst, sweat, broken edges), 3 the kit (digits, glyphs, sparkle), 4
food (the pancakes drawn at run time, butter, syrup, the bottle, the portrait's plate; the same colours as
BG 1), 5 the toppings (the same as BG 2), 6 birds, the balloon, the plane, the satellite, the moon, the cow.

## The core (the Stack feel)
- The **slider**: a pancake of the current width slides at the level just above the tower (4 px above the
  top surface). It enters from **the left, then from the right, alternately** (the 2D stand-in for Stack's
  alternating X and Z axes): it starts `SPAWN_DIST` px to that side of the top pancake's centre and
  ping-pongs between the two sides at a constant speed until the player presses.
- **Drop** (A or any face button): the pancake falls straight down onto the tower (3 frames).
  Its position is frozen at the press, in whole pixels.
  - The part that overhangs the pancake below is **cut off**: it flops down the side of the tower, turning
    over, with a drip of syrup, and falls off-screen. The pancake that stays is the overlap: the next one
    has that width. Cut maths: `overhang = |x - x_below|`, `width' = width - overhang`, the kept part
    starts at `max(x, x_below)`.
  - **Missing completely** (no overlap) = the whole pancake falls: game over.
  - **Perfect**: when `|x - x_below| <= tolerance`, the pancake snaps exactly onto the one below and keeps
    its full width (a ding, a flash, a butter pat on top, a sparkle; the chef cheers). The tolerance is
    7.5 % of the width, between 3 and 4 px (a few pixels; never narrower than one frame of motion, so
    some frame always lands in it).
  - A **chain** of perfects rises the ding's pitch (a scale). From the **8th** perfect in a row, every
    perfect also **grows the pancake back** by 16 px (8 px per side, never out of the first pancake's
    footprint, as the clone clamps its growth):
    a bigger butter pat, a sparkle burst.
- **Speed**: the slide speed rises with the height: +8 % every 15 pancakes, up to +40 % (from pancake 75
  on it stays the same).
- **Wobble** (visual only): the tower sways gently, more as it grows (a slow sine, anchored at the plate,
  its amplitude rising with the height, plus a small jiggle after each landing that dies out). It is a
  per-line scroll of BG2. The slider and the top pancake are drawn with the same offset as the top of the
  tower, so what the player lines up on screen is exactly what the game compares: the wobble never changes
  a result, and it is deterministic (a function of the frame and the height).

## The twist: toppings, syrup and the journey upward
- **Toppings**: after every 10th pancake a topping falls on the tower, in rotation: **strawberry slices,
  blueberries, banana slices, chocolate chips, whipped cream**. It lands on the top pancake with a soft
  plop and becomes a topping layer of the same width (8 px, not cut, not scored); the next pancake lands on
  it and is cut against it. The chef cheers.
- **Syrup**: after the 5th, 15th, 25th... pancake a syrup bottle pours a stream of syrup on the top pancake
  (a glossy puddle shines on it and drips run down its edges). The **next** pancake lands on it slippery: after the drop
  it slides `SYRUP_SLIP` = 6 px on in the direction it was moving, easing out over 12 frames, then it is
  cut. The slide is always the same, so the player can compensate (drop a little early). A perfect is
  judged after the slide.
- **The journey**: the camera rises through the kitchen (the counter, the wall, the window), the **ceiling**
  (the 13th layer crashes through it: plaster chunks and dust fly, the screen shakes, a crash), the attic,
  the **roof** (tiles fly off), the sky with birds, the clouds, the stratosphere, and space, where a cow
  jumps over the moon. The backdrop, the far layer and the music change with the altitude.
- **The chef** (chubby, a tall white toque, a moustache, a red neckerchief, a round belly in an apron)
  stands on the kitchen floor at the left of the counter: he watches, **cheers** (arms up) on a perfect and
  a topping, **panics** (hands on his cheeks, sweat drops) when the pancake is narrow (under 24 px) or a
  big piece is cut off, and despairs at the game over. When the camera leaves the kitchen, he keeps
  watching from a plate-rimmed portrait in the bottom-left corner (the same frames).

## Score, medals, save
- Score = the height in pancakes (toppings do not count) + a **perfect bonus**: +1 for each perfect,
  +2 from the 5th perfect in a row on. The height is shown big at the top, a "+1"/"+2" pops up by the tower
  on a perfect.
- The best score (and the best height) in save RAM, with the number of runs and the medals won.
- Medals, the shared house tiers (bronze, silver, gold, pearl): a **golden fork** set, at 25, 50, 75 and 100
  points.

## Flow (no menus in the loop)
1. **Title**: the logo, the kitchen, the chef, a pancake sliding, "PRESS A", the best score.
2. **Ready**: the same scene without the logo; the first press drops the first pancake (the slider is
   already moving).
3. **Play**.
4. **Game over**: the missed pancake flops down the whole tower, the chef despairs, then the house panel:
   GAME OVER on its banner, SCORE (and the height in pancakes), BEST (NEW BEST), the fork medal. After 0.6 s
   any face button retries at once (back to 2).

**Pause**: Start (or Select) during a run (the picture dims, PAUSED); Start or Select again resumes.

## Controls
| Button | Action |
|---|---|
| A (or B, X, Y) | drop / start / retry |
| Start (or Select) | start on the title and the panel; pause in a run |
| pad 2, a face button on the title or ready screen | player 2 joins (versus) |

## 2-player versus (split screen)
Player 2 joins from the title or ready screen with a face button on pad 2. The screen splits into two
viewports (left and right halves, 159x240 each); each shows its own tower, its own camera and its own
scenery (each viewport has its own scroll; the raster callback draws each viewport's sky and wobble).
Both towers follow the same rules and the same slide timings (the game is deterministic: the same height
gives the same speed and the same side), in a geometry scaled by 2/3 so that a tower fits its half
(64-px pancakes, the same times). **Syrup splash**: every 3rd perfect of a chain splashes syrup on the
rival's top pancake (it flies in from the rival's side), so their next pancake slides (4 px: 6 scaled). A player who misses stops (their tower
stays, their chef despairs); when both have stopped, the tallest tower wins (a tie on height goes to the
score). The panel shows both heights and the winner.

## Audio
- Sound effects synthesised in code at start-up (src/sfx.c): a soft **flop** on landing (a low thump and a
  short "fwup" of noise; its pitch rises a little with the height), a **ding** for a perfect (a round bell;
  the pitch climbs a major scale along a chain, as a Stack chain climbs its notes), a **syrup squish**, the
  **splat** of a cut-off piece, the **ceiling crash** (and a smaller roof crash), a plop for a topping, a
  pour for the syrup bottle, a "whoosh-splat" for a syrup splash, a slide-whistle fall for a miss, a jazzy
  arpeggio for a topping and a regrow; the UI sounds are the house kit's (confirm, pause, medal, game over).
  Made with the house synthesiser (house_audio.c ha_tone, ha_hiss) and stored at 16 kHz (they are low and
  soft: half the sample memory, inside the 64-KiB guideline with the house set); mixed at the house
  loudness (the same RMS as Leady Squid, no clipping).
- Music (tools/make_music.py, 4-channel MODs played by libxmp-lite): a cosy **jazzy kitchen loop** (a
  swung walking bass, soft electric-piano chords, a brushed hi-hat, a little vibraphone melody) that evolves
  with the altitude: the same tune, re-voiced: **airy** in the sky and the clouds (pads and a flute, the bass
  lighter), **spacey** in space (a slow echoing arpeggio, a deep pad). The music crossfades at a segment change.

## Art
All art is code-drawn (tools/make_art.py), in the house style (docs/art-direction.md, games/common/tools/
house_style.py): flat shades (2-4 per material), 1-px dark outlines that are never black, light from the
top-left, readable at 1x.
- **Pancakes**: round-edged, golden, shaded (a light top rim, a golden body, a browner lower band, a dark
  outline); each pancake has its own edge variation (bumps, browned spots) from its number.
- **Toppings**: strawberry slices (red with seeds, a pale core), blueberries (round, a highlight), banana
  slices (cream discs with a seed ring), chocolate chips (dark drops), whipped cream (white swirls).
- **Syrup**: amber, glossy: a puddle on the top pancake, drips as small animated sprites, the bottle and its
  stream, the splash of a rival.
- The chef (32x48 frames: idle, blink, cheer, panic, despair), the birds, the plane, the balloon, the
  satellite, the moon and the cow; plaster, dust, roof tiles, the broken edges; the golden-fork medals in the
  house tiers.
- **The house kit** (games/common): the UI is house_ui.c (the logo, GET READY, the prompt and the A glyph,
  the copyright, the join line, the banner and game-over panel, pause, the digits, the sparkle, the slide-in,
  the shake), the sounds house_audio.c, the art house_style.py (Canvas, outline, shading, ramps: the
  "pancake" and "syrup" accents), the music house_music.py (the MOD writer and the instruments).
- The scenery per segment (above).

## Feel sources
Stack (Ketchapp, 2016) is proprietary; no source code of it is published. Its feel is known from
published guides and from clones that reproduce it.

| # | Source | Licence | What we took (rules and numbers only) |
|---|---|---|---|
| 1 | **open-stack-game**, Oliver Broughton, https://github.com/oli-broughton/open-stack-game (`Assets/Scripts/Stack.cs`, `GameUtils.cs`, `PingPongInfinite.cs`, `BlockGrow.cs`, `PerfectPlacementAudio.cs`, the values in `Assets/ScriptableObjects/Config/*.asset`) | **MIT** (LICENSE, (c) 2018 Oliver Broughton) | block 9 units wide, 1 unit tall; the moving block ping-pongs linearly between +10 and -10 units from the stack's centre, 1.25 s per traverse; the axis alternates (X, Z); a perfect is an offset of at most **7.5 % of the block's size** (PerfectPlacementErrorMargin 0.075), and it keeps the block's full size; a miss is no overlap; the regrow comes after **8 consecutive perfects** (two levels of 4 with a flash, then a pulse) and adds **1.5 units per side** (of 9), capped at the starting size, and each further perfect in the chain grows again; a perfect plays the next note of a scale, a completed scale its own sound; the overhang becomes a falling rigid body |
| 2 | **playstack**, Luke Yoshioka (zurkon), https://github.com/zurkon/playstack (`src/lib/game.js`, `block.js`, `stack.js`) | **MIT** (LICENSE, (c) 2021 Luke Yoshioka) | the cut: `overhang = abs(delta)`, `overlap = size - overhang`, overlap <= 0 is a game over, the kept block is shifted by `-delta / 2` and the overhang falls beside it; each new layer starts on the far side (-10 units for a 4-unit block) and the axis alternates |
| 3 | **Level Winner, "Stack (Ketchapp) Tips, Tricks & Cheats to Get a High Score"**, https://www.levelwinner.com/stack-ketchapp-tips-tricks-cheats-to-get-a-high-score/ | article (facts cited) | a perfectly centred layer flashes; successive perfects make the following blocks larger; the pace **quickens every 15 to 20 points**, and it **stops getting faster after a while** |
| 4 | **148Apps, "Stack guide - How to build the highest tower"**, https://www.148apps.com/stack/stack-guide-how-to-build-a-great-tower/ | article (facts cited) | a perfect shows a white outline and the music changes (a note); an imperfect drop loses a small portion; enough perfects make the blocks grow again; missing the tower is an instant game over; it plays like a rhythm game |

### From the sources to our world
- **Width and slide**: our first pancake is 96 px (the clone's 9 units = 96 px, 10.7 px per unit); the
  slide goes 107 px each side of the top's centre (10 units), 1.25 s (75 frames) per traverse: **2.853
  px/frame** at the start. Stack's first block is roughly as wide as a third of the screen; ours is 30 %.
- **Perfect**: 7.5 % of the width, as the clone, but between **3 and 4 px** ("a few pixels"): 4 px for the
  full 96-px pancake (the clone's 7.2 px felt too generous in 2D, where the offset is seen straight on), and
  never less than 3 px for a narrow one: a 7-px window is wider than one frame of motion at the top speed
  (4.0 px/frame), so some frame always lands in it (a 2-px floor made narrow towers a lottery).
- **Regrow**: after 8 perfects in a row, as the clone, then with each further perfect. The clone grows each
  side by 1.5/9 of the starting size on both of its axes; we have one axis to show, so we grow by 1.5/9 of
  the first width **in total** (16 px, 8 per side): "back a little", as the brief asks.
- **Speed**: the clone keeps a constant speed; the original quickens every 15-20 points and stops after a
  while (source 3): +8 % every **15** pancakes, 5 steps, then constant (x1.4 from pancake 75). The step
  size is ours, set with the bot (below): +10 % steps to x1.5 made the late game a coin toss.
- **Height**: the clone's layer is 1/9 of the width; ours is 8 px, 1/12 (pancakes are flatter than slabs).
- **Chain bonus and pitch**: the scale of source 1 (a note per perfect in the chain, 8 notes before the
  regrow) becomes a major scale on the ding.
- **Ours, not from the sources**: the syrup slip (6 px, 12 frames), the toppings, the wobble (at most 3 px
  at the top of a tall tower, a 3.2-s period), the hover height (4 px) and the 3-frame drop, the scoring
  bonus, the medals, the 2-player splash.

### The final table (games/pancaketower/src/tuning.h)
| Quantity | Reference | Pancake Tower |
|---|---|---|
| first width | 9 units (1) | 96 px |
| layer height | 1 unit (1) | 8 px |
| slide | +-10 units, 1.25 s per traverse, linear ping-pong (1) | +-107 px, 75 frames, 2.853 px/frame |
| sides | alternating axes (1, 2) | alternating left, right |
| cut | overlap = size - abs(delta) (2) | same, in whole pixels |
| perfect | abs(offset) <= 7.5 % of the size (1) | 7.5 %, clamped to 3..4 px |
| regrow | from 8 perfects in a row, +1.5/9 per side, to the start size (1) | from 8 in a row, +16 px (8 per side), to 96 px |
| speed | quickens every 15-20 points, then stops (3) | +8 % every 15 pancakes, max +40 % |
| miss | no overlap: game over (1, 2, 4) | same |
| perfect feedback | flash, a rising note (1, 3, 4) | flash, butter pat, sparkle, a rising ding |
| syrup, toppings, wobble | - | ours (above) |
| 2 players | - | the same, x2/3 (64 px, +-71 px, 1.893 px/frame, perfect 3 px, regrow 10 px, slip 4 px) |

## Tests (make pancaketower-check)
- `tests/test_tower.c`: the rules against the tuning table: the slide (speed, the ping-pong turn, the
  alternating sides, the speed steps and cap), the cut maths (every offset from -width to +width: the kept
  width and position, the piece cut off, the miss), the perfect window (its edges, the snap), the regrow
  (the 8th perfect, the cap at the first width, a cut breaking the chain), the syrup slip (its distance,
  duration and direction, the perfect judged after it), the toppings' rotation and schedule, the score
  bonus, the segments by altitude; a determinism check (two identical games, the same events).
- `tests/smoke_test.sh`: scripted runs: the title waits; A on the title then A drops a pancake (cut to 32 px);
  no press lets the next slider ping-pong forever (no game over by itself); a blind rhythm ends in a miss and
  the panel; a far drop is a miss; the panel's retry lock and the one-button retry; pause (Start) and resume
  (Select); the best score persists in save RAM; player 2 joins; the versus towers both grow; no
  strict-mode warning (VRAM, sprites per line, voices, sample memory) on the title, in a long run and in 2
  players; **determinism**: a scripted run, a bot run and a 2-player run twice, the same state and picture.
- `tests/bot_test.sh`: **the bot** (`--opt bot=1`, src/bot.c) plays from the **screen state only**: the
  sprites in OAM (the slider's and the top pancake's left edges, the syrup puddle on the top) and its memory
  of what it saw. It times its presses from the slide position like a person: it sees the screen with a
  **reaction delay** (it acts on what was on screen 12 frames = 200 ms earlier), fits a line to the slider's
  position against the top pancake (the tower's sway moves both alike) and extrapolates, knows the speed from
  the last passes (the rhythm), commits 6 frames ahead, and each press lands with a **jitter**: a
  deterministic Gaussian spread, sigma 0.8 frame (13 ms), clamped to +-2 frames. It must reach **60 pancakes
  on average over 10 seeds** (the jitter's seed). Measured: 77 83 74 78 88 98 88 85 82 85, **mean 83.8**, min
  74, max 98; the jitters applied (frames -2..+2): 21, 204, 395, 208, 20.
- `tests/ui_test.sh` + `ui_check.py`: the UI screenshot test: the title, ready (2 players), a run, the pause,
  the versus and the game-over panel are rendered headless; each renders, they all differ, the title renders
  the same twice, and each shows what it must (the logo's golden and syrup letters, the copyright line, the
  gold GAME OVER and the navy panel, a fork medal, the dimmed pause and PAUSED, the split screen's divider,
  READY on each half).
- `tests/state_test.sh`: save states (the SDK's test_states): at the title, at six points of a run (the
  syrup, the ceiling crash, the roof included), paused, on the panel, high up in space and in 2 players (bots,
  and a human join): the replay after a load and a fresh process match frame by frame, bad states are refused;
  `tools/state_audit.py` checks that every mutable static is saved (state_audit.txt lists the scratch ones).
- `test_libretro`: the core loads and runs 600 frames in the SDK's libretro loader.
- `tools/screenshots.sh`: docs/screenshots/ (title, early stacking, a perfect chain, the syrup, a topping,
  the ceiling crash, the roof, sky, clouds, stratosphere, space and the cow, a miss, game over, pause, ready
  and versus in 2 players, the 2-player game over).

## Performance (make pancaketower-bench)
Measured on the build host (WSL2, x86-64), per frame (update + draw + PPU + audio, the music on):

| Scene | Host, average | Host, worst | A20 estimate (x15-x20), average |
|---|---|---|---|
| 1 player, a bot run from the kitchen to the sky | 0.34-0.36 ms | 0.89-0.90 ms | 5.1-7.1 ms |
| 1 player in space (125 layers, stars, the moon, the cow, a tumbling piece) | 0.34-0.39 ms | 0.64-0.97 ms | 5.1-7.8 ms |
| 2 players, split screen, both towers in the clouds | 0.40 ms | 0.91 ms | 6.0-7.9 ms |
| 2 players, both crash through the ceiling at once | 0.36 ms | 0.83 ms | 5.4-7.2 ms |

The budget is 16.7 ms: the heaviest average (0.40 ms: 6.0-7.9 ms on the A20) leaves more than half of it
free. The worst frames change from one identical run to the next (host scheduling: 0.64 to 2.4 ms for the
same space run), so they measure the host more than the game; the steady worst is about 0.9 ms (13.5-18 ms on
the A20 at worst, a frame the PPU spends on a crowded line). The PPU is almost all of the cost; the game's
own update and drawing (the run-time pancake tiles included) cost 4-6 us on average, 130 us at most.
