# Raccoon Recycler

An endless, single-screen conveyor sorter for 1-4 players. Nine toys combine
ducks, sea stars and snails with yellow, white and blue. Snail shells carry
their colour; their bodies stay neutral. Press Left, Right or Up/Down to set your
junction; it stays selected. Sorting happens when the toy reaches the fork,
not when the button is pressed. A correct route earns one point; the first wrong
route immediately eliminates that player. Each player has an independent score
and route. There are no hearts or extra lives.

The three bin pictures are the rule. Neutral duck/sea-star/snail signs mean sort
by species; yellow/white/blue rectangular signs mean sort by colour. In each
mode, read the actual signs: the category order reshuffles at every rule change.
All six permutations are possible, and the new order differs from the previous
one. Up and Down both select the centre bin. The opening uses duck/star/snail
order to introduce the station; subsequent orders are seeded and random.
The rule alternates every
12 deliveries. New deliveries stop, every existing toy finishes its conveyor
and chute journey, then the empty station pauses for 1.5 seconds. The signs change
with a three-note chime after 0.5 seconds, changing both the rule and bin order,
giving a full second to read them before
the next delivery starts. No toy is judged against a rule that changed in transit.

The initial toy is already on the belt in the frozen title. Starting preserves
that exact position and seed. The belt is continuous fixed-point motion, with
new objects arriving every 96 frames initially, reaching 48 frames after
36 deliveries and 36 frames after 72. Spacing accounts for belt speed: the
physical gap shrinks from 96 to 72 pixels over the first 36 deliveries, then
stays tight as the belt accelerates. Belt speed rises from 1 to 2 pixels/frame.
The maximum pace leaves 0.6 seconds between toys and roughly a full second of
unobstructed belt below the HUD before the junction. Moving treads follow the same fixed-point speed as
objects and pause during rule changes. Items are emitted before treads in OAM
so conveyor stripes cannot overwrite their pixels. The junction is lower at
y=160; nearby bins have short 64-pixel side chutes and a straight centre chute.
Delivered copies
follow a connected chute, then descend behind the physical bin lip. Multiplayer
uses the same incoming toys and coloured overlapping junction strips; once routed,
personal copies follow each player's selected side. Full-size raccoons stay visible.

During play only centered scores appear above the scene;
there are no item names, countdown bars, streak labels, instruction text or textual
bin labels. House lobby, pause, results, medals, save RAM and state registration remain.

Validation covers arrival-time scoring and last-frame switching, persistent gates,
single-miss elimination and surviving multiplayer opponents, all three persistent
routes across every bin permutation, no mid-delivery order or
rule changes, monotonically decreasing delivery intervals, shrinking physical
gaps and enough separation for every routed toy to finish its chute animation,
16 seeded long runs with four bots using a 12-frame reaction delay
surviving the pace cap and over 30 rule changes,
continuous object positions, strict rendering, frozen title, real input, SRAM,
deterministic screenshots, save-state replay and native Windows 1P/4P execution.
The current bin order belongs to the registered world and restores with the
exact items, rule-change timing and seeded random state.

## Controls
| Input | Action |
|---|---|
| Left / Right / Up or Down | set the junction to the left / right / centre bin (it stays set) |
| A (or Start) on the title | start |
| A, Start or Select on the game-over panel | back to the title |
| Select (or Start during a run) | pause |
| Pads 2-4, A on the title | join; B leaves |

