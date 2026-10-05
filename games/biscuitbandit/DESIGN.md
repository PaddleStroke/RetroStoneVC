# Biscuit Bandit

Endless bakery catching: Left/Right moves a raccoon with smooth D-pad acceleration.
A starts the actual frozen title scene. Every caught cookie immediately earns
10points and accumulates on the plate; there is no capacity limit, basket,
delivery action or emptying pause. One missed/spilled cookie or caught cup
immediately eliminates that player. Multiplayer survivors continue.

The plate shows a compact overlapping heap, with nine representative cookies
retaining Q16 inertia and tilt. Further catches settle into that heap rather
than growing above the screen. Total collected cookies and score keep increasing
independently of visible heap size. Integer counters safely saturate at INT_MAX.
Spilled cookies leave the heap and fall out naturally during loss feedback.

The conveyor already holds the first visible cookie behind the title. New items
arrive every80frames initially, decreasing gradually to40frames over two minutes.
They spend72frames moving visibly along the belt before falling from the actual
visible position. More items enter while earlier cookies are still moving/falling;
there is always a visible stream, without a delivery break. Belt slats animate
with world time and freeze on the title, pause and final results.

Drop positions travel between seeded outer zones across x44..276, reaching both
sides instead of clustering in the middle. Consecutive steps are limited to
104pixels initially, shrinking to56pixels at the fastest cadence; destinations
and step lengths vary by seed. Reaching an outer destination starts a journey
toward the opposite side. Every fifth item is a recognizable cup. The next
journey follows the inward56pixel dodge away from the cup, making avoidance
compatible with catching every cookie even at the fastest interval. No destination
chutes, landing marks or prediction guides are drawn. Falling speed still increases
gradually from2.37to2.73pixels/frame. Both speed and spacing have fixed caps.

One full320x240 cafe, full-size raccoons, independent claims on the same items,
no player collision or priority penalty. Scores are immediate and independent.
The title preserves the exact world/RNG arrangement when A starts. Joining only
rebuilds the frozen start. Pause freezes world and animation. Versioned checked
uint32 SRAM records remain outside save states; the run-best snapshot is registered.

Code-drawn house art: striped tails/masks, colored aprons, wide trays, cookies,
cups, moving conveyor and muted cafe windows, shelves and tiled floor.

Validation covers a1000-cookie capacity fixture, immediate scoring, overflow,
independent multiplayer catches/failures, D-pad inertia, visible spills and
100seeded five-minute four-player runs at maximum stream pace, plus30complete
opening-to-cap runs. These require
all players to survive, catch over300cookies, never miss and never see an idle
stream. Distribution checks require repeated catches in both outer zones and
large movements away from the center, and enforce the56pixel step bound at cap.
Another20cap courses allow an eight-frame reaction pause after each catch;
all four players still collect every cookie and avoid every cup.
Full lobby/input/pause/SRAM/rendering/libretro/save-state checks run too.

## Widescreen (docs/spec.md "Widescreen")

The game opts in (`max_width` = 560, `WIDESCREEN_biscuitbandit = 1`): 320 x 240 is unchanged pixel for pixel; at
427 (16:9), 520 or 560 (phones) **the cafe is wider, the game is the same**.
- **A fixed screen, centred**: `rs_screen_origin(rs_screen_margin())` (draw_init). The heroes, trays, cookies, cups,
  drop positions (x 44..276), the belt's 72-frame run from x 328 and every screen of the house kit keep their 320-px
  places in the middle; the screen shows stage x -margin .. 320 + margin. Sprites are clipped to
  rs_screen_left()..right(); the score chips go to the screen's centre and corners (the kit).
- **The wider room** (make_art.py `hills_wide`, `oven`; build_assets.py): BG3 becomes a 128-tile map of a 1024-px
  room with its own tiles and palette, the classic room pixel for pixel over the stage (build_assets.py checks it).
  To the right, a bread rack (loaves, baguettes and buns on four shelves) and a pot plant; to the left, a hanging
  lamp over a pot plant and the cafe's glazed door. The ceiling rail, the wainscot and the skirting run on with their
  rhythm (the rail's 12-px dashes in stage coordinates, seamless where the picture wraps). The floor (BG2) repeats
  its 512-px pattern on a 128-tile map. The 427 screen ends exactly at the rack's right edge.
- **The cookie oven**: the belt now runs from the screen's left edge to a chrome oven (a 48 x 64 sprite on its own
  palette, OBJ 6) just right of the stage, its flue going up through the top of the screen; the cookies come out of
  its rubber curtain at x 320. It is in front of the cookies (a lower OAM index), so a cookie never pops into view
  in the margin; the shake moves it left only, as the cookies do not shake. At 320 the oven is off screen, as the
  cookies' start always was.
- **The same game**: the rules never look at the width; tests/wide_test.sh compares the whole logs (scores, game
  overs, the final state and its hash) of 1-player and 4-player bot runs at 320, 427, 520 and 560, changes the width
  three times during runs (`--resize`) and loads states saved at one width at the others.
- **Save states load at any width** (the width is not in a state): `state_loaded` (draw_relayout) compares the width
  the maps were laid out for (draw.laid_w, saved) with this one; another width sets this width's maps up again (the
  room, the floor, the UI layer) and the next frame draws the UI again (PAUSED too).
- `make biscuitbandit-check` runs the libretro, smoke, UI and state tests again at 427 and 520, then
  tests/wide_test.sh and the language test at 320, 427 and 520; `make biscuitbandit-screenshots` adds
  title/play/gameover-427/520.png and play-4-players-520.png (tools/wide_shots.sh).
