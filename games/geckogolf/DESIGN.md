# Gecko Golf: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/geckogolf/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

## Pitch
A little leopard gecko in a white visor plays golf on a course that never ends, seen from the side: one hole per
screen, each one starting where the last ball dropped in, over meadows, dunes, snowy hills, a night course and a candy
land. Aim, hold A to charge the swing, let go. A budget of strokes keeps it an arcade game: holes in one and in two
give strokes back, the water costs one more, and the run ends when they run out. Calm to play, one more hole.

## Legal and originality
- Desert Golfing (Captain Games, 2014) is proprietary. We use only its **mechanics** (not protected): an endless,
  procedurally generated side-view course where each hole begins at the last one's cup and the camera pans on.
- No name, art, sound, text, code or level data of it is used or was looked at for this game; the course generator,
  the physics, the surfaces and hazards, the biomes, the stroke budget, the multiplayer turns and the touch slingshot
  are our own design and code (C11 on the RetroStone SDK, integer fixed point).
- The gecko, the props, the skies, the music and the sounds are drawn and composed by code in this repository (tools/,
  src/main.c sound_init) with the 8BCraft house kit.

## Rules
Every number is in src/game.h (the tuning table).

| Rule | Value | Where |
|---|---|---|
| strokes at the start | 12 | START_STROKES |
| a shot | 1 stroke | play.c shoot() |
| a hole in one / in two / in three or more | 3 / 2 / 0 strokes back (at most 30) | REFUND_ACE, REFUND_TWO, STROKES_MAX |
| water, out of bounds | 1 penalty stroke, the ball back where it was hit | PENALTY |
| out | the ball at rest outside the cup with no stroke left (or holed with the last stroke, nothing back) | play.c |
| score | holes completed | world_score |
| medals | bronze 6, silver 12, gold 24, pearl 40 holes | MEDAL_SCORES |

**The shot.** The d-pad turns the aim, in half degrees from 4 to 176 degrees (any direction over the ground, both
ways): Left and Right turn it round the circle, Up raises it towards the sky (it stops straight up), Down lowers it
towards the ground; a tap is half a degree, held it goes faster and faster (1, 2, then 4 half degrees a frame). Four
dots show the direction. **Hold A** to charge: the power gauge fills in a second, then empties again (a ping-pong:
holding too long is never a full-power accident), and **let go** to shoot. While charging the first 26 frames of
the flight show as dots (the start of the arc, no full trajectory: judging the bounces and the roll is the game).
B while charging: no shot; a release below GAUGE_MIN_SHOT (a tap of A) is no shot either. We chose hold-and-release
over "press A twice" because it is one gesture, the backswing shows the power (the gecko's three backswing frames
follow the gauge), and it maps to the touch slingshot (pull, let go).

**The ball** is a point on a height map (one height per pixel), Q16 fixed point, gravity 0.12 px/frame²,
0.3 to 6.6 px/frame (about 330 px on the flat at 45 degrees). In the air it moves in sub-steps of at most 1 px; on the
ground it bounces off the slope's normal (the slope over up to 2 px each side), keeping a part of its speed into the
ground (the bounce) and along it (the slide); slow enough, it rolls: gravity along the slope, the surface's friction,
and it rests when slow on a slope that holds it. A step of more than 3 px between columns is a wall (half the speed
back; a slow ball the slope pushes into it rests there, and a ball never rests against a wall's side but 1 px off it,
so a steep lob can clear it). The ground falling away (a crest, a cliff, the cup) throws the ball into the air again.

| Surface | Bounce | Slide | Roll friction | Holds on | |
|---|---|---|---|---|---|
| grass | 0.38 | 0.84 | 0.022 | 0.030 | |
| sand (bunkers) | 0.06 | 0.16 | 0.25 | 0.12 | stops the ball where it lands |
| ice | 0.43 | 0.98 | 0.0025 | 0.004 | slides and slides (3x the roll of grass) |
| mushroom (gumdrop) | 0.91, at least 1.9 px/frame | 0.94 | | | never rests on one: a nudge off the cap |
| water | | | | | splash: the penalty |

**The cup** is a notch 8 px wide and 9 deep (the ball 4 px). A ball that drops 3 px below the lip is in; rolling over
it slower than 1.7 px/frame it drops in, faster it skips the gap (or hits the far wall and falls back in). Out of
bounds: the stage's sides (x 2 and 318; white stakes with red tops), the same at every width; the sky is open.

## The course
One hole per 320-px stage: the tee at stage x 40 (the last cup), the cup 112 to 258 px further right, on a flat
green (10 px before the cup, 12 after: the next tee). When everyone has holed out (or is out), the cup is filled, the
balls wait on it, and the camera pans right (80 frames, eased) until the cup is the new tee. Generation (play.c
gen_hole) is deterministic from the seed and the hole number, written into a 2048-px ring of heights, surfaces and
biome numbers, 700 px ahead of the camera whatever the width (a wide screen sees the same course).

