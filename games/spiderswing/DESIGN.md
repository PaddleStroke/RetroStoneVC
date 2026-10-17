# Spider Swing

An endless climb using release timing and steering in flight. A releases the
current web without changing the spider's position or velocity. Left / Right
steers only during free flight. An 88-pixel loose silk strand stays attached to
the spider, lifting overhead and trailing its movement. The visible strand is
the catch surface: touching any higher hanging flower pins the silk to it at once,
even when the body is far below. The released flower cannot catch again. Both
strand endpoints and the area swept between frames are checked against the
flower's 12-pixel radius. There is no narrow body zone or hidden flight timer.
The full attached web appears immediately with a blossom sparkle and catch sound.
The spider keeps its position and incoming velocity, and the rope eases toward
the normal swing length. Holding A does not repeat releases, and holding
a direction on a web never advances to another flower.

Swing motion uses a fixed-point rope constraint with gravity and a gentle
automatic pump. The pump stops near turning points so the swing can reverse
naturally. A release keeps the actual tangent velocity; it adds no launch boost.
Flight uses gravity and bounded horizontal acceleration. Attachment preserves
position and incoming velocity, with no phase matching or teleports. The camera
eases in both axes and keeps the complete spider visible below the HUD.

Seeded flower positions vary in height and horizontal position, including runs
in the same direction. Flowers remain fresh for seven seconds initially and
five seconds later. Catches score 10; dewdrops add 3. Bees appear after the
introductory climb, patrolling an outer lane away from the main transfer route.
There are no closing flytraps in this revision. A missed catch, bee collision,
or waiting until a flower wilts ends the run.

Art follows docs/art-direction.md and uses house_style.py. The original spider
is retained. Five hanging flower shapes—fuchsia, bell, star, rosette and orchid—
each come in pink, lilac and amber for fifteen combinations. Shape and colour
are independently seeded and stay fixed at each anchor. All flowers use the
same catch radius and lifetime. Each shape plays a distinct D-major pentatonic
chime on a successful catch: D5, E5, F-sharp5, A5 and B5, respectively. Colour
changes preserve the shape's note; dewdrops keep their separate reward sound.
The catch chime replaces the overlapping generic ding and landing scuff.
Flowers have calyces above their petals;
curved branches attach each blossom to a shaded leafy vine in world coordinates.
All vines, branches, blossoms and leaves use fixed world coordinates. Background
lianas scroll at the same speed as the blossoms, including in small viewports;
foreground vine samples and leaves are anchored to world heights. Loose silk
follows the spider throughout flight and visibly pins at contact. Small flight
trails clarify free movement. Compact sprites keep
small multiplayer panes readable.

Players 2-4 join on the title and receive independent cameras and scores. The
shared house kit supplies the lobby, pause, results, medals, saved records,
font, effects and music. Checked battery scores stay outside save states; the
title's displayed record is cached so restored titles replay consistently.

Validation: strand and swept-strand contacts, body-distance and timing regressions,
twelve courses with broad opening release windows using simple steering,
twelve seeded two-minute climbs using ordinary release and steering
inputs; continuous world motion; correct release velocity; no held-input
shortcuts; real keyboard flight steering; 1-4 player view clipping and maximum
screen movement through release/catch transitions; deterministic pixels,
pausing, battery persistence, libretro/audio and fresh-process state replay.

## Controls

Pad: A (or B, X, Y) starts and releases the web on its press; Left / Right steer
in free flight only; Select or Start pauses; A on the result screen returns to
the title.

Touch (phones, the desktop's mouse; player 1; the game declares `touch_native`, so
the apps pass it the touches, docs/spec.md "Touch"):
- **Tap to let go.** A finger going down anywhere is A's press: on the web it lets
  go at once, on the press (not on the lift); held, it does not repeat, and a
  finger on the web never advances to another flower, like A held.
- **Hold a half to steer.** In free flight each finger held on the left / right
  half of the screen is Left / Right held; both halves cancel out. A finger put
  down in flight steers at once; one that went down before the flight (the finger
  that let go of the web) steers once it has been down longer than a tap (18
  frames), so a quick tap never nudges the flight.
