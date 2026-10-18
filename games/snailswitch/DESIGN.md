# Snail Switch

A lively garden snail races along a curving vine. A flips between the upper and lower surface in a visible 22-frame curl, avoiding exposed thorns, hungry caterpillars and amber sap. Caterpillars patrol 18 pixels either side of their seeded anchor over 240 frames, with two inchworm poses and facing matched to motion. Every fruit must be collected on its vine side (in course order); passing one uncaught immediately eliminates that player. Each player has independent fruit claims.

The title renders the actual frozen starting position. A starts the existing world and camera; its first press does not change sides. Up selects the upper side and Down selects the underside. Held directions remain on that side; a request during a curl completes after landing. A/B/X/Y remain optional toggle buttons. Select or Start pauses. Joined players share the full 320x240 scene and same course speed, retaining full-size characters staggered 27 pixels apart.

The vine, leaves, hazards and fruits use one continuous world coordinate. The course is written ahead by a seeded director (see "The course" below); its first thing is 450-510 world pixels ahead, giving an initial safe stretch. Obstacles affect only one vine surface; the other is always open. Speed starts at 2.4 pixels/frame and gradually rises to a maximum 3.8 over 24,000 pixels. Even the front player has over 50 frames of visible warning at maximum speed.

One point per 24 pixels travelled; fruits add five and are mandatory. The solver (below) proves that every fruit can be collected and every hazard avoided, even with caterpillar patrol at maximum speed. Collision uses the visible exposed side and a 21-pixel horizontal half-width. Switching continuously leaves the danger surface. A crash triggers only the crash sound, freezes the victim briefly and drops them out below the scene. Other players continue; after the last falls, the course freezes and the house results panel ranks scores.

Art: 32x32 expressive feeler-eyed snails, spiraled pink/blue/yellow/green shells, shaded cylindrical vine, curved ovate leaves with varied angles and branching veins, woody hooked thorns, sticky teardrops and a full-height misty forest with shaded branching trunks, clustered canopy, ferns and bushes. Code-drawn sprites use the house kit. Foreground is drawn as runtime indexed tiles, with coherent world-space attachments. Four-channel house MOD music and synthesized action/reward/hit sounds.

Best scores use versioned checksummed battery RAM. Mutable game/drawing/audio/lobby state is registered for save states; battery scores remain outside snapshots.

Validation: full libretro, keyboard/lobby/pause/retry/SRAM, deterministic screenshot, UI, save-state and static audit checks. Physics tests replay sixteen five-minute four-player courses with the bot's ordinary Up/Down decisions, require survival to the capped speed, compare every world frame and verify frozen starts, continuous flips and failure events.

## The course

