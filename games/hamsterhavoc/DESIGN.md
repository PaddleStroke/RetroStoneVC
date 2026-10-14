# Hamster Havoc: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/hamsterhavoc/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

## Pitch
A hamster in a little round glass-dome spaceship defends its cheese moon from waves of mischievous space mice. Slide
along the moon, fire sunflower seeds up at the marching formation, hide behind the cheese shelters while both sides
nibble them away, and shoot the bonus saucer for mystery points and power-ups. The mice march faster as they thin out;
every 4th wave a big mouse mothership comes. Nothing is hurt: the mice pop into sparkles and crumbs, a hit hamster
just bonks its dome. One more try for a better wave, a better medal.

## Legal and originality
- The fixed shooter with a marching formation is the genre of **Space Invaders** (Taito, 1978, proprietary). We use
  only its **mechanics**, which are not protected: a formation that steps sideways, drops a step at each edge and
  speeds up as it thins out (with a march beat that quickens); one player shot on screen at a time; the enemies' few
  kinds of falling shots; shelters eroded pixel by pixel by both sides' shots and nibbled by a low formation; a bonus
  saucer crossing the top for mystery points; game over when the formation lands.
- Nothing else is taken from it: no name, art, sound, music, layout, text or numbers measured from it. The theme (a
  hamster, its cheese moon, space mice), the characters, the shelters' shape, the screens, the power-ups, the
  mothership waves, the co-op for four, the medals, the sounds (our march beat is our own four-note C-E-G-E figure)
  and the music are ours. The code is our own (C11 on the RetroStone SDK, integer fixed point), written from this
  design; no other game's code was read for it.
- Every number below is ours, tuned for our 320x240 screen and a fair, cute game.

## Rules
The hamster's ship slides along the bottom of the screen (the d-pad, 1.5 px a frame) and fires a sunflower seed
straight up (A, held: a new seed as soon as the last one is gone; one seed on screen at a time). The formation of space
mice (5 rows of up to 11) marches sideways a step at a time; when a mouse would leave the play band it drops a step
instead and turns. The fewer mice are left, the shorter the wait between two steps (the march beat quickens with it);
the very last mouse runs. Three kinds of mice by row: the little mint mice with star antennae on top (30 points), the
pink mice in bubble helmets (20), the round grey-blue mice below (10). The mice drop three kinds of shots from the
bottom mouse of a column: a cheese crumb (slow, from above a hamster), a space pea (fast, any column) and a wiggly
jelly bean (the columns in turn). **One hit ends the run**, as in the other house games: a shot that reaches a ship
pops it, the hamster bonks (X eyes, a cracked dome, a shake) and is out; there are no spare ships. Only the bubble
shield (a power-up) saves a hamster: it takes exactly that one hit and is gone. With 2-4 players a hit puts that player
out and the others play on (nothing pauses); the run ends when all are out. A seed and a crumb that meet both pop.

Four cheese wedges shelter the hamsters. A seed from below or a shot from above bites a crater in the cheese (pixel by
pixel: they are dug through bit by bit), and the mice nibble the cheese they march over. Each wave brings fresh cheese.

Now and then a bonus saucer full of crumbs (a cheese wheel, a fat mouse at the wheel) crosses the top: a seed gives its
mystery points (50, 100, 150 or 300) and, 6 times in 10, a power-up capsule falls: catch it with the ship. Half of
them are the bubble shield (it takes one hit, or fades after 20 s); the others double seeds (two side by side) or
rapid seeds (three at a time, faster) for 10 s.

Each new wave starts a little lower and marches a little faster, in another formation (the full block, a castle with
battlements, two wings, an arrowhead, a checkerboard). Every 4th wave a big mouse mothership sails over the shelters
instead: it drops spreads of three peas (and, below half its strength, aimed crumbs) and takes 24 seeds (+8 per extra
player); popping it gives 500 points. The game is over when every hamster has been hit, or when the formation reaches
the moon (THEY LANDED!).

Every number is in src/game.h (the tuning table):

