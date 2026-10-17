# Pogo Mamie: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/pogomamie/LICENSE). The third game for the RetroStone
virtual console (docs/spec.md), in the house style (docs/art-direction.md, games/common).

## Pitch
Mamie, a Parisian grandmother in a headscarf, a cardigan and her handbag, is chasing her runaway cat across the
rooftops of Paris on a pogo stick. The cat is always a bit ahead, sitting on a chimney and looking back at her.
The pogo bounces by itself on every landing: steer her in the air, hold A as she lands for a big bounce over the
wide gaps, and keep going: Montmartre, the Seine, the Haussmann boulevards, the Eiffel Tower, then all night long.
One button, quick return to title, a best distance to beat.

It is Doodle Jump's auto-bounce turned sideways for the 4:3 landscape screen: the world scrolls to the right.
Its mechanics follow the reference feel (a bounce on every landing that sets the same arc, air control with a
little inertia, a spring that launches far higher); nothing else is taken from it: no name, art, sound, layout or
text. The theme, the characters, the screens, the medals, the sounds and the music are ours.

## Legal and originality
- Doodle Jump (Lima Sky, 2009) is proprietary. We use only its **mechanics** (not protected), measured by a
  third-party video analysis and estimated from two permissively licensed clones.
- The clones were read for their numbers only (CC0 and MIT, below); the code of Pogo Mamie is our own (C11 on the
  RetroStone SDK, integer fixed point). Their art was not looked at or used.
- All sources and their licences: "Feel sources" below.

## Screen and layers
320x240 at 60 Hz, fixed step. The world is 512 px tall (y down: the street, or the river, at y = 464) and endless
to the right; the camera leads to the right of the leader (more lead the faster she goes) and follows vertically
with smoothing, keeping in view the roof she bounces on and the highest and the lowest roofs ahead.

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the sky, one colour per line (raster callback, HDMA-like), a sunset, golden hour, dusk or twilight gradient per district, deep blue at night; a 4-line dither step hides the RGB555 bands | camera y / 8 |
| BG4 | the district's landmarks, far and hazy (Sacré-Cœur on the butte; Notre-Dame and the bridges; the Opéra Garnier, the Arc de Triomphe and a small Eiffel Tower; the Eiffel Tower itself, 150 px tall), 3-5 tones, no outline. One 512-px panorama per district, loaded into one of two VRAM slots as the districts go by; they all share the same low skyline at their ends so they join | 1/8, 1/4 |
| BG3 | the mid-ground: rows of distant mansard roofs, chimney pots, a dome, a spire, lit windows at night. It sinks by 56 px in the Eiffel Tower district so the tower shows whole | 1/2 |
| BG2 | the play layer: the buildings, the quays, bridges and barges, the street or the river, the cradles' ropes. Streamed tile by tile in both directions (a 64x32 map; each map cell remembers the world cell it holds) | 1 |
| BG1 | the house UI kit: the logo, text, panels | fixed |
| OBJ | Mamie and Papi, the cat, pigeons, antennas, awnings, window boxes, cradles, clotheslines, crumbling tiles, the baguette, power-ups, dust, glass, feathers, wind streaks, stunt points, the café awning, the HUD | |

Palettes (the house layout): BG 0 the UI, 1 roofs (zinc, terracotta, chimneys, skylights), 2 Haussmann stone, 3
houses, quays and bridges, 4 the street, the river and the barges, 5 the logo, 6 the mid-ground, 7 the backdrops.
The playfield palettes are tinted per district and at night by code (the windows light up at night; the Eiffel
Tower's lights twinkle). OBJ 0 Mamie, 1 Papi (Mamie's palette recoloured: grey hair, a brown jacket), 2 effects,
3 the kit sprites (digits, the A glyph, sparkles) and our medals, 4 animals, 5 props, 6 power-ups, 7 the café.

## Mamie
- About 24x32 px (a 24x32 cell): headscarf in the game's pink accent, round glasses, a purple cardigan, a dark
  skirt, a handbag, the pogo stick (a rubber tip, a spring, foot pegs, a handlebar). Facing where she goes.
- **Squash and stretch** (house_style.squash, the feet anchored): squashed on the landing (1.12, then 1.25 after a
  big bounce or a spring), stretched on the take-off (0.82), a little on the rise; knees up on a big bounce, arms
  flailing after a stumble.
