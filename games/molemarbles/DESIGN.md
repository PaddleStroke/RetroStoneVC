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
- **The mole follows the finger**: while a finger is down, the mole walks at its normal speed (2.7 px/frame, the
  same steps as Left/Right) towards the finger's x in the arena, and stops within 2 px of it. The last point stays the
  target when the finger is lifted, so a tap also sends the mole there. A held finger never shoots.
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

The game opts in (`max_width` = 560, `WIDESCREEN_molemarbles = 1`): 320 x 240 is unchanged pixel for pixel; at 427
(16:9), 520 or 560 (phones) **the cavern is wider, the arena is the same**.
- **A fixed arena, centred**: `rs_screen_origin(rs_screen_margin())` (draw_init). The marbles still bounce off x 0
  and 320, the moles walk where they did, the warnings, ropes and shards are drawn over the same 40 columns (BG2);
  the screen shows stage x -margin .. 320 + margin. Sprites are clipped to rs_screen_left()..right().
- **The arena's walls made visible**: at 320 they are the screen's edges; wider, two squared mine timbers stand
  just outside them (stage x -8..-1 and 320..327, iron-banded), so a marble visibly bounces off a wall.
- **The wider cavern** (make_art.py `hills_wide`; build_assets.py): BG3 becomes a 128-tile map of a 1024-px cavern
  with its own tiles and palette (the ground's tiles after them), the classic cavern pixel for pixel over the arena
  (build_assets.py checks it): the ribs, the distant arches and the ceiling go on round in stage coordinates; beyond
  the timbers a lantern on its chain each side, two more crystal clusters, a mine cart loaded with ore on its rails
  (left) and a ladder up the rock (right). The ground's slabs repeat their 512-px pattern on the 128-tile map.
- **The same game**: the rules never look at the width; tests/wide_test.sh compares the whole logs (scores, hits,
  game overs, the final state and its hash) of 1-player and 4-player bot runs at 320, 427, 520 and 560, changes the
  width three times during runs (`--resize`) and loads states saved at one width at the others.
- **Save states load at any width**: `state_loaded` (draw_relayout) compares the width the maps were laid out for
  (draw.laid_w, saved) with this one; another width sets this width's maps and the cavern's tiles up again (every
  frame draws the UI and the marbles again).
- `make molemarbles-check` runs the libretro, smoke, UI and state tests again at 427 and 520, then tests/wide_test.sh
  and the language test at 320, 427 and 520; `make molemarbles-screenshots` adds title/play/gameover-427/520.png and
  play-4-players-520.png (tools/wide_shots.sh).
