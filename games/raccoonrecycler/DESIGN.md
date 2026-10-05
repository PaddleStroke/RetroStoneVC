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
