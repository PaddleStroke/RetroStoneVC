# Llama Drama: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/llamadrama/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md). The idea is the owner's.

## Pitch
A llama stands in an Andean meadow, and the hornets are coming. They buzz in from the right, in lines, waves, loops,
dives, pairs and little swarms, and they all want to sting the llama's nose. Turn the long neck up and down and spit:
a gob pops a hornet, which tumbles into the grass with a cute bonk and stars. One sting ends the run. The more you pop,
the faster and thicker they come, while the day turns to a sunset, the dusk and a starry night. Up to four llamas
defend the meadow together.

## Legal and originality
- An aim-and-shoot defence game (a fixed shooter turning in place, targets coming at it) is a common mechanic of many
  games; we use only that idea. The theme (a spitting llama, hornets, the Andes), the characters, the rules and every
  number below, the screens, the herd co-op, the medals, the sounds and the music are ours. The code is our own (C11 on
  the RetroStone SDK, integer fixed point), written from this design; no other game's code was read for it.

## Rules
The llama stands on the left; its head is at the top of a long neck. **UP and DOWN turn the head** (and lean the
neck): from 35 degrees down, at the grass, to 55 degrees up, high in the sky; slowly at first (fine aim), then faster
(the whole range in about a second). **A spits** (B, X and Y too): a gob flies from the mouth along the aim at 5 px a
frame, in a slight arc (0.02 px/frame/frame down). The **reload**: 12 frames between two spits and 3 gobs in the air
at most per llama (a press during the reload spits as soon as it can); a gob ends at x 336 (with a little splat), on
the meadow (y 200) or out of the top of the sky. Three faint dots along the spit's path near the mouth show the aim.

**Hornets** come from the right. Each glides in along its lane from beyond the widest screen (x 496) to x 336, where
its pattern starts; its sine waves and zig-zags follow its x (a fixed path in the sky, whatever its speed); nearer than
112 px (in x) to its llama's head it steers to it (0.7 px a frame up or down, faster later). Each hornet goes for the
llama whose head is nearest its lane (another one if that one is out). **A hornet that touches a llama's head or neck
stings it: that llama is out** (one sting, the house rule): an 8-frame hit-stop and a shake, X eyes, stars, the head
and neck droop; the hornet buzzes off, pleased with itself. The run ends when every llama is out.

| Hornet | Hits | Points | Speed | |
|---|---|---|---|---|
| hornet | 1 | 10 | the hornet speed | flies straight, in sine waves, zig-zags, dives from high up, loops, in pairs and swarms |
| fast dart | 1 | 25 | the dash: 2.7 px a frame (+1% per heat point) | it stops at x 288 and buzzes 50 frames (it shakes, a red "!": a warning), then dashes straight at its llama's head |
| big hornet | 3 | 50 | 70% | a slow sine wave; each hit knocks it back 6 px (it blinks); later with two escorts |

A popped hornet hops, tumbles down upside down with two stars circling it, lands in the grass, bounces once, lies there
a moment and fades (nothing is hurt); its points float up. The score is each llama's own.

**The ramp.** The **heat** is the herd's score / 10 (with 2, 3, 4 llamas it counts 1.5, 1.75, 2 times less). It sets:
- the hornets' speed: 0.75 px a frame, +2% per heat point, at most 1.95 (heat 80); the homing and the darts' dash +1%
  per heat point (twice at most);
- the waves: one every 124 - 2 x heat frames (38 at least) per 2 units of cost (a single hornet 2, a pair 3, a dive or a
  loop 2, a dart 3, a swarm 5, a big one 5, 6 with escorts; 2-4 llamas: 1.5-2 times more often);
- what comes (play.c pick_wave), each new kind unlocked by the heat: straight hornets only at first (the lanes in the
  middle of the sky until heat 6), sine waves from heat 3, zig-zags from 5, pairs from 8, dives from 12, loops from 16,
  darts from 22 (FAST DART!), swarms of 4 from 28 (SWARM!: 5 from 50, 6 from 70), big hornets from 36 (BIG HORNET!, with
  escorts from 60); from heat 50 a second wave sometimes comes at once (more and more often, at most 60% of the time);
- the sky: the day until heat 30, then the sunset (30-55), the dusk (55-80) and the night (80-100), eased slowly.

So the first 20 s are gentle (6 to 12 slow hornets, one at a time, straight along the middle of the sky: the first one
takes about 9 s to reach the llama); a good player is busy by the sunset and the night is hard.

