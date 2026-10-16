# Snowball Siege: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/snowballsiege/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

## Pitch
A snowball fight between snow forts, turn by turn. Your two polar bear cubs shuffle in their fort, aim, charge and
lob snowballs over the hills at the other side, with the wind changing every turn. Every splat carves the snow: walls
crumble, hills get holes, and a critter knocked into the frozen pond is out. Win the match and a sharper computer
waits in the next one; with friends, two to four sides take turns on one field. Nothing violent: a hit only chills.

## Legal and originality
- The mechanics come from the turn-based artillery games: **Worms** (Team17, 1995), **Scorched Earth** (Wendell
  Hicken, 1991) and **Gorillas** (Microsoft, 1991), which are proprietary. We use only their **mechanics**, which are
  not protected: teams taking turns on a side-view field, aiming an angle and charging a power, a wind that changes
  every turn and pushes the projectile, destructible terrain carved by the impacts, damage that falls off with the
  distance, units knocked back and falling into water, a turn timer, a few weapons with limited stock.
- Nothing else is taken from them: no name, logo, character, art, colour, sound, music, text, weapon name or
  number measured from them; we did not look at their code or anyone's clone. The theme (a snowball fight, snow
  forts, bear cubs, penguins, foxes and hares with scarves, hearts of "chill", the frozen pond, pine trees), the
  throwables (snowball, big snowball, ice cube), the computer's levels, the frost, the scoring, the touch slingshot,
  the art, the sounds and the music are ours. The code is our own (C11 on the RetroStone SDK, integer fixed point),
  written from this design.
- Every number below is ours, tuned with our own bots for our 320x240 screen and a gentle, fair game.

## Rules
The battlefield is 768 x 240 px, wider than any screen (the camera pans; see "Widescreen"): seeded hills (three
waves), a valley with a **frozen pond** (thin ice over water: 76 px wide, 12 deep), pine trees, and for each side a
**fort**: flat ground of 68 px between two walls of packed snow blocks, 8 px thick, 18 px high towards the middle of
the field (9 at the back; 13 both sides for the side in the middle of three), with crenels. Frozen ground under
y 212 is never carved. Sides at x 120 and 648 (2 sides), 104/384/664 (3: two ponds), 96/288/480/672 (4).

- **Teams**: 2 critters a side (why 2: one critter makes a single lucky hit decide a match, three make a 4-side
  hot-seat match drag on; two give a choice of target, a second chance, and a match of 6-15 turns). Each has
  **3 hearts** (6 half-hearts). The sides' critters take turns in order: side 1's first, side 2's first, ..., then
  each side's other critter.
- **A turn** (20 s): the active critter may **walk** 40 px at most (Left/Right; a tap the other way only turns round;
  steps of 3 px are walked over, walls stop it), **aim** (Up/Down, a degree every 2 frames, -30..88 degrees from the
  horizontal, an aim line of four dots from its paw), **charge** (A held: the gauge goes from 15% by 1.2% a frame, full
  after 1.2 s, then it throws by itself) and **throw** (let go). B switches the throwable. The last 5 s tick; when the
  timer ends: a charging throw goes, otherwise TIME UP and the next turn.
- **Ballistics**: full power is 8.2 px/frame, gravity 0.1 px/frame^2 (full power at 45 degrees on the flat: 670 px);
  the **wind** (-10..10, shown by a windsock and a chevron per 2 levels) adds 0.0015 px/frame^2 per level sideways
  (10 moves a long throw about 100 px); it starts within +-5 and changes by up to 4 every turn.
- **Throwables** (per side and match): **snowball** (any number: splat radius 11, 3 half-hearts at the centre),
  **big snowball** (2: radius 18, 5 half-hearts, feels 60% of the wind), **ice cube** (2: radius 8, 3 half-hearts,
  bounces 3 times off the snow keeping 59% of its speed, 80% of the wind).
- **A splat** carves a disc of snow (a critter hit directly: at its feet), knocks the pine trees in reach down, and
  chills every critter in reach: the damage falls off with the distance from the splat to the critter's body
  (`ceil(D x (R - d) / R)`, d from the body's edge), and pushes it away (0.6 + 0.45 per half-heart px/frame, and up
  1.2). Your own side gets hurt too. A critter whose ground is carved away falls; a fall over 48 px costs half a heart,
  and one more per 32 px.
