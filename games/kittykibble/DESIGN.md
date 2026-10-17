# Kitty Kibble: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/kittykibble/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

## Pitch
A hungry kitten eats every kibble of a room while four robot vacuums try to tidy it up. Dash rushes straight at it,
Sneak cuts it off, Patrol guards its corner of the house and Wobble wanders about and backs off when it gets close. A
ball of yarn turns the tables for a few seconds: the vacuums turn blue and slow, and the kitten can catch them (their
eyes zip back to the charging dock) for points that double with each catch. Clean the kitchen, the living room, the
bedroom, the garden and the attic, each a little faster, then again. One catch and the run is over; one more try for a
better medal. Nothing is hurt: a caught kitten just spins, dizzy, among little stars.

## Legal and originality
- The maze chase is the genre of **Pac-Man** (Namco, 1980, proprietary). We use only its **mechanics**, which are not
  protected: eat every dot of a maze while enemies chase you; a few power pellets that make the enemies vulnerable for
  a while (they turn round, slow down and can be eaten for doubling points, then go back to their pen); enemies with
  different ways of choosing their target, alternating between chasing and going back to their corners; a pen in the
  middle they leave in turn; side tunnels that wrap and slow the enemies; a bonus item twice a level; levels that get
  faster; turning with the stick held or pressed before a junction.
- Nothing else is taken from it: no name, character, art, colour scheme, sound, music, maze layout, text or number
  measured from it; we did not read its code or anyone's clone. The theme (a kitten, robot vacuums, a house's rooms,
  kibble, balls of yarn, a fish treat, the charging dock, the pet doors), the five mazes (drawn by hand for this game
  on our own grid: corridors one cell wide, furniture two cells thick, the dock and its gate, one or two rows of pet
  doors), the vacuums' personalities and their rules (Patrol's post, Wobble's random walk and shyness), the four
  players racing in one maze, one catch ending the run, the touch swipe, the medals, the sounds and the music are ours.
  The code is our own (C11 on the RetroStone SDK, integer fixed point), written from this design.
- Every number below is ours, tuned with our own bots for our 320x240 screen and a fair, cute game.