| Rule | Value | Where |
|---|---|---|
| the ship | 1.5 px/frame, its centre in 20..300 (the stage), hit box 21x16 | SHIP_SPEED, SHIP_MIN_X, SHIP_MAX_X, SHIP_HALF |
| the seed | 5 px/frame up, one on screen per player | SEED_SPEED, MAX_SEEDS |
| the formation | 5 rows x 11 columns, 20 px apart (columns) and 16 (rows), from x 52, y 48 at wave 1 | ROWS, COLS, COL_W, ROW_H, FORM_X0, FORM_Y0 |
| the march | 2 px a step, 8 px a drop; a step every 32 frames with every mouse there, down to every 2 frames with two left (in proportion), the last mouse 3 px every frame | MARCH_DX, MARCH_DY, MARCH_FULL, MARCH_MIN, MARCH_LAST_DX |
| each wave | 6 px lower (at most 30), 5% faster (for 8 waves) | WAVE_LOWER, WAVE_LOWER_MAX, MARCH_WAVE, MARCH_WAVE_MAX |
| the play band | 8..312 (the mice turn there; every width plays the same 320 columns) | BAND_L, BAND_R |
| points | 30, 20, 20, 10, 10 by row | ROW_POINTS |
| the mice's shots | a drop every 48 frames at wave 1 (3 fewer each wave, at least 24), at most 3 on screen (+1 per extra player); the crumb 1.0 px/frame (aimed), the pea 1.6, the jelly 1.25, 4% faster each wave (10 waves) | BOMB_RELOAD, BOMB_LIMIT, BOMB_SPEEDS, BOMB_WAVE |
| the shelters | four wedges of 24x16 px at y 168, x 40, 112, 184, 256; a crater of radius 2 and two nibbles per shot | SHELTER_*, CRATER_R |
| a hit | one hit and the hamster is out (no lives); the bonk 60 frames; the house 8-frame hit-stop when it ends the whole run (with others still playing nothing stops) | POP_FRAMES, HIT_FREEZE |
| the saucer | 15 s into a wave, then every 20 s while 8 mice or more are left, 1 px/frame at y 30; 50/100/150/300 points (30/40/20/10%) | SAUCER_* |
| power-ups | 60% of the shot saucers drop one (falls 0.75 px/frame), half of them the bubble shield (one hit or 20 s); double or rapid seeds 10 s (rapid: 3 seeds, 7 px/frame, one every 10 frames) | POWER_*, SHIELD_ODDS, RAPID_*, SHIELD_FRAMES |
| the mothership | every 4th wave; 24 seeds (+8 per extra player); 0.75 px/frame in 48..272; three peas (0.5 px/frame apart) every 60 frames, every 42 below half (and an aimed crumb every other spread); 10 points a seed, 500 the pop | BOSS_* |
| the waves | WAVE n for 90 frames (the mice fly in, no seeds yet), 90 frames between two waves | INTRO_FRAMES, CLEAR_FRAMES |
| the landing | a mouse's body at line 192 (the ships' line): game over | INVADE_Y |
| medals (1 player) | bronze 500, silver 1500, gold 3000, pearl 6000 | MEDAL_SCORES |

**The medals** were tuned with the human-like bot (`botskill=E`: it looks less far ahead, now and then misses a shot
coming, misjudges its aim by up to E px and hesitates to fire), 8 runs each (a full wave 1 is worth 990 points): a
beginner (E = 10) scores 40..1530, half of the runs past 500 (bronze: half of the first wave); E = 8 170..5810;
E = 6 1660..4340, most past 1500 (silver: into wave 2); good players (E = 2 and 1) 70..10930 and 2310..8880, half and
5 runs in 8 past 3000 (gold: the first mothership, wave 4). Pearl (6000, about wave 8) takes a long, clean game.

**The bot** (`--opt bot=N`: tests, screenshots and card frames) plays from the world: for every place it could slide
to (every 3 px) it plays the next 72 frames ahead (its slide at the ship's speed, every shot's fall, the shelters
stopping them) and keeps the safe places; of those it picks the one where a seed would hit something (the lowest mice
first, the saucer, the mothership; a capsule to catch), not too far; it fires when a seed from where it stands would
hit. With 2-4 bots each keeps to its own lane. `botstop=S` makes it stop dodging once its score reaches S (the tests'
game over). The perfect bot clears 14 waves (3 motherships) in 20000 frames and is never hit.

