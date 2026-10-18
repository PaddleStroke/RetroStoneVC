# Cosmo Corgi: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/cosmocorgi/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

## Pitch
A corgi astronaut flies its little rocket through space, and space is full of wind-up robots. The rocket barks star
shots all by itself: the player just flies it, through a pastel nebula, an asteroid belt, a robot space station and
the icy comets, each guarded by a big robot critter. Robots pop into sparkles and bolts, gold groups and gift
carriers leave capsules (more shots, speed, a shield, puppy pods that fly along), and one hit ends the run. Cute,
readable, never a bullet hell: a 7-year-old gets going in the nebula, then the robots come in ever greater
numbers, from every side; by the station a good player is busy, and the second loop is hard.

## Legal and originality
- The horizontal-scrolling shoot 'em up is the genre of **Gradius** (Konami, 1985) and **R-Type** (Irem, 1987), both
  proprietary. We use only **mechanics**, which are not protected: a world scrolling right to left, a ship flown
  freely on the screen, enemy waves in flight patterns (sine waves, loops, swoops, groups from behind, kamikazes,
  turrets on the walls), a group that drops a power-up capsule when all of it is destroyed, weapon upgrades, a
  shield, "option" helpers that follow the ship's path and fire with it, walls to avoid, a boss with a health bar at
  the end of each stage, and a loop that comes round harder.
- Nothing else is taken from them: no name, art, sound, music, text, layout, stage design or numbers measured from
  them. Gradius's power-up meter (choose what to buy) is not used: our capsules give their power at once. The theme
  (a corgi astronaut, wind-up robot critters, puppy pods, dog bones, barks), the characters and bosses (a clockwork
  ladybird, a rock crab, an owl sentry, a snow walrus), the stages, the screens, the co-op for four, the medals, the
  sounds and the music are ours. The code is our own (C11 on the RetroStone SDK, integer fixed point), written from
  this design; no other game's code was read for it.
- Every number below is ours, tuned for our 320x240 screen and a fair, cute game.

## Rules
The world scrolls from right to left (1 px a frame in the first loop). The rocket flies freely on the stage with the
d-pad, in 8 directions (1.5 px a frame, a diagonal 0.71 as fast on each axis), and **fires by itself all the time**
(the owner's rule: no fire button): a volley every 9 frames. Its hit box is a small core (+-4 x +-3 px, the
corgi's cockpit): robots' shots and robots hit there; the walls hit the rocket's body (+-9 x +-5). **One hit ends
the corgi's run** (the house rule), unless it carries the **shield**, which takes exactly one hit and pops (then the
rocket blinks, untouchable, 1.5 s, and is pushed into the open if a wall hit it). A hit is an 8-frame hit-stop and a
shake, then the rocket tumbles away with X eyes and stars: nothing is hurt.

**Power-ups** come in capsules (a glass orb drifting left, its power drawn inside): every gold group popped to the
last robot drops one (+100 points), and so does every gift carrier. The capsules come in a fixed cycle (spread, pod,
shield, speed, beam, pod, homing, shield, spread, speed, homing, beam), whoever touches one gets it:

| Capsule | Effect | Max |
|---|---|---|
| spread shot | weapon level +1, the spread: 3 shots fanning out (level 1), 5 (levels 2-3; level 3 fires faster) | level 3 |
| star beam | weapon level +1, the beam: a long laser of stars that goes through every robot in its line (damage 1, then 2; two beams at level 3) | level 3 |
| homing bones | weapon level +1, the barks plus dog bones that turn toward the nearest robot ahead (1, then 2 at a time; double barks at level 3) | level 3 |
| speed up | +0.5 px a frame | 2 |
| shield | takes one hit | 1 |
| puppy pod | a puppy in a little pod follows the rocket's path (14, then 28 frames behind) and barks with each volley | 2 |

A weapon capsule raises the weapon level and picks the weapon's style (another style keeps the level). A capsule of a
power already at its maximum gives 500 points (MAX POWER!).

