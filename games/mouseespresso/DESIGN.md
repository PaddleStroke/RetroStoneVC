# Mouse Espresso

An endless café escape inspired by Jetpack Joyride's continuous one-button
flight. The mouse wears an espresso machine: hold A to rise on steam, release
to coast and fall. Its chrome pack, enamel scarf, steam and café art follow
the shared house style.

The title displays the actual starting course and mice resting on the counter.
Starting preserves that world and camera, with no position or seed change.
The first press begins the run; holding it continues into thrust. There is no
separate ready screen. The flight range spans y16–218, with the counter at y232;
there is no ceiling trim or blocked upper part of the room.

One to four players share a full-width screen. Their horizontal slots are spaced
28 pixels apart, and all surviving mice use exactly the same travel speed and
camera. Vertical movement, hits, biscuits and scores remain independent. Mice
never collide with one another. Eliminated mice fall and scroll out of view while
survivors continue. The title lobby, results, pause and checked best-score saves
use the house kit. Shared biscuits disappear after all surviving players have
collected or passed them, allowing each player to receive their own reward.

## The owner's playtest and the redesign (October 2026)

"The game is a bit boring. The obstacles are too far apart and look too much the same. It's too easy: I made 3000+
in my first run. All the obstacles are similar in shape, so most of the time you just jump above." The first version
had seven big hazard families 360 px apart; its own bot never crashed (14,000+ points after ten minutes on every
seed). The redesign keeps the flight (the feel), the café, the biscuits and every house rule, and replaces the
obstacles: **many small café items of varied sizes, close together, arranged by parametrised patterns that make the
mouse weave up and down**, chained by a seeded director along a rising difficulty target, and measured by a solver.
Targets: a decent first run ends around 300-800; 3000 takes real skill.

## Obstacles (src/course.c: shapes, motion, collisions; src/paint.c: the code-drawn art)

All small, all café, drawn at run time in the house style (dark outline, three tones lit from the top left, the
scenery palette) into the playfield layer. Sizes vary per item; collision boxes follow the drawn silhouettes.

- **Hanging from the ceiling, on strings** (the string is solid too): a **lamp** (coral or steel shade, 14-24 px
  wide, a lit bulb), a **tea bag** (with its paper tag on the string), a **cup** hung by its handle, a **coffee
  filter** (a paper cone in a steel ring). Their strings set where their bottom is: from 10 px to 150+ px down.
- **Standing on the counter**: **stacked cups** (1-12 cups of 11 px, nudged by hand, coffee in the top one),
  **sugar-cube towers** (8-px cubes, one or two wide, up to 11 high), a chunky steel **milk jug** (22-38 px).
- **Floating**: a **spoon** (level, rising or falling, 18-28 px), a **coffee bean** (8-12 px), a **cake slice**,
  a **croissant**.
- **Moving** (each has a telegraph, and a pattern always leaves a lane that is safe even when it moves):
  - a **steam jet** from a counter or ceiling nozzle: JET_TELEGRAPH (36) frames of a red light, puffs and wisps that
    rise as high as it will fire (harmless), then it rises in 6 frames, holds, falls back; then rests;
  - a **swinging ladle** hanging from a hook: the handle and the bowl sweep a pendulum arc (sampled with the mouse's
    motion between frames, like the old mixer blades); the space it left is free;
  - a **bouncing sugar cube**: up from the counter to its apex and back, with its shadow on the counter;
  - the **filter's drips** (past UNLOCK_DRIPS, only over a wide gap): a drop every 100-130 frames falls to the counter.
- **Biscuits** (+5) sit inside the patterns as risk and reward: in a narrow passage, at the edge of a gap, in a
  resting jet's column, under the ladle's swing, above a bouncing cube, along a cluster's line, in an arc on a
  breather. Each mouse collects its own (one bit per player on the biscuit).

## Patterns (src/course.c, one function each; parameters drawn inside the difficulty k = 0..255)

