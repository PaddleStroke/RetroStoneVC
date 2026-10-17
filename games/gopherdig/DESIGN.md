# Gopher Dig: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/gopherdig/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

## Pitch
A gopher in a little yellow hard hat drills straight down a well of bright, jelly-soft blocks. Blocks of one colour
stick together; dig out what holds them up and they wobble, then come tumbling down: dodge them, and when four of a
colour meet after a fall they pop by themselves. Keep an eye on the air: grab the air bubbles buried in the soil, and
think twice before drilling through a hard rock. Every 100 m a new stratum (garden soil, red clay, a crystal cave, warm
rocks, fossil beds, a gem core), a little faster and harsher. One hit: a block on the head ends the run. Your score is
how deep you dug.

## Legal and originality
- The mechanics come from **Mr. Driller** (Namco, 1999), which is proprietary. We use only its **mechanics**, which
  are not protected: digging down a well of coloured blocks, a drilled block taking its whole same-coloured group with
  it, groups of one colour sticking together and falling when nothing holds them up (after a short wobble), four of a
  colour joined after a fall disappearing (chains), a block falling on the digger ending the life, the air gauge with
  air capsules to pick up and hard blocks that take several hits and cost air, depth as the score.
- Nothing else is taken from it: no name, character, logo, art, colour scheme, sound, music or text; no level was
  copied and no number was measured from it; we did not look at its code or anyone's clone. The gopher and its hard
  hat, the jelly blocks, the strata and everything buried in them, the checkpoints, the split-screen race for four, the
  touch controls, the medals, the sounds and the music are ours. The code is our own (C11 on the RetroStone SDK,
  integers only), written from this design.
- Every number below is ours, tuned with our own bots for our 320x240 screen.

## Rules
The well is **9 blocks wide** (16-px blocks) and endless downwards; 1 block = 1 metre. Each row is made from the seed
and the row above it as it was made, so every player of a race digs the very same well.

- **The blocks**: red, yellow, green and blue **colour blocks**; **hard rocks** (brown-grey, tinted by the stratum);
  **air bubbles**. Colour blocks of the same colour that touch (up, down, left, right) **stick together** into a group
  (drawn as one jelly); hard rocks and bubbles are groups of one. A row is generated with a block taking its left
  neighbour's colour (30% in the garden, 20% in the gem core) or else the one above it (20%..14%), else a random colour;
  hard rocks (1.2% of the blocks in the garden, 12% in the gem core; none in the first 3 rows), pockets of air (none in
  the garden, up to 2.2%) and a bubble every 10..15 rows (+0..3).
- **Support**: a group stands when one of its blocks rests on a standing block of **another** group (or on the rock
  below the dug part); the gopher holds nothing up. A group that loses its support **shakes** for S_SHAKE frames (54 in
  the garden, 34 in the gem core), then **falls**: a well's falling blocks all move down a row on the same tick, every
  S_FALL frames (8..6); a group lands as soon as one of its blocks meets something. After a landing, a group of **4 or
  more** same-coloured blocks that touches a block that just landed **pops by itself** (a chain; the next landings can
  chain again). A group already standing (even a big one) never pops by itself.
- **The gopher**: Left / Right walk a block (8 frames); pushed against a one-block step for 8 frames, it **climbs** it
  (not two blocks). **A drills** where it faces: the side it last walked to, or **down** after Down (Down + A drills
  down at once; A held drills again every 12 frames). A drilled colour block pops **its whole group**; a hard rock
  takes **5** drills and breaks with a crack, costing **20%** air; a bubble gives its air. Nothing under its feet, it
  drops (4 px a frame).
- **Crushed**: a block (not a bubble) that moves into the gopher's cell (also the cell it is walking or climbing out
  of, for half the step, and for half a climb the cell above it, which it climbs through) **crushes** it: one hit,
  out. A bubble falling on it is breathed in instead.
- **The air**: 100% at the start; it drains every frame (a full tank lasts 60 s in the garden, 39 s in the gem core,
  less still deeper); a bubble gives **+25%**; under 30% the gauge blinks red with a beep every second and the gopher
  puffs little bubbles; at 0 the gopher faints: out.