**Touch** (player 1; `rs_game.touch_native`: the phone apps pass the game its touches, docs/spec.md "Touch"): *tap the
branch the toy should take*.
- **The three branches' bins are the tap zones.** The screen is cut in three columns halfway between the bins' centres
  (draw_route_at, from the bins' drawn positions), over the whole height: the left bin's column (with the left
  workstations; on a wide screen the left margin too) sets the left branch, the middle one (the conveyor, the gate,
  the straight chute) the centre branch as Up/Down, the right one the right branch. A finger going down sets it at
  once, as the pad's press; the junction stays set, and the toy is still judged when it reaches the fork.
- **The menus:** a tap anywhere starts on the title (as A; a finger going down on that frame does not move the
  junction) and goes back to the title from the game-over panel (after the same lock as A).
- **Pause:** the apps' pause button presses Start. While paused touches do nothing, and only the frame a finger goes
  down counts, so a touch in the pause never acts after it. Nothing derived from touches is kept between frames.
- `tests/touch_test.sh` (`make raccoonrecycler-check`, at 320, 427 and 520): scripted touches, each run compared with
  the same run on the pad (the state line, the game-over lines, the picture): the title tap (with a second finger),
  each bin's column routing the first toy of a seed to its bin (wide: the side columns touched in the margins), the
  wrong branch, touches in the pause, the game-over tap (and one too early).

## Widescreen
The game opts in (`max_width` = 560, `WIDESCREEN_raccoonrecycler = 1`): a wider screen shows more of the recycling
yard; the game is unchanged. At 320 columns everything is the 4:3 game byte for byte (the same maps, tiles and tile
numbers; checked against the pre-widescreen pictures and logs).
- **A fixed screen: the stage centred.** `rs_screen_origin(rs_screen_margin())` (draw_init): the conveyor, the
  junction, the bins, the workstations, the scores and every sprite are the same pixels at every width; the screen
  also shows stage x -margin .. 320 + margin (53 px each side at 427, 100 at 520, 120 at 560). The house kit centres
  the title, the panels, the results and the pause; sounds pan by the screen x (the kit).
- **The yard is drawn wider by make_art.py** (`ground_wide`, art/tiles/ground_wide.png), never stretched: the ground
  layer becomes a 128-tile map (map x = stage x & 1023, drawn for stage x -120 .. 440, blank elsewhere); its stage
  columns are checked identical to the classic map (build_assets.py), its new tiles come after the classic ones (a
  320-px screen loads only those) and it uses the classic palette's colours only. The sky bands, the fence, its rail
  and its trees, the kerb and the sand go on; left the workshop shed (its open door with shelves of toys, a window,
  the recycling sign; from 520), a blue barrel and the crate stack with its toys to sort (from 427), a pallet of
  sacks in the yard; right the green recycling skip (from 427), cardboard bales and a hand cart with a barrel. The
  bin lips (BG2) and the UI layer stay within the stage: their 64-tile maps are unchanged.
- **VRAM.** The wide ground adds 206 tiles (518 ground tiles of the 1024 its base allows); the strict runs stay clean.
- **Multiplayer.** A shared screen: the 2-4 raccoons, their junction strips and their scores stay where they are
  on the stage; the margins are scenery only.
- **The same game at every width.** The rules and the bots never see the width: tests/wide_test.sh compares the
  whole logs of 1, 2 and 4-player bot runs (to the game over and the results) at 320, 427, 520 and 560.
- **Save states.** The width is not in a state, and nothing saved depends on it; a state saved at another width
  brings that width's ground map, which `state_loaded` (draw_relayout) lays out again for this width.
  tests/wide_test.sh: states saved at 320, 427, 520 and 560 (a run, the lobby, paused, a rule change, a game over,
  a race, the 4-player results) load and play at the other widths (`--any-width`); a live width change (`--resize`)
  goes on as the run that never changed.
- **Tests at each width of WIDTHS** (427, 520; `make raccoonrecycler-check`): test_libretro (16:9, Wide), the
  integration checks (check_game.py: strict mode, the item and badge pixels at the centred stage), the UI screens,
  the save states, then tests/wide_test.sh and the language test at 320, 427 and 520. Screenshots: title, play and
  gameover at 427 and 520 (`make raccoonrecycler-screenshots`).

## Tall screen
A phone held upright (docs/spec.md "Tall screen", the zoomed recipe): `rs_screen_zoom(ZOOM_H)` (440 lines, game.h)
first in `game_init`. The app scales the 440 lines to the display's height and crops the sides: 188 columns on a
1080x2520 phone, 202 on a 1080x2340 one, enough for the conveyor, the junction and the three bins (stage x 70..250),
5.7x on a 1080x2520 phone. The 240 lines of the stage are the same pixels, at the screen's bottom
(`rs_screen_origin_v`); the 200 lines above them are the conveyor rising into the sky. The rules are unchanged.
- **The conveyor up to the screen's top** (draw.c, the raster): BG3's top row (the sky and the belt) repeated above
  the stage, shown only in the belt's columns (window 2); BG1 and BG2 hidden there (window 1). The treads go on to the
  top, and **the toys still to come are already on the belt** (`upcoming`): a copy of the world is played forward
  (every player routing right, so it never stops) and each toy it delivers is drawn where it is now, its spawn line
  less the belt's run until then, so a toy rides down from the screen's top into the stage without a jump.
- **The sky**: the backdrop carried on from the stage's top colour (the same colour at the seam), bluer toward the top
  with the house 2-line dither; two puffy clouds drawn in code (BG4, palette 7: six of them over a 512-px map) drift
  behind the belt. BG4 is set up on a tall screen only.
- **The HUD at the screen's top** in play: the score (sprites at `rs_screen_top()` + 4 + their y) and, with 2-4
  players, the tags (BG1's top 24 lines pinned at screen line 4: `rs_bg_pin`).
- **Fill or fit**: one raccoon playing fills the display's height, its workstation moved into the columns shown (the
  station stays the same: only where it is drawn); the title, pause, the game-over panel and the results stay zoomed
  like play (house_narrow.h: the logo's words stacked, the hint and the prompt under them, the lines wrapped, a
  narrower ranking; the stations drawn in from the sides to the edges shown, so every raccoon shows); only 2-4 players
  playing show every column (`rs_screen_fill(0)`: the raccoons stand where they are).
- Landscape and 4:3 are the same pixels as before (checked at 2340x1080 and 320x240); the touch columns are the
  bins' at any height.