- **A tap** (a finger lifted) starts on the title and returns to the title from the
  result screen (a tap that began after its lock; a finger left down from the run
  never counts), as A does. Pads 2-4 still join on the title.
- **Pause**: the apps' pause button presses Start; while paused the touches do
  nothing and nothing is kept for later.
- Nothing derived from a touch lasts beyond its frame: the save states are
  unchanged. tests/touch_test.sh (in `make spiderswing-check`) plays scripted
  fingers against the same runs on the pad (the start, A's let-go, Left / Right
  held), plus the quick tap, a finger held on the web, both halves, the pause,
  the result screen and every width.

## Widescreen

A vertical climb: a wider screen (320..560 columns, docs/spec.md "Widescreen") shows
more of the garden on both sides; the climb is unchanged. The game opts in
(`max_width`, `WIDESCREEN_spiderswing`) and centres the 320-px stage
(`rs_screen_origin(rs_screen_margin())`). Each view keeps its 320 "base" area (the
full screen: 320 x 240; a split pane: the width it has at 320) with the same camera,
easing, clamps and zoom, centred in the view; the flowers, their generation, the
physics and the bots never look at the width, so 320 is unchanged pixel for pixel.
A run starts as at 320: the first flowers (and every later flower and bee) sit at
the same distance from the spider, the spider at the same place on the stage (or
in its pane's base area); a wide screen only shows more around them
(tests/test_views.c `start_layout`, at 320/427/520/560).
What widens: the far lianas come from a 1024-px panorama (128-column map: no repeat
on a 560-px screen; the classic six lianas, then six more whose phases are swapped in
pairs, reusing the classic tiles), the foreground layer is drawn as wide as the view,
the course's two vines show in the margins as the camera eases, and further out
more leafy vines of the garden (world-fixed, every 128 px; wide screens only). The
title and results hang their two branches from two vines in the margins. Split
views take the whole width (wider panes, the same zoom as at 320); each pane's
panel keeps its 320 size, centred, on UI columns no neighbour shows. States are
width-independent (the camera is in base coordinates); one loaded at another width
gets this width's maps and tiles (`state_loaded`). `spiderswing-check` runs the
views test, check_game.py (pictures checked at the width) and the state test at 427
and 520, the language test at 320/427/520, then tests/wide_test.sh: picture widths,
the same logs at 320/427/520/560 (pad input, 1-4 bots, results), `--resize` during
runs, and states saved at one width loaded at the others. Screenshots:
title/play/gameover-427 and -520.

## Tall screen

A phone held upright (docs/spec.md "Tall screen", the zoomed recipe): `rs_screen_zoom(ZOOM_H)` (480 lines, game.h)
first in `game_init`. The app scales the 480 lines to the display's height and crops the sides (204 columns on a
1080x2520 phone, 220 on a 1080x2340 one): the spider bigger than in landscape and **twice the climb in sight** (five
flowers above it instead of two or three). The views are as tall as the screen (`rs_viewport_layout`); the maps are 64
rows there; the raster carries the sky's gradient over a view's whole height; the foreground layer's pixel buffer
holds a 320 x 720 screen.
- **One spider climbing** fills the display's height, the sides cropped: its view's base area is the columns shown,
  so the camera (the same easing and clamps) keeps the spider and its next flower in them; the flowers (x 83..237)
  fit, the bees' outer lane shows most of the time and whenever the spider swings toward it. **The HUD at the
  screen's top**: the score, the hint and the dew line (BG1's top 64 lines pinned at screen line 14 with `rs_bg_pin`,
  the score at `rs_screen_top()` + 22), in the columns shown: a line wider than them goes on two rows.
- **Fill or fit**: the title, the panels and the split screens (2-4 players) show every column (`rs_screen_fill(0)`):
  the panes as tall as the screen (4 players: four 160 x 240 quarters at the full zoom).
- Landscape and 4:3: the same pixels as before.

Build: make spiderswing-check / spiderswing-windows / spiderswing-screenshots.
Windows: dist/windows/SpiderSwing.exe. Code MIT; original art/music/sound/design
CC BY-NC-SA 4.0, as stated in LICENSE.
