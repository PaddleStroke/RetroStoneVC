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

Polar bears are separate moving hazards. They enter above the screen and slide
downhill three world pixels per frame faster than the penguins (1.8 screen pixels
per frame relative to the camera), leaving a wake behind them. They overtake the
entire lineup and leave below the screen without looping or teleporting. Seeded
passing lanes account for nearby gates and fixed-obstacle escape routes; unsafe
combinations defer the next bear. Every player checks the same actual bear
position. Bears freeze with the course when the last penguin falls.
Swept ellipse collisions check relative motion each frame, ending the run
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
smooth acceleration, fixed-height goat motion, exact relative bear speed from
top to bottom, off-screen bear retirement, swept bear and fixed-obstacle
collisions, the exact missed-gate frame, absence of success events on a
miss, frozen course motion, valid scoring, steering, twelve two-minute courses
with four ordinary-input bots, shared-screen visibility at identical x, title
continuity, actual keyboard input, pause, SRAM, libretro and save-state replay.

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
