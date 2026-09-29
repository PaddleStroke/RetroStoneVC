# Leady Squid: design

All rights reserved, 8BCraft (Pierre-Louis Boyer). The second game for the RetroStone virtual console
(docs/spec.md).

## Pitch
A little squid put on far too many lead diving weights, so it sinks like a stone. Every tap squeezes its
mantle and shoots a jet of water that sends it back up for a moment. Swim through the gaps between the
obstacles of a sunken world (kelp, coral, the masts of a wreck, anchor chains) for as long as you can.
One button, instant retry, a best score to beat.

It is a one-button "flap" game. Its mechanics follow the genre's reference feel (the original Flappy Bird,
2013): a constant gravity, a tap that **sets** the upward speed, a speed cap on the way down, a steady
scroll, fixed spacing and gap, a point per obstacle. Nothing else is taken from it: no name, art, sound,
layout or text. The theme, the characters, the screens, the medals, the sounds and the music are ours.

## Legal and originality
- Flappy Bird is proprietary and was never open-sourced. We use only its **mechanics** (not protected),
  measured by third parties or estimated by open-source clones.
- Only permissively licensed clones were read (MIT and Apache 2.0), only for their numbers; the code of
  Leady Squid is our own (C11 on the RetroStone SDK, integer fixed point). The clones' art (extracted from
  the original game, which their authors say they do not own) was not looked at or used.
- All sources and their licences: "Balance sources" below.

## Screen and layers
320x240 at 60 Hz, fixed step. The play area is **192 px** tall: the water surface at y = 0, the seabed at
y = 192, a 48-px sandy seabed band below (20% of the screen, like the 21% ground band of the reference).

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the water gradient, one colour per line (raster callback, HDMA-like); lighter near the surface, darker with depth, and **darker as the score rises** (each band of 10 obstacles is a little deeper, with its own tint) | - |
| BG4 | light rays and the surface shimmer (lines 0-111, **added** to the gradient with colour math, switched per line by the raster callback) and the far reef silhouettes (lines 112-191, opaque) | rays 1/8 with a slow sway (per-line scroll), reef 1/4 |
| BG3 | mid-ground: rocks, a treasure chest, a shipwreck, coral and kelp clumps, an amphora | 1/2 |
| BG2 | the obstacles (bodies) and the seabed in front | 1 |
| BG1 | text: the title logo, "GET READY", the pause and game-over panels | fixed |
| OBJ | squid(s), obstacle end caps, bubbles, ink puffs, lead weights, score digits, shell medals | |

Parallax uses per-line scroll tables (the reef and ray bands of BG4 scroll at different speeds, as SNES games
split one layer with HDMA). The obstacle bodies are tiles on BG2's 8-px grid (their x is a multiple of 8
in the world) and their **end caps are sprites at the exact pixel height of the gap**, so the gap can be at
any height while the bodies stay tiles.

