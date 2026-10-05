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
427 (16:9), 520 or 560 (phones) **the hills are revealed wider, to the right**: a side-scroller, so the stage origin
stays 0. The hedgehog runs at the same screen x (HERO_X 96), the frost comes from the left as before, and the screen
shows more of the hills ahead; seeing further makes it a little easier, which the owner accepts (no compensation).
- **The same run**: the terrain's columns (one per 8 px, up to 70 at 560; the frost's tiles after them), the apples of
  every hill in view and the distant forest go to the screen's edge; the world never looked at the screen (its scroll
  is the leader's x - HERO_X). tests/wide_test.sh compares the whole logs (scores, game overs, the final state and its
  hash) of 1-player and 4-player bot runs at 320, 427, 520 and 560. The start of a run is the same too: the first
  hills and apples sit at the same distance from the hedgehog as at 320 (visible from the first frame).
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
