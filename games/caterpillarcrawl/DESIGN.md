# Caterpillar Crawl: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/caterpillarcrawl/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

## Pitch
A round green caterpillar with a big happy face crawls around a garden bed, munching leaves. Each leaf makes it
longer, and the longer it gets the harder it is not to bump into a wall, a stone or its own tail. Twelve leaves and it
spins a cocoon and flutters off as a butterfly, and a new little caterpillar starts in the next garden. With friends
it becomes a battle in the garden bed: everyone grows as they crawl, and the last one still crawling wins the round.

## Legal and originality
- The 1-player game uses the **mechanics of Snake** (the "snake" genre: Gremlin's *Blockade*, 1976, and many
  followers, Nokia's *Snake*, 1997, among them; all proprietary). We use only its mechanics, which are not protected:
  a body that moves a cell at a time on a grid, food that makes it longer, game over when the head hits a wall or the
  body, levels with walls. The 2-4-player arena uses the **mechanics of the light cycles** (Bally Midway's *Tron*,
  1982, and the *Tron* film, Disney; proprietary): everyone moves at once and leaves a growing trail, touching any
  trail or wall is out, the last one moving wins the round.
- Nothing else is taken from them: no name, art, sound, music, layout, text or numbers. The theme (a caterpillar in a
  garden bed, leaves, flowers, berries, the cocoon and the butterfly, the gardens' stones, bushes, pots and
  mushrooms), the characters, the screens, the turn buffer, the treats, the bulges, the arena's rules of growth and
  rounds, the medals, the sounds and the music are ours. The code is our own (C11 on the RetroStone SDK, integer
  fixed point), written from this design; no other game's code was read for it.
- Every number below is ours, tuned for our 320x240 screen and a fair, cute game.

## Rules
The garden bed is a grid of **18 x 12 cells of 16 px** (stage x 16..304, y 40..232) in a wooden frame. A caterpillar
is a chain of cells, head first; on each step of the clock it moves one cell where it is heading, the body following
(the cells stay put: the tail leaves its cell as the head enters a new one). The head slides smoothly between cells;
the body segments are round, every other one with golden spots, and a swallowed leaf makes a fat segment that stays
where it was eaten until the tail passes.

**Turning.** A d-pad press (or a swipe) turns the caterpillar at its next step. Turns are **buffered**: up to two
presses are remembered, so a quick double press (up, then left while going right) makes a U-turn in two steps,
exactly as fast as the hands are. A press the way it already goes, or **straight back into itself**, is ignored (the
buffer compares a press with the last turn already in it, so up-then-down is ignored too): it can never reverse into
its own neck.

**1 player: the garden.** It starts 4 long, crawling right, as soon as A (or the d-pad) is pressed on the title. A
leaf grows on a free cell at least 4 steps away; eating it is 1 point and makes the caterpillar 2 segments longer (one
per step). After every 5th leaf a **flower** blooms (5 points, no growth); after every 7th a **berry** (when no flower
is out): time runs at 60% for 5 s (the caterpillar crawls slower; a berry icon blinks in the corner). A treat wilts
after 7 s (it blinks for the last 2). **One hit ends the run** (the house rule): the frame (a wall), an obstacle or its
own body; a hit-stop, a bonk, X eyes, a shake, then the game over. Crawling into the cell its tail leaves on the same
step is allowed (it can chase its tail).

After **12 leaves** the caterpillar stops and spins a cocoon: its body curls into its head (0.8 s), the cocoon wiggles,
then a **butterfly** hatches (+10 points, a fanfare, BUTTERFLY!) and flutters up off the screen. Then **the next
garden**: GARDEN 2 (1.25 s, the pressed turns are kept), a little caterpillar of 4 again, and a new obstacle layout
(six in rotation: none; four stones; two hedge rows; flower pots; mushroom corners with a stone pair; a maze of
bushes with gaps). The speed rises gently with the length and with each garden.

| Rule | Value | Where |
|---|---|---|
| the plot | 18 x 12 cells of 16 px, at stage (16, 40) | GRID_W, GRID_H, CELL, GRID_X, GRID_Y |
| speed | 6.00 cells/s, + 0.05 per segment beyond 4, + 0.40 per garden, at most 11 | SPEED_START, SPEED_LEN, SPEED_GARDEN, SPEED_MAX |
| the turn buffer | 2 turns ahead | TURN_BUFFER |
| the start | 4 segments, the head at cell (5, 6), crawling right | START_LEN, START_X, START_Y |
| a leaf | +1 point, +2 segments; grows at least 4 steps from the head | GROW_PER_LEAF, LEAF_MIN_DIST |
| a flower | after every 5th leaf, +5 points | FLOWER_EVERY, FLOWER_POINTS |
| a berry | after every 7th leaf: 60% speed for 300 frames | BERRY_EVERY, SLOW_PCT, SLOW_FRAMES |
| treats | stay 420 frames (blink the last 120) | TREAT_LIFE, TREAT_BLINK |
| the butterfly | after 12 leaves: the body curls in 48 frames, the cocoon hatches at 90, +10 points, the next garden at 180 | LEAVES_PER_GARDEN, META_*, BUTTERFLY_POINTS |
| the next garden | GARDEN N shown 75 frames; 6 layouts in rotation | GARDEN_PAUSE, LAYOUTS (play.c GARDENS) |
| the hit | an 8-frame hit-stop, the bonk 40 frames; the panel 60 frames after the hit | HIT_FREEZE, HIT_FRAMES, PANEL_DELAY |
| scoring | leaf 1, flower 5, butterfly 10 | play.c |
| medals | bronze 20, silver 45, gold 90, pearl 150 | MEDAL_SCORES |

**Medals**, tuned with the bot (`botskill`, 12 runs each; a garden is worth about 32 points: 12 leaves, two flowers and
the butterfly): the good bot (flood-fill safety, shortest path) scores 156-840 (gardens 6 to 28); `botskill=2` (an
average player: late turns now and then) 2-311, median about 70; `botskill=4` (a beginner: looks only one step ahead,
late turns) 1-59, median 42, 8 runs in 12 reach bronze; `botskill=6` median 5. So a beginner reaches **bronze** (most
of the first garden), silver is the second garden, gold the third, pearl the fifth.

**2-4 players: the arena.** Pads 2-4 join on the title; P1 starts: ROUND 1 (with "FIRST TO 3 WINS"), and 1.5 s later
everyone crawls, on the same clock (6.5 cells a second, the same all match long). Each caterpillar starts 3 long at its
corner of a pinwheel along the frame (P1 top-left going right, P2 bottom-right going left, P3 top-right going down, P4
bottom-left going up) and **grows one segment every 2 cells it crawls**, never shrinking. Touching **any** body (its
own or another's), the frame or an obstacle is out: the bonk, then its body melts away from the tail (puffs), making
room. Two heads entering the same cell are both out. **The last one crawling wins the round** (a star in the HUD,
P2 WINS THE ROUND!); when the last ones are out on the same step, nobody wins it (DRAW!). The match: **the first to 3
round wins**, or the most wins after 9 rounds; the results rank by rounds won (the house ranking, medals by place).
Each round has the next of 4 arena layouts (none, a centre stone, four pots, two bush walls).

*Why growing as it crawls, not from leaves:* with leaves, careful players can circle forever and a round has no end;
growing all the time (the light cycles' rule) guarantees every round ends, needs nothing to contest, plays the same
with 2 or 4, and keeps it about steering and cutting each other off. A full trail (a new segment every cell) filled the
216 cells in 5-8 s with 4 players, too short to outmanoeuvre anyone; one segment every 2 cells gives rounds of 10-25 s
(measured with the bots: 4 bots about 15 s, 2 bots 13-25 s). The melting bodies of the players out leave the survivors
room for a real duel.

| Arena rule | Value | Where |
|---|---|---|
| speed | 6.5 cells/s | ARENA_SPEED |
| start | 3 segments; grows 1 every 2 cells | ARENA_LEN, ARENA_GROW_EVERY |
| match | first to 3 round wins, at most 9 rounds | ARENA_WINS, ARENA_MAX_ROUNDS |
| pauses | ROUND N 90 frames, the round's end 120 frames | ROUND_PAUSE, ROUND_END |
| melting | a segment every 3 frames | MELT_EVERY |

The bot (`--opt bot=N`, tests, screenshots and card frames) plays from the world, deciding on the frame before each
step: in the garden it takes the shortest path to the leaf (or a treat that is closer and will still be there) among
the moves whose flood fill leaves room for its whole body; in the arena it takes the move with the most room (the
cells the others may crawl into next count as walls), then hugging walls and bodies (it fills its room well), each bot
breaking ties its own way. `botstop=S` crawls into the first wall once the score reaches S; `botskill=E` (1..10) turns
a step late now and then (E*2 %) and, from 3, plans only one step ahead.

## Controls
| | Pad | Touch (player 1) |
|---|---|---|
| start (the title) | A (or Start; the d-pad: also the first turn) | a tap |
| turn | the d-pad (buffered: 2 turns ahead) | a swipe that way, anywhere; a finger kept down steers on: every 14 px it moves (mostly along one axis) is a turn that way, so an L drawn is two turns |
| pause | Select (Start in a run) | the app's pause button |
| back to the title | A (or Select) on the game-over panel | a tap (after the panel's lock) |

Touch is native (`rs_game.touch_native`): the game reads the fingers itself. A swipe is recognised by the game (not the
runtime's once-per-touch swipe) so one finger can steer a whole run; a slanted drag is ignored; while paused nothing
counts; a finger left down from the run never leaves the panel. The swipe's anchors are saved in states.

## Screen and layers
320x240 at 60 Hz, fixed step.

| Layer | Contents | Scroll |
|---|---|---|
| BG4 | the garden: the lawn (a soft 16-px pattern, daisies, flower clumps, tufts, stones, a bee, a butterfly, a snail, ants, a stepping-stone path to each side of the bed, longer grass along the frame), the bed's wooden frame, the soil in a checker of cells with a groove on each cell's edge (the grid reads at a glance), crumbs scattered from cell to cell, a pebble now and then, the frame's shade on its top and left edges. One stage panorama, stage x -128..447: every width shows it once | - (a shake) |
| BG3 | the layout's obstacles: stones, bushes, flower pots, mushrooms (16x16, own palette) | - |
| BG2 | the bodies, one 16x16 metatile per cell, redrawn from the world every frame. Its tile base is the sprites' VRAM: a segment is its sprite's four tiles on the BG palette of its player (BG 1-4 = the four players' colours); the pieces are joined (a neck to each neighbour's side: straight, the four turns, the tail) | - |
| BG1 | the house UI kit: the logo, the title's lines, the panels, GARDEN N, BUTTERFLY!, ROUND N, PAUSED | fixed |
| OBJ | the heads (24x24, sliding between cells: looking ahead, chomping, bonk, beaming; P2-P4 = the greens turned pink, blue and orange: OBJ 1, 4, 5), the cocoon and the butterfly, the leaf, the flower, the berry, the puffs and sparkles, the HUD's pips and small digits, the kit's digits, glyphs and medals | |

HUD: 1 player: the score centred at the top, the garden's 12 leaves as pips (eaten or to go) top right, the berry while
time is slow top left. The arena: each player's head and its round-win stars along the top (a star pops as it is won;
an out player's head blinks).

**Widescreen** (docs/spec.md "Widescreen"): the game opts in (`max_width`, `WIDESCREEN_caterpillarcrawl = 1`). A wider
screen (427 for 16:9, 520 for phones) shows more of the lawn on both sides; the bed and its grid are the same at every
width (identical play). A state saved at one width loads at another (`draw_state_loaded` sets the maps up again).

**Tall screen** (docs/spec.md "Tall screen", the zoomed screen): a phone held upright plays on **the bed turned
upright**. `rs_screen_zoom(512)` first in `game_init` (draw.c `draw_zoom`): 512 lines filling the display's height,
218 columns shown on a 1080x2520 phone (9:21), 232 at 1080x2340, 256 at 720x1440, about 4.9x. The 18 x 12 bed, 304 px
with its fence, would lose a third of its cells to the cropped sides, so in portrait the view turns it a quarter turn
clockwise: **12 cells across and 18 down** (208 px with the fence: every cell shown on the narrowest phone). Only the
view turns: the world, the rules, the layouts, the bot and the save states are the same (the plot's x runs down the
screen, its y from right to left), so a run goes on when the phone turns (the app restarts the game from a state in
the other view). In portrait:
- The garden is its own art (make_art.py `garden(tall=True)`, tiles/garden_tall.png, 320 x 512; loaded instead of the
  panorama, BG maps 64 rows tall): the bed upright with its frame lit from the top-left like the landscape one (the
  tiles are not symmetric: drawn again, not turned), the soil's checker, crumbs and grooves; the lawn above and below
  with a stepping-stone path up towards the HUD and down off the screen, flowers, tufts, stones, a bee, a snail, ants;
  plain lawn under the HUD. A shorter tall screen (a tablet: every column) shows its middle.
- The controls turn with it: the d-pad and the swipes are the screen's directions (main.c `draw_dir_in`).
- The HUD at the screen's top, centred in the columns shown and clear of the app's corner buttons: the score, the 12
  leaves under it, the berry on its left; the arena's heads and stars two by two.
- The heads, bodies (their joins and tails worked out on the screen), obstacles, treats and effects are drawn on the
  turned plot; the butterfly flutters up to the screen's top.
- The same zoomed view on every screen (never `rs_screen_fill(0)`): the stage's 240 lines (the UI's 30 rows) centred
  on the bed, so GARDEN N, ROUND N, PAUSED, the game-over panel (sliding up from the screen's bottom) and the results
  sit on it. The title: the logo on two lines over the lawn above the bed (make_art.py `logo_tall`, the house logo,
  CATERPILLAR at 3x over CRAWL at 4x, in the kit's logo tiles), the slots 5 columns apart, PRESS A TO CRAWL, BEST, the
  join line and the credits cut into lines of at most 25 columns; a banner whose 2x title is wider than 26 columns
  (the Norwegian GAME OVER, the Greek P1 WINS!) uses the small letters on a narrower panel.
- A screen 241-351 lines tall (a near-square display) keeps the landscape bed, the raster drawing more lawn past the
  stage (the lawn of the stage's top 32 lines, repeated every 32 lines and shifted sideways band by band) and the HUD
  at the screen's top. Landscape and 4:3: 240 lines, the same pixels as before.

## Screens (the house flow: no "get ready", the title is the only menu)
title (the logo, a caterpillar inching along with a leaf ahead, the player slots, PRESS A TO CRAWL with the A glyph,
BEST, the join line, the copyright line) -> P1 presses A (or the d-pad, or taps) -> play (1 player: it crawls at once;
2-4: ROUND 1 first) -> the hit (1 player) or the match decided -> game over (1 player: the banner, the panel sliding
up: score, best / new best, the medal, the butterflies of the run; 2-4 players: P2 WINS! and the ranking by rounds
won) -> one press: back to the title (the same players start again with A).

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation. The caterpillar:
a fresh green ramp lit from the top-left, a navy outline, a round head with the house eyes looking where it crawls,
rosy cheeks, a smile (open to chomp), two antennae with golden tips; round segments with little feet, joined by a
neck with a soft crease (on turns too), golden spots on every other one, a fat one with a leaf going down, a pointed
tail. The players' colours: green, pink, blue, orange
(house_style.player_palettes on the greens; P2 by P2_HUE; P4's orange its own ramp, P4_HUE: lights to yellow,
shadows to red). Props: a swaying leaf, a pink flower, a blueberry with a shine, a silky cocoon, a butterfly in the
player's green (pale wings with a dark rim, golden eye spots), soft puffs. The logo: CATERPILLAR in leaf
green, CRAWL in petal pink (3x: the name is long).

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects (main.c sound_init): a crunchy two-bite munch
(a leaf), the softest tick (a turn, the players' own only), a sparkly G6-B6-E7 chime (a flower, with echo), a long
slowing "wooo" (a berry), a soft bonk and wobble (a hit, the only shake), a rising hum (the cocoon), a C6-E7 fanfare
(the butterfly), a pop (a treat sprouts); the house sounds for the menus, the rounds, the medal and the game over.

Music: "Inchworm Picnic" (tools/make_music.py): G major, speed 6, A A' A B (about 31 s): the house pluck pattering in
arpeggios, an oom-pah bass, the soft pad, the breathy lead with a tune that inches up step by step, hops and wiggles
back down; A' answers higher with a music-box doubling, B climbs (vi IV I V) and lands home. Two alternatives
(tools/make_music_alt.py -> music-alt/alt1.mod, alt2.mod; the owner picks): "Dewdrop Waltz" (a marimba waltz in D)
and "Leaf Parade" (a perky chiptune march in C with drums). To switch, call make_music_alt.py OUTDIR N from
tools/build_assets.py instead of make_music.py.

## Tests
`make caterpillarcrawl-check`: the rules alone (tests/test_rules.c: waiting and the start, the clock, the turn buffer,
no instant reverse, the U-turn in two steps, growing from a leaf and the bulge, the wall, a stone, its own body, the
cell its tail leaves, the treats and the slow time, the speed, the butterfly and the next garden, the arena: growing as
it crawls, out on the wall and melting, the last one crawling wins, any body is out, heads meeting, the match,
determinism, the bot), the libretro loader, the smoke tests (flow, start inputs, turns, the wall and the game over,
pause, save RAM, joining and leaving, 4 players, the arena, a 2-bot match to the results, the bot, determinism), the
touch tests (a tap starts, swipes turn, a finger steering, ignored swipes, the panel, every width; portrait: the
swipes and the d-pad turned with the upright bed, a phone turned mid-run plays on the same game), the UI screens
(tests/ui_test.sh), the save states (the lobby, a run, a race, a later garden, the butterfly, paused, 4 bots, the game
over, the results) and their audit, all at 320, 427 and 520 columns, states across widths, and the 22 languages
(tests/i18n_runs.txt).
