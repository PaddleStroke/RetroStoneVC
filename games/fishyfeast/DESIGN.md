# Fishy Feast: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/fishyfeast/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

## Pitch
You are a little fish in a big pond. Nibble the plankton to grow, swallow any fish clearly smaller than you, and keep
away from the bigger ones: fifteen bot fish with names and moods (greedy grazers, timid ones, hunters, opportunists)
share the pond, and so do up to three friends in split screen. Split to snap up a fish that thinks it is safe, spit
a pellet to bait one, lure a big bully onto a sea urchin and watch it burst... and become the biggest fish in the
pond. One life: eaten is eaten. Your score is the biggest you ever grew.

## Legal and originality
- The mechanics come from **agar.io** (Matheus Valadares, published by Miniclip, 2015), which is proprietary. We use
  only its **mechanics**, which are not protected: round "cells" that grow by eating pellets and smaller cells
  (about 25% smaller), slower as they grow, a camera that zooms out with the size, splitting into pieces that shoot
  forward and merge back after a while, ejecting mass, spiky viruses that burst a big cell, mass decay, a
  leaderboard of the biggest.
- Nothing else is taken from it: no name, logo, art, colour scheme, sound, music, text, skin, map size or number
  measured from it; we did not look at its code or anyone's clone. The theme (a cute pond, fish with big eyes and
  pale fins, plankton, pellets, sea urchins, a stone rim, light rays), the bots and their personalities and names,
  the fair-play rules, the split screen for four, the touch stick, the medals, the sounds and the music are ours.
  The code is our own (C11 on the RetroStone SDK, integer fixed point), written from this design.
- Every number below is ours, tuned with our own bots for our 320x240 screen and a cute, fair game.

## Rules
The pond is a rounded rectangle of 1920 x 1440 px (corners of radius 240), its floor seen from above through the
water. A fish is a disc: its radius is 2.2 x sqrt(mass) px (a new fish of mass 10: 7 px; 100: 22 px; 1000: 70 px).
The d-pad swims in 8 directions at **full speed for its size**: 64 / (radius + 30) px a frame (1.73 for a new fish,
1.23 at 100, 0.81 at 500, never under 0.5), with a little glide (the velocity goes 1/6 of the way each frame).