- Hitbox: a 12x26 body above the feet; the pogo's foot is 7 px wide (it catches an edge by up to 3 px).
- **Papi** (player 2): the same frames with Papi's palette and his navy beret and moustache drawn over the head.

## The bounce and the air control
- Every landing SETS the vertical speed (the reference: the same arc whatever the fall before). A normal bounce
  peaks 75 px up after 36 frames (0.6 s) and lasts 72 frames.
- **Hold A on a landing**: the big bounce, 120 px up, 91 frames (1.6 x the height, 1.26 x the time).
- **Left/Right**: accelerate to 1.875 px/frame (0 to top speed in 15 frames), brake harder (to a stop in 10
  frames) down to a slow backward drift; with no input the pogo drifts forward at 1 px/frame (an endless run).
- Walls are solid: a building rising in front of her stops her (she falls along it). Everything else (chimneys,
  props, pigeons) is one-way, landed on from above.
- A landing on a slope nudges her down it (0.5 px/frame on the 45-degree zinc mansards, 0.25 on the 1:2 tiles).

## Things to bounce on
| Thing | Where | What it does |
|---|---|---|
| flat zinc roofs | most buildings | a normal bounce; the seams and the gutter are drawn, the first opaque row is the surface |
| mansard roofs | Haussmann buildings: 45-degree zinc slopes at both ends, dormer windows | the slopes nudge her outwards |
| pitched roofs | Montmartre houses: terracotta tiles, 1:2 slopes to a ridge | the slopes nudge her downhill |
| chimneys | on the flat roofs, 16 px wide, two terracotta pots | a small target: a stunt (+50, chained) |
| bouquinistes' boxes, a barge's cabin | the Seine | the same small targets |
| awnings | striped, over a window, sticking out of a wall into a gap | a spring: 226 px up (the reference's spring, 0.94 of the screen height) |
| clotheslines | across a gap, with shirts, socks and bloomers | they sag 14 px under her for 12 frames, then slingshot her 150 px up |
| crumbling tiles | an old tiled ledge over a gap | one bounce, then they fall |
| window boxes of geraniums | on a wall, in a gap | one bounce, then they fall |
| window-cleaner cradles | hanging from a davit on the taller building, going up and down | a moving platform |
| skylights | glass in a flat roof | the glass breaks: she drops one floor into the attic (24 px), slowed down (half her speed) |
| quays, bridges, barges | the Seine: stone quays with the bouquinistes' boxes, bridges with arches over the river, low barges | |

## Hazards
- **Pigeons** strut on the roofs (and stop to peck). Bouncing on a pigeon's head: +100 (chained) and it flies
  off; running into one from the side: knocked back with a hop.
- **Gusts of wind**: zones of 160-320 px, mostly headwinds, drawn as white streaks; they push her 0.375 px/frame.
- **TV antennas**: touching one is a stumble: her horizontal speed is gone.
- **The street** (or the river): falling below both roofs of a gap, with nothing to land on under her, is the end:
  a whistle, arms flailing, she lands in a café awning (boing), tumbles onto the sidewalk and sits there cursing
  (#@$%! in a bubble, a grumble); in the Seine she splashes down and floats in a life ring.
- **No impossible gap**: every gap is checked in the worst case (below).

## Power-ups (rare: one every ~360 m, floating over a roof)
| Power-up | Effect |
|---|---|
| umbrella | 4 s of slow glide: a third of the gravity, falling at 0.75 px/frame at most |
| baguette | a baguette plank over the next gap, once |
| knitting yarn | saves one fall: it hooks the nearest ledge and reels her up onto it |
| croissant | 3 s of speed: 2.75 px/frame |

## The endless generator and the districts
- Districts change every 512 m (one backdrop panorama at 1/8 speed): **Montmartre** (houses with shutters and
  pitched roofs, a few Haussmann buildings, Sacré-Cœur), **the Seine** (quays with bouquinistes' boxes, bridges,
  barges; the river instead of the street; Notre-Dame), **the Haussmann boulevards** (tall uniform cream stone
  buildings, mansards, continuous balconies, shops on the ground floor; the Opéra, the Arc de Triomphe),
  **the Eiffel Tower** (the tower whole behind the roofs, twilight); then the loop starts again **at night**
  (lit windows, a deep blue sky, the tower twinkling), harder.
- Difficulty rises linearly with the distance to its maximum at 3000 m (and beyond at night): wider gaps (the
  largest from 56 to 136 px), fewer neighbours without a gap (35% to 10%), more pigeons (8% to 75% of the roofs),
  wind (none before 400 m, then up to 30% of the buildings), more skylights and antennas (15% to 60%), fewer rescue
  props (55% to 20% of the gaps), narrower Haussmann buildings.
- **Every gap is reachable in the worst case**: the generator simulates, with the real physics and the wind, a
  bounce from a standstill 8, 24 or 40 px before the edge (normal or big), holding Right until past the next
  building's edge, then braking; while none lands on the next building, the gap narrows and the heights get
  closer, down to a plain flat roof. The props never count (they are extras). The ends of a roof (the take-off
  points and the landing zone: 40 to 48 px) are kept free of pigeons, antennas and skylights; a roof is at most
  96 px lower than the one before it (the camera shows both).

## Score, medals, save
- The distance in metres (8 px = 1 m, from the first roof), in the kit's big digits at the top.
- The score: the distance plus the stunts: +50 on a chimney top (once per chimney), +100 on a pigeon's head,
  multiplied by the chain of stunts in a row (up to x8); any other landing breaks the chain.
- "Cat catch" medals by distance, in the house tiers: **bronze whisker** at 250 m, **silver whisker** at 500 m,
  **gold whisker** at 1000 m, and at 2000 m (the end of the Eiffel Tower district) **the cat caught** (pearl).
- Save RAM (`PMAM` v1, with a checksum): the best distance, the best score, the best race, the runs, the medals,
  the best chain. Never part of a save state.

## Flow
The title is the only menu: Mamie, the cat, logo, PRESS A TO BOUNCE and the player lobby. A or the D-pad
starts immediately. After a fatal contact or missed roof, the fall leads to the game-over panel. A returns
to the title with the same lobby; the next A starts again. Start or Select pauses and resumes during play.

## Controls
| Button | Action |
|---|---|
| Left / Right | steer in the air |
| A (B, X, Y too), held on landing | big bounce |
| A on title / results | start / return to title |
| Start or Select in play | pause / resume |
| Pads 2-4, A or Start on title | join race; B leaves |

### Touch (docs/spec.md "Touch")
The game reads the touches itself (`rs_game.touch_native = 1`), so the phone apps pass it the fingers and draw no
buttons. Touch is player 1, alongside the pad (Papi and the other racers join with pads):

| Touch | Action |
|---|---|
| hold the left / right half of the screen | steer in the air (= Left / Right held); both halves cancel out |
| swipe up, anywhere | arm the big bounce: the next landing makes it (= A held until then); a glint on her pogo meanwhile |
| tap anywhere on the title / "get ready" | start (= A) |
| tap anywhere on the game-over panel | return to the title (after the same lock as A) |
| the app's pause button | pause / resume (it presses Start) |

- **The swipe arms the bounce.** The bounce is automatic and its speed is set on the landing frame, so a swipe in
  the air arms the next one (A is held for her until a landing makes the big bounce; a swipe just after a landing
  is for the next one: the bounce already left, changing it then would be a rule the pad does not have). The arming
  is kept in the save state (`touch_armed`: touches are not part of a state); a fall loses it, a new run clears it.
  The swipe is the SDK's: 20 px within 24 frames, so a quick flick of a finger already held (steering) counts too.
- **A swipe never steers.** A finger steers from its 3rd frame (33 ms: a flick has risen 4 px or more by then),
  not while it is above where it went down (more up than sideways: a flick starting) and never once it has swiped
  up; a sideways slide keeps steering (from one half to the other). So a flick from either half is the same run,
  and a finger held to steer then flicked up stops steering as it arms the bounce.
- The halves are the screen's (its centre at any width). While paused the fingers do nothing, not even later (a
  swipe in a pause arms nothing). Without a touch the pad game is unchanged, byte for byte.
- `tests/touch_test.sh` (in `make pogomamie-check`, at every width) plays scripted fingers through the headless
  runner and compares the runs with the pad's: a half held = Left / Right held, a swipe = A held to the landing.

## Two to four players (race)
All racers share the rooftops with separate pads and player colours. The camera tracks the race; players
left too far behind drop out. Survivors continue until everyone is down. Results rank distance and score.

## Widescreen (docs/spec.md "Widescreen")
The game opts in (`max_width` = 560, `WIDESCREEN_pogomamie = 1`): 320 (4:3) is unchanged pixel for pixel; at 427
(16:9) or 520 (phones) **Paris is simply revealed wider**: **a third of the extra width on the left**, behind the
players, **two thirds on the right**, ahead (`WIDE_LEFT_SHARE_NUM / _DEN` in tuning.h: 35 + 72 columns at 427,
66 + 134 at 520, 80 + 160 at 560).
- The camera is unchanged: its 320 columns (the stage) are those of 320 pixel for pixel, the stage origin (the
  screen column of stage x 0) is the left share. Mamie (and Papi, Tata, Tonton) bounce at the same place on the
  stage; the screen shows the roofs she has passed on the left and further ahead on the right. Seeing further
  makes it a little easier; the owner accepts it. The players' rules are the stage's: they still cannot go back
  past its left edge, and a racer left 24 px behind it drops out (in the left view, where one sees it happen).
- The left view shows the world as it is: world.c keeps the buildings up to 96 px behind the camera (and the things
  up to 128), more than the widest left view (80 + a tile + the shake; a static assert in draw.c). Behind the start
  (world x < 0, where the camera never goes) it shows decor: the Haussmann building next to the first roof, a little
  taller, under a mansard (draw.c, not part of the world), and the backdrop before the first panorama (the last
  district's, which the first one follows anyway). A skipped start (`skip=`, tests) shows the street there.
- **The course is the same at every width.** The generator already works 640 px (2 x 320) past the camera's left
  edge, beyond the widest screen (560): the same roofs, gaps, cafés, pigeons, balloons and gusts at the same places,
  and a new building (with its things) always starts past the real right edge, so nothing pops in on screen
  (tests/test_align.c checks it at every width, and the roofs, the street and the sprites in the left view). The
  bot reads the stage's 320 columns only (never the left view nor the right), so its runs are identical:
  tests/wide_test.sh compares the events, distances, scores and the final state at 320, 427, 520 and 560 (1, 2 and
  4 players, the districts, the night).
- The play layer, the mid-ground and the backdrop use 128-tile maps on a wide screen (64 at 320). The mid-ground is a
  1024-px panorama there: its second half (tiles/mid_wide.png: other roofs, a gothic tower and the Panthéon; ~50 more
  tiles in BG3's free VRAM, not loaded at 320) follows the first, so it never repeats on screen. The district
  backdrops follow each other at 1/8 speed; a screen wider than 504 px may show the end of one, the next one and the
  start of a third, so they get three VRAM slots of 168 tiles (panorama k in slot k % 3) instead of two.
- The district's look (the sky, the palettes, the night), the Eiffel district's lowered mid-ground and the music are
  decor: picked at the centre of the actual screen.
- The UI (title, logo, prompt, HUD, panels, results, pause) is centred: the UI layer scrolls by -hu_ui_x() and the
  game's UI sprites (the m of the distance, the medal) add hu_ui_x() (the kit's: the margin minus the origin); the
  power-up icons stay in the screen's top-left corner (rs_screen_left()); the score chips go to the halves
  (2 players) or corners (3-4, with their tags). The UI layer is 128 tiles wide beyond 512 columns.
- Save states load at any width: `state_loaded` sets the maps up again (everything streamed again, the UI and PAUSED
  redrawn on the next frame); a live change of width (`--resize`) goes on with the same run.
- `make pogomamie-check` runs the alignment, smoke, touch, UI and state tests again at 427 and 520, then
  tests/wide_test.sh (which also finds the 320 picture at its place in the wider ones: the left share).

## Tall screen (docs/spec.md "Tall screen", the zoomed recipe)
A phone held upright: `rs_screen_zoom(ZOOM_H)` (480 lines, pm.h) first in `game_init`. The app scales the 480 lines to
the display's height and crops the sides: 221 columns on a 1080x2340 phone, 205 on a 1080x2520 one, 240 on a 720x1440
one (Mamie about 4.9x on a 1080x2340 phone, a little bigger than in landscape). A larger H shows more columns (what is
ahead) but smaller sprites; 480 is nearly the world's whole height (512), so Paris shows from the sky to the street.
- **The view**: the camera (the stage's 240 lines, world.c, unchanged) is shown with `TALL_ABOVE` (72) lines of sky
  above it, and the rest of the screen below it, down to the street at most (WORLD_H): when the camera is low the
  screen stays still on the street and the stage moves in it (`rs_screen_origin_v` each frame; the shake still shakes
  the playfield). The play layer's, mid-ground's and backdrop's maps are 64 rows tall there (the world's height): the
  facades go down to the shops, the pavement and the road (or the river); above the world's top (y < 0) the raster
  hides the play layer (its map would wrap there).
- **The scenery**: the mid-ground and the backdrop are placed from the world y of the screen's top with their
  parallax (`TALL_MID_Y`, `TALL_FAR_Y`, draw.c), so the landmarks (the Eiffel Tower whole) and the far roofs stand
  above the playfield as on the stage; the sky's gradient runs over the screen's height; the city below the far roofs
  (the mid-ground's last row, repeated) gets darker with depth (raster_tall: no flat band).
- **The columns shown** (`rs_screen_focus_x`): one player playing (or fallen) fills the display's height, panned so
  that the leader stays from 32 to 60 columns into the columns shown (at most 2 px a frame; `pan_x0`, saved): most of
  them are ahead, and the stage's left edge (where she cannot go back past) comes into view when she brakes. The roofs,
  things and the gusts' streaks come in at the visible right edge (BG2 is streamed over the stage's 320 columns, the
  sprites culled at the screen's edges, not the stage's).
- **Fill or fit**: the title, "get ready", the game-over panel and the races (2-4 players: every racer must be seen,
  the camera waits for them) show every column (`rs_screen_fill(0)`: the whole 320 x 480 picture across the width).
- **The HUD at the screen's top** while playing: the UI layer's top 48 lines pinned (`rs_bg_pin`: the score, the
  tags), the distance and the score chips at `rs_screen_top()` (draw_hud_y), centred in the columns shown
  (draw_ui_dx); the power-up icons under the app's back button. The UI layer shows only the stage's lines otherwise.
- **Touch**: the halves are those of the columns shown (their middle, main.c `touch_buttons`); at 320, 427 or 520
  columns that is the screen's middle, as before.
- Landscape and 4:3 are the same pixels as before (checked at 2340x1080 and 320x240).

## Audio
- House sounds (games/common/src/house_audio.c): the confirm chime (start, join), the pause blip, the medal
  arpeggio, the game-over sting, the swish, the ding (the stunts), the thud (the sidewalk).
- Ours, synthesised at start-up with the house synthesiser (sfx.c): the **boing** (a rising sproing; played lower
  for a big bounce and lower still for a spring), the pigeon's **coo** (croo-croo), a tile's **crack**, **glass**,
  the umbrella's **pop**, the cat's **meow**, the fall's **whistle**, the antenna's **clang**, a power-up's
  arpeggio, a **splash** and Mamie's **grumbling**. Stored at a third of the house rate: the sample-memory guideline
  (64 KiB as ADPCM) holds with the house set.
- Music (tools/make_music.py with the house MOD writer): **musette waltzes**, one per district and one for the
  night: 3/4 at a calmer tempo, oom (the house bass) on beat 1, pah-pah (the left hand's chord buttons, a
  just-tuned major or minor triad) on 2 and 3, a brush tick, and the melody on the musette accordion: gently detuned
  reeds with reduced upper harmonics. Montmartre in C major, the
  Seine in A minor, Haussmann in F major, the Eiffel Tower in G major, the night in D minor and slower. The house
  loudness (music 56, effects 70-110).

## Art
Code-drawn in the house style (tools/make_art.py with games/common/tools/house_style.py): flat shades, a 1-px
outline outside the characters and props (never on the far layers), light from the top-left, readable at 1x.
Nothing goes through image generation. The asset build maps every pixel to its palette index exactly (the palettes
of tools/pm_sheets.py), so the code can tint the palettes per district and at night.

## Feel sources
The reference: Doodle Jump (Lima Sky, 2009, proprietary). No source code of it is public; its feel is known from a
video analysis and from clones tuned against it. We take their numbers only.

| # | Source | Licence | What we took (numbers only) |
|---|---|---|---|
| 1 | **"Basic Doodle Jump HTML and JavaScript Game"**, Steven Lambert (straker), https://gist.github.com/straker/b96a4a68bd6d79cf75a833d98a2b654f | **CC0 1.0** (stated in the gist) | a 375x667 canvas at 60 Hz (requestAnimationFrame); gravity 0.33 px/frame², a bounce sets the speed to -12.5 px/frame; horizontal speed 3 px/frame while a key is held, a drag of 0.3 px/frame² when released; the doodle 40x60, platforms 65x20 |
| 2 | **MisaghM/Doodle-Jump** (C++/SDL2), https://github.com/MisaghM/Doodle-Jump (`src/consts.hpp`) | **MIT** (LICENSE file of the repository) | a 400x640 window; per millisecond: bounce 0.7 px/ms, spring 1.3 px/ms, horizontal 0.42 px/ms, gravity 0.0014 px/ms² (at 60 Hz: 11.67 and 21.67 px/frame, 7 px/frame, 0.389 px/frame²) |
| 3 | **"How high does Doodle Jump?"**, Dragonfly Training, 2014, https://dragonflytraining.wordpress.com/2014/10/21/how-high-does-doodle-jump/ (a video analysis of the original with Vernier Video Physics) | article (facts cited, no code) | the original's jump is a constant acceleration of about 10 m/s² to a height of about 2.3 m (on the scale "4.5 m between two blocks"): the apex after sqrt(2 x 2.3 / 10) = 0.68 s |

### From the sources to our world
- **Time** is kept (60 Hz everywhere; the per-millisecond values of 2 converted).
- **The arc as a share of the screen height**: the apex of a bounce is 0.355 (1) and 0.273 (2) of the screen's
  height, mean 0.314 = 75 px of our 240. Doodle Jump's axis of progress is vertical and ours horizontal, but gravity
  stays vertical: a bounce covers the same share of the screen's height.
- **The time to the apex**: 37.9 (1), 30.0 (2) and 40.7 (3) frames, mean 36.2: 36 frames (0.6 s). Gravity and the
  bounce speed follow: g = 2 x 75 / 36² = 0.116 px/frame², v = 2 x 75 / 36 = 4.17 px/frame.
- **The spring** (2) peaks at 0.94 of the screen height: our awning, 226 px.
- **Horizontal speeds** in the references' pixels at the scale of our screen height (x 240 / H): 3 x 240/667 =
  1.08 and 7 x 240/640 = 2.63 px/frame, mean 1.85: our top speed 1.875 px/frame. So the arc keeps the references'
  shape (width against height).
- **Inertia**: (1)'s drag 0.3 x 240/667 = 0.108 px/frame²: our acceleration 0.125 (0 to top speed in 0.25 s);
  braking is 1.5 x that (the references stop at once when the key is released; a little inertia is ours).
- **Ours, not the references'**: the big bounce on A (Doodle Jump has no button), the cruise speed with no input
  (an endless run to the right), the solid walls (Doodle Jump's platforms are all one-way), the slopes.

### The final table (games/pogomamie/src/tuning.h)
| Quantity | References (60 Hz) | Pogo Mamie |
|---|---|---|
| apex / screen height | 0.355, 0.273 | 0.314: 75 px (72.9 px in the game's steps) |
| time to the apex | 37.9, 30.0, 40.7 frames | 36 frames (0.6 s); 72 in the air |
| gravity | 0.33, 0.389 px/frame² (their pixels) | 0.1157 px/frame² |
| bounce speed | 12.5, 11.67 px/frame (their pixels) | 4.167 px/frame (set on every landing) |
| big bounce (A held) | - | 120 px, 5.27 px/frame, 91 frames |
| spring | 0.94 of the height (2) | 226 px (the awning), 7.23 px/frame |
| top horizontal speed | 1.08, 2.63 px/frame at our scale | 1.875 px/frame (2.75 with the croissant) |
| acceleration / braking | drag 0.108 at our scale (1) | 0.125 / 0.1875 px/frame²; cruise 1 px/frame |
| range of one bounce | - | 135 px at top speed, 122 px from a standstill, 158 px big |
| difficulty over distance | rising (platforms thin out) | gaps 24..56 px at the start, up to 136 at 3000 m |

## Tests (make pogomamie-check)
- `tests/test_physics.c`: the tuning against the references (the shares, the times, the speeds), the arcs in the
  game's fixed-point steps (normal, big, spring), the air control (15 frames to top speed, 10 to stop, cruise,
  croissant, the umbrella's glide), the roof shapes (mansard and pitched slopes, the skylight's pit, chimneys),
  collisions (a wall stops her, a 4-px step does not, the foot catches an edge by 3 px, one-way props, an antenna,
  a pigeon's head and side, a skylight, the props between the roofs, the baguette, doomed or not), **the
  generator** over 3000 seeds (300 of them to 3300 m, the others to 1100 m, about 195000 gaps and steps): every
  one reachable in the worst case by an independent simulation, no bad shape (the grid, the ranges, the roof
  widths, the drops), no unfair spawn (pigeons, antennas and skylights clear of the edges, props in their gap and
  clear of the walls, power-ups over their roof, no overlapping gusts or chimneys) and the difficulty ramp (a table
  per 375 m: the gaps widen, more pigeons, wind only after 400 m); determinism.
- `tests/test_align.c` (run by `tests/align_test.sh` in each district, at night and in a 2-player race): the whole
  game with the bot, 12000 frames each; after every frame the play layer is rendered alone (on a flat magenta
  backdrop) and every roof column's first BG2 pixel must be exactly the surface the physics uses, with nothing above
  it, and the street at its height in the gaps; every prop, pigeon and antenna in view has its sprite at its world
  position minus the camera; on every landing Mamie's pogo tip is on the row above the surface. All 64 x/y scroll
  phases must be seen.
- `tests/smoke_test.sh`: the flow (title, immediate start), a fall with no steering, big bounces, retry, pause,
  save RAM, Papi joining, a race, the bot (tests/bot_test.sh) and determinism (a script and a bot run twice: the
  same state and picture).
- **The bot** (`--opt bot=1`, src/bot.c): it plays from the **screen only** (OAM, the BG2 map through its scroll
  registers and the tiles' shapes); 10 seeds, one run each. With fatal hazards the retuned bot averages 459 m
  (75..1290 m). The regression requires a 350-m mean and at least one 1000-m run; the older 1000-m mean relied
  on recoveries after hits. The generator still validates every gap against hazard-free trajectories.
- `tests/ui_test.py`: single-player and 2-4-player results; clear space between the winner banner and ranking,
  no single-player dialog behind multiplayer results, readable scores and menu prompt, slide background,
  hardware limits and return to the title with the same players.
- `tests/state_test.sh`: save states (the title, each district, the night, a race, two bots, paused,
  the fall, the panel, 4 random points of runs), replayed in the same process and in a fresh one; bad states are
  refused; `tools/state_audit.py` checks every mutable static.
- `make pogomamie-bench` (tools/bench.sh) and `make pogomamie-screenshots` (tools/screenshots.sh).

## Performance (make pogomamie-bench)
Measured on the build host (WSL2, x86-64), 3300 frames per scene after 300, music on; the A20 estimate is 15-20x
(docs/spec.md "Performance"):

| Scene | Host, per frame | A20 estimate |
|---|---|---|
| the Seine / Haussmann with gusts, the bot playing, average | 0.35 ms (the game and the bot 0.02 ms, the PPU 0.32 ms) | 5.2-7.0 ms |
| night, 2 bots racing (the heaviest), average | 0.35 ms | 5.3-7.0 ms |
| the game and the bot, worst frame (a new building generated and checked) | 0.21 ms | 3.1-4.2 ms |

The frame budget is 16.7 ms. Worst whole frames on the host (about 1 ms) are the host's scheduling: the PPU's
work is the same every frame (4 layers, at most 32 sprites, 16 on a line).
## Current completion
The title is the only menu: A starts immediately; pads 2-4 join with A or Start, B leaves. After game over A
returns to the title with the lobby retained. Up to four racers share the rooftops and a distance ranking panel.
Hazard contacts end the run through a tumble and fall: roofs, steering, yarn and umbrellas cannot recover a hit.
A pigeon stomp from above remains a bounce. The clothesline and slanted-roof changes are retained.
Music uses a quieter, less detuned accordion, softer accompaniment, slower tempo and eight varied phrases.
A WAV preview is included beside the Windows executable. Physics tests cover fatal contact and the stomp exception;
lobby, four-player, state replay and rooftop alignment tests cover the flow and rendering.