## Rules
The house is a maze of 27 x 24 cells of 8 px (216 x 192 px, the stage's middle): corridors one cell wide between
furniture two cells thick, the vacuums' charging dock in the middle (its little gate on top lets only them through),
and one or two rows of pet doors on the sides: the row wraps round to the other side (the vacuums slow down in them,
the kitten does not). Every corridor cell holds a kibble, except the kittens' start cells, the fish treat's spot and the
pet doors' mouths; four balls of yarn lie near the corners.

The kitten runs along the corridors and turns at junctions. The d-pad gives the way: held, the kitten takes it at the
next junction where it is open; pressed and let go, the kitten still takes it if it reaches such a junction within
TURN_BUFFER frames (a turn pressed just before a junction is never lost); a turn is taken up to CORNER_PX before or
after the junction's centre (snapped onto it: the corners feel round); turning round is always at once; with a
diagonal held, the way across wins at the next junction. Without input it runs on until a wall and waits there. Each
kibble stops it for a frame (eating), so a vacuum right behind gains on a kitten that eats.

The four vacuums leave the dock in turn (Dash starts outside) and roam: at each cell they take the open way (never
back the way they came) that brings them closest to their target:
- **Dash** (red, a lightning bolt) heads straight for the kitten's cell.
- **Sneak** (violet, half-closed eyes) aims SNEAK_AHEAD cells ahead of where the kitten faces: it cuts it off.
- **Patrol** (teal, a siren) guards its post in the bottom-left corner: it chases a kitten that comes within
  PATROL_RANGE cells of the post, and otherwise patrols round it.
- **Wobble** (yellow, a googly eye and a springy antenna) wanders at random at each junction, and backs off to its
  corner when a kitten is within WOBBLE_SHY cells.
With 2-4 players each vacuum hunts the nearest kitten still playing. The vacuums alternate between chasing and going
back to their corners (scatter: a short break, rooms 1-2 longer than later); they all turn round when it changes.

A **ball of yarn** (5 points) scares every vacuum outside its home trip for yarn time (7 s in room 1, 0.75 s less each
room, at least 2.5 s): they turn round, turn blue, slow down, wander at random and flash in the last 2 s. A kitten that
touches a scared vacuum catches it: 20 points, then 40, 80 and 160 for the next ones of the same yarn; everything
stops half a second, the points show, and the vacuum's eyes zip home to the dock (fast), where it recharges 1.5 s and
comes out again, not scared. The **fish treat** appears on the room's fish spot when 30% and again when 70% of the
kibble is eaten, for 9 s: 25 points in room 1, then 50, 75, 100, 150, 200.

**One catch ends the run**, as in the other house games: a vacuum (not scared) that touches the kitten catches it
(everything stops half a second, a shake, the kitten spins dizzy, then it is out). With 2-4 players a caught kitten is
out and the others play on (the hit-stop is shorter); the run ends when all are out. When the last kibble (and ball of
yarn) is eaten, the room is clean: the furniture flashes for 2 s, then the next room (its name for 2 s while nothing
moves: ROOM 2 / LIVING ROOM), the kittens still playing back on their start cells. The rooms come round again after the
attic, and each room is a little faster: the kitten +3% a room, the vacuums +7% (both up to room 7), the vacuums leave
the dock sooner and yarn time is shorter. The score is the points; the room reached shows on the game-over panel.

Every number is in src/game.h (the tuning table):

| Rule | Value | Where |
|---|---|---|
| the maze | 27 x 24 cells of 8 px at stage (52, 28) | MAZE_W, MAZE_H, CELL, MAZE_X, MAZE_Y |
| the kitten | 1.15 px/frame in room 1, +3% a room (rooms 2-7), +8% during yarn time; 1 frame to eat a kibble, 3 a ball of yarn | KIT_SPEED, KIT_SPEED_UP, KIT_YARN_UP, KIBBLE_PAUSE, YARN_PAUSE |
| turning | a turn kept 20 frames; taken up to 3 px before or after a junction | TURN_BUFFER, CORNER_PX |
| the vacuums | 0.85 px/frame in room 1, +7% a room (rooms 2-7); 55% in the tunnels and when scared; 3 px/frame zipping home; 0.75 leaving the dock | VAC_SPEED, VAC_SPEED_UP, VAC_TUNNEL, VAC_SCARED, VAC_HOME, VAC_LEAVE |
| the dock | out at 0, 1.5, 6 and 11 s into room 1 (12% sooner each room, rooms 2-6); a caught vacuum recharges 1.5 s | DOCK_RELEASE, DOCK_FASTER, DOCK_REGEN |
| the personalities | Sneak 4 cells ahead; Patrol's post (6, 19), range 9 cells; Wobble shy within 6 cells; the scatter corners | SNEAK_AHEAD, PATROL_*, WOBBLE_SHY, HOME_CORNERS |
| scatter / chase | rooms 1-2: 7 s / 20 s / 6 s / 20 s / 5 s / 25 s / 4 s / chase; rooms 3+: 5 / 22 / 4 / 24 / 3 / 30 / 2 / chase (the clock stops in yarn time) | MODES_EASY, MODES_HARD |
| yarn | 7 s in room 1, 0.75 s less each room, at least 2.5 s; the last 2 s flash | YARN_FRAMES, YARN_LESS, YARN_MIN, YARN_FLASH |
| meeting | closer than 6 px on both axes; a catch stops everything 30 frames | CATCH_PX, CATCH_FREEZE |
| points | kibble 1, yarn 5, vacuums 20 / 40 / 80 / 160, the fish 25 / 50 / 75 / 100 / 150 / 200 by room | KIBBLE_POINTS, YARN_POINTS, CATCH_POINTS, FISH_POINTS |
| the fish treat | at 30% and 70% of the kibble, for 9 s | FISH_AT, FISH_FRAMES |
| the flow | the room's name 2 s; the clean room 2 s; the hit-stop 30 frames (12 with others playing); the dizzy spin 70 frames | INTRO_FRAMES, CLEAR_FRAMES, HIT_FREEZE, HIT_FREEZE_MP, CAUGHT_FRAMES |
| touch | a swipe step of 12 px, mostly along one axis | TOUCH_DRAG |
| medals (1 player) | bronze 200, silver 600, gold 1500, pearl 3000 | MEDAL_SCORES |

**The medals** were tuned with the human-like bot (`botskill=E`, below), 6 runs each (seeds 1-6, 20000 frames at
most; room 1 alone is worth about 240 for its kibble and yarn, plus the vacuums caught and the fish): the worst player
(E = 10) scores 87..351, 4 runs in 6 past 200; a beginner (E = 8) 113..322, 5 in 6 past 200 (bronze: most of the
kitchen eaten); E = 6 273..521; a good player (E = 4) 502..2191, most past 600 (silver: the kitchen clean, the yarn
used well) and 2 in 6 past 1500 (gold: room 4 or 5); E = 2 672..1294, and two runs still going after 20000 frames.
Pearl (3000) takes a long, clean game through the whole house.

**The bot** (`--opt bot=N`: tests, screenshots and card frames) plays from the world. It works out when each corridor
cell can be reached by the kitten and by the first dangerous vacuum (a scared one only once its fright is over, one in
the dock once it is out), keeps the cells the kitten reaches well first, and among them heads for the nearest kibble
(a ball of yarn when a vacuum is near, a scared vacuum it can still catch, the fish treat) that leaves enough safe
corridor beyond it not to be cornered; with danger near, it plays the world on 2.5 s ahead (a copy) for the way it
chose, and changes its mind when the kitten would be cornered. It turns round at once when a vacuum comes the other
way. `botskill=E` (1..10) is a human-like player: it reacts a few frames late, sees less far, keeps a thinner margin,
looks ahead less (not at all from E = 7), slips now and then and, from E = 6, does not play the yarn cleverly. `botstop=S` makes it run into a vacuum once its score
reaches S (the tests' game over).

## Screen and layers
320x240 at 60 Hz, fixed step. The house layout (docs/art-direction.md):

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | (hidden by the room) | - |
| BG3 | the room around the maze, one picture per room: the floor (kitchen checks, honey planks, a peach carpet, gravel paths in a lawn, dusty blue boards), the back wall at the top (windows, a clock, pictures, shelves; the garden's fence and sky, the attic's roof and round windows), furniture seen from above with soft shadows in the margins (the kitten's bowls, chairs, a table, armchairs, a bed, a bedside lamp, toy boxes, a tree, a pond, flower beds, cardboard boxes, a trunk), the pet doors | fixed (shakes on a catch, the sprites with it) |
| BG2 | the maze: the walls (each room's furniture: tiled kitchen counters, tufted plum sofas, quilted pillows, hedges, taped cardboard boxes; 47 rounded shapes chosen by a cell's 8 neighbours, lit from the top-left with a dark front face), the kibble, the gate and the dock's charging floor; rebuilt from the world each frame | the maze's 4-px offset (shakes) |
| BG1 | the house UI kit: the logo, the title's lines, the room's name, ROOM CLEAR!, P2 CAUGHT! (a little panel in the back wall's band), the panels, PAUSED | fixed |
| OBJ | the kittens (P1 OBJ 0; P2-P4 OBJ 1, 4, 5: fur and collar recoloured), the yarn, the fish, points, dust, the eyes zipping home (OBJ 2), the kit's digits, glyphs and medals (OBJ 3), the vacuums (OBJ 6; scared and flashing: OBJ 7) | |

Window 1 hides the sprites outside the maze's columns on its lines (the raster callback sets it per line): the actors
go out through the pet doors and come back on the other side. VRAM: the UI and its glyph pool 0..767, the maze at 1024
(47 wall shapes + 4), the room at 1280 (up to 720 tiles: 180-457 by room, loaded with its palettes when a room
begins), the sprites at 2048 (202 tiles, then the kit's).

**Widescreen** (docs/spec.md "Widescreen"): the game opts in (`max_width`, `WIDESCREEN_kittykibble = 1`). The maze and
the play are the same at every width (the stage's 320 columns are centred); a wider screen (427 for 16:9, 520 for
phones) shows more of the room around it: more floor and furniture (a table, armchairs, a bed, a tree, boxes), and on
the widest the room's side walls. The rooms are "stage panoramas" (576 px from stage x -128) so no width shows them
twice; each prop lies wholly inside or wholly outside each width's view (stage x 0..319, -53..373, -100..419, its
shadow included), so no screen edge cuts a piece of furniture. A state saved at one width loads at another.

## Screens (the house flow: no "get ready", the title is the only menu)
title (the kitchen, the logo, the kitten bobbing with its ball of yarn, the four vacuums gliding by, the player slots,
PRESS A TO PLAY with the A glyph, BEST, the join line, the copyright line) -> P1 presses A (or Start, the d-pad; a tap)
-> ROOM 1 / KITCHEN (2 s) -> play (the score at the top; Select, or Start, pauses) -> caught: a shake, the kitten spins
-> game over (the banner, the panel sliding up: score, best / new best, the medal, the room reached; 2-4 players: the
ranking by score with medals) -> one press (or a tap): back to the title (the same players start again with A).

## Controls
| Input | Action |
|---|---|
| d-pad | steer the kitten (held: at the next open junction; pressed and let go: kept 20 frames) |
| A (or Start, the d-pad) on the title | start |
| Select (or Start in a run) | pause / resume |
| pads 2-4: A or Start on the title | join (B leaves) |
| A, Start or Select on the game-over panel | back to the title |

### Touch (docs/spec.md "Touch")
The game reads the touches itself (`rs_game.touch_native = 1`): the phone apps pass it the fingers and draw no
buttons. Touch is player 1, alongside the pad (players 2-4 join with pads).

| Touch | Action |
|---|---|
| a swipe (12 px, mostly along one axis) | the kitten's next way: exactly a d-pad press (kept for the next junction, or at once to turn round) |
| a finger that stays down and swipes on | each new step of 12 px turns again (measured from where the last one was): one finger can steer through a whole room |
| a tap or a resting finger in play | nothing |
| a tap (a finger lifted) on the title | start |
| a tap on the game-over panel | back to the title (after the same lock as A; a finger left down from the run does not count) |
| the app's pause button | pause / resume (it presses Start); the fingers do nothing while paused |

The swipe's anchor (where the last step was measured from) is in the world (registered state): a load forgets the
fingers, one still down starts again. The swipe works the same at every width (only the finger's movement counts).
`tests/touch_test.sh` (in the check, at every width) plays scripted fingers through the headless runner and compares
with the pad's runs.

## Up to four players
Pads 2-4 join with A on the title (B leaves). Everyone plays in the same maze at once, racing for the kibble (each
kibble goes to the kitten that eats it); the vacuums hunt the nearest kitten; a ball of yarn scares them for everyone,
and each kitten's catches double on their own. A caught kitten is out (P2 CAUGHT!), the others play on, nothing pauses
for long; the room ends when all its kibble is eaten (the kittens still in play go on together); the run ends when all
are out. The kittens: ginger (P1), silver, snow white and chocolate, each with a collar (red, blue, green, purple:
tools/build_assets.py swaps make_art.py's FUR and COLLAR). The HUD shows each score (3-4 players in the corners, the
bottom ones under the maze); the results rank everyone by score (the same score, the same place) with medals.

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation. The kitten (16x16): a
round ginger tabby with big ears, the house eyes, a pink nose, cream paws and muzzle, a collar with a little bell, a
curled tail; it walks right (flipped for left), down (its face) and up (its back, the tail swishing), munches (the
mouth open) as it eats, and spins dizzy with X eyes when caught. The vacuums (16x16): round robots seen from above at
an angle, a shell lit from the top-left with a darker rim, a grey bumper, a dark visor with two bright eyes looking
where they go, spinning side brushes; Dash red with a lightning bolt and frowning eyes, Sneak violet with half-closed
eyes and a ninja band's knot, Patrol teal with a blinking siren, Wobble yellow with a googly eye and a springy
antenna; scared: blue, trembling, with big worried eyes and a wobbly mouth, flashing white with a red face at the end.
A caught vacuum's eyes zip home with speed lines. The ball of yarn (pink, its wraps and a loose end) pulses; the fish
treat (silver-blue with a golden fin) flaps its tail. The kibble: little brown biscuits, round and fish-shaped, side by
side. The rooms and the walls as above. The title logo: KITTY in ginger, KIBBLE in biscuit brown.

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects (main.c sound_init): a little "nom" for each
kibble (two pitches in turn), a springy boing for the yarn, the scared vacuums' worried "wub" twice a second during
yarn time, a pop and a zip for a vacuum caught and a whoosh as its eyes fly home, a bleep-bloop as a vacuum leaves its
dock, G6 C7 E7 for the fish treat (the house ding when it appears), a sad falling "mew" and a bump when the kitten is
caught, C6 E6 G6 C7 for a clean room; the house sounds for the menus, the medals and the game over. The lengths keep
the samples under the 64 KiB guideline with the house set.

Music: "Kibble Swing" (tools/make_music.py): a playful little jazz tune in F, swung eighths (125 BPM), A A B A (61 s):
the house pluck comping in a Charleston rhythm, a walking bass, a ride cymbal, the breathy lead tiptoeing like a
curious kitten (a chromatic slide down on D7, blue notes on C7), the bridge climbing through Bb6, B diminished, F and D7
before it turns home. Two alternatives (tools/make_music_alt.py -> music-alt/alt1.mod, alt2.mod; previews in
docs/music-alternatives/kittykibble/): "Whisker Waltz" (a lilting jazz waltz in Bb) and "Catnip Boogie" (a bouncy
12-bar blues shuffle in C with drums). To switch, call make_music_alt.py OUTDIR N from tools/build_assets.py instead
of make_music.py.

## Tests
`make kittykibble-check`: the rules alone (tests/test_rules.c: the five mazes (symmetric, no dead end, all connected,
the counts, the gate), the start and the intro, moving and the walls, turning round, the turn buffer (taken at the
junction; forgotten after its frames; held; just after the centre; a diagonal), the tunnels (the wrap, the vacuums'
slowdown), kibble and the fish treat, the clean room and the next one, each vacuum's target (Dash, Sneak and the way the
kitten faces, Patrol near and far from its post, Wobble far and close, scatter), the dock's releases, the yarn (scared,
turning round, slower, the doubling catches, home and out again calm), one catch ending the run (the hit-stop, the
spin, out; with 2 players only that kitten), the touch swipe (steps, on without lifting, diagonals, a swipe before a
junction), determinism and the bot clearing the kitchen), the libretro loader, the smoke tests (flow, start inputs,
the intro, turning, the turn buffer, eating, the game over and back to the title, pause, save RAM, joining and
leaving, 4 players, the five rooms, no strict-mode warning, the bot, determinism with 1 and 4 players), the touch
tests, the UI screens (tests/ui_test.sh: the title with 1-4 players, the intro, play, pause, 4 players, yarn time,
the vacuums in their own colours in a calm moment and blue in yarn time (pixels of the pictures),
the game over with a medal, the results), the save states (the title's lobby, a run, yarn time, another room, 2 and 4
players, a finger swiping, paused, the game over, the results) and their audit, all at 320, 427 and 520 columns,
states across widths, and the 22 languages (tests/i18n_runs.txt).