- **Difficulty** grows with the hole number up to 36: longer holes (+4 px of random range per hole), the cup's height
  further from the tee's (18 + 3 per hole px, up to 90), more of the tricky shapes, hazards from hole 1 (bunkers),
  3 (ice, mushrooms) and 4 (ponds).
- **Shapes**: rolling (1-2 bumps), a hill to shoot over, a valley, a cliff up, a cliff down (from hole 2), a cup on a
  ledge (from hole 7: a 22-40 px wall below the green; the next hole starts by dropping off it; never two in a row).
- **Hazards** by biome (percent per hole): meadow ponds 55, bunkers 40, mushrooms 25; dunes bunkers 75, an oasis 20;
  snowy hills ice 70, ponds 25; night ponds 50, glowing mushrooms 55; candy land gumdrops 75, ponds 40. A pond fills a
  valley's bottom, or is dug in a flat stretch; never on the tee or the green.
- **Biomes**: 6 holes each, meadow, dunes, snowy hills, night course, candy land, then again (harder). The palette of
  the course, the sky's gradient, the far hills and the clouds change; the camera's pan blends the skies; the new
  biome's name shows in big letters. Decor: flowers and tufts, cacti and rocks, pines and a snowman, glowing
  mushrooms and fireflies, lollipops and candy canes (sprites, not in the way).
