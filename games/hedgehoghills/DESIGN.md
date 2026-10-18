# Hedgehog Hills

A round hedgehog races home through rolling woodland hills before the frost catches it.
Hold a face button to curl up, accelerate downhill and dive in flight. Release before the
uphill section to preserve momentum and launch. Endless, one control, one life, a best score
to beat. Inspired by the slope-and-release feel of Tiny Wings, with original code, artwork,
terrain, audio and a four-player race. Reference: https://andreasilliger.com/.

## Rules and tuning

The deterministic terrain consists of 384-pixel cosine hills, with seed-dependent crest
heights (118-146 pixels) and valleys (198-220). The same continuous surface drives physics
and the pixel grass fringe, avoiding a separate visual approximation. Ground movement
uses Q16 momentum: curling accelerates downhill but brakes uphill. Releasing on the convex
uphill slope allows a ballistic launch. Air gravity is 0.16 pixels/frame squared uncurled,
0.65 curled. Good downhill landings give a 0.9 pixel/frame boost and an increasing combo;
rough landings lose 28% of speed and spill up to three apples. Flight and feet are drawn
with the house kit's squash, stretch, and readable 32-pixel sprite cells.

The frost starts 360 pixels behind the origin and moves at 4.8 pixels/frame, gradually
rising to 8. It freezes players it reaches. There are three apples per hill, each worth
five points. Score is distance / 16 plus apples and smooth-landing bonuses. There are
bronze/silver/gold/pearl medals at 100/250/500/1000. Meadow, autumn, and winter palettes change
every 14,000 pixels. The vertical camera follows large jumps without hiding the hero.

One to four players use the same hills. Multiplayer gives each player an independent
camera: two players use top/bottom views, three use a top half and two bottom panels,
and four use quadrants. Each panel follows its own hedgehog horizontally and vertically,
clips its sprites and shows its own score, apples and frost margin. Players only drop out
when the frost reaches them; a rival moving ahead cannot eliminate them. Highest score wins; equal
scores share a place. Each player collects their own apples. The title lobby, results,
pause, font, buttons, medals and sounds use games/common. A returns from results to the
title, retaining joined pads, as in the other games.

## Controls and builds

X / Z / C / V / Up on keyboard, or any gamepad face button: hold to roll, release to launch.
Enter: start / pause. Select, Backspace or Esc: pause / resume. Players 2-4 join with A on
their pad on the title and leave with B. F11 toggles fullscreen, F12 captures a screenshot.

`make hedgehoghills`, `make hedgehoghills-windows`, `make hedgehoghills-check`,
`make hedgehoghills-screenshots`, and `make hedgehoghills-art` use the normal SDK workflow.
The Windows output is `dist/windows/HedgehogHills.exe`, statically linked to SDL2.
Art is code-drawn by `tools/make_art.py` using house_style; music uses house_music.

## Verification

Physics tests compare coasting, continuous holding, and timed slope control across twelve
seeds; check determinism, continuous surfaces, and four-player scores. Flow tests cover
held input, frost deaths, pause, lobby join/leave, retry, SRAM persistence, strict console
limits, and identical pixels in repeated one- and four-player runs. Multiplayer rendering
checks verify visible characters thousands of pixels apart, independent cameras and seasons,
sprite clipping, sudden high jumps, frost bands, pause and return to the shared lobby.
The SDK's save-state
tests verify replay in the current and a fresh process and reject corrupt saves. State
audit checks all mutable objects. Screenshots cover title, play, pause, death and four-player
results. Native Windows startup is checked independently of the Linux headless runner.

Code MIT; game art, music, sound, design and screenshots CC BY-NC-SA 4.0, as described in LICENSE.

## Art revision (2026-10-02)

The owner selected C, Spiky Sprinter: a lean hedgehog with a long snout and bold
triangular quills. Four code-drawn options and motion previews are retained in
`docs/art-options/`; their reusable drawing functions are in `tools/hedgehog_designs.py`.
The advancing winter front now uses a 96-pixel band of scattered frost, a gently
uneven snow edge, and drifting snow wisps. The original collision position is preserved.

## Widescreen (docs/spec.md "Widescreen")

The game opts in (`max_width` = 560, `WIDESCREEN_hedgehoghills = 1`): 320 x 240 is unchanged pixel for pixel; at
427 (16:9), 520 or 560 (phones) **the hills are revealed wider: a third of the extra width behind the hedgehog, two
thirds ahead** (the owner's decision: with all of it ahead the hedgehog sat too far left before a long empty stretch).
A side-scroller: the stage origin stays 0 and the camera (draw.c cam_x; split.c each view's scroll) starts a third of
the view's extra width left of the world's scroll (`WIDE_LEFT`, game.h: 36 px at 427, 67 at 520, 80 at 560, 0 at
320; a half view a third of its own extra columns), so the hedgehog runs that much right of HERO_X 96 and sees the
rest ahead; seeing further makes it a little easier, which the owner accepts (no compensation). Behind it the hills
are drawn like the rest: the terrain, the apples and the frost coming from the left, and before the start (the title
too) the ground behind the starting line.
- **The same run**: the terrain's columns (one per 8 px, up to 70 at 560; the frost's tiles after them), the apples of
  every hill in view and the distant forest go to the screen's edge; the world never looked at the screen (its scroll
  is the leader's x - HERO_X). tests/wide_test.sh compares the whole logs (scores, game overs, the final state and its
  hash) of 1-player and 4-player bot runs at 320, 427, 520 and 560. The start of a run is the same too: the first
  hills and apples sit at the same distance from the hedgehog as at 320 (visible from the first frame). Its view
  check (`--opt view=1` logs the view at exit) asserts the third on the left, the hedgehog's screen x (that of 320
  plus the third) and the next apple's distance on the screen (that of 320), at the start, in a run and in P1's view
  of a 2-player game.
