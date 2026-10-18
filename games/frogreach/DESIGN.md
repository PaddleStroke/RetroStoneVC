# Frog Reach

A one-button endless crossing game in a quiet swamp. Hold a face button to extend
the frog's tongue toward the next lily pad. Release while its tip is over the
pad to latch on and pull the frog across. Too short or too long means a splash.
A release within four world pixels of the fly catches it for a perfect bonus.
Inspired by Stick Hero's simple distance judgement; the tongue, swamp, frog,
moving lily pads, code, sound and artwork are original.

Pads score 10; a perfect fly adds 5. The first two pads are 64 and 56 pixels wide;
later widths vary by seed between 24 and 64, narrowing with distance. Distances
also vary. From pad 4, every third pad drifts sideways by 8–16 pixels. A moving
pad carries its resting frog; a caught target continues drifting through the leap.
Waiting too long sinks the current pad: 5.5 seconds initially, falling to 3 seconds later.
The tongue grows 4 pixels/frame, rising to 6 with distance. The jump is a 28-frame
arc; the tongue stays visibly tethered to the caught pad. One life, endless score.

The simulation is fixed-step Q16. Release validation uses the same position and
width shown by the renderer. The bot uses ordinary held/released button input
and the current target's predicted next position; it does not bypass misses.
Twelve course seeds test short, overlong, held and accurately released reaches,
perfect bonuses, determinism and four-player scoring.

The title draws the actual starting pond, pads and frog. Starting preserves the
course and camera position. A brief starting tap leaves the frog resting safely;
a continuing hold begins extending after a two-frame guard. Later presses extend
immediately.

Up to four players join on the shared title. Each has an independent course view
and progress; falling behind a rival never eliminates a player. Two use top and
bottom views, three a top half and two lower panels, four quadrants. Two-player
panels use the full-width, 32-pixel frog. Narrow panels show a wider world area
with a compact 16-pixel frog. Multiplayer overlays only a small player label and
score in each view, leaving the pond visible without status panels or repeated
instructions. Pause freezes all worlds and cameras.

Art is code-drawn by tools/make_art.py using the shared house_style kit. Big eye
bulbs, folded hind legs, cream throats and a long mouth make the frog recognizable.
The swamp has hazy reeds, clouds and water glints; runtime foreground pixels draw
exact lily-pad widths and a three-shade pink tongue. Flies flap, catches sparkle,
and failures splash. The font, title, lobby, results, medals, pause, sounds and
tracker music use games/common, matching Leady Squid and the other house games.

Build: make frogreach / frogreach-windows / frogreach-check / frogreach-screenshots /
frogreach-art. Windows output: dist/windows/FrogReach.exe, statically linked SDL2.
Code MIT; art, music, sound and design CC BY-NC-SA 4.0, as stated in LICENSE.

## Widescreen (docs/spec.md "Widescreen")

The game opts in (`max_width` = 560, `WIDESCREEN_frogreach = 1`): 320 x 240 is unchanged pixel for pixel; at 427
(16:9), 520 or 560 (phones) **the swamp is revealed wider: a third of the extra width behind the frog, two thirds
ahead** (the owner's decision: with all of it ahead the frog sat too far left before a long empty stretch). A crossing
seen from the side: the stage origin stays 0 and each camera starts a third of its view's extra width further left
(`WIDE_LEFT`, game.h; a full view: 36 px at 427, 67 at 520, 80 at 560, 0 at 320; a half view a third of its own extra
columns), so the frog sits that much further right and sees the rest ahead; seeing further makes it a little easier,
which the owner accepts (no compensation). Behind it the pond is drawn like the rest: the pads already crossed, the
water and the reeds, and before the start (the title too) the pond behind the first pad.
- **The same course**: the pads beyond the third ahead are drawn, with their flies, as long as the view still shows
  them (perch_pos: the course never looked at the screen); the rules, the timings and the bot are unchanged.
  tests/wide_test.sh compares the whole logs (scores, game overs, the final state and its hash) of 1-player and
  4-player bot runs at 320, 427, 520 and 560. The start of a run is the same too: the first pads sit at the same
  distance from the frog as at 320 (a wider screen shows them from the first frame), no longer run-up. Its view
  check (`--opt view=1` logs the view at exit) asserts the third on the left, the frog's screen x (that of 320 plus the
  third) and the next pad's distance on the screen (that of 320), on the title, at the start, in a run and in P1's
  view of a 2-player game.