**Robots** (wind-up critters, they pop into sparkles and bolts) come in scripted waves (play.c WAVES), each in a
flight pattern; a wave marked gold is drawn gold, and drops a capsule when all of it is popped (one that gets away,
none):

| Robot | Strength | Points | Flight |
|---|---|---|---|
| wind-up ladybug | 1 | 10 | sine waves, straight lines |
| propeller drone | 1 | 10 | loops, swoops from the top or the bottom, from behind (from the left, late in the game), lines |
| little UFO | 2 | 30 | comes in, hovers, fires once (a fan of 3 from the second loop), leaves up or down |
| dasher fish | 1 | 20 | stops, winds up for 50 frames while sliding to a corgi's line (it shakes: a warning), then dashes at 4 px a frame |
| gift carrier | 2 | 50 | slow and straight; drops a capsule |
| robo-turtle | 10 | 100 | slow and tough; a fan of 3 every 2.5 s |
| turret | 3 | 50 | on the walls (the asteroid belt, the station, the comets); an aimed shot every 2.7 s |
| asteroid, comet | 3 | 20 | drift down across the screen |

Robots never fire at a corgi nearer than 64 px, nor from outside the stage; their shots are slow (1.25 px a frame in
the first loop) and pink, drawn in front of everything. From the second loop the bugs, drones and dashers fire once
each. The walls (8-px tiles from the top and the bottom) are a hit; there are always 13 tiles (104 px) open.

**Sections and bosses.** Four sections make a loop, each 2560 px of waves (about 45 s), then BIG ROBOT! and its boss
(a 64x48 robot critter with a health bar at the bottom of the screen; untouchable while it flies in; 10 points a hit
and a big bonus to whoever pops it; after 90 s it flies away, no bonus):

| Section | Walls | Boss and pattern |
|---|---|---|
| the pastel nebula | none (open space, gentle) | the **clockwork ladybird**: sways up and down; fans of 3 aimed shots (5 when it is below half its strength) |
| the asteroid belt | rocky walls, turrets, drifting asteroids, dashers | the **rock crab**: follows a corgi's line; shots from both claws; every 4th volley two drones from its mouth |
| the robot station | metal walls with gates, turrets, drones from behind | the **owl sentry**: a figure of eight; rings of 8 shots, turning (12 and an aimed shot when angry) |
| the icy comets | icy walls, comets, dashers, drones from behind | the **snow walrus**: slides up and down; icicles in threes along its line; a big slow snowball |

After the fourth boss the next loop begins (LOOP 2): the scroll is 0.25 px a frame faster (up to 2), the robots 15%
faster, their shots 0.25 px a frame faster (up to 2.25) and their reloads 25% shorter (down to 40%), the scripted
waves have 2 more robots each; the bosses take 25% more hits each loop.

**The ramp.** On top of each section's script come extra waves (play.c ramp_step), more and bigger as the
**intensity** grows: I = 13 per section + 0..10 across the section + 20 per loop (the nebula 0..10, the asteroid belt
13..23, the station 26..36, the comets 39..49; loop 2 20..69). The first 22 s of the nebula (1300 px) have none: the
script alone, gentle (24 robots in the first 20 s). Then an extra wave every 400 - 9 I px (110 at least), of 3 + I / 7
robots (8 at most), of more and more kinds as I grows: front waves (sine, lines) from the start, swoops from the top
and the bottom from 5, drifting rocks or comets from 8, dashers and UFOs (two at once; three from 30) from 10, turrets
on the walls from 12, loops from 14, drones from behind from 18, robo-turtles from 20 (two from 45); from 34 a second
wave of another kind comes at the same time, from another side. Every 4th extra wave of bugs or drones is gold (a
capsule): the power-ups keep coming as the waves thicken. Lanes stay in 56..184, clear of the highest walls; the
robots still never fire at a corgi nearer than 64 px. Robots per minute of waves (the rules test measures it, an
untouchable corgi waiting):