| Pattern | The idea | Parameters (k easy -> hard) |
|---|---|---|
| alternate | gaps at the top, middle or bottom, alternating; a half gate halfway through long moves | 3-7 gates, gap 100 -> 46 px, spacing from the flight's physics x slack |
| narrow | narrow passages between a ceiling item and a counter item, 1-2 items deep | 1-3 passages, gap 70 -> 40 px, heights +-70 |
| stairs | counter items climbing or ceiling items descending, a parallel line at higher k (a diagonal corridor) | 4-7 steps of 16-30 px, climb 1.3 -> 1.9 px/frame |
| zigzag | a corridor up then down (or down then up) | 2-3 legs, amplitude 40-100 px, gap 94 -> 62 px |
| cluster | floating spoons, beans, cakes and croissants with one safe line through them (sealed at both ends) | 140-330 px long, line gap 90 -> 62 px, slope +-40 px |
| corridor | a short tunnel of ceiling and counter items | 60-195 px long, gap 86 -> 46 px |
| jets | 2-3 steam jets from the counter and the ceiling, firing in turn; their lanes alternate | period 150 -> 108 frames, firing 44 -> 64, lane 76 -> 52 px |
| ladles | 1-2 swinging ladles, a counter item keeping a lane under the swing | length 52-117, period 150 -> 104, lane 78 -> 54 px |
| bouncers | 1-3 bouncing sugar cubes, a ceiling line keeping a lane above them | apex 150 -> 96, lane 80 -> 66 px, 40-60 px apart |
| breather | open floor, one small item, a biscuit arc | 220-300 px |

Every pattern places its own lead-in: the frames needed to move to its first gap **with the flight's own physics**
(course.c move_frames: starting while drifting the wrong way at 1.5 px/frame), times a slack that shrinks with k
(1.33 -> 0.97). After a pattern that does not hold the mouse to one height (a breather, jets, ladles, bouncers),
the next lead-in assumes the mouse may be anywhere, at the ceiling or on the counter. Past UNLOCK_MIXED (6500 px), a
counter stack may be a steam jet of the same height instead (mixed dynamic hazards).

## The director (src/course.c director_next; every number in src/tuning.h)