Every number is in src/game.h (the tuning table):

| Rule | Value | Where |
|---|---|---|
| the herd | P1's shoulder at (44, 182), neck 64 px; P2 behind (24, 170), 92; P3 in front (68, 196), 30; P4 at the back (4, 160), 118 | SLOT_*, DRAW_ORDER |
| aiming | -35..55 degrees (-100..156 of 1024), from 5; 2 units a frame for 8 frames, then 5; the neck 61..89 degrees | AIM_*, NECK_* |
| the head | 7 frames 15 degrees apart; the mouth at (13, -3) head units; a sting within 10 px of the head (+ the hornet's radius), 6 of the neck; the scared face with a hornet within 60 px | HEAD_*, NECK_R, SCARED_DIST |
| the spit | 5 px/frame, gravity 0.02, radius 3, 3 in the air, reload 12 frames, a press kept 12 frames, the puffed cheeks 10 frames, the reach x 336, the meadow y 200 | GOB_*, RELOAD, SPIT_BUFFER, PUFF_FRAMES, GROUND_HIT |
| touch | a tap at least 12 px right of the mouth aims there | TOUCH_MIN_DX |
| hornets | hits 1/1/3, points 10/25/50, radius 6/5/11, speed 100/100/70%; 24 at once at most | KIND_*, MAX_HORNETS |
| speed | 0.75 px/frame, +2% per heat point, +160% at most | HORNET_V* |
| patterns | homing from 112 px at 0.7 px/frame; lanes 36..172 (64..150 at first); sine 5/1024 of a turn per px, amplitude 14/22/30; zig-zag 24; the dive 1.7x; the loop radius 28; the dart perches at 288 for 50 frames, dashes at 2.7 | HOME_*, LANE_*, SINE_*, ZIG_AMP, DIVE_PCT, LOOP_R, DART_* |
| the ramp | heat = score / 10 (x4/6, 4/7, 4/8 for 2-4 llamas); waves 124 - 2 x heat frames (38 at least) per 2 units of cost; unlocks at 5, 8, 12, 16, 22, 28, 36; double waves from 50; swarms 4/5/6; escorts from 60 | HEAT_*, GAP*, UNLOCK_*, DOUBLE_FROM, SWARM_N, ESCORT_FROM |
| the sky | sunset 30, dusk 55, night 80 | SKY_* |
| hits | a hit-stop of 8 frames, the bonk 60 | HIT_* |
| medals (1 player) | bronze 150, silver 450, gold 1000, pearl 3000 | MEDAL_SCORES |

**The medals** were tuned with the good bot and the human-like bot (`botskill=E`: it decides only every E/2 + 1
frames, sees the hornets only once they are left of x 320 - 6E, aims off by up to 2E units (0.7E degrees, a new error
each spit), spits before its aim is right (within 3 + E units) and, from E = 6, no longer counts on its gobs in the
air), 12 runs each. Scores (and the mean time):

| bot | scores | mean | bronze | silver | gold | pearl |
|---|---|---|---|---|---|---|
| E = 10 (a beginner) | 50..510 | 274 (48 s) | 10 | 1 | 0 | 0 |
| E = 8 | 180..655 | 400 (61 s) | 12 | 4 | 0 | 0 |
| E = 6 | 140..715 | 473 (62 s) | 11 | 6 | 0 | 0 |
| E = 4 | 550..1080 | 859 (78 s) | 12 | 12 | 2 | 0 |
| E = 2 | 750..3485 | 1582 (100 s) | 12 | 12 | 8 | 1 |
| E = 0 (the good bot: it sees everything) | 1360..7190 | 2654 (130 s) | 12 | 12 | 12 | 4 |

So a beginner gets bronze (10 of 12), silver needs a steady aim through the sunset, gold the darts and the first big
hornets, pearl the night (real skill). 11 of the 12 beginner runs lasted more than 20 s (one ended at 19 s).

**The bot** (`--opt bot=N`: tests, screenshots and card frames) plays from the world: it forecasts every hornet 72 frames
ahead (play.c hornet_move on a copy, its homing included, until it would sting), works out which ones the gobs already
in the air will pop, takes the most pressing of the others (the soonest at a head; its own llama's first), and the aim
that pops it soonest, counting the head's turn, the reload and the gob's flight; it turns there (letting go before it
overshoots) and spits. `botstop=S` makes it stop playing once its score reaches S (P2-P4: S + 20, + 40, + 60): the
tests' game over.

## Screen and layers
320x240 at 60 Hz, fixed step. The house layout (docs/art-direction.md "Backgrounds"); the scenery is still (no scroll):

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the sky, one colour per line (raster callback): blue by day, a pink and orange sunset, a violet dusk, a navy night; a 2-line dither step every 8 lines | - |
| BG2 | the scenery, two palettes: the Andes (a hazy far range, snowy peaks lit from the left, the green puna with terraces; stars in the sky, whose colour the raster callback sets per line: the sky's own by day, white by night) and the meadow (grass, tufts, little flowers, pebbles); each palette eased from the day to the sunset, the dusk and the night (build_assets.py time_of_day) | fixed (shakes on a sting) |
| BG1 | the house UI kit: the logo, the title's lines, the hint, the new hornets' names, the panels, PAUSED | fixed |
| OBJ | the llamas (head, neck pieces, body; P1 OBJ 0, P2-P4 OBJ 1, 4, 5: the wool and the tassels recoloured), the hornets (OBJ 2), the kit's digits, glyphs and medals (OBJ 3), the spit, splats, bonk stars, aim dots and small digits (OBJ 6), the sun, the moon and the clouds (OBJ 7, recoloured by the time of day; priority 0: behind the mountains, so the sun sets behind them) | |

The neck is a stack of 16x24 woolly ovals (one every 11 px, 3 leans) between the shoulder and the head, with an
outline only across: it reads as one fluffy neck that tilts smoothly with the aim; the head (32x32) has 7 aim frames x
3 faces (normal, puffed cheeks after a spit, scared with a sweat drop when a hornet is close) and the stung face.

**Widescreen** (docs/spec.md "Widescreen"): the game opts in (`max_width`, `WIDESCREEN_llamadrama = 1`). The house rule
for games where enemies come from the right: about **a third of the extra width behind the llamas and two thirds ahead**
(the stage origin, WIDE_LEFT_NUM / WIDE_LEFT_DEN: 35 + 72 columns at 427, 66 + 134 at 520, 80 + 160 at 560); the
stage's 320 columns are those of 320 pixel for pixel, and **the gameplay is the same at every width**: the rules
(play.c) never ask the width. The hornets appear at x 496, beyond the widest screen's right edge (480), and glide in
along their lane to x 336 where their pattern starts, so they reach the llamas at the same moment at every width (a wide
screen shows them coming sooner, never earlier or nearer); the spit ends at x 336 (a wide screen sees the little splat
there). The extra width shows more of the meadow and the herd behind (the bodies of the llamas at the back), and more
sky and mountains ahead. The scenery is a 512-px panorama that repeats seamlessly (128-tile maps beyond 512 columns).
Tests: the rules test checks every hornet of a long two-bot run appears beyond the widest screen and no spit passes
x 336; the touch test plays the same taps and the same bot run at 320, 427, 520 and 560 and compares them; a state saved
at one width loads at another.

## Tall screen (docs/spec.md "Tall screen")
A phone held upright: the zoomed screen, `rs_screen_zoom(ZOOM_H)` (480 lines) first in `game_init`. The app scales the
480 lines to the display's height and crops the sides: in play the screen's left columns are shown
(`rs_screen_focus_x(0)`: 205 at 1080x2520, 221 at 1080x2340, 240 at 720x1440), the herd and the sky ahead of it,
4.9-5.3x on a phone (v1: 3.4x). Layout only: the rules, the 320-px field, the spit's reach and the spawns are those of
every width (game.h). The stage sits low (`rs_screen_origin_v`: TALL_BELOW 32 lines of meadow under it, 208 above).
- **The field's right part.** A hornet the 320 columns would show but the columns shown do not gets a "!" at the
  right edge at its height (blinking; steady for a dart winding up to dash: draw.c `edge_marks`), the warning a wider
  screen gives; the spit still flies its whole reach (a pop past the edge counts).
- **The HUD at the screen's top**: the scores as sprites at `rs_screen_top()` in the columns shown (`tall_scores`: 1
  player centred; 2-4 with each llama's icon, P1 P2 side by side, P3 P4 under them; no corner tags); the UI layer is
  scrolled so its centred texts (the hint, the new hornets' names, PAUSED) sit in the middle of the columns shown.
  The title and the panels keep the same view (zoomed, the same columns): their texts and the kit's sprites moved
  onto the columns shown too, the logo on two lines, the lines wrapped, a banner too wide on two lines
  (games/common/src/house_tall.h); the hornet waits by the logo's first word.
- **Above the stage** (draw.c `raster_out`): a far range of the Andes rises behind the near one: BG3 holds the same
  panorama FAR_DY 120 px higher and FAR_DX aside in a hazy palette (PAL_FAR 6: eased toward the horizon, the greens
  turned to rock), fading into the sky's colour over the stage's first FAR_MIST lines (the raster eases PAL_FAR per
  line; BG3 hidden past them). The stars' colour is also the snow's lit side, so BG2 uses a copy of the scenery tiles
  without the lone stars (VR_BG2_TALL) and the night stars come from BG4 (the panorama's starry top rows, over the
  sky above the stage only). Four more clouds drift there (a 400-px round: seen often), the sun and the moon in the
  columns shown, from under the HUD; the sprites are culled at the screen's edges, not the stage's.
- **Below**: the meadow's flowers (its last 32 lines, shifted); the UI and the sprites hidden there.
Line 0 puts the stage's windows and scrolls back; BG3 and BG4 exist only on a tall screen (maps_setup, set up again
when a state of another size loads: `drawn_height`), so landscape and 4:3 are the same pixels. A touch's aim point is
taken in stage coordinates (`t->y - rs_screen_origin_y()`), so a tap in the sky above the stage aims there.

## Screens (the house flow: no "get ready", the title is the only menu)
title (the logo, the meadow, the herd waiting on the left and looking around, a hornet buzzing on the right, the player
slots under the logo, PRESS A TO SPIT with the A glyph, BEST, the join line, the copyright line) -> P1 presses A (the
press also spits; or B, X, Y, Start, Up, Down; a tap) -> the llamas hum, play (the score at the top, or in the corners
for 3-4; the hint "^~ AIM A: SPIT" or "TAP THE SKY TO SPIT" for 5 s; FAST DART!, SWARM!, BIG HORNET! the first time
each comes; Select, or Start, pauses) -> the last llama's sting -> game over (the banner, the panel sliding up:
score, best / new best, the medal, the hornets popped; 2-4 players: the ranking by score with medals) -> one press (or
a tap): back to the title (the same players start again with A).

## Controls
| Input | Action |
|---|---|
| UP / DOWN | turn the head up / down (slowly, then faster) |
| A (or B, X, Y) | spit |
| A (or B, X, Y, Start, Up, Down) on the title | start |
| Select (or Start in a run) | pause / resume |
| pads 2-4: A or Start on the title | join (B leaves) |
| A, Start or Select on the game-over panel | back to the title |

### Touch (docs/spec.md "Touch")
The game reads the touches itself (`rs_game.touch_native = 1`): the phone apps pass it the fingers and draw no buttons.
Touch is player 1, alongside the pad (players 2-4 join with pads).

| Touch | Action |
|---|---|
| a finger put down anywhere right of the llama's mouth | the head turns toward that spot (within the aim range: the aim whose spit, arc included, passes nearest the spot) and spits there at once; the same reload (a tap during it spits when it ends) |
| that finger held and moved | the head follows it (no more spit until a new tap) |
| a finger left of the mouth | nothing |
| a tap (a finger lifted) on the title | start |
| a tap on the game-over panel | back to the title (after the same lock as A; a finger left down from the run does not count) |
| the app's pause button | pause / resume (it presses Start); taps while paused do nothing |

The touches are turned into stage pixels in main.c (x - the stage origin): the same tap aims the same at every width.
`tests/touch_test.sh` (in the check) plays scripted fingers through the headless runner.

## Up to four players
Pads 2-4 join with A on the title (B leaves). The herd stands one behind another along the left edge, heads one above
another: P1 the cream llama with red tassels in the middle (the 1-player llama), P2 a brown one in blue behind it,
taller; P3 a grey one in green in front, shorter; P4 a caramel one in purple at the very back, the tallest (heads at
about y 117, 78, 163, 44: the whole sky is covered). Each aims and spits on its own and scores its own pops; the spit
passes through the other llamas. Each hornet goes for the llama whose head is nearest its lane (and stings whichever it
touches); a stung llama is out (it droops, X eyes) while the others play on; the run ends when all are out. The heat
counts the herd's score 1.5-2 times less and the waves come 1.5-2 times more often. The HUD shows each score (3-4
players in the corners); the results rank everyone by score (the same score, the same place) with medals. On the
title, 2-4 llamas wait 44 px further left, clear of the player slots (row 11).

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation. The llama: cream wool
in 4 shades lit from the top-left, a long rounded muzzle, banana ears with pink insides and a red pompom (the player's
colour), a woolly fringe, big dark eyes with a glint and lashes, a little smile and a blush; the head is drawn in head
units and turned to each aim (the eye stays crisp and upright). Faces: puffed pink cheeks and pursed lips ("ptoo!",
eyes squeezed) for 10 frames after a spit; wide eyes, raised brows, an O mouth and a sweat drop when a hornet is close;
X eyes and the tongue out when stung. The body: a woolly torso, a tail tuft, four legs with hooves, an Andean blanket
with zigzags and tassels in the player's colour. The hornets (they face left): yellow with dark stripes, a grumpy brow,
flapping wings; the dart slimmer and orange; the big one 32x32 with a fuzzy orange thorax, mandibles and two grumpy
brows; bonked: X eyes, wings folded. Props: a mint gob of spit, splats, bonk stars, the aim dots, a red "!", small
digits. The sky: a sun with a dithered glow, a crescent moon, two clouds. The Andes panorama and the meadow (above).
The title logo: LLAMA in warm wool, DRAMA in tassel red.

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects (main.c sound_init): a "ptoo!" for the spit (a
puff of air and a wet pop), a cartoon "boing" and a twinkle for a popped hornet, a "thwack" for a hit on a big one, a
zap and a falling "ouch" with a wobble for a sting, an angry rising buzz for a dart about to dash, a soft thump when a
bonked hornet lands, the llamas' "mmmh?" hum at the start; the house sounds for the menus, a new kind of hornet (the
ding), the medals and the game over. The lengths keep the samples under the 64 KiB guideline with the house set.

