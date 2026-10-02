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

## Two to four players (race)
All racers share the rooftops with separate pads and player colours. The camera tracks the race; players
left too far behind drop out. Survivors continue until everyone is down. Results rank distance and score.

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