- Seeded (the run's seed: the frame of the first press; `--opt seed=N`); integers only (the same course on every CPU).
- The course is generated GEN_AHEAD (1100) px ahead of the travel and dropped KEEP_BEHIND (320) px behind it, into a
  ring of COURSE_N (256) items saved with the world (save states need nothing else). It never looks at the screen:
  the same course at every width; the first obstacle stands at x 360 (+-2) on every course, as at 320.
- **The target** k(d) = K_START + (K_END - K_START) d / (d + K_HALF_PX): 18 at the start, about 120 at a score of 300,
  175 at 800, 215 at 3000 (the patterns' geometry stops tightening at K_GEOM_MAX = 200; the speed goes on);
  plus **waves** of WAVE_PX (2200 px, about 12 s): k rises by WAVE_AMP through a wave, then a
  **breather**. The first EASY_PX (1300 px, about 9 s) stay at k <= 34, yet the mouse weaves within seconds (the
  first gates are at x 360 and every 100-200 px after).
- The pattern is drawn by weight among the unlocked ones, never one of the last two; each new hazard arrives with a
  boost (jets at 1500 px, clusters 2300, zigzags 3000, ladles 3700, bouncers 4600, drips 5400, mixed 6500: all by a
  score of about 550). k jitters +-14 around the target.
- The travel speed rises smoothly from 2.3 to 3.5 px/frame over SPEED_RAMP_PX (22000 px).

## Measured difficulty and the validator (tests/solver.c, tests/validate.c; Blueberry Tumble's method)

The **solver** flies the game's own physics (play.c hero_fly) through the game's own course (course_hits) and
searches "hold / release" frame by frame, each input lasting at least 4 frames (a tap): a depth-first search with a
memo of dead states, and a breadth-first sweep when the search runs out of budget; every path it reports is flown
exactly. The **careful player** keeps its input while that does not crash within 150 frames; when it would, the
frames on which changing still leads on alive form runs (the **windows**): it takes the latest comfortable run (>= 6
frames, else the widest) and changes in its middle. When it is squeezed (a window under 3 frames), it reconsiders:
changing earlier, then its last six decisions (their other runs), up to 120 frames back.

Each pattern instance is scored 0-100 from its careful path: 25% the tightest window (3 frames -> 1, 14+ -> 0), 20%
the mean window, 20% the changes per second (4/s -> 1), 10% the vertical speed, 10% the moving hazards, 15% the travel
speed. `make mouseespresso-check` runs the validator (tests/validate.c, 16 processes) and fails unless:
1. every stream is passable (thousands of seeds over the check and `mouseespresso-validate`);
2. every window is >= 3 frames, in the one-player slot and in the four players' slots (a share of the seeds);
3. the measured score per 3000-px band rises, follows the director's k (correlation >= 0.5), breathers are easier
   than their neighbours (>= 90%), and the start is easy (no window under 6 frames before x 1660) yet weaves (two
   changes of input in the first 5 s);
4. the ring never fills, no biscuit lies inside an item, the same seed flies the same path.

## The bot (src/bot.c: a first-run player made of code, from the screen only)

`--opt bot=N`: it reads the playfield picture (the same paint.c code that draws the screen; only the 320-px view, so
it plays the same at every width; as it looked over the last 64 frames: it remembers where a jet fired or the ladle
swung, and sees the jets' wisps), its own sprite, and the feel of its own steam; it never reads the course's list.
It plans "hold / release" through what it sees with a safety margin and a few frames of slack it relaxes only when it
must, and its hand is a little late: a decided change happens on each frame with chance 33% (never more than 8 frames
late; about 2 frames on average, a human's spread).

**Measured** (tests/bot_test.sh in `make mouseespresso-check`, 48 seeds): min 73, 25% 412, **median 529**, 75% 935,
90% 1317, max 2615, none at 3000 (the check requires the median in 300-800 and none at 3000). The old course's bot
never crashed: 14,000+ after ten minutes on every seed. Validator (`make mouseespresso-check`, 128 seeds to a score of
1780, 16 of them in the four players' slots): all passable, the tightest window 3 frames (0.3% of the changes at 3-4
frames), the measured score per band rising 20 -> 46, correlation with k 0.61, breathers easier 94%.

Biscuits add five points; every twelve world pixels travelled adds one; clearing a pattern rings a ding. One life,
an endless course, medals and a battery best score. Physics uses fixed 60 Hz Q16 arithmetic, and the course is
generated with integers only; its generation has no art dependencies, so the setting can still be replaced while
keeping the flight engine. Behind the game-over panel the camera stays where the run ended.

Artwork: the mouse, its steam, the biscuits and the puffs are code-drawn by tools/make_art.py; the café items are
code-drawn at run time by src/paint.c (the same code gives the bot its eyes). No generated bitmap artwork is used.
Music and sound use games/common.

Validation (`make mouseespresso-check`): tests/test_physics.c (every item's shape and motion, the jets' telegraph,
swept collisions, the director's course the same whether generated in steps or at once, sorted, the first obstacle
at x 360, every item and pattern, the unlocks, the ramp, thrust/coast, bounds, four mice, determinism, each mouse's
own biscuits), the validator, the bot's distribution (tests/bot_test.sh), the views, the libretro test, the input,
lobby, results, pause and SRAM flow (tests/check_game.py), deterministic save states, the state audit, widescreen
and the language test. tests/review_scenes.c renders three moments of every pattern kind.

Build with make mouseespresso-check / mouseespresso-validate (thousands of seeds) / mouseespresso-windows /
mouseespresso-screenshots.
Windows output: dist/windows/MouseEspresso.exe, statically linked SDL2.
Code MIT; art, music, sound and design CC BY-NC-SA 4.0, as stated in LICENSE.

## Widescreen (docs/spec.md "Widescreen")

The game opts in (`max_width` = 560, `WIDESCREEN_mouseespresso = 1`): 320 x 240 is unchanged pixel for pixel; at
427 (16:9), 520 or 560 (phones) **the cafe is revealed wider: a third of the extra width behind the mice, two thirds
ahead** (the owner's decision: with all of it ahead the mice sat too far left before a long empty stretch). The shared
camera starts `WIDE_LEFT(width - 320)` px further left (game.h: 36 at 427, 67 at 520, 80 at 560, 0 at 320), so the
mice fly that much further right on the screen and see the rest further ahead; seeing further makes it a little easier,
which the owner accepts (no compensation). The cafe behind the mice is drawn like the rest: the items already
passed, the back wall and the counter, and before the start (the title too) the cafe behind the starting line.
- **The same course**: the café items and the biscuits are drawn as far as the screen goes
  (the course is generated ahead of the travel, never of the screen); the counter runs the whole width.
  tests/wide_test.sh compares the whole logs (scores, game overs, the final state and its hash) of 1-player and
  4-player bot runs at 320, 427, 520 and 560. The start of a run is the same too: the first hazards sit at the same
  distance from the mice as at 320 (a wider screen shows them from the first frame), no longer run-up. Its view check
  (`--opt view=1` logs the view at exit) asserts the third on the left, the mice's screen x (that of 320 plus the
  third) and the distance to the next hazard on the screen (that of 320), on the title, at the start and in runs.
- **The back wall** (BG3) goes on with its mirror image (the same tiles h-flipped on a 128-tile map: no VRAM, seamless
  joins), so a 560-px screen never shows a stretch twice; the furniture layer (BG2) is drawn over the whole width (a
  pixel buffer as wide as the widest screen).
- **The UI is centred**: the UI layer scrolls by -hu_ui_x() and the game's UI sprites (the title's player icons, the
  results' icons, the sparkles) add it; the score and the players' scores keep their centred places.
- **Save states load at any width**: `state_loaded` (draw_relayout) compares the width the maps were laid out for
  (draw.laid_w, saved) with this one and sets this width's maps up (every frame draws the whole screen again).
- `make mouseespresso-check` runs the libretro test, tests/check_game.py (strict mode included) and the state tests
  again at 427 and 520, then tests/wide_test.sh and the language test at 320, 427 and 520;
  `make mouseespresso-screenshots` adds title/play/gameover-427/520.png and play-4-players-520.png
  (tools/wide_shots.sh).

## Tall screen (docs/spec.md "Tall screen", the zoomed recipe)
A phone held upright: `rs_screen_zoom(ZOOM_H)` (540 lines, game.h) first in `game_init`. The app scales the 540 lines to the
display's height and crops the sides: 231 columns on a 1080x2520 phone, 249 on a 1080x2340 one, 270 at 720x1440; the
mice are 4.7x on a 1080x2520 phone (3.4x before). The flight is the same (the ceiling and the counter stay where they
are); the camera pans (`rs_screen_focus_x`) so the columns shown start `TALL_BEHIND` (32) px behind the first mouse: the
most of the course ahead (199 px of the 248 at 4:3 on a 1080x2520 phone, 217 on a 1080x2340 one).
- **A taller café**: the stage sits low, 40 lines of the counter's wooden front under it (the backdrop: a shadow under
  its top, darker with depth). Above it the back wall goes on (draw.c `raster_out`): 24 lines of planks, then the wall's
  upper storey (BG3's map rows 32..45 on a tall screen: the wall's top 112 lines, half a period along, so boards stand
  over the windows and windows over the boards), then planks (BG3's row 14 repeated) up to the ceiling's beam at the
  screen's top (dark wood, a lit edge, its shadow on the wall), a little darker toward it; the strings of the hanging
  lamps, cups and tea bags carried up to the beam (BG2's line 0 repeated).
- **The HUD on the beam**: BG1's top 44 lines pinned at the screen's top (`rs_bg_pin`) while the mice fly, the score
  sprites at `rs_screen_top()`, centred in the columns shown; 2-4 players: the tags on row 4, under the frontend's corner
  buttons, spread over the columns shown.
- **One view** (the owner, on his phone: "when in menu the view is different from when in game"): the title, the
  pause, the game-over panel and the 2-4 players' results stay in the run's zoomed view (`rs_screen_fill(1)`, the same
  columns from `TALL_BEHIND` px behind the first mouse: the title is the run's first frame), never the full width. Play
  is one shared screen for 1-4 players, so nothing needs every column. When fewer than 320 columns show
  (`ht_narrow()`), the screens are laid out in them with games/common/src/house_tall.h: the game-over panel narrower
  (each value under its label, wide enough for the retry line), the ranking, the banners, PAUSED centred. The title
  (draw.c `title_narrow`): the logo on two lines (one at 720x1440) is drawn in the UI rows 18..28, the band behind the
  mice resting on the counter (else unused), turned by whole tiles to centre it in the columns shown, and pinned
  (`rs_bg_pin`) 16 lines above the stage, the raster showing BG1 there; the stage's rows 0..17 hold, from the top, the
  hint, the players' slots, the prompt, BEST, the join line (each wrapped) and the credits on two lines, up to two rows
  between them while they fit (the long Polish, Greek or Japanese lines: fewer); the counter and the mice stay clear.
  Checked in en, de, el, fr, ja, pl, ru, tr and uk at 1080x2520 (231 columns), 1080x2340 and 720x1440.
- Landscape and 4:3 are the same pixels as before (checked at 2340x1080 and 320x240). The frontend's touch zone (hold
  anywhere) is the whole screen.
