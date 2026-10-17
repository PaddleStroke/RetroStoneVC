# Radish Rascals

A four-direction endless garden defence game: press the matching D-pad direction
to shoo rabbits from the four beds; spare friendly hedgehogs. The farmer holds a
pitchfork, turns to face all four directions and visibly thrusts toward the
pressed bed, then retracts it. Up uses a back view, Down a front view and the
side directions use the existing profile. A single missed rabbit, wrong bed,
early shoo or shooed hedgehog ends that player's run immediately. Later input
on that frame cannot score. Each player has one harvest heart; a mistake does
not end another player's run. Holding a direction cannot silently repeat it.

The opening now uses the former level-eight maximum, the pace reached by roughly
1300 points: visitors every 54 frames and a 40-frame active response window.
After each 12 visitors the interval shrinks by 4 frames and the response window
by 2, down to fixed limits of 38 and 32 frames. The 18-frame soil warning remains
visible before every emergence. Friendly visits occupy a bed and finish naturally;
new visitors only use empty beds. The first warning still comes 25 frames after
start, allowing the player to orient themselves.

The new code-drawn farmer is a 48x48 profile with shaded straw hat, visible nose,
apron, boots and a forward arm during the thrust. Short multiplayer views use a
dedicated 24x32 drawings with the same readable silhouette. Front and back views
have their own idle and thrust drawings at both sizes, with the pitchfork's
hand position matching each orientation. Unused old squash/stretch frames were
removed to keep the enlarged set within the console's VRAM budget. The pitchfork rotates
through all four directions, extends and retracts over the existing 12-frame action
animation, and remains visible in hand between actions. Fork art is presentation:
it does not delay scoring or alter the input timing.

The garden has a layered hedgerow, picket fence, shaded flowering shrubs, lawn
texture and worn paths between the beds. Beds are framed raised planters with
soil, timber sides, front boards, nail highlights and visible radish bulbs/leaves.
The title shows the same planted garden and farmer used for the actual start.
During play graphic hearts and scores replace large per-view panels, instructions
and combo text. Independent split views and full house lobby/results remain.

Validation checks immediate single-mistake elimination, independent multiplayer
loss and no scoring after a fatal miss; all four actual farmer sprite tiles and
their action variants at both sizes; the exact old-cap opening and new fixed cap; warnings, correct
and mistimed inputs, held directions, friendly visitors; four-player deterministic
runs; 24 schedules at the new cap with a 12-frame reaction delay after emergence;
1-4 player sprite visibility and viewport clipping; real frontend input, pause,
checked SRAM, strict rendering, pixel determinism, saved-state replay and audit;
plus native Windows startup/play for one and four players.

## Controls
| Input | Action |
|---|---|
| Up / Right / Down / Left | defend the top / right / bottom / left bed (held: no repeat) |
| A (B, X, Y, a direction, Start) on the title | start (this press is not a defence) |
| A (or any start input), Start or Select on the game-over panel | back to the title |
| Select (or Start during a run) | pause |
| Pads 2-4, A on the title | join; B leaves |