**Why it changed (the owner's playtest):** "The fruits are always before an obstacle, and always at the same distance
from that obstacle. It makes the game feel repetitive: it's like a repetition of empty, fruit, obstacle, and repeat. It
should be more random." The course used to be one unit repeated every 260 px (+-30): a fruit, then 80 px later a hazard
on the same side. Every fruit was followed by a hazard on its side at exactly 80 px.

**Now** the director (src/course.c) writes the course ahead of the snails as a chain of small **segments**, each ONE
idea with its own random parameters, picked by weight for the difficulty. The rules are the same: Up/Down choose a
surface, the curl takes 22 frames, a hazard hits only on its side, and every fruit must be caught on its side (in
course order, each player for itself): a fruit passed uncaught puts that snail out.

| Segment | The idea | Its random parameters | From |
|---|---|---|---|
| breather | nothing: the vine and its leaves | its length (80-130 frames, 60-100 hard) | after 4-10 things (easy) to 11-17 (hard) |
| lone fruit | a fruit with nothing after it | side | the start |
| lone hazard | a thorn, caterpillar or sap, no fruit | side, kind (caterpillars commoner with the distance) | the start |
| fruit trail | 3 to 5 fruits along one side; later a guard hazard opposite | count, side, every gap, guard | the start |
| zigzag | 2 or 3 fruits alternating sides: quick double flips | count, first side, every gap | the start (trios from 6000 px) |
| fruit after | a hazard, then a fruit just after it on its side: flip back for it; later with a fruit opposite before | side, kind, gap, lead fruit | 1500 px |
| slalom | 3 to 5 hazards alternating sides | count, kinds, every gap | 2250 px |
| caterpillars | 2 or 3 caterpillars: on one side with a fruit half-way (its safe window, timed against their patrols), or alternating | count, layout, phases, gaps | 4500 px |
| sap stretch | 3 to 5 sap drops on one side, sometimes fruits opposite | count, side, gaps, fruits | 3000 px |
| greedy fruit | a fruit between two hazards on its side: in and out quickly | side, kinds, gaps | 9000 px, at most one in 8 segments |
| fruit then hazard | the old unit, now at a varying distance | side, kind, gap | the start |
| fork | a hazard and a fruit nearly level, on opposite sides | which comes first, kind, gap | the start |

**Rhythm.** Every gap is drawn at random from a range written in FRAMES at the course speed of the place
(src/tuning.h: px = frames x speed), so the speed ramp and the difficulty ramp add up instead of fighting. Segments come
in **phrases** of 2 to 4, linked closely (36-60 frames, 25-38 hard), with a longer gap after a phrase (70-110, 44-72
hard) and a breather now and then. The director never repeats a kind twice in a row (nor across a breather), never a
pair of kinds met in its last 12 transitions, and makes the kinds of its last 3 segments 4x rarer. The first segment
(a lone fruit or hazard, a trail, a fork or a fruit-then-hazard) puts its first thing at world x 450..510: 334..394 px
ahead of the leading snail, just past a 4:3 screen's edge, at every width.

**Difficulty** rises with the distance (0..256 over 24 000 px, with the speed: 2.4 to 3.8 px/frame): the ranges of
every gap slide from their easy to their hard bounds, the harder kinds come in, trails and slaloms grow longer. It is
**measured** by the solver below (presses per 10 s and the timing windows per 30 s of a run).

**The course buffer.** The world holds the course from 160 px behind the scroll to the next segment past 800 px ahead
(world.it, at most 48 things; about 20 at most in practice): the director's state and the buffer are part of the world
(save states). It never looks at the screen's width: the same seed gives the same course at 320, 427, 520 and 560.

**Proofs and measures** (`build/host/snailswitch_test_course`, tests/test_course.c with src/plan.c, part of
`make snailswitch-check`). The solver steps the game's own rules (hero_step) over every state a snail can be in (its
side, its curl's frame, whether it holds the fruit it is passing: 88 states) and frame by frame proves each stream
completable; then a careful player keeps its side until it must change and presses in the middle of the frames that
still work (its window), counting only frames after the thing it reacts to has entered a 4:3 screen.

Results (1000 seeds; "before" is the old generator, measured the same way):

| | before | after |
|---|---|---|
| distinct segment-kind sequences, first 30 / 60 / 120 s | 1 / 1 / 1 (always fruit, hazard, repeated) | 1000 / 1000 / 1000 |
| fruits followed within 120 px by a hazard on their side | 100% (always 80 px) | 14% (1423 distinct fruit-to-next-hazard distances) |
| fruits with nothing for 200 px after them | 0% | 55% |
| pairs of kinds met again within the director's memory | n/a | 0 of 61 419 |
| completable streams (3 min; leading snail / the 3 behind on 250 seeds) | not measured | 1000 / 1000; 750 / 750 |
| tightest timing window (>= 3 required), least reaction from the 4:3 edge | not measured | 5 frames; 31 frames |
| the bot (5 min; old: reads the world, new: sees the 4:3 screen) | 200 / 200 alive, score 3802 (headless, seeds 1-10: all 3805) | 1000 / 1000 alive, mean score 3660 (headless, seeds 1-10: all alive, mean 3661) |

Measured difficulty (the careful player, per 30 s): 4.8 presses per 10 s with a 48-frame mean window (0-30 s, 2.4-2.7
px/frame), 6.8 and 38 (60-90 s), 9.2 and 28 (120-150 s, 3.7-3.8 px/frame), then a plateau (9.4 and 27). The tightest windows sit in
the "fruit after", caterpillar, greedy and trail segments at full speed. The bot (src/plan.c, `--opt bot=N`) plans with
the same 88-state model over 60 frames, from what a 4:3 screen shows (the things up to x 320, never the director), and
presses 20 frames before holding its side would stop working (at least 4 before the last moment that works).

## Widescreen
Snail Switch is a side-scroller: it opts in (`max_width` = 560, `WIDESCREEN_snailswitch = 1`) and a wider screen simply reveals more of the vine: a third of the extra width behind the snails, two thirds ahead. 320 (4:3) is unchanged pixel for pixel. The owner's decision: with all of the extra width ahead the snails sat too far left before a long empty stretch. The stage origin is `WIDE_LEFT(width - 320)` (game.h: 36 px at 427, 67 at 520, 80 at 560, 0 at 320): the snails race that much further right on the screen (x 116 + the origin, the others 27 px behind) and the screen shows the rest further ahead. Behind them the vine is drawn like the rest (the foreground canvas starts at the screen's real left edge, its camera the scroll less the origin): the passed leaves, hazards and fruits, and before the start (the title too) the vine behind the starting line. The world is width-independent: the vine, its leaves, the hazards and their fruits are placed in one world coordinate by the seeded director (the course buffer is filled to 800 px past the scroll whatever the width), and the rules never look at the width, so the same inputs give the same game, bots included, at 320, 427, 520 and 560. The start of a run is the same at every width: the first thing on the vine sits 334..394 px ahead of the snails, just past a 4:3 screen's edge and already in view from the first frame on a wider one (no longer run-up on a wide screen; test_scenery.c asserts it). Seeing further ahead makes it a little easier; that is accepted, not compensated. The foreground is drawn every frame for the screen's tile columns (its hazards and leaves culled at the real right edge, at most 370 runtime tiles at 560, within its 512-tile range). The misty forest is 512 px wide: up to 512 columns its 64-tile map never shows twice; beyond, its map is 128 tiles with the second half the first one mirrored (the same tiles h-flipped: no VRAM, seamless joins). The UI layer is scrolled by -hu_ui_x(), so the title, panels, results and pause are centred, and the slot and results icons add hu_ui_x(); the score chips go to the screen's centre (1 player), halves (2) or corners (3-4, with their tags; the UI map is 128 tiles beyond 512 columns). Multiplayer shares the one wider screen, the snails where they were. A state saved at one width loads at another (`state_loaded` sets the maps up again). Tests: tests/test_scenery.c checks the foreground at 320/432/520/560 (no pop-in, the 4:3 picture extended, the tile budget, the vine behind the start in the third on the left); `make snailswitch-check` runs the libretro, smoke, UI and state tests again at 427 and 520, then tests/wide_test.sh (picture widths, strict mode, the same log at every width, the view: the third on the left, P1's screen x, that of 320 plus the third, and the next hazard's distance on the screen, that of 320, from `--opt view=1`; live resizes, states across widths).