## Screen and layers
320x240 at 60 Hz, fixed step. The house layout (docs/art-direction.md "Backgrounds"):

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the sky, one colour per line (raster callback): deep navy at the top to violet at the moon's horizon; a 2-line dither step every 8 lines | - |
| BG4 | the stars: faint ones and two twinkle groups (their colours set every frame) | drifting slowly |
| BG3 | space decor, faint, no outline: a nebula wisp, a far moonlet; in the wide margins a ringed planet, a purple planet with craters, a moonlet and a comet | fixed |
| BG2 | the big cheese moon at the bottom (its curve) and the four cheese shelters: their 24 tiles are drawn every frame from the world's bitmaps (eroded pixel by pixel) | fixed (shakes on a hit) |
| BG1 | the house UI kit: the logo, the title's lines, the panels, WAVE n, MOTHERSHIP!, the messages, PAUSED | fixed |
| OBJ | the ships (P1 OBJ 0; P2-P4 OBJ 1, 4, 5: the fur recoloured), the seeds, the power-ups and the bubble (OBJ 0), the mice, their pops, small digits and the mothership's bar (OBJ 2), the kit's digits, glyphs and medals (OBJ 3), the shots and the saucer (OBJ 6), the mothership (OBJ 7, whitened for a few frames when hit) | |

**Widescreen** (docs/spec.md "Widescreen"): the game opts in (`max_width`, `WIDESCREEN_hamsterhavoc = 1`). The play
(the formation's travel, the shelters, the ships) stays in the central 320 columns at every width; a wider screen (427
for 16:9, 520 for phones) shows more space on both sides: the moon's curve going down, a ringed planet on the right, a
purple planet on the left, a comet and a moonlet further out. The moon and the decor are "stage panoramas" (576 px
from stage x -128) so no width shows them twice; the stars repeat every 512 px. The saucer comes in from the margins.
A state saved at one width loads at another.

## Screens (the house flow: no "get ready", the title is the only menu)
title (the logo, the hamster's ship bobbing between two marching mice, a seed flying up, the player slots, PRESS A TO
PLAY with the A glyph, BEST, the join line, the copyright line) -> P1 presses A (or B, X, Y, Start, Left, Right; a tap)
-> WAVE 1: the formation flies in -> play (the score at the top; Select, or Start, pauses)
-> a hit: the ship pops (or the mice land) -> game over (the banner, the panel sliding up: score, best / new best, the
medal, the wave reached; 2-4 players: the ranking by score with medals) -> one press (or a tap): back to the title
(the same players start again with A).

## Controls
| Input | Action |
|---|---|
| d-pad Left / Right | move the ship |
| A (B, X, Y too), held | fire: a seed as soon as the last one is gone |
| A (or B, X, Y, Start, Left, Right) on the title | start |
| Select (or Start in a run) | pause / resume |
| pads 2-4: A or Start on the title | join (B leaves) |
| A, Start or Select on the game-over panel | back to the title |

### Touch (docs/spec.md "Touch")
The game reads the touches itself (`rs_game.touch_native = 1`): the phone apps pass it the fingers and draw no buttons.
Touch is player 1, alongside the pad (players 2-4 join with pads).

| Touch | Action |
|---|---|
| a finger down anywhere | the ship slides toward its x at its normal top speed (1.5 px a frame) and stops under it; it fires by itself while the finger is down (A held) |
| a finger that moves | the ship follows it |
| a tap (a finger lifted) on the title | start |
| a tap on the game-over panel | back to the title (after the same lock as A; a finger left down from the run does not count) |
| the app's pause button | pause / resume (it presses Start) |

The finger's x is a stage x (the screen's x minus the margin), so the same finger plays the same at every width. Nothing
derived from a touch is kept between frames: the save states need nothing more. `tests/touch_test.sh` (in the check, at
every width) plays scripted fingers through the headless runner and compares with the pad's runs.