**Touch** (player 1; `rs_game.touch_native`: the phone apps pass the game its touches, docs/spec.md "Touch"): *tap the
bed to defend*.
- **The four beds are the tap zones.** A finger going down acts at once (as the pad's press, no wait for the release)
  on the bed nearest to where it went down: the garden is cut halfway between the beds' centres (draw_bed_at, from
  the beds' drawn positions), so a touch on a bed, its radishes, a path or the farmer's side picks that bed, and on a
  wide screen the margins belong to the left and right beds. Holding the finger does not repeat (as a held direction);
  another finger on another bed acts at once. With 2-4 players P1's beds are those of its own (smaller) view; a touch
  in another player's view acts on no bed.
- **The menus:** a tap anywhere starts on the title (as A; the start never defends a bed, and a finger already down
  then does nothing until lifted) and goes back to the title from the game-over panel (after the same lock as A).
- **Pause:** the apps' pause button presses Start. While paused touches do nothing, and a finger that went down during
  the pause never acts (only fingers that go down while the run plays defend: `touch_armed`, in the saved state).
- `tests/touch_test.sh` (`make radishrascals-check`, at 320, 427 and 520): scripted touches, each run compared with
  the same run on the pad (the state line, the game-over lines, the picture): the title tap, the beds of a seeded run
  of visitors touched off their centres (all four beds; wide: the side beds in the margins), a wrong bed, a held
  finger, touches in the pause, the 2-player views, the game-over tap (and one too early).

## Widescreen
The game opts in (`max_width` = 560, `WIDESCREEN_radishrascals = 1`): a wider screen shows more of the garden; the
game is unchanged. At 320 columns everything is the 4:3 game pixel for pixel (checked against the pre-widescreen
pictures and logs).
- **A fixed screen, centred.** The farmer stands between four beds that never move: `rs_screen_origin(
  rs_screen_margin())` centres the 320-px stage (53 px of garden each side at 427, 100 at 520, 120 at 560). The beds,
  the farmer, the visitors, the score and the kit's title, panels and results are the same pixels at every width.
- **The garden goes on (make_art.py, art/tiles/hills-wide.png).** The hills layer is drawn 1024 px wide (a 128-tile
  map in stage coordinates; stage x -120..439 is ever shown): its 320 stage columns are hills.png exactly (asserted by
  make_art.py and build_assets.py); right of the stage hills.png already went on; left of it the hedgerow and the
  picket fence go on by the same formulas, over hills.png's own grass. In the margins, a kitchen garden: on the left
  three rows of cabbages and carrots dug in the ground (not raised beds: nothing there plays), pink cosmos and a
  wheelbarrow with soil and two pulled radishes; on the right more cosmos, terracotta pots and a pumpkin patch by the
  far corner. The decor's pinks and oranges use one more BG palette (PAL_WIDE = 3); its 186 tiles load after the
  hills' tiles and their blank, only on a wide screen (nothing moves at 320).
- **VRAM.** On a wide screen the views' foreground tiles (paths, beds, fork, heart: drawn every frame) share identical
  tiles within a frame, which more than pays for the decor; the strict runs stay under the guideline at every width.
- **2-4 players: wider views** (the standard layouts over the whole width: 212 / 259 / 279 px at 427 / 520 / 560 for
  2 and 4 players, the 3-player top view the whole width). Each view draws its 4:3 garden (159 or 320 px) centred,
  the hills behind it going on at its sides, its score and PAUSED centred. The kit checks and writes text in the
  screen's UI columns, so each column of views shows its own block of UI columns (the left views' widest text from
  column 0, the right views' block just after): no text of a view shows in another. PAUSED is 2x when it fits in 20
  columns (else 1x).
- **The same game at every width.** The rules and the bots never see the width. `tests/wide_test.sh`: the same logs at
  320, 427, 520 and 560 (1-4 bots, several runs, to the results), live width changes (`--resize`) that go on exactly
  as the run that never changed, and states saved at one width that load and play at the others.
- **Save states.** The width is not in a state; one of another width brings maps 64 or 128 tiles wide:
  `state_loaded` sets up this width's maps and, from a 320 state, loads the wide hills. The views, their tiles and the
  UI are drawn every frame.
- **Tests at each width of WIDTHS** (`make radishrascals-check`): test_libretro (16:9, Wide), check_game.py (smoke,
  input flow, strict mode, determinism, the picture's width, the garden in both margins), state_test.sh; test_views
  at 320, 427, 520 and 560 (each farmer at its view's centre, sprites clipped, views independent, margins not flat);
  the language test at 320, 427 and 520; wide_test.sh. Screenshots: title, play and game over at 427 and 520
  (docs/screenshots/*-427.png, *-520.png).

## Tall screen
docs/spec.md "Tall screen" (the zoomed recipe): a phone held upright. `rs_screen_zoom(ZOOM_H)` (480 lines, game.h)
first in `game_init`: the RetroStone app scales the 480 lines to the display's height and crops the sides (220 columns
on a 1080x2340 phone, 204 on a 1080x2520 one), so the farmer, the beds and the rabbits are about 1.5x the size of the
old 320-column strip.
- **One garden, the beds closer together**: one view draws its garden GARDEN_TALL_W = 248 px wide instead of 320
  (draw.c `garden_w`, the same mapping as a split screen's smaller gardens): the side beds come in, all four beds,
  their rabbits and the fork's reach stay inside the columns shown. The rules, the beds' order and the timing are
  unchanged; a tap goes to the nearest bed of the garden as drawn (`draw_bed_at` uses the same width).
- **The stage high** (`rs_screen_origin_v`: TALL_ABOVE = 40 lines of sky above it, with the score at the screen's top:
  sprites at `rs_screen_top()` + 8), the trees and the fence at its top as in landscape.
- **More garden below** (raster `raster_out`): the lawn goes on with the wide garden's decor (hills-wide.png, the
  widescreen's art): BG3 is 128 tiles wide on a tall screen and holds two chunks of it beside the views' columns
  (`tall_decor`, columns 40..127): 96 lines with the flower pots and the carrot patch, then the vegetable rows and the
  wheelbarrow, each band taken at its own x (no tiled stripes, the bands cut in plain lawn). BG1, BG2 and the
  sprites are hidden past the stage (window 1); line 0 puts the stage's settings back, so a 240-line screen is
  unchanged.
- **Fill or fit**: one garden in play fills the display's height; the title, the panels and the split screens (2-4
  players) show every column (`rs_screen_fill(0)`), the split views over the stage's 240 lines with the sky above
  them and the two decor bands below (three more viewports: no layer above, BG3 below).
- Landscape and 4:3 are the same pixels as before (checked at 2340x1080 and 320x240). A touch's y is taken from the
  stage's origin (`rs_screen_origin_y`), so the beds are tapped where they are.
