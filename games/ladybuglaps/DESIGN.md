# Ladybug Laps: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/ladybuglaps/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

## Pitch
Tiny ladybugs race round the garden table and through the garden, seen from above: round teacups and sugar cubes,
across spilt jam, through puddles, under the leaves, over a spoon or a twig with a hop of their wings. Four racers,
always: you (and up to three friends) and the bots. The screen follows the leader: fall too far behind and you are
**left behind**: the leader takes your point and you are put back behind it. Ten races, the five tracks and then the
same five the other way round; finish in the top 3 to go on.

## Legal and originality
- The mechanics come from **Micro Machines** (Codemasters, 1991, and its sequels), which is proprietary. We use only
  its **mechanics**, which are not protected: tiny vehicles racing on everyday surfaces seen from above, the camera
  following the leader in a multiplayer race, a racer who leaves the screen losing the point and being put back next
  to the leader, falling off a table, races won by laps or by points.
- Nothing else is taken from it: no name, logo, vehicle, track layout, art, colour scheme, sound, music, text or
  number measured from it; we did not look at its code or anyone's clone. The ladybugs, the five tracks and their
  layouts (designed on our own grid of 64-px pieces), the scenery, the points table, the bots and their names, the
  championship, the touch controls, the medals, the sounds and the music are ours. The code is our own (C11 on the
  RetroStone SDK, integer fixed point), written from this design.
- Every number below is ours, tuned with our own bots for our 320x240 screen.

## The race
- **Four racers**, always: the players (1-4) and bots for the other places. Colours: red (P1), yellow (P2), blue
  (P3), green (P4); the bots are RUBY, SUNNY, BLUEBELL and BASIL (never translated), in the colours left.
- **Three laps.** The start: the lights (three red lamps, 2 s), GO. The grid is 2 x 2 behind the line.
- **The camera follows the leader**, the racer furthest along the track, looking ahead of it (its speed x 22
  frames, at most 56 px), easing at most 5 px a frame (a new leader: a smooth pan). Play.c owns it: the rules never
  ask the screen's width.