## Up to four players
Pads 2-4 join with A on the title (B leaves). Everyone defends the same moon at once, each with their own ship, seeds
and score (the ships pass through each other); the mice shoot at all of them (one more shot on screen per extra
player) and the mothership takes 8 more seeds per extra player. A hamster hit is out (its bubble shield aside); the
others play on, nothing pauses; the run ends when all are out. The hamsters' fur: golden (P1), white, grey and ginger
(tools/build_assets.py swaps the fur ramp of make_art.py FUR_P; the ship, the dome and the eyes stay). The HUD shows
each score (3-4 players in the corners); the results rank everyone by score (the same score, the same place) with
medals.

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation. The ship: a silver
saucer hull with three lights, a glass dome with a glint, the hamster inside (round head, ears, the house eyes, pink
cheeks; puffed cheeks and squeezed eyes as it fires, a blink now and then, X eyes and a cracked dome when hit), a seed
launcher on top. The mice: round grey-blue mice with big ears and antennae, pink mice in bubble helmets waving, little
mint mice with star antennae, two march frames each, a mischievous grin; they pop into a sparkle burst, then sparkles
and crumbs. The shots: a cheese crumb, a green space pea, a wiggly jelly bean. The saucer: a cheese wheel with
blinking lights, a fat grey mouse at the wheel in a dome. The mothership: a big violet saucer with blinking lights and
three crumb cannons, its captain in a cheese crown under a big dome. The shelters: cheese wedges with holes and a rind,
a mouse hole underneath. The moon: cheese with holes, curving away at the screen's sides. The title logo: HAMSTER in
cheese gold, HAVOC in space violet.

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects (main.c sound_init): a little "pip" for a seed,
a bubble pop and a sparkle for a mouse, a bonk with a wobble for a hit, the march beat (a soft woody note played C, E,
G, E: one step, one note, quicker as the mice thin out), a plip for a falling shot, the saucer's warble, a quick run up
for the saucer popped, E-G-C for a power-up, a "blup" for the bubble, a splat on the cheese, a clonk on the mothership
and a big puff when it pops; the house sounds for the menus, a wave (the ding), the medals (the medal
jingle) and the game over. The lengths keep the samples under the 64 KiB guideline with the house set.

Music: "Moon Patrol Picnic" (tools/make_music.py): D lydian (the raised fourth gives the major key its floating, starry
colour), speed 6, A A' A B (about 31 s): the house pluck twinkling high, a bouncing octave bass, the soft pad, the
breathy lead leaping by fifths; A' lifts the tune an octave and doubles it; B (B minor, A, D, E) turns home. Two
alternatives (tools/make_music_alt.py -> music-alt/alt1.mod, alt2.mod; the owner picks): "Orbit Bossa" (a lazy space
bossa nova in A minor) and "Cheese Rocket" (a bouncy chiptune march in G with drums). To switch, call
make_music_alt.py OUTDIR N from tools/build_assets.py instead of make_music.py.

## Tests
`make hamsterhavoc-check` (about 3 minutes): the rules alone (tests/test_rules.c: waiting for the first press, the
intro, the march step and its beat, the drops and turns at both edges (the edge is the outermost mouse's), the
speed-up as the mice thin out and from wave to wave, the layouts, the seeds (one on screen, double, rapid), a seed
popping the bottom mouse of its column, the shelters bitten from below and from above and dug through, the mice
nibbling them, the bubble shield taking exactly one hit, one hit ending the run (the hit-stop, nothing moves after),
2 players: one out, the other on, nothing paused, the end with the last one; the landing, the saucer's points and its
capsule, the mothership and its spreads, the touch slide, determinism and the bot), the libretro loader, the smoke tests (flow, start inputs, the intro and the march,
moving and firing, the game over and back to the title, pause, save RAM, joining and leaving, 4 players, the mothership,
no strict-mode warning, the bot, determinism with 1 and 4 players), the touch tests, the UI screens (tests/ui_test.sh: the
title with 0-3 players joined, play, pause, 4 players, the mothership, the game over with a medal, the results), the save
states (the title's lobby, a run, the mothership, 2 players, 4 players, paused, the game over, the results) and their
audit, all at 320, 427 and 520 columns, states across widths, and the 22 languages (tests/i18n_runs.txt).