- **The strata**: every **100 m** a new stratum (its own earth, rocks and buried things; 6 themes, then again, the air
  draining 3 more each time): a **checkpoint**: the well stands still for 1.5 s while the gopher cheers ("200 M", the
  stratum's name, the medal jingle) and the air gets **+30%**.
- **The score**: the deepest depth reached (the gopher's feet: row + 1). The run ends when every player is out.

### Tuning (every number in src/game.h)
| Rule | Value | Where |
|---|---|---|
| the well | 9 blocks of 16 px; a ring of 40 rows follows the gopher (14 above it) | COLS, CELL, ROWS, ROWS_ABOVE |
| walking, climbing, dropping | 2 px a frame; a step after 8 frames pushing, 10 frames; 4 px a frame | WALK_SPEED, CLIMB_*, GOPHER_FALL |
| drilling | 9 frames a swing; held: every 12; hard rocks 5 drills | DRILL_FRAMES, DRILL_REPEAT, HARD_HITS |
| pops | 4 after a fall | POP_MIN |
| air | a tank of 100000; drain 28..43 a frame by stratum (+3 a stratum past the 6th); bubble +25000; hard rock -20000; checkpoint +30000; low under 30000 | AIR_*, S_DRAIN, HARD_AIR, CHECK_AIR |
| strata | every 100 m; shake 54, 50, 46, 42, 38, 34 frames; fall 8, 8, 7, 7, 6, 6 frames a row | STRATUM_M, S_SHAKE, S_FALL |
| generation | hard rocks 12..120 per mille; bubble gap 10..15 rows; same as left 300..200, same as above 200..140, pockets 0..22 per mille | S_HARD, S_AIRGAP, S_SAME_*, S_HOLE |
| checkpoint | 90 frames standing still | CHECK_FREEZE |
| medals (1 player) | bronze 60 m, silver 200 m, gold 400 m, pearl 600 m | MEDAL_DEPTHS |

### Medals
Tuned with the bot (`bot=1`), 40 wells each, up to 20 minutes:
- **The bot** (skill 0): a rule of thumb (dig where the rock below is soft, fetch a bubble when the air is under 60%,
  step out from under anything shaking or falling, else open a side, else dig away) and a **look-ahead**: each move it
  could make (walk or drill left or right, drill down) is played forward on a copy of the well for a shake and six
  fall ticks with the rule of thumb playing on, and the one that lives and digs deepest wins (a bot stuck on one row
  for 2.5 s takes the dig unless it is deadly at once). It digs about 5 m a second: mean 384 m, median 403, 180..644
  (half crushed, half out of air): 5 bronze, 24 silver, 11 gold, no pearl.
- **The human-like bot** (`botskill=S`: it thinks every 2S..5S frames and holds the pad in between, misjudges the
  columns, forgets the danger 6S% of the time and the look-ahead 12S%): S = 2: median 129 m (26 of 40 bronze or
  better, 5 silver); S = 3: median 126 (24 bronze or better); S = 4, a **beginner**: median 76 (16 of 40 bronze);
  S = 5: median 58.
- So **bronze (60 m)** is a beginner's good run, **silver (200 m)** the red clay passed with a steady pair of hands,
  **gold (400 m)** past the crystal cave and the warm rocks (the bot's median), **pearl (600 m)** a run into the gem
  core that even the bot rarely makes.

## Controls
| | Pad | Keyboard (pad 1) | Touch (player 1) |
|---|---|---|---|
| walk, climb a step (push on it) | Left / Right | arrows | a finger left or right of the gopher, away from the blocks next to it: it walks to that column (climbing steps on the way); dragged: it follows |
| face down | Down | Down | |
| drill (where it faces) | A (held: again) | X | a finger on the block beside the gopher (left, right) or anywhere under it in its column; held: again |
| drill down at once | Down + A | Down + X | |
| start (title) | A, Start, Down | X, Enter | a tap |
| pause | Select (or Start in a run) | Backspace, Esc | the app's pause button |
| back to the title (game over) | A, Start, Select | X, Enter | a tap (after the panel's lock) |

Touch is declared (`rs_game.touch_native`): the RetroStone Arcade apps pass the fingers, the desktop's mouse is one.
What the fingers set is kept in the save state (`main.touch_*`, the gopher's `walk_to`).

## Screen and layers
320x240 at 60 Hz, fixed step, the well seen from the side; the camera keeps the gopher 76 px from the top (34 in a
quarter of the screen) and follows smoothly (1/4 of the way a frame).

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the sky above the grass (a gradient by line, the house 2-line dither); below, the back wall's colour (round block corners) | the camera |
| BG4 | the well: the standing blocks, the dug-out back wall (shadowed under a block), the earth around it with its stone lining, roots, pebbles, burrows, side tunnels and the buried things of the stratum, the grass, flowers and clouds on top | the camera |
| BG3 | the shaking blocks: the whole layer jiggles a pixel left and right | the camera, +-1 |
| BG2 | the falling blocks: the whole layer slides down with the well's fall clock (all of a well's falling blocks move a row on the same tick) | the camera + the fall's progress |
| BG1 | the house UI kit (priority 1); the air gauge's tiles (a bubble, a bar filled 0..8 px blue or red; again on the panel's fill, the empty part a dark groove) after the kit's glyph pool | fixed |
| OBJ | the gophers (OBJ 0; P2-P4 OBJ 1, 4, 5: the hard hat recoloured, the fur stays), the drill bits, the air bubbles, sparkles, dust, stars (OBJ 2), the blocks' crumbs (OBJ 6), the kit's digits (OBJ 3) | |

- **The blocks** are drawn by quarter tiles: each 8x8 quarter by whether the block sticks to its side neighbour, its
  top or bottom neighbour and the diagonal one, so a stuck group reads as one round jelly with a cute shine on each
  block (4 colours x 4 quarters x 8 states, 2 BG palettes).
- **The strata** share their tiles (earth, back wall, rocks, lining, tunnels) drawn in palette indices; each stratum
  has a palette (BG 3) and its buried things another (BG 6), which the raster callback loads line by line from the row
  under that line: the border between two strata shows where it is, in every view.
- **The map** is a ring of 16 block rows per layer (32 tile rows); each player's well has a region of the map's width
  (1 player: all of it, 2: half, 3-4: a quarter; 64 tiles wide, 128 for 3-4 players or a screen wider than 512), the
  well centred in it with the earth around; the rows coming into view are written (the earth once, the well every
  frame). A view is never wider than its region (quarters at 520 px: 256 of 259 px, the divider takes the rest), so
  no strip of the next player's earth shows at its edge.
- **The HUD**, 1 player: two navy panels on the earth beside the well's top (a longer word widens them towards the
  screen's edge): the depth in big digits (DEPTH, the unit right of the digits) and the air gauge (AIR); the buried
  things and side tunnels scrolling past stay behind them, the well's top stays clear. The controls hint (the first 6 s)
  on a panel over the well's bottom. Split screen: the depth and the gauge at the bottom of each view, the player's tag
  at its top left; 3 players: the race on a panel in the 4th quarter, with each player's icon.
- **A checkpoint** shows "200 M" in gold and the stratum's name on a panel under the gopher, in every view (over the
  scene when the name is too long for a quarter).

**Up to four players** (the house lobby: pads 2-4 join with A on the title, B leaves): everyone races down the same
well at once, each in their own: 2 players side by side, 3-4 in quarters (3: the 4th quarter shows the race, ranked by
depth). Each is out on their own (CRUSHED! or OUT OF AIR! in their view); the run ends when the last is out; the
results rank everyone by depth with medals.

**Widescreen** (docs/spec.md "Widescreen"): the game opts in (`max_width`, `WIDESCREEN_gopherdig = 1`). The well and
the rules are the same at every width; the 320-px stage is centred and a wider screen shows more of the earth around
the well: its roots, burrows, side tunnels with something buried at their end, and the stratum's treasures (worms and
carrots, clay pots and coins, crystals, lava and coals, fossils, gems and a treasure chest). The split views are wider
too. A state saved at one width loads at another (`draw_state_loaded` sets the maps up again). `make gopherdig-check`
runs the tests at 320, 427 and 520.

**Tall screen** (docs/spec.md "Tall screen"): a phone held upright. `rs_screen_tall(RS_SCREEN_H_MAX)` first in
`game_init`; the RetroStone app gives the game the display's height at 320 columns. The 240 lines of the stage are the
same pixels; the stage sits a little high (`rs_screen_origin_v`: the well above it no taller than the 14 rows the world
keeps above the gopher, ROWS_ABOVE). One full view (1 player, the title, the game over) draws the whole height: the
well layers' ring is then 64 block rows (128 tile rows; 16 otherwise), and above the stage the real well goes on up
(what was dug, the blocks left, the strata passed, the grass and more sky with clouds of its own on the title), while
below the stage the earth goes on down, the shaft filled with plain earth inside its stone lining (`write_below`):
what lies ahead stays unseen, exactly as far as on a 240-line screen, so the game stays the same game. The UI layer is
hidden off the stage (window 1 in the raster). The split screen keeps its views on the stage (`view_layout` clamps the
tall layouts; the margins in the divider's colour). A touch is read in stage coordinates (`touch_cell`: y -
`rs_screen_origin_y()`), so a tap above the stage aims at the cell drawn there. A 240-line screen is unchanged (the
same pixels in landscape).

## Screens (the house flow: no "get ready", the title is the only menu)
title (the logo over the sky, the gopher bobbing under it, the player slots, PRESS A TO DIG with the A glyph, BEST,
the join line, the copyright line; the grass of the well at the bottom) -> P1 presses A (or Start or Down, or taps):
the camera drops from the sky to the gopher on the grass -> play (Select pauses) -> a crush (a shake, stars) or out of
air -> game over (1 player: the banner, the panel with the depth, BEST or NEW BEST, the medal and how it ended; 2-4
players: the ranking) -> one press: back to the title (the same players start again with A).

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation. The gopher: warm brown
fur, a cream belly and snout, a pink nose, buck teeth, a big house eye under the brim of a **yellow hard hat with a
lamp** (players 2-4: the hat recoloured blue, then the two farthest hues); poses: idle (blinking), walking, drilling
sideways and down (the drill bit is a spinning sprite of its own), dropping, climbing, cheering, flattened (with stars),
fainted. The blocks: bright strawberry, lemon, lime and berry jellies with a white shine. Each stratum: its earth, back
wall, rocks and stone lining (6 palettes of the same tiles) and four buried things of its own. The surface: grass tufts,
flowers, a mushroom, clouds. Previews: `make gopherdig-art` then `tools/make_art.py --preview` (art/preview/, with
strata.png: the six strata side by side).

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects: the drill's buzz, a bubbly pop (a brighter one
for a chain), the clank of a drill on rock and the rock crumbling, a rising gloop for air, a low rumble when a group
starts shaking, a soft thud when blocks land, steps, the crush, a sad deflating whistle out of air, a beep-beep when the
air runs low; the house medal and ding at a checkpoint. The music (tools/make_music.py, house instruments):
**"Burrow Boogie"**, G major, speed 5 (150 BPM): a perky square lead, a hopping octave bass, kick, snare and hats, and
the water-drop pluck clinking on the off-beats like a drill (A A' B C, about 26 s). Two alternatives in music-alt/
(tools/make_music_alt.py): "Tunnel Stroll" (calm, D dorian, no drums) and "Lava Shuffle" (a warm E minor shuffle);
previews in docs/music-alternatives/gopherdig/ (not committed).

## Tests
`make gopherdig-check`: the rules alone (tests/test_rules.c: drilling and groups, support, the shake then the fall,
chains of 4 and not 3, crushing and stepping aside, the air: drain, bubbles, hard rocks' cost, out of air; climbing,
the strata and the checkpoint, touch inputs, the same well for every player, the bot), the libretro loader, the smoke
tests (the flow, walking and drilling on the pad, the game over, pause, save RAM, joining and leaving, 2, 3 and 4
players, the bot's depth and checkpoint, determinism), touch (start, drill, walk, pause, game over, the same run at
every width), the UI screens, the save states (the title, the lobby, a run, a checkpoint, touch, 2 and 4 players,
paused, the game over, the results) and their audit, states across widths, and the 22 languages (tools/i18n_test.sh)
at 320, 427 and 520.
