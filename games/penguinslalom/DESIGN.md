# Penguin Slalom

An endless downhill slalom through a snowy forest. Left / Right steer with
lateral inertia between paired red and blue flags. The original penguin, its
sardine-tin skis, scarf colours and fish sprites are preserved.

The course is viewed from above at a shallow angle. Snow covers the entire
screen, with powder drifts and evergreen trees along the edges. There is no
lake, mountain horizon or approaching avalanche. Short flag poles have bases
and shadows planted on the crossing line. Gates and terrain move up the screen
as the penguins ski downhill; twin tracks record each penguin's actual path.

Passing between the flags scores a gate. Missing either side fails on the exact
frame the penguin crosses the gate line. A pole collision also ends the run.
A miss emits only the hit event: no gate ding, fish award or style bonus. The
course freezes immediately when the final penguin falls; results follow the
brief crash presentation. Other players continue if they are still racing.

Centre passes collect fish for five points. Close, valid passes build a style
multiplier. Distance scores one point per twelve world pixels. Travel speed
rises smoothly from 3.3 to 4.7 pixels per frame over 24,000 world pixels. The
starting pace matches the previous version at about 1,000 points (measured
3.29 pixels per frame, seed 4). Gate
openings vary between 104 and 152 pixels early on, narrowing to 74–122 later.
Seeded spacing varies between 219 and 331 world pixels; short/long pairs keep
endless course lookup constant time. Their centres follow a winding course,
leaving enough time to steer at speed.
The previous automatic jumps and per-player speed boosts are removed.

Faceted dark rocks protrude from the snow with uneven cream caps and snowdrifts
around their bases. Rocks, evergreen trees, icy holes and horned mountain goats
appear between gates. Most sit beside the turning line; some obstacles on longer,
straighter stretches require a deliberate sidestep. Goats trot left and right
along a fixed contour, with no vertical motion of their own.