- **Multiplayer**: the views split the whole width (2 players: top and bottom, 3: a top half and two lower panels, 4:
  quadrants). A panel is compact (the 16-px frog, the half zoom) when it is narrower than 3/4 of the screen, as at 320,
  so a 520-px quadrant keeps the compact frog and simply shows more of its pond. The right-hand views use map columns
  64 on (128-tile maps); each view's text (P1 ..., PAUSED) sits in the UI columns of its place on the screen.
- **The scenery**: the reeds' and clouds' panorama (BG3) goes on with its mirror image (the same tiles h-flipped on the
  128-tile map: no VRAM, seamless joins), so a 560-px view never shows a stretch twice; the pond (BG2) is drawn over
  the view's whole width (a pixel buffer as wide as the widest screen).
- **The UI is centred**: the UI layer scrolls by -hu_ui_x() and the game's UI sprites (the title's player icons, the
  results' icons, the sparkles) add it; the score is centred on the screen.
- **Save states load at any width**: `state_loaded` (draw_relayout) compares the width the maps were laid out for
  (draw.laid_w, saved) with this one and sets this width's maps up (every frame draws every view again).
- `make frogreach-check` runs the libretro test, tests/check_game.py (strict mode included) and the state tests again
  at 427 and 520, then tests/wide_test.sh (widths, the same game, `--resize`, states across widths) and the language
  test at 320, 427 and 520; `make frogreach-screenshots` adds title/play/gameover-427/520.png and
  play-4-players-520.png (tools/wide_shots.sh).

## Tall screen (docs/spec.md "Tall screen", the zoomed recipe)
A phone held upright: `rs_screen_zoom(ZOOM_H)` (576 lines, game.h) first in `game_init`. The app scales the 576 lines
to the display's height and crops the sides: 246 columns on a 1080x2520 phone, 265 on a 1080x2340 one, so the frog
is 1.3x the size of the old 320-column strip. The columns shown are centred (no pan): the frog (stage x 56) sits 20 px
from their left edge on a 9:21 phone and the next pad, up to 224 px ahead, stays in them. The 240 lines of the stage
are the same pixels; the stage sits low (`rs_screen_origin_v`: 272 lines of sky above it, 64 of pond below).
- **The HUD at the screen's top**: the score (kit sprites at `rs_screen_top()` + 8) and the hint and the flies/pad
  line (BG1's top 72 lines, `rs_bg_pin`) while one frog plays; a hint wider than the columns shown goes on two rows.
- **One view**: the title, the panels and two frogs' split screen stay zoomed like the play (never the letterboxed
  full width), laid out in the columns shown by games/common/src/house_tall.h: the hint wrapped from row 11, the
  slots, the prompt right under them (above the frog, who sits at the left), BEST, the join line and the credits
  wrapped; GAME OVER and the score panel narrower (each value under its label; draw.c `over_panel`: the panel wide
  enough for the retry line too); the ranking narrower. Two frogs: their P1/P2 tags in the columns shown, each view's
  frog and next pad in them as for one frog. Only 3-4 frogs playing show every column (`rs_screen_fill(0)`): the
  lower panels are 159 px wide with the compact frog 32 px in from each panel's left edge, so zoomed the left panel's
  frog would be cut. The split screens' views keep the stage's 240 lines with two more viewports over the margins
  (the sky with its clouds and birds above, the water's colour below).
- **The sky's decor** (BG4, made at start-up: draw.c `decor_init`, its tiles shared: no VRAM past the guideline): a
  far range of snowy mountains rising behind the hills, lit from the left, its rock and snow hazier toward its foot
  (the raster sets their four colours line by line from the sky's, dithered over two lines), scrolling at an eighth
  of the camera; clouds above it in a band of their own scroll, drifting; two flocks of birds (16x8 sprites, flapping
  and gliding) crossing the columns shown, already on screen at the start. The sky's gradient goes on above the
  stage, dithered (no bands).
- **The pond below**: one view draws the pond's canvas down to the screen's bottom (BG2's map 64 rows tall there): the
  ripples go on, no repeat; the raster darkens its blues with depth from the stage's own (no seam at line 240).
- Rules, timings, the course and the camera are unchanged. Landscape and 4:3 are the same pixels as before (checked
  at 2340x1080 and 320x240).
