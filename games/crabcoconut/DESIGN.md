# Crab Coconut

Move Left / Right to bounce your coconut with the crab's claws. Where it hits
changes the rebound angle. Break the raining shells before they reach the crab.
Missing your coconut ends your run; shells reaching the sand end the shared round.

The full 320x240 screen is one court, with walls at the screen edges. Coconut
centres bounce between x8 and x312 and can rise to y8. Solo crabs rest at y224
with their catch line at y209. Every player uses the same full 32x32 crab sprite,
including multiplayer. Multiplayer crabs are staggered by three pixels in height,
ending at y224 so the entire sprite fits; catch lines follow their actual claws.
The initial eighteen shells are scattered across x18–302 and y40–126, with no
overlaps, columns or shared rows. Score labels overlay the upper edge without a large HUD panel.
The expanded height increases the flight time between shells and crab.

Coconuts start at a total speed of 1.8 pixels per frame. Speed increases smoothly
with elapsed play time to 4.6 pixels per frame after two minutes. Rebounds preserve
that total speed: edge catches change direction without an abrupt speed boost.
Fixed-point vector normalization and four collision substeps prevent tunnelling.

One to four players share a single shell array and camera. Each has its own
crab, coconut, movement, score and life. A coconut rebounds only from its owner.
Shell damage is immediately visible to every player, and a destroyed shell scores
once for the player who broke it. Coconut outlines match the four player colours.
Crabs and coconuts can cross each other without collisions. Surviving players
continue after another player's miss.

Shells rain down individually from above the top edge at seeded random horizontal
positions and intervals. Each has its own falling speed, 0.035–0.055 pixels per
frame. Entry positions avoid overlapping nearby shells. Arrivals become more
frequent over time and when fewer than ten shells remain. An empty board gets
one falling shell immediately; it never refills with an ordered formation.
After a minute, occasional tougher shells require two hits.
If shared shells reach y196, every remaining player loses. Individual rebounds,
shell damage and score bonuses use ordinary steering inputs and seeded boards.
Shells render in front of the sand so they remain visible near the failure line.
After a round ends, remaining shells accelerate downward and continue behind
the results transition. A shell is removed only after its entire 16 px sprite
has passed below the screen at y248. This animation awards no extra points and
spawns no new targets.

A starts; Start/Select pauses all players. Players 2–4 join and leave on the
title, with P1 starting the run. Results return to the title with the lobby intact.
Best scores use the existing checked battery format. Simulation, renderer,
title, UI and audio are registered for deterministic save-state replay.

The original crab, coconut, shells, beach, music and house presentation are
preserved. Artwork is code-drawn using house_style.py and runtime court drawing.
Physics remains integer Q16 at 60 Hz, independent of artwork.

Validation covers scattered non-overlapping starts, individual arrivals and
empty-board replenishment, falling motion, full-size crab/claw alignment,
shell visibility across the sand and bottom edge, off-screen retirement,
gradual speed, rebound directions, full-court boundaries,
misses, low-shell failure, a shared shell hit scoring only once, independent
coconuts, twelve two-minute four-player courses, deterministic replay, one board
drawn on the full screen, actual controls, pause, SRAM, libretro and save states.

Build: make crabcoconut-check / crabcoconut-windows / crabcoconut-screenshots.
Windows output: dist/windows/CrabCoconut.exe, statically linked SDL2.
Code MIT; art, music, sound and design CC BY-NC-SA 4.0, as stated in LICENSE.

## Widescreen (docs/spec.md "Widescreen")

The game opts in (`max_width` = 560, `WIDESCREEN_crabcoconut = 1`): 320 x 240 is unchanged pixel for pixel; at
427 (16:9), 520 or 560 (phones) **the beach is wider, the court is the same**.
- **A fixed court, centred**: `rs_screen_origin(rs_screen_margin())` (draw_init). The walls stay at x 2-3 and
  316-317 and the top wall at y 2-3: the coconuts bounce between x 8 and 312 as before, the shells fall where they
  did, the crabs stand where they did; the screen shows stage x -margin .. 320 + margin, the beach beyond the walls.
  The one full view (rs_viewport_layout) now starts at stage x -margin: the sand layer's pixels are drawn in the
  view's columns (a buffer as wide as the widest screen, the grains in stage coordinates, so the court's sand is the
  same at every width), the score is centred on the view, PAUSED on the screen.