## Tall screen (docs/spec.md "Tall screen", the zoomed recipe)

A phone held upright: `rs_screen_zoom(ZOOM_H)` (560 lines, game.h) first in game_init. The app scales the 560 lines to
the display's height and crops the sides: 258 columns on a 1080x2340 phone, 240 on a 1080x2520 one, 280 on a 2:1
one, so the snails are as big as in landscape (4.5x on a 1080x2520 phone; the old 320 x 700 strip: 3.4x). The 240
lines of the stage are the same pixels; a 240-line screen (landscape, 4:3, wide) is unchanged.
- **The columns shown end at the stage's right edge** (`rs_screen_focus_x(320)`): the hazards and fruits come in
  there as at 4:3, so every snail sees exactly as far ahead (the warning is the same); only the forest behind the
  snails is cropped. When the last snail would not fit with 8 px behind it (2 players on a 9:21 phone, 3-4 players),
  the play shows every column (`rs_screen_fill(0)`): panning instead would cut the warning ahead below that of 4:3
  (the rule above), so that shared screen is the one exception.
- **One look: the title and the panels stay zoomed** (the owner, on his phone: "when in menu the view is different from
  when in game"). They keep the play's view (the same 560 lines, the columns ending at the stage's right edge; the
  title pans left only as far as needed to show every joined snail) and are laid out in the columns shown with
  games/common/src/house_tall.h when fewer than 320 show (`ht_narrow`): the logo on two lines (SNAIL over SWITCH, the
  kit's logo tiles moved to centre each word; draw.c title_logo) with the hint wrapped under it, BG1's rows 0..13+ pinned
  over the canopy (`rs_bg_pin`), clear of the vine; the slots under the snails, then the prompt, BEST, the join line
  and the credits stacked to the stage's bottom; the game-over panel narrower (each value under its label, the panel as
  wide as the retry line), the 2-4 player ranking, and PAUSED centred in the columns shown. Every language fits at
  1080x2520 (240 columns), 1080x2340 and 720x1440; landscape and 4:3 are the same pixels as before.
- **The HUD at the screen's top**: 1-2 players, the scores' digits centred in the columns shown, over the canopy; 3-4
  players, P1 / P2 on a first row and P3 / P4 under them (`hu_score_tags(players, 20)`, BG1's top 32 lines pinned
  there: `rs_bg_pin`).
- **The stage sits 3/10 down** (`rs_screen_origin_v`: 96 lines above it, 224 below). **Above: the canopy** goes on
  (the forest's first 64 lines mirrored up from the stage's edge, then down: no seam), darker toward the top. **Below:
  a forest pool**: the forest and the vine, its leaves, fruits and hazards mirrored in still water (BG3 and BG2 read
  upward from the stage's last line, their palettes tinted toward a deep teal, a one-pixel ripple drifting down; the
  224 lines never repeat a line).
- The raster draws these lines; the UI is hidden there (window 1) but in the HUD's band (the title: the logo's), the sprites below the stage
  (a falling snail leaves at the stage's bottom, as before). Line 0 puts the stage's windows and palettes back on
  every screen (a state saved on a tall screen may hold the margins' settings), BG2's and BG3's scroll on a tall one.
