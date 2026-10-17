# Mole Marbles

A Pang-inspired endless arena starring a miner mole and bouncing gemstones.
One full 320x240 cavern, four equal-sized players and no player collision.
Left/right moves at 2.7 pixels/frame; A sends a harpoon vertically upward.
Both tip and trailing rope hit balls, and holding A repeats after its shot ends.
Shots climb 6 pixels/frame and last at most 56 frames; 8-frame refire interval.

Four sphere radii: 22,15,10,6 pixels. Each larger ball splits into two of the
next size, with opposite horizontal velocities. Smallest balls disappear in
visible crystal fragments. Split = 10 points, final shatter = 15 points.
The opening is one tiny radius-6 ball, frozen behind the title and retained
when A starts. All ball sizes have 30-percent slower initial horizontal and
vertical motion. Gravity is reduced proportionally to preserve approximately
50-percent higher bounce arcs: tiny 82 pixels, small107, medium140 (previously
54/70/92). Largest bounce uses available cavern height and respects the ceiling.
Floor y208; ceiling y24. Moles have a16x22 collision body. Ball/hero collision
interpolates both trajectories; rope collision sweeps balls. Walls reflect;
each marble casts a floor shadow.

Timed arrivals run independently of destruction. Normal opening arrivals are
single tiny balls roughly4.5-6.5seconds apart, including1.5seconds of visible
ceiling warning. An empty cavern schedules a replacement after exactly1second.
Warnings show every incoming position when a wave contains several marbles.
A marble about to drop is announced by its oval on the ceiling only (the warning used to add a white bar and two
lines under it).
Splits remain harmless42frames; opening marble harmless60frames. Protection
pulses the gemstone shading, with no plus/minus symbols beside the balls.

Difficulty uses the best current player's score so multiplayer does not speed
up solely because more people joined. At60points, two-piece balls enter the
random size pool; at180four-piece balls; at420giants, which remain uncommon.
Groups begin with one ball, then include randomized pairs from45points and
one/two/three arrivals from150points. New arrivals choose varied sizes, colours,
positions and timer spacing. Split children still conserve mass. Weighted mass
budget rises from6to20; a full arena skips a scheduled wave instead of waiting
for the previous giant to be destroyed. The next clock continues independently.

All existing balls gradually accelerate with score, from70percent of the old
opening velocity to120percent at1500points. Gravity/bounce are scaled together
so faster play retains useful arc height. Arrival cadence also rises gradually
to about2.5-4.5seconds. Both speed and density have firm caps.

Hit feedback is immediate. Dead player is removed after a brief hit pose;
survivors continue. Eight-frame hitstop freezes shared motion. Everyone down
leads to the house results panel, records, medals and return to the lobby.
Pause, four gamepad ports, keyboard players1/2, SRAM and registered save states.
Run-start best snapshot keeps NEW BEST presentation deterministic after state
load without rolling back battery records.

Code-drawn art: stout mole, digging claws, mining helmet/light; hue-swapped
players keep eyes, metal and light colors. Navy-violet cavern uses distant
arches, stone ribs and muted crystal clusters; foreground gems have saturated
three-shade facets, a dark round outline and glints. No instructions during
play; scores use a compact top row in multiplayer. All moles use the same
32x32 cell in every mode, with a body approximately24pixels wide.

Validation: standalone physics fixtures cover frozen title, finite held-shot
repeat, all bounce apices, swept collisions, mass-conserving splits, visible
grace, normal90-frame warnings and60-frame empty refills, individual player elimination and survivors.
Fixtures measure the actual bounce increase, opening speed, independent timed
arrivals, randomized late groups and score-based acceleration. Twelve ordinary-input four-player bot runs compare entire worlds every frame
for deterministic replay and check capped density. Bots predict visible ball
arcs and issue only left/right/A. Full integration suite covers actual scripted
pads, lobby/pause/results, SRAM, libretro, screenshots and save-state audits.

## Touch (docs/spec.md "Touch"; the phone apps)
The game reads the touches itself (`rs_game.touch_native`): player 1, the pads stay as they are.
- **The mole follows the finger**: while a finger is down, the mole walks at its normal speed (2.7 px/frame, as with
  Left/Right) towards the finger's x and lands exactly on it (the world's `goal_x`: the last step is the distance left,
  so it glides to a stop, no jitter). The last point stays the goal when the finger is lifted, so a tap also sends the
  mole there; a pad direction cancels it. A held finger never shoots.