- **The distant forest** (BG3) goes on with its mirror image (the same tiles h-flipped on a 128-tile map: no VRAM,
  seamless joins), so a 560-px screen never shows a stretch twice.
- **Multiplayer** (split.c): the views split the whole width (2 players: top and bottom, 3: a top half and two lower
  panels, 4: quadrants), each following its own hedgehog; a view is compact (its hedgehog 48 px from its left edge)
  when it is narrower than 3/4 of the screen, as at 320. The right-hand views use map columns 64 on; each view's text
  (score, frost margin, PAUSED, SMOOTH) sits in the UI columns of its place on the screen.
- **The UI is centred**: the UI layer scrolls by -hu_ui_x() and the game's UI sprites (the title's player icons, the
  results' icons, the sparkles) add it; the score chips go to the screen's centre and corners (the kit).
- **Save states load at any width**: `state_loaded` (draw_relayout) compares the width the maps were laid out for
  (draw.laid_w, saved) with this one; another width sets this width's maps up and the next frame draws the UI again
  (PAUSED too).
- `make hedgehoghills-check` runs the libretro test, tests/check_game.py (strict mode included) and the state tests
  again at 427 and 520, then tests/wide_test.sh and the language test at 320, 427 and 520;
  `make hedgehoghills-screenshots` adds title/play/gameover-427/520.png and play-4-players-520.png
  (tools/wide_shots.sh).

## Tall screen (docs/spec.md "Tall screen", the zoomed recipe)

A phone held upright: `rs_screen_zoom(ZOOM_H)` (520 lines, game.h) first in `game_init`. The app scales the 520 lines to
the display's height and crops the sides: 222 columns on a 1080x2520 phone, 240 on a 1080x2340 one; the hedgehog is 4.8x
on a 1080x2520 phone (3.4x before). The run is the same; the camera pans (`rs_screen_focus_x`) so the columns shown start
40 px behind the hedgehog (182 px of the hills ahead on a 1080x2520 phone, 200 on a 1080x2340 one; 224 at 4:3).
- **The stage 150 lines down** (in proportion on a shorter screen): the sky above it is where the hedgehog flies, so the
  camera rises only when it flies past that sky (the stage's camera rule, moved up by the lines shown above).
- **The HUD at the screen's top**: BG1's top 56 lines pinned there (`rs_bg_pin`), the score sprites at `rs_screen_top()`,
  centred in the columns shown.
- **Decor**: the raster draws the lines past the stage (draw.c `raster_out`): above, the sky carried on (deeper toward
  the top) with clouds drifting in two bands, slower than the forest (BG3's cloud strip, rows 32..34 on a tall screen:
  the panorama's top rows without the cloud on the crest); below, the earth: BG2's map is 64 rows tall there and
  terrain() fills rows 30..63 with the soil (frost included). The frost's flakes and wisps fall over the whole height.
  Line 0 puts the stage's layers and scrolls back, so a 240-line screen is unchanged.
- **One look**: the title, the game-over panel, the ranking and the pause stay in the zoomed view of the play (the same
  520 lines, the same columns, from TALL_BEHIND px behind the hedgehog's place), laid out in the columns shown with the
  kit's `house_tall.h` only, at its default rows, like every house game (draw.c, when `ht_narrow()`): `ht_banner`,
  `ht_gameover_*`, `ht_results_*`, `ht_retry_line_at`, `ht_pause`; the title `ht_logo` (HEDGEHOG over HILLS, row 2)
  and `ht_title_draw` from row 15 (the slots, the prompt, BEST, the join line, the credits). The title's hint has no
  room there (the run's HUD shows HOLD / RELEASE at the start); the hedgehog (scene, not UI) floats in the sky above
  the stage. The frost's flakes fall over the whole height on the panels too.
- **Fill or fit**: only the split play (2-4 hedgehogs) shows every column (`rs_screen_fill(0)`): its views split the
  width (two columns of views from 3 players) and each needs the hills ahead. The split
  screen (split.c) keeps the stage's 240 lines (the layout of the whole height scaled to them; BG2 and BG3 are 32 rows
  then), the sky above its views and the earth below them (two layer-less views: the raster's colours). (It crashed on
  a tall screen before: the views spanned the whole height.)
- Landscape and 4:3 are the same pixels as before (checked at 2340x1080 and 320x240).