- **Every hole can be played**: tests/test_rules.c plays the first 40 holes of 4 seeds with the best bot (each
  within 3 strokes), so no hole is unreachable (the ledges' foot taught the bot its straight-up lobs).

## Up to four players
Pads 2-4 join with A on the title (B leaves). Everyone plays the same holes with their own ball (in their colour: the
gecko's orange, then blue, green, purple), pad and stroke budget; the balls pass through each other. **Turns: the
player farthest from the cup plays next**, as in golf (the honour at a new tee: the fewest strokes on the last hole,
then the player number). We chose it over a fixed order because it keeps the group together and makes a long first
shot pay; the active gecko stands at its ball with its P1..P4 tag. A player is out when their strokes run out; the
hole goes on for the others; the game ends when everyone is out. HUD: holes as score chips (centred, halves, corners)
with each player's strokes as pips in their colour, HOLE n at the top. Results: holes, then the fewest strokes, with
the house medals.

## Touch (rs_game.touch_native)
Player 1 on a touch screen (or the mouse) plays with a **slingshot**: put a finger down anywhere (not necessarily on
the ball: the finger never hides it), pull back away from where the ball should go; the aim and the power follow the
pull (play.c touch_shot: the pull's direction, at least 10 px, full power at 110 px; pulled upwards the shot goes
along the ground), the gauge and the arc show, a ring marks where the finger went down; let go: the shot, exactly the
pad's shot at that aim and power. Back inside 10 px: no shot. Fingers do nothing on another player's turn or while
paused. A tap starts from the title and leaves the game-over panel (after its lock; a finger left down from the run
does not count). tests/touch_test.sh checks it at 320, 427, 520 and 560.

## Screen and layers
320x240 at 60 Hz, fixed step.

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the sky's gradient per biome, one colour per line (raster callback) | - |
| OBJ (palette 6, priority 0) | the sun or the moon, clouds drifting, stars at night: behind the hills | 1/8 |
| BG3 | the far hills, 5 tones recoloured per biome, high priority (in front of the sky's sprites) | 1/4 |
| BG2 | the course: drawn by draw.c into tiles (below) | 1 |
| BG1 | the house UI kit: the logo, the messages, the hint, the panels, PAUSED | fixed |
| OBJ | the gecko, the balls, the flag, the aim's dots, the gauge, the stakes, the splash, the decor, the kit's digits, glyphs and medals | |

**The course's tiles**: each 8-px column of the screen is cut into tiles: blank above the ground, 11 shared deep soil
tiles below it (specks, pebbles lit from the top-left, two wavy strata at rows 16 and 23, clear of the text rows),
and the tiles the surface crosses drawn pixel by pixel (the dark top line, the grass, sand, ice or mushroom band,
thicker on a steep slope so it keeps its look, the water, the cup, the cliffs' outline and their face lit facing left
or shaded facing right, roots under the meadow's and the night's grass, the topsoil fading into the deep soil with an
ordered dither), shared when two are the same (about 200-320 tiles a screen, of 448). They are drawn again only when
the camera moves (the pan) or the course changes (a cup dug or filled). Two BG palettes (1, 2) hold the biomes at the
screen's two edges, so both show during the pan; at their border the soil fades from one biome's into the other's over
48 px (an ordered dither, in BG palette 4, which holds both soils), never a straight cut.

**Widescreen** (docs/spec.md "Widescreen"): the 320-px stage (one hole) is centred; a wider screen (427, 520) shows
more of the course on both sides: the last hole behind, the next one ahead (no flag there yet: a cup is dug when its
hole starts). The course, the physics and the out-of-bounds stakes are the same at every width (stage coordinates),
so a run is the same run. A state saved at one width loads at another (draw_state_loaded sets the maps up again and
draws the course's tiles again). `make geckogolf-check` runs the tests at 320, 427 and 520.

## Screens (the house flow: no "get ready", the title is the only menu)
title (the logo, the gecko bobbing, the first hole behind, the player slots, PRESS A TO SWING with the A glyph, BEST,
the join line, the copyright line) -> P1 presses A (or the d-pad, Start; that press only starts) -> play (the hint on
the first hole; Select or Start pauses) -> messages: HOLE IN ONE! +3 STROKES (sparkles round the flag), IN TWO! +2,
IN THE HOLE!, SPLASH! / OUT OF BOUNDS! PENALTY STROKE, NO STROKES LEFT, the biome's name -> game over (the banner, the
panel: holes, best / new best, the medal, the holes in one; 2-4 players: the ranking with medals) -> one press: back to
the title (hu_over_back; the same players start again with A).

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation. The gecko: a warm
orange (its main material, turned for players 2-4: build_assets.py P2_HUE), a cream belly, brown spots, a big eye,
a white visor, a grey club; 32x32: address (2 frames), backswing (3, by the gauge), follow-through, cheer (2: the
club raised), sad (water, out). The course is drawn by draw.c in each biome's 15 colours (TERRAIN_PAL); the far hills
and the sky's sprites are drawn in key colours and recoloured per biome (HILL_PAL, SKY_PAL, SKY_GRAD); the decor has
one palette for every biome. The logo: GECKO in the gecko's orange, GOLF in fairway green.

## Sound and music
The house synthesiser for the effects (main.c sound_init): the swing (a swish and a crisp tock), the ball on grass
(a soft wooden tok), sand (a muffled pff), ice (a glassy tink), a mushroom (boing), the cup (a hollow rattle and a
bright plink, with echo), the water (a splash and a bloop), out of bounds (a falling whoop), a hole in one (a happy
arpeggio), the aim (a tiny tick every 5 degrees), out of strokes (two sad notes); the house set for the UI (the ding
of a refund, the swish of the pan). Music "Fairway Stroll" (tools/make_music.py): G major, speed 8 (94 BPM), the
house instruments, A A' B A' C (about 51 s): lazy broken chords on the pluck, a strolling bass, the pad holding the
chords with high dewdrops, a sunny breathy lead. Two alternatives in music-alt/ (tools/make_music_alt.py, `make
geckogolf-music-alt`): "Siesta Green" (D dorian, slower, sleepy) and "Tee Time Shuffle" (F major, a light shuffle with
soft drums); previews (uncommitted) in docs/music-alternatives/geckogolf/.

## The bot (bot=1; tests, screenshots)
play.c bot_plan_shot simulates every candidate shot with the game's own physics (sim_shot): 25 angles towards the cup
(3 to 89.5 degrees: lobs out of a ledge's foot) times 20 powers, then a finer search round the best four; the cost is
the distance from the cup where the ball rests (water and out of bounds: the worst). It aims and charges through the
same inputs as a player (main.c bot_input: it turns the aim, holds A, lets go at its power). botskill=E plays like a
human: the aim off by up to E half degrees and the power by up to E x 2.5% of the shot, no fine search from 5.
botstop=H: after H holes it shoots out of bounds until it is out (the tests' game over).

**Medals, tuned with the bots** (tests/test_rules.c --tune S 30: 30 runs each):

| Bot | Average holes | No medal / bronze / silver / gold / pearl |
|---|---|---|
| botskill 10 | 6.8 | 15 / 10 / 5 / 0 / 0 |
| botskill 8 (a beginner) | 7.9 | 9 / 16 / 5 / 0 / 0 |
| botskill 4 | 10.9 | 6 / 11 / 12 / 1 / 0 |
| botskill 2 (a good player) | 15.5 | 6 / 4 / 13 / 7 / 0 |
| botskill 1 (an expert) | 27.6 | 2 / 2 / 10 / 9 / 7 |
| botskill 0 (the test bot: exact) | endless (mostly holes in one) | |

A beginner reaches bronze (6 holes) in 70% of runs; silver needs a steady hand, gold an expert, pearl a great run.

## Tests
`make geckogolf-check`: the rules alone (tests/test_rules.c: the course's determinism, heights, greens and shapes,
every hole played through by the bot, the ball on each surface, the cup slow and fast, the water and out-of-bounds
penalties, the gauge, the refunds and out of strokes, the turn order and the honour, the slingshot, determinism), the
libretro loader, the smoke tests (the flow, the aim, the gauge and the shot, B and a tap are no shot, game over and
back, pause, save RAM, joining and leaving, 2 players on 2 pads, 4 bots, a later biome, the bot hook, determinism),
the touch tests, the UI screens (tests/ui_test.sh), the save states (the title's lobby, a run, a pan, a shot charged
on the pad and on the touch screen, a later biome, 4 players, paused, the game over, the 4-player results) and their
audit, states across widths, and the 22 languages (tests/i18n_runs.txt), at 320, 427 and 520.