- **A quick tap shoots** (a tap: lifted within 0.3 s without moving): the harpoon fires as A would; when the
  previous one is still up, the shot waits up to 12 frames for it.
- A tap on the title starts the run (the start press is also the first harpoon, as with A), a tap on the game-over
  panel goes back to the title. While paused (Select, Start, or the app's pause button) touches do nothing.
- Tests: `tests/touch_test.sh` (at 320, 427 and 520; stage coordinates), and the pad runs stay byte-identical.

## The game-over panel's best
NEW BEST only when the run beats the best saved before it (and so scores more than 0); the value shown is then the
new best, otherwise BEST and the saved best (`tests/best_test.sh`; it used to show the best before the run: "NEW
BEST 0" after a first run).

## Widescreen (docs/spec.md "Widescreen")

The game opts in (`max_width` = 560, `WIDESCREEN_molemarbles = 1`): 320 x 240 is the classic game, unchanged; at 427
(16:9), 520 or 560 (phones) **the arena is the whole screen** (the owner's choice: it looks better, and it makes the
game a little easier).
- **The arena spans the screen**: the stage stays centred (`rs_screen_origin(rs_screen_margin())`, the UI centred
  with it) and the world's walls are `world.left`..`world.right` (main.c `world_arena`): 0..320 at 320, else the
  screen's edges 6 px in (stage x -margin + 6 .. 320 + margin - 6). The marbles bounce off them, the moles walk to
  12 px from them, the waves spread over the width (the classic positions at 320: `x = left + 32 + rnd % (span - 63)`
  and the like), a marble entering on the left half goes right; the bot weighs the same. The heroes start and the
  title's frozen marble wait where they always did. The marbles' layer (BG2) is drawn one screen wide (BG2 scrolled
  so that its column 0 is the screen's; 128 tiles past 512 columns).
- **The walls**: two squared mine timbers at the screen's edges (6 of their 8 px in view), iron-banded, as sprites
  (OBJ palette 7, tiles drawn at start-up). At 320 the screen's edges are the walls, as before.
- **The wider cavern** (make_art.py `hills_wide`; build_assets.py): BG3 becomes a 128-tile map of a 1024-px cavern
  with its own tiles and palette (the ground's tiles after them), the classic cavern pixel for pixel over the 320-px
  stage (build_assets.py checks it): the ribs, the distant arches and the ceiling go on round in stage coordinates;
  behind the arena now, as background art: the mine cart loaded with ore on its rails (left, stage -48..-7) and a
  ladder up the rock (right, 330..347), both in view from 16:9 on, a lantern on its chain each side and two more
  crystal clusters. The ground's slabs repeat their 512-px pattern on the 128-tile map.
- **Save states load at any width**: `state_loaded` lays this width's maps out again (draw.laid_w, saved) and, when
  the arena saved is another width's, sets this one's (`world_arena`: the marbles and moles kept inside its walls).
- Tests: tests/test_physics.c (in the check now) bounces every size across a 520-px arena, walks the mole to both
  walls and runs the bot on it (determinism, density, marbles near both walls); tests/wide_test.sh checks the
  picture's width, the arena reported by the dump at 320, 427, 520 and 560, that each run is the same twice, the bot
  scores at every width, width changes during runs (`--resize`) and states saved at one width loaded at the others.
- `make molemarbles-check` runs the libretro, smoke, UI, touch and state tests again at 427 and 520, then
  tests/wide_test.sh and the language test at 320, 427 and 520; `make molemarbles-screenshots` adds
  title/play/gameover-427/520.png and play-4-players-520.png (tools/wide_shots.sh).

## Tall screen (docs/spec.md "Tall screen")
A phone held upright: `rs_screen_tall(RS_SCREEN_H_MAX)` first in `game_init`, so the RetroStone app gives the game the
display's height at 320 columns. The arena and the 240 lines of the stage are the same pixels, centred (the default
origin; the frontend's touch buttons sit in the band below). The raster draws the lines past the stage (draw.c
`raster_out`): above, the cavern rising on (BG3's band of pillars, map lines 16..47, repeated up to the screen's top,
the backdrop the stage's first colour); below, the stone floor going down (the bricks of map lines 224..239 repeated).
The UI and marbles' layers are hidden there (window 1 over the whole width: their 32-row maps would repeat); line 0
puts the stage's windows and BG3's scroll back, so a 240-line screen is unchanged. Nothing of the play changes: the
ceiling stays at y 24 and the floor at y 208.