| | the nebula | the asteroid belt | the station | the comets |
|---|---|---|---|---|
| loop 1 | 87 (24 in the first 20 s) | 114 | 162 | 285 |
| loop 2 | 212 | 361 | 309 | 371 |

(at most 44 robots at once: the second loop's later sections are full.)

Every number is in src/game.h (the tuning table):

| Rule | Value | Where |
|---|---|---|
| the rocket | 1.5 px/frame (+0.5 per speed boost, 2 at most), its centre in 14..300 x 14..226 (the stage), hit box +-4 x +-3 (walls +-9 x +-5) | ROCKET_*, SPEED_*, HIT_*, WALL_* |
| auto-fire | a volley every 9 frames (7 with the spread at level 3), the bark 6 px/frame, 16 shots on screen per player, the reach x 336 | FIRE_GAP, BARK_SPEED, SHOTS_PER_PLAYER, SHOT_RANGE |
| the weapons | spread side shots 1.0/1.25/1.5 px/frame up and down; the beam 8 px/frame, every 14/11/11 frames, damage 1/2/2; bones 4 px/frame, turning 7/256 a frame, every 34/34/22 frames, 1/2/2 at a time, damage 2, 150 frames | SPREAD_VY, BEAM_*, BONE_* |
| pods | 2 at most, 14 frames of path apart | PODS_MAX, POD_DELAY |
| hits | an 8-frame hit-stop, the bonk 60 frames; the shield's blink 90 frames | HIT_FREEZE, HIT_FRAMES, SHIELD_INVUL |
| the scroll | 1 px/frame, +0.25 each loop, 2 at most | SCROLL_* |
| sections | 4, 2560 px of waves each (480 px more at most for the last robots to go), the name 150 frames, BIG ROBOT! 120, clear 180 | SECTIONS, SECTION_LEN, SECTION_MAX_WAIT, *_FRAMES |
| robots | strength, points, speeds by kind (above), +15% speed each loop (4 loops); 44 at once at most | KIND_*, LOOP_SPEED_PCT, MAX_ENEMIES |
| the ramp | intensity 13 a section + 0..10 across it + 20 a loop; none in the nebula's first 1300 px (200 px in the others); an extra wave every 400 - 9 I px (110 at least) of 3 + I / 7 robots (8 at most), two at once from 34, every 4th gold; the scripted waves +2 robots a loop | RAMP_*, LOOP_WAVE_N |
| flight | sine 2/256 turn a frame; loops of radius 32 px; the dasher winds up 50 frames, dashes 4 px/frame; the UFO hovers 110 frames | SINE_W, LOOP_*, DASH_*, HOVER_FRAMES |
| their shots | 1.25 px/frame (+0.25 a loop, 2.25 at most), never at a corgi nearer than 64 px; 48 at most; the turtle every 150 frames, a turret every 160; reloads 25% shorter a loop (40% at most) | EB_*, MAX_EBULLETS, TANK_FIRE, TURRET_FIRE, FIRE_LOOP_PCT |
| capsules | 0.5 px/frame; the cycle above; 500 points when maxed; a gold group +100 | CAPSULE_SPEED, CAP_ORDER, MAXED_POINTS, GROUP_BONUS |
| bosses | 60, 84, 96, 96 hits (+300% per extra player, +25% each loop); 1000, 1500, 2000, 2500 points (+1000 each loop); 10 a hit; fights at x 252 after 120 frames flying in; flies away after 90 s | BOSS_* |
| walls | 13 tiles always open; the rock 1-5 tiles from each side, the station 2-4 with gates of 6, the ice 1-4 above and 0-3 below; one tile per column at most; plateaus for the turrets | TER_*, play.c ter_make |
| medals (1 player) | bronze 2000, silver 7000, gold 20000, pearl 40000 | MEDAL_SCORES |

**The medals** were tuned with the good bot and the human-like bot (`botskill=E`: it decides only every E/2 frames,
looks 28 - 2E frames ahead instead of 32, misses 7E% of the shots coming and misjudges its aim by up to 3E px), 12
runs each, against the ramp. The good bot (`bot=1`, superhuman: it sees every shot) is hit once in 12 runs of 40000
frames: about 2400 points at the end of the nebula, 6300 after the asteroid belt, 12000 after the station, 18700
after the first loop, 51000 after the second. The human-like bots now fall early: E = 10 (a beginner) mostly in the
asteroid belt (1600-16100, mean 6100: 10 of 12 bronze or better, 2 silver, 2 gold), E = 8 (mean 9700), E = 6 (mean
10500: 6 bronze, 1 silver, 5 gold), E = 4 (mean 16500, best 38000), E = 2 (mean 12500, best 28600): none reaches
40000. So bronze is the nebula's end (a beginner gets it), silver the asteroid belt and its crab popped and into the
station (the owner's 5000 of the first version lands on bronze), gold the whole first loop, pearl the second loop's
station and comets (real skill).

**The bosses** (seconds from their arrival to their pop, the bot): 1 corgi with the bark 29, 12, 36, 58 (the ladybird,
the crab, the owl, the walrus); with the spread shot at level 3 6, 9, 10, 13; 4 corgis with the spread shot at level 3,
pods and shields 16, 26, 23, 32; 4 with homing bones at level 3 9, 10, 12, 17.

**The bot** (`--opt bot=N`: tests, screenshots and card frames) plays from the world: for each of the 9 directions held
for the next 32 frames it forecasts every robot's shot and every robot along its last move (a dasher winding up:
its dash), the boss and the walls (scrolling), and adds up the danger (the sooner, the worse); of the safest it picks
the one nearest its aim: a capsule (if it is the nearest corgi to it), else the line of the robot ahead (or the
boss; not a turret: those sit by the walls), near its home column. With 2-4 bots each keeps its own lane and column and none flies on top of another.
`botstop=S` makes it fly into the robots once its score reaches S (the tests' game over).

## Screen and layers
320x240 at 60 Hz, fixed step. The house layout (docs/art-direction.md "Backgrounds"):

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | one colour per line (raster callback), each section's own: violet to pink (the nebula), navy to dusty brown (the asteroids), navy to teal (the station), navy to ice blue (the comets), blended in as a section begins; a 2-line dither step every 8 lines | - |
| BG4 | the stars: faint ones and two twinkle groups (their colours set every frame); every other band of 24 lines scrolls twice as fast (line scroll): two depths of stars | 1/16 and 1/8 |
| BG3 | the clouds of the nebula and the planets (a ringed planet, a moon, a far planet), no outline; one palette per section, blended | 1/4 |
| BG2 | the walls: the world's ring of columns drawn every frame from 8 tiles per theme (surface, corners, fill, sides; the ceiling is the floor V-flipped) | 1 (shakes on a hit) |
| BG1 | the house UI kit: the logo, the title's lines, the section's name, BIG ROBOT!, the messages, the panels, PAUSED | fixed |
| OBJ | the rockets, pods, shields and the corgis' shots (P1 OBJ 0; P2-P4 OBJ 1, 4, 5: the paint and the fur recoloured), the robots and the boss (OBJ 2; the gold groups and the boss's hit flash in OBJ 7), the kit's digits, glyphs and medals (OBJ 3), the capsules, the pops, the robots' shots, the warning arrows, the boss bar, small digits (OBJ 6) | |

One boss at a time: its two 64x48 frames are loaded into a slot of 96 sprite tiles as its section comes (draw.c
boss_tiles), so the four bosses cost the VRAM of one.

**Widescreen** (docs/spec.md "Widescreen"): the game opts in (`max_width`, `WIDESCREEN_cosmocorgi = 1`). The house rule
for scrolling games: about **a third of the extra width behind the corgis and two thirds ahead** (the stage origin,
WIDE_LEFT_NUM / WIDE_LEFT_DEN: 35 + 72 columns at 427, 66 + 134 at 520, 80 + 160 at 560); the stage's 320 columns are
those of 320 pixel for pixel, and **the gameplay is the same at every width**: the rules (play.c) never ask the width.
The rockets fly on the 320-px stage; robots from the right appear at x 496, beyond the widest screen's right edge (480),
and glide in along their entry line to x 336 where their pattern starts, so they reach the stage at the same moment at
every width (a wide screen shows them coming sooner, never earlier or nearer); robots from behind appear at x -96,
beyond the widest left edge (-80); turrets are placed on the walls at x 496; the corgis' shots reach x 336 and end
there: over their last 16 px they shrink to a tiny star, then to dust (a wide screen sees them fade; at 320 it is
just off the edge); robots fire only from the stage. The warning arrow for robots from behind is at the screen's
left edge.
The extra width shows more of the stars, clouds, planets and walls. A state saved at one width loads at another.
Tests: the rules test checks the spawns, the reach and the fire over a whole loop; the touch test plays the same drag
and the same bot run at 320, 427, 520 and 560 and compares them.

**Tall screen** (docs/spec.md "Tall screen", the zoomed recipe): a phone held upright. `rs_screen_zoom(ZOOM_H)` (600
lines, game.h) first in `game_init`: the app scales the 600 lines to the display's height and crops the sides, 256
columns on a 1080x2520 phone, 276 on a 1080x2340 one, 300 on a 720x1440 one (the rockets and the robots 4.2x on a
1080x2520 phone; v1's 320 x 720 strip: 3.4x). Layout only: the rules, the 320-px stage, the spawns and the reach are
those of every width. The stage is centred (180 lines above and below it at 600).
- **The camera** (draw.c `camera`): in play the view pans over the stage's 320 columns with more room ahead of the
  corgis (the robots come from the right): one corgi about 30% from the view's left edge, several their middle at 40%,
  every corgi at least 24 px from the edges (the leftmost first), a boss flying in or fighting kept in view; eased (3
  px a frame at most; `cam_x` is in the states).
- **Fairness**: a robot on the stage (the 320 columns would show it) but outside the columns shown gets the warning
  arrow at that edge, at its height (blinking; `edge_marks`); the arrow for the robots from behind is at the view's
  left edge.
- **The HUD at the screen's top**: the scores as sprites at `rs_screen_top()` + 8 in the columns shown (1 player
  centred; 2-4 with each corgi's icon, P1 P2 side by side, P3 P4 under them; no corner tags); the UI layer is scrolled
  so its centred texts (the section's name, BIG ROBOT!, the power-ups, PAUSED) and the boss bar sit in the middle of
  the columns shown (a big text wider than them uses the small letters). The title and the game-over panels stay zoomed too (one
  look in portrait), the view centred, their text in the columns shown (games/common/src/house_tall.h).
- **Past the stage** (BG2-BG4 maps 128 rows tall on a tall screen: the stage's rows, then the rows below it from row 30
  down and the rows above it up from row 127, the wrap putting them in place without a raster): the walls go on in a
  band of rock, metal or ice 3-6 tiles deep, rolling, its outer side a lit surface (`walls_tall`); in the nebula (and
  the title) a bank of clouds (art/tiles/bank.png, the clouds' palette, its wisps reaching into the stage's first and
  last rows). Both are drawn in front of the robots past the stage (the map's priority bit): a swoop comes out from
  behind them. The clouds and the stars go on from other parts of their panoramas, the clouds without their planets
  and moon (art/tiles/clouds_bare.png: no copy of what the stage shows); a big banded planet rises at the screen's
  bottom (art/tiles/planet.png on BG4's 1024-px map, its own palette 4, scrolling with the stars: never twice in view;
  the raster anchors its rows at the screen's bottom). The backdrop's gradient goes on, darker toward the screen's
  edges; the UI is hidden there; line 0 puts the stage's settings back.
- Landscape and 4:3 are the same pixels (the maps 32 rows, the extra tiles not loaded; checked at 320x240, 520x240 and
  2340x1080). The touch drag is relative: nothing changes.

## Screens (the house flow: no "get ready", the title is the only menu)
title (the logo, the corgi's rocket bobbing with a puppy pod and a bark flying ahead, a ladybug, the player slots,
PRESS A TO FLY with the A glyph, BEST, the join line, the copyright line) -> P1 presses A (or B, X, Y, Start, the
d-pad; a tap) -> the corgis take off ("arf!"), PASTEL NEBULA -> play (the score at the top, or in the corners for 3-4;
the boss bar at the bottom; Select, or Start, pauses) -> the last corgi's bonk -> game over (the banner, the panel
sliding up: score, best / new best, the medal, the stage reached; 2-4 players: the ranking by score with medals)
-> one press (or a tap): back to the title (the same players start again with A).

## Controls
| Input | Action |
|---|---|
| d-pad | fly the rocket (8 directions) |
| (nothing) | it fires by itself, all the time |
| A (or B, X, Y, Start, the d-pad) on the title | start |
| Select (or Start in a run) | pause / resume |
| pads 2-4: A or Start on the title | join (B leaves) |
| A, Start or Select on the game-over panel | back to the title |

### Touch (docs/spec.md "Touch")
The game reads the touches itself (`rs_game.touch_native = 1`): the phone apps pass it the fingers and draw no buttons.
Touch is player 1, alongside the pad (players 2-4 join with pads).

| Touch | Action |
|---|---|
| a finger dragged anywhere | **relative drag**: the rocket moves by the finger's movement, 1:1 in screen pixels (at most 8 px a frame, clamped to the stage), so the finger never hides it; it fires by itself all along |
| a finger put down again | a new drag: no jump, the rocket goes on from where it is |
| a tap (a finger lifted) on the title | start |
| a tap on the game-over panel | back to the title (after the same lock as A; a finger left down from the run does not count) |
| the app's pause button | pause / resume (it presses Start); a drag while paused does nothing, and no jump when it resumes |

The finger's last point is kept in a registered state (main.c touch_*): a load forgets the fingers (docs/spec.md), and
a finger still down starts a new drag. The movement is the same in screen and stage pixels: the same drag plays the
same at every width. `tests/touch_test.sh` (in the check) plays scripted fingers through the headless runner.

## Up to four players
Pads 2-4 join with A on the title (B leaves). Everyone flies at once, each with their own rocket, shots, power-ups and
score (the rockets pass through each other; whoever touches a capsule gets it); the robots aim at the nearest corgi;
the bosses take 50% more hits per extra player. A corgi hit is out (its own hit); the others fly on; the run ends when
all are out. The rockets: red, blue, green and purple paint, with a red-tan, sable, black (tricolour) and fawn corgi
(tools/build_assets.py swaps make_art.py's ACC and FUR ramps; the hull, the eyes and the glass stay); their shots
take their paint. The HUD shows each score (3-4 players in the corners); the results rank everyone by score (the
same score, the same place) with medals.

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation. The rocket: a white
hull with the player's paint (nose cone, stripe, fins), a porthole, a flickering flame, the corgi in its open cockpit
(big pointed ears, a white blaze and muzzle, the house eye); its nose tilts up or down as it climbs or dives; X eyes
and a dent when hit. The puppy pods: a corgi puppy in a glass bubble, ears flopping. The shield: a glass bubble rim.
The player icon (title slots, results): the corgi's head with a collar in its rocket's paint. The shots: a
little ")" bark, a four-point star (the spread: the robots' shots are round), a star beam, a spinning dog bone; the
walrus' snowball is outlined (it shows on the pale comet clouds). The robots: a wind-up ladybug (its
key turning), a propeller drone with one big eye, a little UFO with a robot in its dome, a dasher fish (a key while
it winds up, a flame when it dashes), a drone carrying a gift box, a dome turret aiming (V-flipped on a ceiling), an
asteroid, an icy comet, a robo-turtle; gold groups are the same robots in gold. The bosses (64x48, two frames): the
clockwork ladybird (its key on its back, wings peeking out), the rock crab (two claws opening), the owl sentry (big
eyes, wings out, a blink), the snow walrus (goggles, tusks, its mouth opening). The capsules: a glass orb with the
power's colour and icon (a fan of shots, a beam, a bone, chevrons, a bubble, a puppy). The pops: a sparkle burst, then
sparkles and nuts and bolts. The walls: rock with a lit lip, metal plates with hazard stripes and vent lights, ice
with a snow cap and icicles. The title logo: COSMO in starry violet, CORGI in corgi orange, stars twinkling round it in turn.

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects (main.c sound_init): a soft "pew" for the
barks (every other volley, quietly: they never stop), a bubble pop, a sparkle and a bolt's clink for a robot, a bonk
with a wobble for a hit, a tiny plip for a robot's shot, E-G-C for a capsule, a "blup" for the shield, a clonk on a
boss and a big puff when it pops, a cute two-tone alarm for BIG ROBOT!, a zip for a dasher, a whoosh for a bone, and
the corgi's "arf!" at the take-off and over a popped boss; the house sounds for the menus, a section (the ding), a gold
group (the ding) and the medals and the game over. The lengths keep the samples under the 64 KiB guideline with the
house set.

Music: "Star Fetch" (tools/make_music.py): A major, speed 5 (150 BPM), A A' B A' C (about 32 s): the house pluck
streaming in arpeggios, a bass bouncing octaves on the off-beats, the kit's drums in a happy gallop, the breathy lead
with a bright call that leaps up a sixth like a corgi jumping for a ball; A' answers it an octave up with the pluck
doubling, B climbs (D E C#m F#m), C brings it home with a snare roll. Two alternatives (tools/make_music_alt.py ->
music-alt/alt1.mod, alt2.mod; the owner picks; previews in docs/music-alternatives/cosmocorgi/): "Nebula Groove" (a
bouncy space disco in E dorian) and "Comet Galop" (a fast chiptune galop in C major). To switch, call
make_music_alt.py OUTDIR N from tools/build_assets.py instead of make_music.py.

## Tests
`make cosmocorgi-check`: the rules alone (tests/test_rules.c, 61 checks: the start, auto-fire, the d-pad and its
diagonals, the touch drag (1:1, the 8-px cap, the stage), hits, the shield and its blink, touching a robot and a wall,
the power-up levels and MAX POWER!, the spread's volleys, the pods following the path and barking, the piercing beam,
a homing bone finding a robot off its line, the sine waves and the loop, a gold group's capsule and bonus, the walls'
gap and the turrets on them, the boss (untouchable flying in, its strength, the bonus, the next section, the next loop
faster, more hits for more players, flying away after 90 s), the widescreen spawns over a whole loop with two bots
(every robot appears beyond the widest screen, no shot past the reach, no robot fires from outside the stage),
the ramp (robots per minute of waves rising across the nebula, from section to section and by 25% at least in
loop 2, a gentle first 20 s), determinism and the bot), the libretro loader, the smoke tests (flow, start inputs, flying, auto-fire, the game over
and back to the title, pause, joining and leaving, 4 bots, a boss with 4 powered-up corgis, no strict-mode warning,
save RAM, the bot through the first boss, determinism), the touch tests, the UI screens (tests/ui_test.sh: the title
with 0-3 players joined, play, the walls, pause, 4 players, a boss, the game over with a medal, the results), the save
states (the title's lobby, a run, the walls, a boss, 2 players with a finger dragging, 4 players, paused, the game over,
the results) and their audit, at 320, 427 and 520 columns (the widescreen passes run the flow, the screens and four
states), states across widths, and the 22 languages (tests/i18n_runs.txt).