- **Left behind.** The race zone is a fixed 320 x 240 box round the camera, **the same at every screen width**
  (docs/spec.md "Widescreen": a wider screen shows more of the track, and on it two dotted lines mark the zone's
  sides). A racer whose centre leaves it by more than 4 px (after the first 2 s of a race) is left behind: **the
  leader takes a point, and the one left behind loses one** (never under 0); it is gone half a second, then put back
  3 waypoints behind the leader, in its own lane, at 85% of the leader's speed, blinking for a second (it goes
  through the others meanwhile). **The ground it is carried does not count for its place**: the places (the
  standings, the finish) are by the distance each racer actually drove, so being put back next to the leader keeps
  you in the race (Micro Machines' way) but never moves you up (a ladybug that does not drive at all finishes last).
  A "!" blinks over a racer near the zone's edge.
- **Off the table** (the two table tracks): over the edge a ladybug falls (it shrinks away), and 0.7 s later it is
  back on the track where it fell, still, blinking. If the race went on without it, it is soon left behind too.
- **The checkpoints** are the waypoints of the centre line (16 px apart): a racer only moves on to one of the next
  five (or back two), so a shortcut over the table gains nothing, and the lap counts only when every waypoint was
  passed in order. Backwards over the line takes the lap back.
- **The finish**: when the leader completes the third lap, the race ends for everyone; the places at that moment
  (by how far along the track) score **3, 2, 1 and 0** more points. **Most points wins the race**; equal points: the
  better place. The flag waves for 2.5 s while everyone drives on, then the results.
- **1 player: the championship.** Ten races: the five tracks, then the five again driven the other way round. A
  **top-3 place goes on** to the next race; 4th is the end (the game-over panel). The **score** is the points of all
  the races; winning the tenth is CHAMPION! (the note on the panel). The bots get faster and steadier race by race.
- **2-4 players: a cup of three races** on three tracks in a row (the first one changes from cup to cup), the bots at
  a middle level; the results rank the players by their points (the house ranking with medals).

### The point duel
The design brief allowed "top 3, or win the point duel". All the races here are the same Micro Machines race (laps,
the leader's camera, points for the leader); there is no separate duel race. An owner's decision (below).

## Handling: the ladybug turns toward the direction held
The camera never turns (Micro Machines' top-down view), so **the d-pad gives the direction the ladybug turns to**
(at 12/1024 of a turn a frame, half that at a standstill): Right drives east whatever the ladybug's heading, Down
south... (the shorter way round; diagonals with two directions). Let go and it keeps its heading. A is the gas, B
the brake, and from a standstill B backs up (the nose still turns toward the d-pad). Why not "left/right turn the
ladybug" (Micro Machines' own)? On a screen that never turns, rotating steering is reversed whenever the car comes
down the screen, and children (and the touch stick) expect the direction they push; the same control works for the
stick: the ladybug turns toward where the finger is dragged. Tight lines are still possible: a tap of a direction
turns a little.

The physics (play.c, Q8 pixels a frame): the velocity is split into its forward and sideways parts. The gas adds
10/256 px a frame per frame up to the surface's top speed (2.75 px a frame on the band), the brake takes 24, no
pedal 3; reversing up to 1.25. The **grip** eats a share of the sideways part each frame: on the band 60/256, so a
hard turn at speed slides out a little (the **drift**, the tests measure it); on a puddle 5/256: a long slide.

| Surface | Top speed | Accel | Grip | Drag | |
|---|---|---|---|---|---|
| the band (the track) | 704 (2.75 px/f) | 100% | 60 | 0 | |
| off the band (wood, grass, soil, slabs) | 420 | 70% | 80 | 5 | "leaving the track slows you down" |
| jam, glue, juice, mud | 230 | 45% | 160 | 12 | sticky |
| a puddle, spilt water | 704 | 55% | 5 | 0 | slippery |
| a bump (spoon, pencil, twig) | - | | | | at more than 1.17 px/f: a **hop** (it opens its wings, flies straight, no grip) |
| off the table | | | | | a fall |

**Bumps**: two ladybugs bounce off each other (restitution 0.59, pushed apart); objects (cups, the teapot, sugar
cubes, crayons, pots, pumpkins...) and the garden's frame stop them with a bounce (0.43). A hop flies over the
others.

## The bots: smart but fair
play.c bot_think, the table BOT_TABLE in game.h: each bot drives the **racing line** (the centre line shifted toward
the inside of the turn ahead, up to 14 px), aiming 3 waypoints ahead (more when fast), with
- **some error**: a lateral offset of a few px, a new one every 10-16 frames (its reactions);
- **wider turns**: a personality pushes its line outwards in the turns (SUNNY the most);
- **braking for sharp corners**: it reads the sharpest heading change over the next waypoints and slows to a safe
  speed (a sweeping corner at full speed, a tight one at about 1.6 px a frame), taking it harder or more carefully
  by personality (corner %); it lifts off when badly aimed;
- **bumps**: nothing special; it backs out when stuck against something;
- a **mild rubber band** (1 player only): a bot more than 260 px ahead of you eases off by 3% of its top speed, one
  as far behind tries 3% harder;
- **the difficulty**: their top speed goes from 81-84% of yours (race 1) up to 94-97% (race 10), and their line's
  error shrinks.

| Bot | Top speed % (base) | Corner % | Error px | Wider | Reacts every |
|---|---|---|---|---|---|
| RUBY (red) | 97 | 100 | 6 | 2 | 10 frames |
| SUNNY (yellow) | 95 | 108 | 8 | 6 | 14 |
| BLUEBELL (blue) | 98 | 94 | 5 | 4 | 12 |
| BASIL (green) | 96 | 104 | 7 | 3 | 16 |

The **player bot** (`--opt bot=1`, also P2-P4 with bot=N) uses the same brain at 100% with a sharper line; with
`--opt botskill=E` (1..10) it becomes a human-like driver (slower, later reactions, more error, timid corners).

## The tracks
Each track is a grid of 64-px pieces (src/tracks.c): straights, tight corners (radius 32, round a cell's corner),
sweeping corners (radius 96, round a 2 x 2 block) and crossings; the band is 48 px wide (four ladybugs abreast).
tools/make_art.py draws each track whole from the same file, and play.c traces the same pieces into its centre line.

| # | Track | World | Lap | Surface, edges | Scenery and surfaces |
|---|---|---|---|---|---|
| 1 | Tea Time | 1280 x 896, a table | 3656 px | a linen table runner with blue stitching on light wood planks; off the table: the lawn far below | the teapot, teacups on saucers, sugar cubes on the band's edges, biscuits, jam (sticky, twice), a spoon to hop, a chicane, a U-bend, leaves of the tree over a corner |
| 2 | Crayon Desk | 1216 x 960, a desk | 2826 px | a strip of paper with red crayon edges on a pale green desk; the carpet below | a **figure of eight** (a crossing), crayons in four colours, erasers, paint jars with a brush, glue (sticky), spilt water (slippery), a pencil to hop |
| 3 | Picnic | 1536 x 960, walled | 3148 px | a sandy path in the lawn, a hedge round it | the gingham blanket, the basket, a sandwich, apples, juice cups, spilt juice (sticky), a twig to hop, stones, the tree's leaves over the track |
| 4 | Veggie Patch | 1088 x 640, walled | 3396 px | a wood-chip path between the rows of a raised bed (soil) | zig-zag hairpins between lettuces, carrots (their fronds over the track), pumpkins, mud (sticky), a puddle, a twig, a watering can |
| 5 | Rainy Patio | 1152 x 896, walled | 2568 px | a sandstone path on wet flagstones, a brick wall round it | a big puddle across the straight (slippery), flower pots, a snail on the track, stones, fallen leaves, a twig, a watering can |

Driven the other way round (races 6-10), the same line from the same start, reversed.

### Tuning (every number in src/game.h)
| Rule | Value | Where |
|---|---|---|
| the pieces | 64 px; the band 48 px; waypoints 16 px apart | CELL, HALF_W, WP_STEP |
| the ladybug | radius 6 px; gas 10, brake 24, coast 3, reverse 6 up to 320 (Q8); turn 12 (6 still) /1024 a frame | CAR_R, ACCEL, BRAKE, COAST, REV_*, TURN_* |
| the surfaces | the table above | SURF_TABLE |
| hops | from 300 (Q8), up 180 + speed/3, gravity 24 | HOP_* |
| bumps | 150/256 between ladybugs, 110/256 off objects | BUMP_E, WALL_E |
| fall, left behind | fall 42 frames; gone 30 frames, back 3 waypoints behind at 85%; 60 frames blinking; no left-behind in the first 120 | FALL_FRAMES, DROP_*, GHOST_FRAMES |
| the zone, the camera | 320 x 240 + 4 px; look ahead 22 frames (56 px); ease 1/8, 5 px a frame | ZONE_*, LOOK_*, CAM_* |
| points | the leader +1, the one left behind -1; the places 3, 2, 1, 0 | DROP_POINTS, DROP_LOSS, FINISH_POINTS |
| the race | 3 laps, the lights 120 frames, the flag 150 | LAPS, COUNTDOWN, FINISH_FRAMES |
| the championship | 10 races, top 3 goes on; the cup: 3 races | CHAMP_RACES, QUALIFY_PLACE, CUP_RACES |
| the bots | the table above; difficulty: top speed - 14% + 1.5% a race (at most +2%), error shrinking | BOT_TABLE, DIFF_*, BOT_* |
| rubber band | 260 px, 3% | RUBBER_* |
| touch | the stick on the left 62%, a 24-px ring, 6-px dead zone; BRAKE radius 22 at 34, 34 from the bottom right | STICK_*, BTN_* |
| medals (1 player) | bronze 20, silver 80, gold 160, pearl 280 | MEDAL_SCORES |

**The medals** were tuned with the player bot through whole championships (3 seeds each): a human-like beginner
(`botskill=10`) went out in races 5 to 8 with 36 to 100 points (all bronze, one silver), a fair driver (`botskill=7`)
went out in races 8 and 9 with 140 to 187 (silver and gold), the player bot (skill 0) won the championship with 256
to 341 (two pearls of three). Bronze is a beginner's first good run, silver a few races won, gold most of the championship, pearl a
championship led from the front.

## Screen and layers
320x240 at 60 Hz, fixed step, seen from above; the camera scrolls in all directions.

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | dark (never seen: BG4 covers it) | - |
| BG4 | below the table (the lawn, the carpet) or beyond the garden's frame: a 64-px texture repeated | 1/2 under a table (far below), 1 for the gardens |
| BG3 | the track's world: a 64 x 32 map (128 x 32 beyond 504 columns) **streamed** from the track's whole map (up to 192 x 120 entries) round the camera every frame; off the table: transparent | the camera |
| BG1 | the house UI kit: the logo, the lap, the standings, the messages, the panels, PAUSED; on a wide screen the zone's dotted sides | fixed |
| OBJ | the kit's digits, glyphs and medals (OBJ 3); the touch controls, tags, the "!", the lights, the flag, dust, splashes, sparks, "+1" and the shadows (OBJ 2); the leaves over the track (OBJ 6, see-through in the middle); the ladybugs (OBJ 0, 1, 4, 5) | |

**The tracks on a tile console.** A track's world is drawn whole by make_art.py, then cut into 8x8 tiles, the
duplicates (also flipped) shared: the grounds and the band's textures repeat every 64 px **and are mirrored in each
cell**, so a corner and its three flipped twins are the same tiles, and repeated objects sit at the same place
modulo 64 px. A track is 400-545 tiles (at most 600: VRAM 768..1367, loaded when its race starts) in 4-5 BG
palettes (1-4 and 7; build_assets.py packs the tiles' colours, the objects drawn from a few shared colour
families). The VRAM in English: about 1.9k tiles' worth with the maps (the 64 KiB guideline holds; strict mode is
on in the tests). **The ladybugs** are 9 headings (0 to 90 degrees, 16 x 16; the hop 24 x 24 with the wings open),
flipped for the other 23 of the 32. Sprites: at most about 40.

**Widescreen** (docs/spec.md "Widescreen"): the game opts in (`max_width`, `WIDESCREEN_ladybuglaps = 1`). A wider
screen (427, 520) shows **more of the track round the camera**; the rules are unchanged: the camera and the race
zone are play.c's, 320 x 240 at every width, and two dotted lines on the UI layer mark the zone's sides (a racer
past them is left behind, even when the wide screen still shows it). The HUD stays in the 320 columns (the standings
at the screen's right edge); the touch BRAKE button follows the screen's bottom-right corner. A state saved at one
width loads at another. The touch test drives the same stick at 320, 427, 520 and 560 and compares the races.

## Screens (the house flow: no "get ready", the title is the only menu)
title (the logo over the four bots racing, the player slots, PRESS A TO RACE with the A glyph, BEST, the join line,
the copyright line) -> P1 presses A (or B, Start, the d-pad; a tap) -> the race: the track's name and RACE 1/10 under
the lights, GO!, LAP 1/3 at the top left, the standings at the top right (place, a ladybug of the racer's colour, its
points), FINAL LAP!, LEFT BEHIND!, the controls hint in the first race, FINISH! with the flag -> the results (RESULTS:
the places, the names, +points, the totals; 1 player: 4TH PLACE: OUT! when out) -> A (or a tap, or after 6 s): the
next race, or the game-over panel (1 player: GAME OVER, SCORE = the championship's points, BEST, the medal, the note
CHAMPION! or RACE 4 OF 10; 2-4 players: the ranking with medals) -> one press: back to the title.

## Controls
| Input | Action |
|---|---|
| d-pad | the direction the ladybug turns to (8 directions; let go: it keeps its heading) |
| A | gas |
| B | brake; from a standstill, reverse |
| A (or B, Start, the d-pad) on the title | start |
| Select (or Start in a race) | pause / resume |
| pads 2-4: A or Start on the title | join (B leaves) |
| A, Start or Select on the results and the panel | go on / back to the title |

### Touch (docs/spec.md "Touch")
The game reads the touches itself (`rs_game.touch_native = 1`): the phone apps pass it the fingers and draw no
buttons. Touch is player 1, alongside the pad (players 2-4 join with pads).

| Touch | Action |
|---|---|
| a finger down on the left 62% of the screen, dragged | a **virtual stick**: a ring where it went down, a knob under the finger; the ladybug turns toward the drag (as the d-pad, any angle) and **accelerates while the finger is down**; a drag under 6 px keeps the heading |
| the round **BRAKE** button (bottom right) | brake / reverse (as B), with the stick or alone |
| a tap on the title | start |
| a tap on the results / the game-over panel | go on / back to the title (after the same lock as A; a finger left down from the race does not count) |
| the app's pause button | pause / resume (it presses Start); the fingers do nothing while paused |

The controls are drawn in the house style of the apps' drawn buttons (a see-through mid-blue disc, the house
checker, a cream ring, a navy outline; filled and lower while pressed), BRAKE with its icon, once a finger has
touched the screen. What the fingers set (the stick's finger and origin, the button) is in the registered state
(main.c TV, touch_*). `tests/touch_test.sh` (in the check) plays scripted fingers: the stick's drag is the very race
of the pad's direction + A.

## Up to four players
Pads 2-4 join with A on the title (B leaves). Everyone races on one screen with the camera on the leader (bots take
the free places); a player left behind loses the point and is put back behind the leader: nobody is ever out of a
race. A cup of three races; the results rank the players by their points, with medals.

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation. The ladybugs: round
shells in four colours (red, yellow, blue, green) with black spots and a split down the middle, a black head with two
white eyes, a glint top-left, the outline the shell's darkest shade; the hop: the shells open, the pale wings out.
Big everyday objects seen from above with their shadows down-right: porcelain teacups with patterns and tea, a
teapot, sugar cubes, biscuits, a spoon; crayons with paper wrappers, erasers, paint jars with a brush, a pencil;
the gingham blanket, a wicker basket, a sandwich, apples, juice with a straw; lettuces, carrots, pumpkins, a watering
can; flower pots, a snail. Glossy jam and glue, puddles with a shine. The canopy: green and autumn leaves and carrot
fronds over the track (sprites, see-through in the middle with the house checker so the ladybugs show). The title
logo: LADYBUG in ladybug red, LAPS in leaf green.

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects (main.c sound_init): a soft "bonk" when two
ladybugs bump, a "thock" off a cup, a "boing" with buzzing wings for a hop, a little "pat" on landing, a falling
"wheee" off the table, a sad "boop" when left behind, a bright "ting-ting" for the leader's point, the lights'
beeps and a longer GO, a splash, a sticky squelch, two notes for a lap, a fanfare at the finish; the house sounds
for the menus, the medals and the game over. The effects are panned by where the racer is on the screen.

Music: "Garden Grand Prix" (tools/make_music.py): G major, speed 5 (150 BPM), A A' B C (about 26 s): off-beat pluck
stabs, a driving bass in eighths, kick, snare and hats, the soft square lead with a cheeky call that zips up the
chord; a snare roll into the next lap. Two alternatives (tools/make_music_alt.py -> music-alt/alt1.mod, alt2.mod;
previews in docs/music-alternatives/ladybuglaps/): "Dewdrop Dash" (a light, skipping garden tune in D mixolydian, no
drums) and "Sugar Rush" (a fast, punchy chase in A minor). To switch, call make_music_alt.py OUTDIR N from
tools/build_assets.py instead of make_music.py.

## Tests
`make ladybuglaps-check`: the rules alone (tests/test_rules.c: the five tracks both ways (closed, the centre line on
the band, the grid on the band and apart), the handling (top speed, the brake, reverse, turning toward the direction
held at the turn rate, the heading kept, the drift), the surfaces (rough, jam, the puddle's slide, a hop over the
spoon and not when slow, a fall off the table and back on the band), the bumps (head on, the teapot, a sugar cube, the
garden's wall), the laps and the checkpoints (a lap, a shortcut gains nothing, backwards takes the lap back, the
finish), the left-behind rule (the zone's edge to the pixel, the leader's point and the loss, put back behind the
leader in the zone, the grace at the start), the points and the championship's order, the touch geometry at four
widths, the bots finishing every track both ways at the easiest and the hardest level, the player bot and a
beginner-like bot, determinism), the libretro loader, the smoke tests (the flow, the start inputs, driving,
steering, braking, pause, joining and leaving, the game over and back to the title, the results, 4 players, save RAM,
the bot, determinism with 1 and 4 players), the touch tests, the UI screens (tests/ui_test.sh), the save states (the
title's lobby, the lights, a race, a race by touch, 4 players, paused, the Rainy Patio, the results and the game over)
and their audit, at 320, 427 and 520 columns, states across widths, and the 22 languages (tests/i18n_runs.txt).