### Palettes
BG: 0 text and panels, 1 seabed, 2-5 the four obstacle themes, 6 mid-ground, 7 rays + reef (the title logo
borrows palette 5 on the title screen only, before any chain obstacle can show). OBJ: 0 squid (player 1),
1 squid (player 2, pink), 2 bubbles, ink and weights, 3 digits and medals, 4-7 the obstacle caps (the same
colours as the theme's BG palette, from one shared 15-colour palette per theme).

## The squid
- Purple, grumpy-cute: a big rounded mantle with two fins, a heavy-lidded frowning eye, a small pout,
  short tentacles, and a **belt of lead weights** around the mantle (grey blocks sticking out above and
  below, a gold buckle). About 24x20 px on screen, drawn in 32x32 cells.
- It swims to the right, mantle first, tentacles trailing behind (left).
- **Flap** = a jet of water: the squid **squeezes** (mantle narrows, tentacles bunch, 3 frames), a small
  **ink and bubble puff** is left behind and drifts away with the current, and a soft **"bloop"** plays.
- **Tilt**: after a flap the squid points nose-up (+20 degrees) while it rises, then its nose drops at a
  constant rate down to a vertical nose-down dive (-90), as in the reference. Six pre-drawn angles (+20, 0,
  -22, -45, -67, -90), SNES style (no run-time rotation). Pointing down, its **tentacles trail upward**.
  The tilt follows the velocity: both are linear in the time since the last flap.
- Hitbox: the body (mantle and head, not the tentacles or the weights), 16x12 px level, turning with the
  tilt: 16x12, 16x12, 15x13, 14x14, 13x15, 12x16 for the six angles (the reference bird is 34x24 px on a
  404-px play height: 16x11.4 once scaled).

## Obstacles
- Vertical pairs, 24 px wide, a **48-px gap**. Top part from the surface down to the gap, bottom part from
  the gap to the seabed.
- A new pair every **72 px** (1.2 s); the gap's top edge is random in [38, 104] (a uniform draw, as the
  reference's range 20%..60% of the play height minus the gap).
- **Themes by distance band** (every 10 obstacles, then repeating): 0-9 **kelp columns** (green-brown
  stalks, leaf blades, air bladders, a frond crown at the gap), 10-19 **coral pillars** (pink branching
  coral, polyps, a brain-coral dome cap), 20-29 **sunken ship masts** (dark wood, rope shrouds and
  ratlines, iron hoops, a yard with a torn sail above the gap, a crow's nest below it), 30-39 **anchor
  chains** (rusty links; the top chain ends in a **hanging anchor**, the bottom one rises to a mooring buoy).
  All themes share exactly the same hitbox: the 24-px column.
- The theme also tints the water (deeper, bluer, then indigo).

## Scoring, medals, save
- +1 when the squid's centre passes the centre of an obstacle (the reference's scoring moment), with a
  soft "ding".
- Best score in save RAM (with the number of runs and the medals won). Shell medals, ours: **bronze
  shell** at 10, **silver shell** at 20, **gold shell** at 30, **pearl shell** (a shell holding a pearl) at 40.
- The score is shown in big digits at the top (sprites), and on the game-over panel.

## Flow (no menus in the loop)
1. **Title**: the "LEADY SQUID" logo, the squid bobbing gently in its "get ready" pose, bubbles, a blinking
   A button and "PRESS A TO SWIM", the best score, "(C) 2026 8BCRAFT".
2. **Get ready**: after a run, the same scene without the logo ("GET READY" instead). The first flap starts
   the run (and is a flap).
3. **Play**.
4. **Death**: hitting an obstacle or the seabed. The squid gets a dazed face (X eyes), a thud, the scroll
   stops, and it **sinks to the seabed** under the extra weight (a faster fall, the nose turning down);
   it lands with a **"clank-clank"** as lead weights fall off and bounce on the sand.
5. **Game over panel** slides up: SCORE, BEST (with "NEW" when beaten), the shell medal. After 0.6 s any
   flap button retries at once (back to 2). No choice to make.

**Pause**: Select, during a run (the picture dims, "PAUSED"); Select again resumes.

## Controls
| Button | Action |
|---|---|
| A, B, Up, Start (also X, Y) | flap / start / retry |
| Select | pause |
| Pad 2, a flap button on the title or get-ready screen | player 2 joins (race mode) |

Keyboard (desktop): Z, X, Up, Enter flap; Backspace or Right Shift pause (the SDK's SNES mapping).

## Optional extra: 2-player race (shared screen)
Separate from the core (a player count in the run; the one-player game is unchanged): player 2 joins from
the title or get-ready screen by pressing a flap button on pad 2 ("P2 JOINED"). Both squids swim the same
course at the same time (player 2 is pink and swims 32 px behind player 1), each flapping with its own pad
and scoring its own obstacles. The scroll goes on while one of them is alive; a dead squid sinks, lies on
the seabed and drifts away with it. The panel shows both scores and the winner. The best score counts
either player.

## Audio
- Sound effects synthesised in code at start-up (sfx.c): **bloop** (a short rising sine with a bubbly
  burble), **ding** (a soft two-partial bell), **thud** (a low falling sine and a little noise), **clank**
  (inharmonic metal partials, two hits), plus a soft swish for the panel, a sparkle for a medal, a join
  chime and a pause blip.
- Music: a gentle underwater loop (tools/make_music.py, a 4-channel MOD played by libxmp-lite): slow
  tempo, a soft pad, a round bass, a water-drop arpeggio; mixed low, with a little echo on the effects.

## Art
- Placeholder art: code-drawn by `tools/make_placeholders.py --game leadysquid` (which runs
  `games/leadysquid/tools/make_art.py`), in the clean NES/SNES style of Bomber Mole's code-drawn art: flat
  shades (2-4 per material), 1-px dark outlines, light from the top-left, readable at size.
- The BG tiles (obstacle bodies, seabed, reef, rays) stay **code-drawn by default**: AI tiles at 16x16 read
  as noise (the Bomber Mole finding). Sprites, caps, mid-ground props and the logo go through the image
  agent (art/incoming/TODO.md, the same format and workflow as Bomber Mole, docs/art-workflow.md).

## Balance sources
The reference: Flappy Bird (dotGEARS, 2013, proprietary, never open-sourced). No source code of it exists
publicly; its feel is known from video measurements and from clones built at the original's resolution
(288x512) and tuned against it.

| # | Source | Licence | What we took (numbers only) |
|---|---|---|---|
| 1 | **FlapPyBird**, Sourabh Verma, https://github.com/sourabhv/FlapPyBird: the single-file `flappy.py` of 2014-2019 (e.g. commit 5c239c6, 2019-03-11; 8108b2a, 2023) and the current `src/entities/player.py`, `pipe.py`, `floor.py`, `src/utils/window.py`, `src/flappy.py` | **MIT** (LICENSE file of the repo) | 288x512 screen, 30 fps, ground at 0.79 of the height (a 404-px play area); gravity 1 px/frame², a flap sets the speed to -9 px/frame, maximum fall speed 10 px/frame, no gravity on the flap frame; pipe gap 100 px, pipe width 52 px, pipe speed 4 px/frame (128 px/s in 2023), a new pipe every half screen width (144 px); gap top random in [0.2, 0.6] of the play height minus the gap; bird 34x24 px at x = 0.2 of the width; score when the bird's centre passes the pipe's centre; tilt: +45 degrees on a flap, shown capped at +20, -3 degrees per frame down to -90; crash fall: gravity 2 px/frame², max 15 px/frame, -7 degrees per frame; get-ready bob: +-8 px, 1 px per frame |
| 2 | **floppybird**, Nebez Briefkani, https://github.com/nebez/floppybird (`js/main.js`, `css/main.css`) | **Apache 2.0** (LICENSE file) | 60 Hz loop; gravity 0.25 px/frame², a jump sets the speed to -4.6 px/frame; pipe gap 90 px, width 52 px; a pipe every 1400 ms, pipes crossing 1000 px in 7.5 s (133 px/s); a 420-px fly area; bird 34x24 px; hitbox narrowed by up to 8 px with the rotation; tilt = min(speed / 10 x 90, 90) degrees |
| 3 | **Frank Noschese, "Flappy Bird: When reality seems unrealistic"**, 2014, https://fnoschese.wordpress.com/2014/01/30/flappy-bird-when-reality-seems-unrealistic/ (video analysis of the original game on an iPad with Logger Pro; also reported by Latin Times, 2014) | article (facts cited, no code) | the original's fall is a constant acceleration (9.75 m/s² at the scale "bird = a 24-cm robin"); a tap **sets** the upward speed to the same value (a bit more than 2 m/s) whatever the speed before, it does not add to it |

### From the sources to our world
- **Time** is kept identical: everything is converted to 60 Hz (FlapPyBird's 30-fps values: speeds / 2,
  accelerations / 4).
- **Vertical distances** are scaled by the play height: K_V = 192 / 404 = 0.475 (our 192-px water column
  against the reference's 404-px sky). This keeps the arc of a flap, the gap and the range of gap heights
  in the same proportion of the play area, so the same timing works.
- **Horizontal distances** are scaled by 0.5 instead of 0.475 so that the obstacles sit on BG2's 8-px
  tile grid and the scroll is exactly **1 px per frame** (72-px spacing, 24-px obstacles): the time between
  obstacles (1.2 s) is exactly the reference's. The time the hitbox spends inside an obstacle column is
  40 frames against 43.
- Our screen is 4:3, wider than the reference's 9:16, so more obstacles are visible ahead; the squid sits
  at x = 96 (30%) to show its trailing ink. The first obstacle starts just off-screen (3.9 s after the first
  flap, 3.7 s in the reference).
- Where the two clones differ we take FlapPyBird (built from the original's assets at the original's frame
  rate), floppybird as a check: gravity (0.25 px/frame² at 60 Hz in both), flap (-4.5 vs -4.6 px/frame),
  gap (100 vs 90 px; FlapPyBird's newer versions use an easier 120), spacing (1.2 s vs 1.4 s).
- Noschese's measurement confirms the rules (constant gravity, a flap sets the speed). His numbers depend
  on an assumed real-world scale; in bird lengths they give a flap speed within 10% of the clones' but a
  stronger gravity (the top of an arc after 0.22 s against 0.30 s). We keep the clones' values, which were
  tuned against the original on screen; the table makes the other choice a one-line change.

### The final table (games/leadysquid/src/tuning.h)
| Quantity | Reference (60 Hz, reference px) | Leady Squid (60 Hz, our px) |
|---|---|---|
| gravity | 0.25 px/frame² | 0.119 px/frame² (Q16 7786) |
| flap (sets the speed) | -4.5 px/frame | -2.139 px/frame |
| maximum fall speed | 5 px/frame | 2.376 px/frame |
| arc of a flap | 42.75 px (45 at 30 fps), top after 18 frames = 0.30 s, back to the flap height after 37 frames | 20.3 px, top after 19 frames, back after 38 frames |
| fall from the surface to the seabed (from rest) | 85 frames over the same share of the sky | 85 frames (1.42 s) |
| scroll | 2 px/frame (120 px/s) | 1 px/frame |
| obstacle spacing | 144 px = 1.2 s | 72 px = 1.2 s |
| obstacle width | 52 px | 24 px |
| gap | 100 px | 48 px |
| gap top range | [80, 222) of 404 | [38, 105) of 192 |
| hitbox | 34x24 (mask) | 16x12 turning to 12x16 |
| scoring moment | centre passes centre | same |
| tilt | +45 on a flap, capped at +20, -1.5 degrees/frame to -90 | same |
| death fall | 0.5 px/frame², max 7.5 | 0.238, max 3.56 |
| get-ready bob | +-8 px, period 1.07 s | +-4 px, period 64 frames |
| difficulty over time | constant | constant |
| medals | 10 / 20 / 30 / 40 | bronze / silver / gold / pearl shells at 10 / 20 / 30 / 40 |

## Tests (make leadysquid-check)
- `tests/test_physics.c`: the physics against the tuning table: the arc of a flap (height and duration),
  the fall time from the surface to the seabed, the speed cap, the flap that sets (not adds) the speed, the
  tilt timing, the hitboxes, the gap range and spacing over 10000 obstacles, collision cases (edges of the
  gap and of the column, the surface, the seabed) and scoring (one point per obstacle, at the centre).
- `tests/smoke_test.sh`: scripted runs (no input: sinks to the seabed with score 0; flapping at a fixed
  rhythm dies on an obstacle), pause, retry, save RAM (the best score persists), 2-player join.
- **The bot** (`--opt bot=1`): it plays from the **screen state only**: the sprites in OAM (the squid's
  sprite and the obstacle caps, whose edges are the gaps) and its own flaps (from which it knows its speed,
  like a player). It must reach **30 or more** with the default settings: the game is fair and possible.
- **Determinism**: the same input script gives the same run (state and screenshot hashes), twice.
- `tools/screenshots.sh`: docs/screenshots/ (title, mid-flap, a dense section, game over with a medal, each
  obstacle theme, pause, 2 players).