- **The wider beach** (make_art.py `hills_wide`; build_assets.py): BG3 becomes a 128-tile map of a 1024-px beach with
  its own tiles, the classic one pixel for pixel over the court (build_assets.py checks it): the sea, the sandbanks
  and a palm island every 128 px all round, the ripples in stage coordinates, so nothing breaks where the picture
  wraps. On the sand beyond the walls (theme.inc `theme_decor`, two more colours in the sand palette): a striped beach
  umbrella over its shadow, a towel and a starfish on the left; a sandcastle with its flag, a bucket and a spade on
  the right. The 427 screen shows the umbrella and the castle; 520 the towel, the bucket and the spade too.
- **The same game**: the rules never look at the width; tests/wide_test.sh compares the whole logs (scores, misses,
  game overs, the final state and its hash) of 1-player and 4-player bot runs at 320, 427, 520 and 560, changes the
  width three times during runs (`--resize`) and loads states saved at one width at the others.
- **Save states load at any width**: `state_loaded` (draw_relayout) compares the width the maps were laid out for
  (draw.laid_w, saved) with this one; another width sets this width's maps and the beach's tiles up again (every
  frame draws the whole screen anyway).
- `make crabcoconut-check` runs the libretro test, tests/check_game.py and the state tests again at 427 and 520 (the
  strict mode included: VRAM, sprites per line), then tests/wide_test.sh and the language test at 320, 427 and 520;
  `make crabcoconut-screenshots` adds title/play/gameover-427/520.png and play-4-players-520.png
  (tools/wide_shots.sh).

## Tall screen (docs/spec.md "Tall screen")

A phone held upright. **Why not the zoom (v2)**: the court is the whole 320 columns (its walls at the screen's edges,
the coconuts bouncing from x 8 to 312, the shells raining anywhere across it), and a zoomed screen crops the sides (240
of them at most at a usable height): it would hide play. So the game keeps `rs_screen_tall(RS_SCREEN_H_MAX)` (every
column shown, 320 x up to 720 lines) and composes the portrait around the court instead. The court's 240 lines are the
same pixels; a 240-line screen (landscape, 4:3, wide) is unchanged.
- **The court sits 2/5 down** (`rs_screen_origin_v`: 180 lines above it, 272 below on a 1080x2340 phone).
- **The HUD at the screen's top**: one crab, the score's digits (sprites at `rs_screen_top()`); several, the scores'
  row (BG1's row 1 pinned there: `rs_bg_pin`, one column in from the corner buttons).
- **Above: the sky** carried on from the court's top colour, deeper toward the top, with the sun, clouds and gulls.
  The shells still come in at the court's top edge (the sprites show above the court only in the HUD's band).
- **Below: the beach**: the court's sand carried on (flat: no colour band), with the umbrella and its striped towel,
  the sandcastle with its flag, the bucket and the spade, starfish, footprints toward the sea and a beach ball.
- **How**: the raster draws the margins (the backdrop: the sky, the sand); the sky's and the beach's things are BG4,
  drawn once for the screen's height (draw.c `tall_decor`: a canvas cut into at most 896 tiles, the same tile once,
  after the beach's tiles; its 64 x 64 map holds the sky's things just above line 256 and the sand's from 256). BG1-3
  and the sprites are hidden past the court (window 1) but in the HUD's band; line 0 puts the court's settings back
  on every screen. A state of another height draws BG4 again (`laid_h`, saved).
- **One view**: the view of rs_viewport_layout is the court's 240 lines, and a tall screen draws it without a
  viewport even with several players (at 320 columns the view is the whole court at x 0, the same scrolls).