- **Out**: a critter at 0 hearts is **snowed under** when everything has landed (a snow mound with its ears poking
  out); one that falls into the pond is out at once (**SPLASH!**). The last side standing wins the match (no side
  left: a draw).
- **Frost**: from the 30th turn, the cold bites: every critter loses half a heart a turn (never its last); the 80th
  turn ends the match (the most hearts left wins). Matches always end.
- **1 player**: matches against the computer, one after the other; you start match 1, then the sides alternate. A
  won match brings a new field and a better computer (penguins, then foxes, then hares...); **a lost match is the game
  over**. 2-4 players: one match, hot seat (each player's pad on its side's turn; the other pads do nothing then).

### Score and medals (1 player; the same score ranks 2-4 players)
| What | Points |
|---|---|
| each half-heart of damage to the other side (a hit, or a fall it caused) | 10 |
| a critter of the other side snowed under | 50 |
| one knocked into the pond (SPLASH!) | 100 |
| a throw that hits two critters of the other side (DOUBLE HIT!) | 50 |
| a hit from 300 px away or more (LONG SHOT!) | 30 |
| a match won | 150 + 50 per match before it, + 20 per half-heart your side has left |

Medals: **bronze 300, silver 900, gold 1800, pearl 3000** (MEDAL_SCORES). Tuned with the player bot as a human-like
player (`botskill=L`: the computer's own errors at level L), 12 runs each: L = 1 (a beginner: no wind sense, wild
aim) scored a median of 630 (9 of 12 bronze or better, 3 silver, 1 gold: up to 4 matches won); L = 2 and L = 4: see
the tuning notes below. So bronze is a beginner's first match won, silver two or three matches, gold a long run,
pearl a run deep into the computer's sharp levels.

### The computer (play.c cpu_think; every number in game.h)
It plays by the same rules and the same inputs as a player: it turns round, aims a degree every 2 frames, charges
the gauge and lets go. Its shot is a **ballistic search**: it simulates every throw from where it stands (angles
-10..86 every 2 degrees x every second gauge step, towards each side where an enemy stands, 90 trajectories a frame
spread over its thinking time), with the field, the critters and the wind **as it reads it**, and scores each landing
(damage to the other side, more for a critter it would snow under, much less for hurting its own, the distance to the
nearest enemy for a miss), then refines around the best. Then its **errors**:

| Level (match) | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8+ |
|---|---|---|---|---|---|---|---|---|
| aim error, +- degrees | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 1 |
| power error, +- (of 1000) | 90 | 75 | 60 | 46 | 34 | 24 | 14 | 8 |
| the wind it allows for | 20% | 40% | 55% | 70% | 82% | 92% | 100% | 100% |
| the wind misread by, +- levels | 2 | 2 | 1 | 1 | 1 | 0 | 0 | 0 |
| thinking time, frames | 80 | 70 | 60 | 54 | 48 | 42 | 36 | 32 |

