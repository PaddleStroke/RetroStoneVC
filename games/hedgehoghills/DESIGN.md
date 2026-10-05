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