Polar bears are separate moving hazards that land anywhere across the course,
each announced about a second ahead at the very spot it lands on (below, "Polar
bears"). Swept ellipse collisions check relative motion each frame, ending the run
immediately with CRASH! and only the hit sound. The original penguin and flag
graphics are preserved.

One to four players share a single 320x240 view, the same course and exactly the
same downhill speed. Their positions are staggered along the slope so that
penguins steering to the same x remain identifiable. Gate checks use each
penguin's actual position on that shared course. Scoring, steering and failures
remain independent; penguins do not collide with each other.

The title shows the real starting lineup and course. Starting retains its seed,
positions and camera. Lobby labels and prompts sit below the lineup. P1 starts;
players 2–4 join with A and leave with B. Start/Select pauses everyone. Results
return to the title, preserving the lobby and checked battery best scores.

Art is code-drawn with house_style.py and the runtime terrain renderer. House
UI/audio supplies the font, logo, lobby, pause, rankings, medals and music.
Simulation uses integer Q16 at 60 Hz, with seeded gates independent of artwork.
Trail history, simulation, presentation, UI and audio are saved for deterministic
replay; battery best scores remain outside save states.

Validation covers the measured faster starting pace, varied widths and spacing,
smooth acceleration, fixed-height goat motion, the bears' warning, landing spot,
exact relative speed and retirement, swept bear and fixed-obstacle
collisions, the exact missed-gate frame, absence of success events on a
miss, frozen course motion, valid scoring, steering, twelve two-minute courses
with four ordinary-input bots, the bears' fairness proof, shared-screen visibility
at identical x, title continuity, actual keyboard input, pause, SRAM, libretro and
save-state replay.

## Polar bears
The owner's playtest: bears only fell on the sides, and fast. Now they land anywhere
on the course, and each is announced at its landing spot first. All the numbers
are in src/tuning.h.
- **The warning** (BEAR_WARN_EARLY 60 frames, shrinking with the distance to
  BEAR_WARN_LATE 50, never under BEAR_WARN_MIN 40): at the very spot the bear will
  land, the bear's whole footprint is dashed on the snow from the first frame, a
  shadow grows in it and darkens, powder slides down its lane from the top of the
  screen, and a round sign on a short post (navy rim, cream face, red "!") stands
  over it, flashing red for the last frames. Snow colours and the house outline,
  unlike the flags (cloth on poles) and the trees; the sign is 17 px wide, so it
  reads at 1x and at the phone's scale. A low rumble with a powder hiss plays as it
  appears (panned to it), a soft whump when the bear lands, throwing up a ring of
  snow puffs. The marker itself is harmless.
- **Where**: the spot keeps its place on the screen, BEAR_LAND_GAP 75 world px
  above the uphill-most penguin (screen y 67 with one player: its sign under the
  HUD; never above y 43). The bear then slides straight down its lane BEAR_SLIDE 3
  world px per frame faster than the penguins (1.8 screen px per frame), over the
  whole lineup, and leaves below the screen. Every player checks the same bear.
- **Anywhere, seeded**: a warning is either aimed at the racing line (the line
  through the gates' centres, where the bear will cross the penguins; within 40 px
  early, 12 px late, then further out on alternate sides, BEAR_STEP_OUT 12 px at a
  time, until a spot is fair) or anywhere across the course (centres 40..280: its
  body stays on the course's 24..296, never in the widescreen margins). The share
  aimed ramps from 30 % to 85 % over BEAR_RAMP 24,000 world px (the speed's ramp),
  the gap between warnings from 330..480 to 170..250 frames: more bears, closer to
  the line (measured: 2.4 times as many bears, 41 px from the line on average
  instead of 54). Up to BEAR_TRIES 12 seeded spots per warning, the first fair one
  taken; none fair: try again 20 frames on.
- **Fair, checked in the game** (play.c bear_fair, about 0.5 ms on a PC with 4
  players): over the course's 4-px columns, 4-frame steps and speeds (inertia:
  2 px per frame of change per step at most), from the moment the player has
  reacted (BEAR_REACT 16 frames after the marker) until 100 frames after the bear
  has passed the last penguin. Every "comfortable" state, one that can follow the
  course and the bears already warned never faster than 3 px per frame sideways,
  must keep a dodge of the new bear at the penguin's full 4 px per frame, ending
  comfortable again; the course 4 px wider for both, swept through, the new bear
  14 px wider and 2 steps longer at both ends; for every player of the lineup. That
  is the only rule: a bear may land on the racing line, cross a gate or pass a
  goat, as long as the dodge exists.
- **Fair, proved** (tests/test_fair.c, in the check): every bear the generator
  warns on seeded courses (1 to 4 players, runs from the start and from 9,000,
  18,000 and 30,000 px) is proved on the exact steering of world_step (the same
  integers, swept ellipses, gates and poles, the course's and the bears' own
  frames): every comfortable state (x and lateral speed; never faster than 3 px per
  frame, keeping 6 px from everything) keeps a path through the next gates and past
  the bear, holding Left / Right / nothing for 4 frames at a time (15 decisions a
  second), 2 px clear of everything and 8 px clear of the new bear. 264 bears, 1.73
  million comfortable states, none trapped, in about 10 s (`--long`, 5x: 1,301
  bears, 8.3 million states, none); of 120 random spots (600), every one the game
  accepts is proved fair (it turns down about 1 in 4, a few of them truly unfair).
  It also checks the warnings (>= 40 frames), that the spots cover the whole course,
  and the ramp (more bears late, nearer the line).
- **The bot** (bot_decide, a test hook) plays from what the screen shows: the
  gates and obstacles down to the bottom of the screen, the bears' markers (the
  spot; the time left, by the marker's size) and the sliding bears. It plans over
  the same columns, steps and speeds (inertia included, keeping 14 px of room where
  it can, planning at 3 px per frame) and holds Left / Right towards the plan.
  Its distances in world px, to a 20-minute cap (334,000 px). Before (the old
  side-lane bears, which it read from the game's state): 1 player, 200 runs, 192
  reached the cap, the 8 falls all on obstacles, the shortest 19,961; 4 bots, 50
  courses, 190 of 200 reached it. Now: 1 player, 100 runs, 91 reach the cap, the
  shortest 64,525 (3 falls on bears, 6 on obstacles); 4 bots, 25 courses, 83 of
  100 reach it, 10 % fall before 138,500 (9 on bears, 8 on obstacles).

## Widescreen (docs/spec.md "Widescreen")
The game opts in (`max_width` = 560, `WIDESCREEN_penguinslalom = 1`): 320 x 240 is
unchanged pixel for pixel; at 427 (16:9), 520 or 560 **the slope is wider, the
race is the same**.
- **A vertical scroller, centred**: `rs_screen_origin(rs_screen_margin())`
  (draw_init). The course keeps its 320 columns in the middle: the gates, their
  widths, spacing and winding, the obstacles, the bears, the steering limits
  (24..296), the scoring and the bots never look at the width. The HUD, the title,
  the panels and the results are the house kit's, centred; sprites are clipped to
  rs_screen_left()..right(); sounds pan by the screen x (the kit).
- **The wider scenery** (draw.c `forest`): the ground (BG2) is drawn each frame on
  a canvas as wide as the screen, on a 128-tile map beyond 320 columns. Each bank
  goes on as a powder berm, then a snowy forest: the same evergreens (four sizes)
  in five lanes a side, 24 px apart, every 40 world px down the slope, and powder
  drifts, all placed by world position and stage x (never by the width: a wider
  screen reveals more of the same forest) and scrolling with the course. The edge
  trees cut by the 320 screen show whole.
- **VRAM**: on a wide screen the ground's tiles are shared within a frame (one per
  distinct 8x8 picture: the forest's grid makes lone trees the same tiles), so 560
  columns use at most 467 ground tiles (423 at 320, unshared) and 882 tiles in
  all; the strict-mode runs stay clean at every width.
- **Multiplayer**: one shared view, as at 320: the penguins, their tags and
  scores where they were, the forest at both sides.
- **Save states load at any width**: the ground is redrawn every frame;
  `state_loaded` sets the ground's map up again for this width.
- **Tests**: the check runs test_libretro (16:9, Wide), test_views, check_game.py
  (at a wider width: the 320 columns of the course are the 320 picture, pixel for
  pixel, and the forest fills both margins) and the state test again at 427 and
  520, then tests/wide_test.sh: the picture widths, the same logs at 320, 427, 520
  and 560, width changes during runs (the same run, the picture of the new width)
  and states across widths. Screenshots: title, play and gameover at 427 and 520.

Build: make penguinslalom-check / penguinslalom-windows / penguinslalom-screenshots.
Windows output: dist/windows/PenguinSlalom.exe, statically linked SDL2.
Code MIT; art, music, sound and design CC BY-NC-SA 4.0, as stated in LICENSE.

## Tall screen (docs/spec.md "Tall screen", the plain tall screen)
A phone held upright: `rs_screen_tall(TALL_H)` (`RS_SCREEN_H_MAX`, 720 lines, game.h) first in `game_init`: the
display's height at 320 columns, every column shown, no pan (720 lines on a 1080x2520 phone, 693 on a 1080x2340 one,
640 on a 720x1440 one). The owner, after the zoomed version (512 lines, 219 columns panning with the penguin): "it
didn't need to be zoomed so much; it's a vertical course". The course is as wide as the screen (the gates and the
penguin 24..296, the penguin's sprite 8..312, the banks at both edges), so a zoom showing only the slope would gain
1.5% at most: the whole width it is, and the height shows more of the course. The 240 lines of the stage are the
same pixels, at the bottom of the screen (`rs_screen_origin_v`: the lines above it, none below).
- **One view**: the title, play, pause and the panels are the same picture (every column, so the kit's own layouts,
  unchanged); no `rs_screen_fill` / `rs_screen_focus_x`.
- **Fair**: the lines above show the slope behind the penguins (the gates passed, the trails, the bears sliding in
  from uphill); below the penguin the course shows exactly the 128 lines of 4:3, so the warning of the gates, the
  obstacles and the bears' landing spots is unchanged. Rules, speeds, the course and the bears are unchanged.
- **The canvas** (the ground, redrawn by the CPU each frame) covers the 320 columns by the whole height: 320 x 720 =
  230k pixels against 320 x 240 = 77k in 4:3 and 576 x 240 = 138k at 560 columns (the host's run time: 4:3 1.0x,
  560 columns 1.5x, the tall screen 1.8x). Only phones have a tall screen: the RetroStone2 and the other handhelds
  draw 240 lines, their cost unchanged. Its tiles are shared as in widescreen (one per distinct picture: at most
  about 770 of the layer's 1024 measured, 4 players), BG2's map 128 rows tall above 512 lines. The drifts and the
  bank's pines go on over the whole height; uphill of the start (the title shows it), pines scattered on both sides
  and a striped start arch (canvas pixels, gone once the race has moved on).
- **The HUD at the screen's top**: the score (kit sprites at `rs_screen_top()` + 8) and the gates/fish line (BG1's top
  48 lines, `rs_bg_pin`), centred; 2-4 players their tags on row 4 (from column 2), clear of the frontend's back and
  pause buttons in the top corners. BG1 is hidden past the stage elsewhere (its map would wrap). The snow's colour
  carries on above the stage (the raster).
- Landscape and 4:3 are the same pixels as before (checked at 2340x1080 and 320x240).
