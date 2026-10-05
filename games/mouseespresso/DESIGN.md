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

Seven hazard families give the café both route choices and moving hazards:

- Wooden stools, padded stools and chairs, with solid seats, legs and crossbars.
- Toasters with lever/light warnings and two visible, staggered launches.
- Hanging shelves carrying cups, jars or colourful crockery.
- Wooden mousetraps with cheese, spring coils, warning glints and snapping bars.
- Striped cat paws with tapered forelegs, four toes and pink pads, swiping from
  above or below. A 72-frame (1.2-second) warning slides the palm into view, then
  makes three small tentative strokes at 24–34 pixels of reach. These warning
  movements are harmless. A six-frame extension follows, with a short hold and
  a slower retreat. The paw withdraws completely between swats. Difficulty
  shortens only the hidden rest; the warning always keeps its full duration.
- Appliances: espresso machines and kettles with pressure/light warnings and
  timed steam bursts from their actual spouts. Steam grows, holds, then
  dissipates. Stand mixers have four rotating chrome blades, like a windmill.
- Long café tables, 152–208 pixels wide. The solid tabletop blocks the middle
  of the room; mice can fly above or below. Rear supports sit behind the flight
  lane and do not block the underneath route. Biscuit trails suggest either route.

The first seven obstacles introduce these families, with an espresso machine as
the first appliance. Later seeded blocks shuffle their order, variants and
heights. Hazards are 360 world pixels apart so mice can change height around the
wider tables and moving appliances even at maximum speed. Cat cycles and steam
cycles shorten with difficulty; mixer rotation accelerates. Paw collisions follow
the tapered foreleg and rounded paw through each motion step. Mixer collisions
sample the actual rotating blade segments and mouse motion, leaving empty space
between the blades passable. Steam collision follows the visible burst height,
including motion between frames. A fully withdrawn paw or extinguished steam
burst cannot hit a mouse. Furniture collision rectangles follow their silhouettes;
empty spaces beneath stools remain passable. Clearing a table scores only after
the mouse passes its far edge.

Toast starts in the toaster slots at y213. Two slices launch eight frames apart,
follow parabolic arcs, tumble and fall beyond the counter on either side.
Angles and heights vary deterministically for each launch. Toast collision uses
both the previous and current projectile position. Travel speed rises from
2.3 to 3.5 pixels per frame over five difficulty steps. Those steps shorten toast
flight time from 76 to 56 frames and launch periods from 114 to 84 frames,
increasing projectile speed and frequency while preserving readable arcs.

Biscuits add five points; every twelve world pixels travelled adds one. One life,
an endless course, medals and a battery best score. Physics uses fixed 60 Hz Q16
arithmetic. Seeded course generation has no art dependencies, so the setting can
still be replaced while keeping the flight engine.

Artwork is code-drawn by tools/make_art.py and src/draw.c. The existing mouse and
café palette remain, with four tilted toast poses and shaded runtime furniture,
rounded tabby paws, flowing steam and rotating chrome mixer blades.
No generated bitmap artwork is used. Music and sound use games/common.

Validation includes twelve two-minute, four-player ordinary-input runs, the full
72-frame harmless cat warning at every difficulty on both edges, tentative
movement reversals, fast swat and full-retreat checks, off/warning/on steam cycles, rotating blade
contacts and empty gaps, both table routes, launch origin and arc checks,
swept collisions, continuous thrust/coast, screen bounds,
shared camera and spacing, title continuity, actual keyboard input, lobby and
results flow, pause, SRAM persistence, libretro and deterministic save states.
tests/review_scenes.c renders each hazard family, cat poses from both edges,
espresso and kettle steam phases, mixer angles and every toast animation frame.

Build with make mouseespresso-check / mouseespresso-windows / mouseespresso-screenshots.
Windows output: dist/windows/MouseEspresso.exe, statically linked SDL2.
Code MIT; art, music, sound and design CC BY-NC-SA 4.0, as stated in LICENSE.

## Widescreen (docs/spec.md "Widescreen")

The game opts in (`max_width` = 560, `WIDESCREEN_mouseespresso = 1`): 320 x 240 is unchanged pixel for pixel; at
427 (16:9), 520 or 560 (phones) **the cafe is revealed wider, to the right**: a side-scroller, so the stage origin
stays 0. The mice fly at the same screen x and the shared camera shows the hazards, the biscuit trails and the
counter further ahead; seeing further makes it a little easier, which the owner accepts (no compensation).
- **The same course**: the furniture, toasters, paws, appliances and biscuits are drawn as far as the screen goes
  (obstacle_at, collectible_at: the course never looked at the screen); the counter runs the whole width.
  tests/wide_test.sh compares the whole logs (scores, game overs, the final state and its hash) of 1-player and
  4-player bot runs at 320, 427, 520 and 560. The start of a run is the same too: the first hazards sit at the same
  distance from the mice as at 320 (a wider screen shows them from the first frame), no longer run-up.
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