Music: "Huayno de la Llama" (tools/make_music.py): A minor, speed 3 (125 BPM to the quarter), A B A C B (about 19 s),
an Andean huayno's skipping rhythm (long, short, short on every beat): the house pluck strummed like a charango (the
arpeggio effect rolls each chord), a bass on the root and the fifth, the bombo and a shaker (the kit's kick and hat), the
breathy house lead as a quena with a call that tumbles down the pentatonic scale to A, answered an octave up; C turns to
C major and climbs, E major (the Andean dominant) brings it home. Two alternatives (tools/make_music_alt.py ->
music-alt/alt1.mod, alt2.mod; the owner picks; previews in docs/music-alternatives/llamadrama/): "Condor Glide" (a slow,
airy air in D dorian: long quena notes over the pad and harp-like broken chords) and "Altiplano Hop" (a quick, bouncy
sanjuanito-like dance in E minor with a hopping square lead). To switch, call make_music_alt.py OUTDIR N from
tools/build_assets.py instead of make_music.py.

## Ideas (not in the game; for the owner)
- A power-up was not needed: the game is complete without one. If wanted: a golden hornet now and then that, popped,
  gives a "big gob" (a wide splash that pops everything near it) for one spit; or a "triple spit" for 5 s.
- A condor that crosses the sky at night (a bonus target, 100 points), or a cheering crowd of alpacas at a new best.
- On phones, a second finger could spit again without moving the head (today any new finger aims there).

## Tests
`make llamadrama-check`: the rules alone (tests/test_rules.c, 139 checks: the start, the aim's range, speed and
frames, the mouth and the neck at each aim, the spit along the aim and its arc, the reload and 3 gobs at most, the
reach, the splat on the meadow, a gob popping a hornet (bonk, points), the big one's three hits and knock-backs, the
dart's 25, a sting on the head and on the neck (the run ends), the scared face, the homing, the speed rising with the
score up to its cap, more hornets as the score rises and the new kinds and patterns, a gentle first 20 s, the sine wave,
the zig-zag, the dive, the loop, the dart's perch and dash, pairs and swarms, the widescreen spawns and reach over a
long two-bot run, tap-to-aim (the spit passes through the tapped point), touch held and moved, the reload with touch,
taps left of the mouth, 4 llamas and their targets, determinism and the bot), the libretro loader, the smoke tests
(flow, start inputs, the aim's speed and range, mashing A, the game over and back to the title, pause, joining and
leaving, 4 bots, a busy night with 4 llamas, no strict-mode warning, save RAM, the bot a minute without a sting,
determinism), the touch tests, the UI screens (tests/ui_test.sh), the save states (the title's lobby, a run, a busy
night, 2 players with P1 aiming by touch, 4 players, paused, the game over, the results) and their audit, at 320, 427 and
520 columns (the widescreen passes run the flow, the screens and four states), states across widths, and the 22
languages (tests/i18n_runs.txt).