It throws a big snowball 60% of the time at a critter with 2 hearts or more or at two huddled ones, an ice cube 12%
of the time. Measured by the rules test (24 throws per level on seeded fields): a mean miss of 108 px at level 1,
30 at level 4, 9 at level 8 (and 7 with no error: the tests' bot). Beatable: aim at its fort's wall to bring it
down, use the wind it misreads at low levels, keep your critters apart against its big snowballs.

## Screen and layers
320x240 at 60 Hz, fixed step, a side view. The house layout:

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the winter sky, one colour per line (raster callback), the house 2-line dither | - |
| BG3 | the far mountains and a row of far pines: 256 px that repeat, 3-5 pale tones, no outline | 1/2 of the camera |
| BG2 | **the snowy field, drawn at run time from the terrain bitmap** (below) | the camera |
| BG1 | the house UI kit: the logo, texts, panels; the field's map at the bottom (24 x 3 tiles) | fixed |
| OBJ | the critters and their hearts, the aim line, the gauge, the throws, splats, chunks and puffs, splashes, falling snow, pine trees (behind the snow), the HUD (the team icon, the timer, the throwable, the windsock), the map's dots and view, the touch button, the kit's digits and medals | |

**The field on a tile console.** The terrain is a 1-bit bitmap (768 x 240, 23 KB, saved in states); each 8x8 cell of
BG2 shows it: empty cells a blank tile, cells deep in the snow (or the frozen ground) one of 4 shared tiles each (a
sprinkle of specks with a 16-px period), and the cells at an edge are drawn pixel by pixel (the house's dark top
line, the light top rows, a lit left edge, a shaded right one, the overhangs' dark undersides, the forts' snow
blocks and their mortar, the pond's thin ice with cracks and its water) into a **pool of 400 tiles, deduplicated by
content** (identical cells share a tile; a cell that changes frees its tile). A splat redraws only the cells it
touched (play.c tells draw.c the carved rectangles), and the map's columns under it. VRAM stays within the 64 KiB
guideline (strict mode is silent in the tests): the kit and the logo, the map 72 tiles, the field 408, the far
layer 177, the sprites 265 and the kit's 118, and the maps.

**The camera** (play.c camera_step; presentation only) follows the active critter while it aims, the throw in
flight (looking 18 frames ahead), a critter knocked through the air, then the splat; it never shows past the field's
edges. L/R (or a finger dragged on the field) looks along the field during your turn; aiming or walking brings it
back. A throw above the screen shows as an arrow at the top. The **map** at the bottom shows the whole field (4 px a
column, 6 px a row from y 96), the critters as dots in their team's colour, the throw, and the screen's view as two
brackets.

**Widescreen** (docs/spec.md "Widescreen"): the game opts in (`max_width`, `WIDESCREEN_snowballsiege = 1`). The
battlefield and every rule are the same at every width (play.c never asks the width); a wider screen (427, 520, 560)
shows more of the same field around the camera. The UI kit centres itself; the touch button follows the screen's
bottom-right corner. A state saved at one width loads at another. The touch test throws the same pull at 320, 427,
520 and 560 and compares the runs.

## Screens (the house flow: no "get ready", the title is the only menu)
title (the logo, the four teams under it throwing in turn, the field dimmed behind with the camera drifting along it,
the player slots, PRESS A TO THROW, BEST, the join line, the copyright line) -> P1 presses A (or Start, or a tap)
-> match 1: MATCH 1 and YOUR TURN!, a hint panel during your first turn (^~: AIM - {}: WALK, HOLD A: THROW - B: SWITCH)
-> turns (CPU'S TURN, or "P2: YOUR TURN!" in hot seat; SPLASH!, SNOWED UNDER!, DOUBLE HIT!, LONG SHOT!, TIME UP!,
BRRR! FROST! as they happen; Select or Start pauses) -> MATCH WON! and the next match (1 player), or the end ->
game over (1 player: the banner, SCORE, BEST / NEW BEST, the medal, "2 MATCHES WON"; 2-4 players: the ranking by
survival then score, with medals) -> one press (or a tap): back to the title.

## Controls
| Input | Action |
|---|---|
| Left / Right | walk (a tap the other way turns round) |
| Up / Down | aim |
| A held, let go | charge, throw |
| B (or Y) | switch the throwable (snowball, big snowball, ice cube: the ones left) |
| L / R | look along the field |
| A (or Start) on the title | start |
| Select (or Start in a run) | pause / resume |
| pads 2-4: A or Start on the title | join (B leaves) |
| A, Start or Select on the game-over panel | back to the title |

### Touch (docs/spec.md "Touch")
The game reads the touches itself (`rs_game.touch_native = 1`): the phone apps pass it the fingers and draw no
buttons. Touch is player 1 (its side's turns), alongside the pad.

| Touch | Action |
|---|---|
| a finger down on your critter (22 px), pulled back | a **slingshot**: the throw goes the other way from the finger (the pull's direction is the aim, -30..88 degrees, its length the power: full at 72 px); the aim line, the gauge and a band of dots to the finger follow it |
| let go | throw (a pull shorter than 10 px is cancelled) |
| a tap beside the critter (56 px either side) | shuffle 8 px that way |
| the round button at the bottom right (the throwable's icon on it) | switch the throwable |
| a finger dragged elsewhere on the field | look along it |
| a tap (a finger lifted) on the title | start |
| a tap on the game-over panel | back to the title (after the same lock as A) |
| the app's pause button | pause / resume (it presses Start); the fingers do nothing while paused |

The button is drawn in the house style of the apps' drawn buttons (a see-through mid-blue disc, the house checker
dither, a cream ring, a navy outline; solid and lower while pressed), once a finger has touched the screen. What the
fingers set (the slingshot's finger, the button's, the drag's) is in the registered state (main.c): a load forgets
the fingers, one still down starts again. `tests/touch_test.sh` (in the check) plays scripted fingers.

## Up to four players
Pads 2-4 join with A on the title (B leaves). Each player is a side with its own species: P1 the polar bear cubs (a
red scarf), P2 the penguins (blue), P3 the foxes (green), P4 the hares (purple), on the house's player palettes
OBJ 0, 1, 4, 5. One match, hot seat; the results rank the sides: the last standing first, then those out later, the
score between equals.

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation. The critters are
16x16 (a body about 12 px), each species its own sprite palette (fur 4 shades, a second material, the scarf 3 shades,
bead eyes with a glint, a pink cheek), a navy-violet outline; poses: idle (breathing), walk, throw (a paw up with a
snowball), hurt (squeezed eyes, a snow clump on the head), air (stretched, the scarf flying), the snow mound, the
icon. Props: snowballs (4 snow shades), the ice cube, snow chunks and powder rings (the house checker to fade),
water splashes, hearts (red, a frosty blue when empty), the gauge, the aim dots, the windsock and chevrons, pine
trees with snow on each tier. The field's palette is make_art.py TERRAIN. The title logo: SNOWBALL icy blue, SIEGE
scarf red.

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects (main.c sound_init): a soft whoosh and "hup"
for a throw, a crunchy "pfff" for a splat (longer and lower for a big one), a chilly "brr" for a critter hit, water
and a falling "bloop" for the pond, a soft sad slide and a poof when snowed under, a clink for the ice cube's
bounces, tiny crunches for the steps, a tick in the last seconds, a "plink" to switch, a rising jingle for a match
won, the snow falling off a knocked tree; the house sounds for the menus, the turns, the medals and the game over.
Within the 64 KiB sample guideline.

Music: "Frosty Fort" (tools/make_music.py): G major, speed 6 (125 BPM), A A' B A' C (about 38 s): the water-drop
pluck in skipping arpeggios (snowflakes), a round bass bouncing root-fifth, the warm pad with sleigh bells on the
off-beats, the breathy lead with a bright phrase that hops up the chord and slides home; A' sings it an octave up, B
(C D Bm Em) winds up, C comes home. Two alternatives (tools/make_music_alt.py -> music-alt/alt1.mod, alt2.mod; previews
in docs/music-alternatives/snowballsiege/): "Quiet Snowfall" (a calm music-box feel in E minor) and "Sledge Hop" (a
bouncy sledge run in C major with drums). To switch, call make_music_alt.py OUTDIR N from tools/build_assets.py
instead of make_music.py.

## Tests
`make snowballsiege-check`: the rules alone (tests/test_rules.c: trigonometry, ballistics (the range, the angles,
the power, the wind both ways, the big snowball's lesser wind, the ice cube's bounces), carving (in the radius only,
the frozen ground never, the dirty rectangles, a generated field with forts, a pond and critters on the ground for
2, 3 and 4 sides), damage and knock-back and the score, falls (safe and costly), the pond (falling in, a plop, a
SPLASH! scored), the turns (the timer, TIME UP, the computer's turn, the critters' order, A held from before, the
charge, a throw when the timer ends, 3 players in hot seat), walking (the budget, turning round, a step climbed, a
wall), switching and the stock, the computer's accuracy by level, winning and losing (the next match, the game over,
the ranking), the frost and the last turn, the touch slingshot's aim and power, the camera, determinism), the
libretro loader, the smoke tests (flow, start inputs, aiming, turning, walking, switching, charging, a throw carving
the field, TIME UP, pause, the game over and back to the title, save RAM, joining and leaving, hot seat, 4 bots to the
results, the bot winning, determinism), the touch tests, the UI screens (tests/ui_test.sh), the save states (the
title's lobby, the bot's run with splats, a turn on the pad, the slingshot pulled, 4 players, paused, the game over,
the results) and their audit, at 320, 427 and 520 columns, states across widths, and the 22 languages
(tests/i18n_runs.txt).