- **Plankton**: 800 specks (1 mass each) drift on the floor; an eaten one grows again elsewhere 6-16 s later.
- **Eating**: a fish eats another (a bot's or a player's piece) when it is **25% heavier** (mass x 4 >= prey x 5)
  and swims over it (the distance between the centres is at most its radius minus 40% of the prey's). The eater gets
  the whole mass. Two fish of nearly the same size just swim past each other.
- **Split (A), repeatable**: every piece of mass 36 or more splits in two; each new half **shoots forward** in the
  swim direction (or where it looks): 6 + radius/12 px a frame, keeping 7/8 a frame (it carries about 8x that: 50-70
  px). Press again and every piece big enough splits again: **1 -> 2 -> 4 -> 8 -> 16 pieces** at most; a piece
  under 36 does not split. All your pieces are steered together (the same direction, each at its own speed), drift
  towards their common centre and push each other apart without overlapping; they **merge** back only after their
  timer: 15 s + 2 frames per mass unit of the piece (at most 25 s), then they swim together and merge. **Each piece is
  vulnerable on its own**: any fish 25% heavier than that piece eats it, so a split to catch prey is a gamble. Your
  score and size are the total of your pieces; you are **out only when your last piece is eaten**. The bots follow
  the same rules.
- **Eject (B)**: every piece of mass 32 or more spits a pellet forward (it costs the piece 10; the pellet holds 8 and
  flies about 70 px); anyone can eat a pellet (you can eat your own after 2/3 s); a pellet left alone dissolves after
  a minute. Hold B to spit one every 8 frames. Use it to feed a friend, or to **bait** a bot.
- **Sea urchins**: 14 fixed urchins (size 120, radius 24 px). A fish piece heavier than 120 that touches one (its
  centre within its radius + half the urchin's) **bursts** into up to 8 pieces (16 in all at most) flying outwards,
  each with its merge timer: easy prey. Smaller fish pass under the spikes and **hide** there (drawn behind the
  urchin): a big fish cannot follow without bursting.
- **Decay**: a fish heavier than 150 loses 0.25% of its mass a second (so nobody grows for ever); a bot heavier than
  450 loses 3% more a second (the bots' cap, so a skilful player becomes the biggest fish).
- **One life**: eaten = out. The run ends when every player is out; the score is the **biggest mass reached**
  (the leaderboard shows the five heaviest fish now, and your rank). An eaten bot comes back 2.5-5 s later, small
  (10-15), born away from the fish that could eat it.

### The bots: kind of smart, never unfair
Fifteen bots, named BUBBLES, FINLEY, COOKIE, MOCHI, PEBBLE, SPLASH, WIGGLES, PUDDING, NOODLE, BISCUIT, JELLY, GUPPY,
MARBLE, PICKLE and WAFFLE (names are never translated), in eight colours, start at masses from 12 to 130. Each has a
**personality** (play.c bot_think, the table BOT_KIND_TABLE in game.h):

| Kind | Reacts every | Flees a threat within | Chases one prey | Splits at prey | Forgets urchins | Misjudges sizes |
|---|---|---|---|---|---|---|
| grazer (prefers plankton) | 16-28 frames | 2.0 x its radius + 24 px | 1.8 s | never | 6% | 15% |
| timid (flees early) | 12-22 | 3.8 x | 1.3 s | never | 4% | 12% |
| hunter (chases, splits) | 14-24 | 1.6 x | 5.5 s | 30% | 22% | 18% |
| opportunist (loves split pieces: x2.6) | 12-20 | 2.1 x | 3.3 s | 25% | 12% | 15% |

- **Limited vision**: a bot sees what a player of its size would see on a full screen (about 340 x 260 px at mass
  10, more as it grows: the same zoom steps).
- **Reaction delays**: it decides every 12-28 frames and keeps its heading in between, so it reacts late to a split.
- **Mistakes**: sizes misjudged by up to 12-18% (it may approach a fish it cannot eat, or miss a threat); urchins
  forgotten now and then (it may burst); a hunter chases too long and can be lured; its aim wobbles by up to 8
  degrees; a split attack aims at where the prey is, not where it will be.
- **What it does**: FLEE what could eat one of its pieces (also what could split-kill it: a hunter, an opportunist, a
  player at half its mass within split reach), a small one runs for the shelter of an urchin; else CHASE the best prey
  (a fish 25% lighter: big and near is better; it leads its aim a little; after its chase time it gives up on that
  prey for 4 s); a hunter or an opportunist with 1-2 pieces may **split** at a prey within reach when nothing threatens
  it; else GRAZE the best plankton or pellet in view (pellets count 8: **bait**); else wander. Pieces that would burst
  keep 56 px from the urchins (unless it forgets); all keep off the pond's edge. Bots hunt each other too.
- **Fair play**: no bot hunts a player in its first 15 s; a player lighter than 40 is only taken if it swims right by
  (48 px); one bot at most hunts a player under 150 at a time (two above). The bots' cap (above) keeps them beatable:
  they hover around 450 at most, while a good player passes 1000.
- **Beatable**: bait a grazer with pellets next to your split reach; let a hunter chase you past an urchin (it often
  forgets it); split-kill a greedy bot that grazes too close; hide among the spikes when a big one comes; and a hunter
  that split at you is in two vulnerable pieces.

### Tuning (every number in src/game.h)
| Rule | Value | Where |
|---|---|---|
| the pond | 1920 x 1440 px, corners 240 | POND_* |
| size, speed | radius 2.2 sqrt(m); 64 / (r + 30) px a frame, 0.5 at least; glide 1/6 | RADIUS_K, SPEED_*, ACCEL_DIV |
| eating | 25% heavier, covering (40% of the prey's radius may stick out) | EAT_* |
| plankton | 800, regrow 6-16 s | PLANKTON_* |
| split | from 36, 16 pieces, 6 + r/12 px a frame kept 7/8, merge 15 s + 2 frames a mass unit (+10 s at most), cohesion 1/40 | SPLIT_*, BOOST_KEEP, MERGE_*, COHESION_* |
| eject | from 32, costs 10, pellet 8 at 9 px a frame, your own after 40 frames, a minute's life | EJECT_*, PELLET_LIFE |
| urchins | 14, size 120, touch +50%, burst into 8 at 5 px a frame | URCHIN*, BURST_* |
| decay | above 150: 42 ppm a frame (0.25%/s); bots above 450: +500 ppm (3%/s) | DECAY_*, BOT_CAP* |
| bots | 15; start masses 12-130; respawn 2.5-5 s at 10-15; the kinds above | BOTS, BOT_*, BOT_KIND_TABLE |
| fair play | grace 15 s, newbies under 40 (48 px), 1 hunter under 150 then 2 | PLAYER_GRACE, NEWBIE_*, CHASERS_* |
| zoom | steps 1, 0.84, 0.71, 0.60, 0.51, 0.43, 0.36, 0.30, 0.26, 0.22; the fish at 18 px (full screen), 16 (halves), 13 (quarters) | ZOOM_* |
| touch | the stick: the left 60% of the view, full speed at 28 px; SPLIT radius 20, EJECT 16 at the bottom right | STICK_*, BTN_* |
| medals (1 player) | bronze 80, silver 250, gold 600, pearl 1200 | MEDAL_SCORES |

**The medals** were tuned with the player bot (`bot=1`: the bots' brain as a "player" personality: quick, no
misjudging, splits often) and the human-like one (`botskill=E`: it reacts 2E frames later, misjudges sizes by 3E%,
forgets the urchins 3E% of the time and aims 2E/256 of a turn worse), 12 runs of up to 10 minutes each: E = 10 (a
beginner) reached a mean best of 245 (9 of 12 bronze or better, 3 silver, 1 gold); E = 6: 553 (all bronze or better,
5 silver, 3 gold, 1 pearl); E = 3: 546; the player bot: 1072 (5 pearls). So bronze is a beginner's first good run,
silver a few fish eaten, gold the top of the pond for a while, pearl a pond-ruling run.

## Screen and layers
320x240 at 60 Hz, fixed step, seen from above the pond. The house layout, with the SNES-like tricks this tile console
needs to **zoom**:

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the pond's stone rim colour (a gradient by line, the house 2-line dither); outside the rim it is fogged into deep water (`rs_fog` outside window 2) | - |
| BG4 | the pond floor (muted teal sand, pebbles, weed tufts seen from above, shells): the **affine layer** (Mode-7-like), a 64x64-tile texture that repeats, **scaled by the camera's zoom**, its matrix and scroll set per view by the raster callback; hidden outside the pond (window 1, computed per line and per view from the rounded rectangle) | the camera |
| BG3 | the plankton specks: a **screen-space map rewritten every frame** (each visible speck becomes a dot tile at its place on a 2-px grid, the layer scrolled by the camera's pixel parity so that they move with the floor 1 px at a time; 3 sizes by the zoom; 3 colours by palette), so hundreds of specks cost no sprites | the camera |
| BG2 | the light rays: soft bands at the house slope 1/2, **added** to what is below (colour math), drifting and swaying (line scroll); not in the quarters of 3-4 players (the A20's budget) | drift |
| BG1 | the house UI kit: the logo, the title's lines, the leaderboard, ranks and messages, the panels, PAUSED | fixed |
| OBJ | the fish (players OBJ 0, 1, 4, 5; bots in the B ramps of those and OBJ 6, 7), pellets, urchins, bubbles, markers and the touch controls (OBJ 2), the kit's digits, glyphs and medals (OBJ 3) | |

**The zoom on a tile console.** Each view has a camera that follows its fish (the centre of its pieces) and a zoom
that **steps** out as the fish grows (10 steps from 1 to 0.22, eased over about a second; a margin before it steps
back in), also to keep all its pieces in view and so that the biggest fish in sight fits the biggest sprite. The floor
is the affine layer, so it scales smoothly. **Sprites do not scale**: a fish is drawn in the **size tier** nearest its
radius on screen: 3, 5, 7, 9, 12, 15, 19 and 24 px are single sprites (the small ones with a tail-flick frame); the
**big tiers, 30 and 40 px, are built from parts**: the body's four quarters (a top and a bottom one, each mirrored
left-right: lit from above, the small fish's bands level), the face (eye, cheek, mouth, side fin), the tail and a
highlight, 7 sprites. Urchins come in 5 tiers, pellets in 3. Plankton is on
BG3. Bigger fish are drawn in front (the order of the OAM), the urchins between the fish lighter and heavier than
them, so a small fish hides under the spikes; a view keeps its sprite budget (116 for one view, 58 for halves, 29 for
quarters: 128 in all) and always draws its own fish first. Every fish has two body ramps (A, B) in the same palette
index layout, so 8 palettes give the 4 players and 8 bot colours; a player's outline is its **colour ring** (the
palette's index 1), and in split screen a chevron of that colour floats over its biggest piece. Measured: no strict-mode
warning with 4 views (sprites per line, OAM, VRAM: 64 KiB in English). Cost on the build host (`--bench`, 3000 frames):
the PPU 0.66 ms a frame with one view, 0.76 ms with four (the floor's matrix is decoded for each view crossing a
line, so the quarters leave out the rays), the rules and the bots 0.03-0.05 ms: 10-15 ms on the A20 (x15-x20).

**Split screen** (play.c view_layout, draw.c): 1 player: the full screen (no viewports: the kit's sprites stay in
place); 2 players side by side (159 x 240 at 320); 3 and 4 players in quarters (159 x 119), the 4th quarter of a
3-player game showing **the whole pond as a map** (every fish a dot, the players with their chevron). Each view is an
`rs_viewport` with its own scroll, sprites and raster state (the floor's matrix, the pond's windows); its fish's mass
in big digits at its top with its fish icon, its rank under it (#3), EATEN BY ...! in it when it is out. The quarters
start one zoom step out and aim the fish at 13 px so they stay readable. The leaderboard: the 5 heaviest fish (and
your rank if you are not in it) at the top right in 1 player; at the bottom right of the screen in split screen; its
rows 2 px apart (BG1's line scroll maps the screen lines to the layer's rows, the gaps to a blank row).
While paused, a band across the views shows PAUSED.

**Widescreen** (docs/spec.md "Widescreen"): the game opts in (`max_width`, `WIDESCREEN_fishyfeast = 1`). A wider screen
(427, 520) shows **more pond** (the views are wider at the same zoom), the rules are unchanged (play.c never asks the
width; the bots' vision is fixed). The UI kit centres itself; the leaderboard follows the screen's right edge; the
touch buttons the view's bottom-right corner. A state saved at one width loads at another. The touch test plays the
same drag at 320, 427, 520 and 560 and compares the runs.

**Tall screen** (docs/spec.md "Tall screen", the zoomed recipe): a phone held upright. `rs_screen_zoom(ZOOM_H)` (420
lines, game.h) first in `game_init`: the app scales the 420 lines to the display's height and crops the sides (180
columns on a 1080x2520 phone, 194 on a 1080x2340 one), so the fish are bigger than in landscape; the 240-line stage is
centred. The views that touch the stage's top or bottom reach the screen's (play.c `views_tall`): the pond over the
whole height. The raster draws the floor and the pond's rim for every line by itself; past the stage it carries on the
rim's end colours, sways the rays by BG2's scroll (the line-scroll table is the stage's) and turns the UI layer off
(line 0 turns it back on). The specks' map is 128 rows on a tall screen; the sprite budget grows by 80.
- **One fish playing** fills the display's height, the sides cropped: its camera fits its pieces in the columns shown
  and the whole height (`view_shown`; only the picture: the bots' vision is fixed). **The HUD at the screen's top**:
  the mass (a sprite at `rs_screen_top()` + 8) and the leaderboard, right-aligned in the columns shown (BG1's top 80
  lines pinned at screen line 22 with `rs_bg_pin`, under the frontend's pause button; the raster shows BG1 there).
  The touch SPLIT / EJECT buttons sit at the bottom-right of the columns shown, the stick's zone in their left 60%.
- **One view throughout**: the title and the panels keep play's zoomed view (the logo on two lines, the lines wrapped,
  a banner too wide on two lines: src/tall_ui.h). Only the split screens (2-4 players) show every column
  (`rs_screen_fill(0)`: the whole 320 x 420 picture across the width, the views as tall as the screen).
- A 240-line screen is unchanged (the same pixels in landscape and 4:3).

## Screens (the house flow: no "get ready", the title is the only menu)
title (the logo, the pond going on behind it with the bots, a goldfish bobbing, the player slots, PRESS A TO SWIM with
the A glyph, BEST, the join line, the copyright line) -> P1 presses A (or B, Start, the d-pad; a tap) -> play (the
mass at the top, the leaderboard, A: SPLIT - B: EJECT on a panel at the bottom for the first 5 s; BIGGEST FISH!
when you become the heaviest (1 player: under the leaderboard, above your fish);
Select, or Start, pauses) -> eaten (a shake, EATEN BY BUBBLES!; in split screen the others swim on and the eaten
player's view watches the pond) -> game over 1.5 s after the last player is out (the banner, the panel sliding up:
SCORE = the biggest mass reached, best / new best, the medal, EATEN BY ...!; 2-4 players: the ranking by biggest mass
with medals), the pond going on behind -> one press (or a tap): back to the title.

## Controls
| Input | Action |
|---|---|
| d-pad | swim in 8 directions (full speed for your size); let go: the fish glides to a stop |
| A | split (every piece of 36 or more) |
| B | eject a pellet (from every piece of 32 or more); held: one every 8 frames |
| A (or B, Start, the d-pad) on the title | start |
| Select (or Start in a run) | pause / resume |
| pads 2-4: A or Start on the title | join (B leaves) |
| A, Start or Select on the game-over panel | back to the title |

### Touch (docs/spec.md "Touch")
The game reads the touches itself (`rs_game.touch_native = 1`): the phone apps pass it the fingers and draw no
buttons. Touch is player 1, alongside the pad (players 2-4 join with pads).

| Touch | Action |
|---|---|
| a finger down on the left 60% of player 1's view, dragged | a **virtual stick**: a ring where it went down, a knob under the finger; the fish swims that way, from slow (a short drag) to full speed (28 px and more) |
| the round **SPLIT** button (bottom right) | split (as A) |
| the round **EJECT** button (left of it) | eject (as B) |
| a tap (a finger lifted) on the title | start |
| a tap on the game-over panel | back to the title (after the same lock as A; a finger left down from the run does not count) |
| the app's pause button | pause / resume (it presses Start); the fingers do nothing while paused |

The buttons are drawn in the house style of the apps' drawn buttons (a see-through mid-blue disc, the house checker
dither, a cream ring, a navy outline; filled and lower while pressed), each with an icon (two pieces flying apart; a
fish spitting a pellet), once a finger has touched the screen. Two fingers work at once (the stick and a button). What
the fingers set (the stick's finger and origin, the buttons held) is in the registered state (main.c TV, touch_*): a
load forgets the fingers, one still down starts again. `tests/touch_test.sh` (in the check) plays scripted fingers.

## Up to four players
Pads 2-4 join with A on the title (B leaves). Everyone swims in the same pond with the bots, in split screen; players
can eat each other (and feed each other with pellets). Each player is out when eaten; the run ends when all are out;
the results rank everyone by their biggest mass, with medals. The fish: a goldfish (orange with a pale yellow ring), a
blue, a green and a purple one (tools/build_assets.py turns the A ramp and the ring with house_style.player_palettes);
fins, eyes and the bots' B ramps stay.

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation. The fish: a round
body (4 shades, light from the top-left; the big ones lit from straight above, with a highlight), a big eye with a glint, a
pink cheek, a tiny mouth, pale fins (a fanned, notched tail with darker rays; a side fin), a 1-px outline (navy, or
the player's ring); tiers of 3 to 40 px; a tail flick when swimming. The bots' colours: pink, mint, lavender, sky,
lemon, coral, teal, orchid. The urchins: a purple ball with a sleepy little face and pale-tipped spikes. Pellets: a
blob in the spitter's colour. Bubbles rise where a fish is eaten, merges or bursts. The floor: muted teal sand with
pebble clusters, weed tufts and shells; the stone rim; deep water outside. The touch controls as above. The title
logo: FISHY in goldfish orange, FEAST in pond aqua.

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects (main.c sound_init): a tiny "nip" for
plankton and pellets (at most every 4 frames), a round "gulp" and a bubble for a fish eaten, a "bloop" and a whoosh
for a split, a little "pop" for a pellet, a pop and scattering notes for a burst, a sad falling "wooo" when eaten, a
soft "blub" when pieces merge; the house ding for BIGGEST FISH!, the house sounds for the menus, the medals and the
game over.

Music: "Pond Picnic" (tools/make_music.py): F major, speed 6 (125 BPM), A A' B A' C (about 38 s): the water-drop
pluck in bubbly arpeggios, a soft round bass, the warm pad with tiny bubble plips, the breathy lead with a curious
little phrase that swims up and turns back; A' sings it an octave up, B wanders off (Bb C Am Dm), C swims home. Two
alternatives (tools/make_music_alt.py -> music-alt/alt1.mod, alt2.mod; previews in
docs/music-alternatives/fishyfeast/): "Lazy Lagoon" (a dreamy slow sway in D dorian) and "Reef Bounce" (a bouncy
reef tune in C major with drums). To switch, call make_music_alt.py OUTDIR N from tools/build_assets.py instead of
make_music.py.

## Tests
`make fishyfeast-check`: the rules alone (tests/test_rules.c: sizes and speeds, swimming and gliding to a stop,
plankton, the eating threshold (125 eats 100, 124 and 120 do not, not without covering), split (the minimum mass,
1 -> 2 -> 4 -> 8 -> 16 pieces and no more, the mass kept, the front half ahead, a piece eaten by a mid-size fish while
the player swims on, out only when the last piece is eaten, no merge before the timer, merged after), eject (the cost,
the flight, too light, a bot eating the pellet), the urchins (a small fish hides, a big one bursts into 8 that
scatter, the mass kept), the decay and the bots' cap, the bots (they flee a bigger fish, a hunter catches a still
player, no hunting in the grace, one hunter at most on a small player, 112 never eats 100, a full pond for 200 s:
eating each other, coming back, under the cap, determinism; the player bot grows), the split-screen layouts (1-4
players, 427 columns) and the touch stick and buttons), the libretro loader, the smoke tests (flow, start inputs,
swimming, split and eject on the pad, the game over and back to the title, pause, joining and leaving, 2-4 players in
split screen, the map view, a big fish zoomed out, no strict-mode warning, save RAM, the bot growing, determinism),
the touch tests, the UI screens (tests/ui_test.sh), the save states (the title's lobby, a run, a fish in pieces, a
big fish, a run swum by touch, 4 players, paused, the game over, the results) and their audit, at 320, 427 and 520
columns, states across widths, and the 22 languages (tests/i18n_runs.txt).
