# Duck Parade: design

(c) 2026 Pierre-Louis Boyer (8BCraft). Code MIT, assets CC BY-NC-SA 4.0 (games/duckparade/LICENSE). A game for the
RetroStone virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md, games/common).

## Pitch
Mother Duck has lost her ducklings all over town. Hop across endless roads, rivers, railways, park paths and
lily ponds, pick up the lost ducklings on the grass, and lead them home in a line behind you, hop for hop,
like the Snake game. A long parade is worth a lot at the next **nest pond**, but every duckling can be hit on
the way. One hop per press, quick return to title, a best score to beat; a friend can join as Father Duck.

It is a "hop across the lanes" game in the family of Frogger (Konami, 1981) and Crossy Road (Hipster Whale,
2014), crossed with Snake. Only their **mechanics** are used (a grid hop, lanes of traffic, logs to ride, an
enemy that punishes lingering), measured from published analyses and a permissively licensed clone (see
"Feel sources"); no name, art, sound, layout or text is taken. The duck family, the parade, the nest ponds,
the fox and every picture and sound are ours.

## Orientation and screen
The screen is 320x240 (4:3 landscape), so the crossing runs **left to right**:

- the world is a sequence of **lanes**, each a vertical **column 16 px wide**; the duck travels right;
- traffic runs **up and down** each column; logs drift up or down a river column; a train rushes down its
  track;
- the play field is 14 cells tall: rows 0-13 at screen y = 16..239; the top 16 lines are the **sky band**
  (the house raster sky, one colour per line) that holds the HUD;
- the camera scrolls horizontally only.

| Layer | Contents | Scroll |
|---|---|---|
| BG1 | UI (the house kit: font, panels, banners, the title logo, the HUD's text; priority 1) | fixed |
| BG2 | rows 0-1: the **sky band**, high priority, over the world's sprites (cars entering the field slide in under it); its sky entry is the house raster gradient (the raster callback sets it line by line) and a band of clouds drifts at 1/4 of the camera. Rows 2-29: **cloud shadows** drifting over the field, low priority, blended half and half with it (colour math) | band: per-line scroll; shadows: camera + wind |
| BG3 | the lanes, top-down, drawn column by column into a 32-column ring as the camera moves: grass with flowers and trees, asphalt with dashes and kerbs, bike lanes, rails on sleepers, sandy park paths, rapids (the tiles shift with the current, like the logs), lily ponds, nest ponds with reeds and the nest | camera |
| OBJ | ducks, ducklings, shadows, cars, bikes, buses, joggers, lawnmowers, logs, lily pads, the swan boat, the train, the crossing lights, the fox, splashes, feathers, hearts, the HUD digits and icons, the egg medals | |

Palettes: BG 0 the UI (kit), 1 grass, 2 roads, 3 rapids, 4 railways and paths, 5 the logo (kit), 6 ponds, 7
the sky band and the shadows. OBJ 0 Mother Duck, 1 Father Duck (the same tiles), 2 ducklings and effects, 3 the
kit (digits, glyphs) and our egg medals (drawn with its palette), 4-5 traffic, 6 river things, 7 the train, the
lights and the fox. The title hazes the field (colour math with a light fixed colour) behind the logo; a
banking flashes it (added white, fading); the fox's warning tints the left edge red (the window and the fog).

## Controls
| Button | Action |
|---|---|
| Right, A | hop forward (to the next lane) |
| Up, Down | hop along the lane (sidestep) |
| Left | hop back |
| Start | pause / resume |
| A, Start on the game-over panel | return to the title |
| Pads 2-4, A or Start on the title | join co-op; B leaves |

One hop per press (no auto-repeat, as in Crossy Road); a press in the last 4 frames of a hop is buffered and
starts the next hop on landing, so fast tapping never loses a press. Keyboard (desktop): arrows, X = A,
Enter = Start (the SDK's mapping).

## The hop
- A hop moves exactly one cell (16 px, Frogger's step) in **12 frames** (0.2 s, the clone's two 0.1-s
  tweens). The sprite rises on an arc (up to 6 px, highest at 50%), moves 75% of the way by mid-hop (the
  clone's "in-air position"), and **stretches** (1.2 tall) then **squashes** (0.8) on landing, with a
  bouncy recovery (the clone's scale timeline). A soft shadow stays on the ground.
- For collisions the duck belongs to the lane it left during the first half of a hop and to the lane it
  lands in from the middle on (the clone rounds the position: the same rule).
- Blocked hops (a tree, the field's top or bottom edge) play a short **bump** (4-px lean, 8 frames) and a
  muffled quack; the duck does not move.
- Idle: a slow breathing squash (0.8 tall over 0.6 s, the clone's idle), a blink now and then.

## Lanes
| Lane | Hazard | Safe cells |
|---|---|---|
| **grass** | none; **trees** and bushes block cells | every free cell |
| **road** | cars (several colours), bikes (fast, short), a bus (long, slow) | none: time the gaps |
| **river** (rapids) | the water: Mother is swept away | riding a **log**, a drifting **lily pad** or the **paddle boat** (a swan) |
| **railway** | the train, telegraphed by a flashing light and a bell | none while the light flashes |
| **park path** | joggers (in small groups), a lawnmower (slow, long) | none: time the gaps |
| **lily pond** | the deep water | the static lily pads |
| **nest pond** | none: calm shallow water, a reed island with the nest | every cell; entering it **banks** the parade |

- Objects move at a constant speed along a **loop** of 22 cells (the clone's 22-tile wrap), of which the 14
  visible rows are a window; positions are a closed-form function of the frame number, so the game, the
  fairness judge and the bot all agree exactly.
- Riding: on a log, a pad or the boat, the duck moves with it and may hop up or down along it; landing off
  it, or being carried past the top or bottom row, is a splash. Hopping from a platform to firm ground snaps
  the duck to the nearest row.
- The train: a locomotive and 2-4 carriages cross the track at 12.8 px per frame (the clone's 0.8 tiles per
  frame); the crossing light flashes and the bell rings **2 s** before the locomotive shows (the clone's
  2.2 s; never less than 1 s), until the last carriage is gone.

## The parade (the twist)
- **Lost ducklings** wait on grass cells, peeping. Mother walks over one: it **joins the line right behind
  her** (a peep, a little hop of joy) and the whole line follows her **exact path, hop for hop**: duckling k
  is where Mother was k hops ago (Snake's rule). The ducklings take off 2, 4 or 6 frames after her (a
  marching ripple repeating down the line) and all land with her. On a log, the path is kept relative to the
  log. A lost duckling waits on 28% of the grass columns (two of them on 15% of those).
- Every duckling can be **hit**: a car, a bike, a bus, a jogger, a mower or the train **knocks it off the
  line** (a tumble, feathers, a horn): it flutters back to the nearest grass on screen (invulnerable while
  it flutters) and waits there to be picked up again. The ducklings behind it **stay with the line**: they
  hurry forward along the path to close the gap, one hop at a time, when the next cell is clear.
- A duckling whose log has drifted away when it gets there, or that is carried past the edge, **paddles**
  back to the grass the same way (ducklings float; they are not lost).
- A duckling left behind the screen's left edge runs home: it is gone.
- **Mother being hit, swept away or caught is game over.**
- **Nest ponds** come every 40-50 lanes. When Mother enters one, the whole line **banks**: the ducklings
  hop into the pond one after the other, a happy splash, a camera **flash** and a "family photo" frame with
  the count; the line starts over. Points: **n x n** for a line of n ducklings (10 ducklings = 100).
- The line holds up to 24 ducklings.

## Pressure: the fox
The camera follows Mother (it eases 3% of the distance per frame, the clone's CAMERA_EASING, to keep her at
column 8 of 20) and also **creeps forward on its own**, a little faster with distance, never backwards.
Lingering lets the left edge catch up:

1. **Warning**: Mother within 3 columns of the left edge: the **fox** trots in at the edge, ears up, eyes
   on her; a "!" bubble, a growl, the edge of the picture pulses red. Nothing happens yet.
2. **Pounce**: Mother's centre crosses the left edge: the fox leaps and catches her: game over.

Standing still from Mother's usual place, the fox comes after **8 s** at the start and **5 s** from lane 300
on (Crossy Road's eagle comes after about 5 s of idling). Hopping forward always pushes the edge back.

## Generator and fairness
Lanes are made in **units**: a hazard group (1-4 lanes of one family; from lane 120 a lane may take another
family) followed by a grass run (1 grass column, 2 for 25% of the runs, 3 for 8%; a nest pond run is grass,
nest, grass). The first 8 columns are a safe meadow (a hedge on the left, bushes on the top and bottom rows).

Family mix (after the meadow), from the clone's row mix (grass 1/3, road-type 1/3 of which 1/4 railway,
water 1/3), with park paths and ponds taken from the road and water shares: road 25%, railway 8%, park 9%,
river 25%, lily pond 8%, and the grass runs between groups. Measured over 2000 courses of 300 lanes (the
fairness test): grass 47%, road 21%, river 17%, park 5%, railway 4%, lily pond 4%, nest ponds 2% (the early
groups are short, so the first lanes are grassier than the clone's third).

**Difficulty** d = lane / 300 (capped at 1; lanes past 300 add up to 20% more speed by lane 600):
| | lane 0 | lane 300 |
|---|---|---|
| cars, speed (px/frame) | 0.32-0.70 | 0.32-1.28 (the clone: 0.02-0.08 tiles/frame) |
| cars per loop | 1-2 | 2-3 (the clone: 1-2 per 22 tiles) |
| road group | 1-2 lanes | 2-4 lanes (Frogger: 5) |
| logs, speed | 0.32-0.60 | 0.32-1.12 (the clone: 0.02-0.07) |
| log length | 3-4 cells | 2-3 cells |
| river group | 1-2 lanes | 1-3, alternating directions (Frogger's river) |
| train period | 8-10 s | 5-8 s (the clone: 4.6 s) |
| camera creep | 0.28 px/frame | 0.45 px/frame |

**The judge** (lanes.c): every hazard group is checked before it is used, by an exact time-expanded
reachability search over the duck's own moves (hop 12 frames in 4 directions, waiting, riding, snapping),
on a pixel-accurate model of the lanes (bitsets of the 224 positions of each column, per frame):
- the start set is every cell of the previous grass run that the duck can reach;
- from **each of 4 start times**, spread over the group's longest traffic period (every phase of a slow
  mower or a train is tried), the group must be crossed into the next grass run within **4 s** (the fox's
  grace is 5-8 s);
- the judge runs one start per frame at most (16 us on the host), columns are made about 14 ahead of the
  screen, so the generator never costs a frame;
- a group that fails is re-rolled (up to 4 tries, easier each time), then replaced by grass: the generator
  can never hand out an impossible pattern.
- Constructive rules on top: trains are telegraphed 2 s ahead (>= 1 s is checked); adjacent river lanes flow
  in opposite directions; a lily pond keeps a pad in reach of the cells before it (the clone's rule); every
  grass column keeps 4 free cells, the free cells of a grass run are connected, and the run's first column
  has a row free straight ahead of a free cell of the column before the group (no crossing needs a long
  walk up or down inside the traffic).

The fairness test (tests/test_rules.c) replays the generator over 2000 seeds x 300 lanes: every group is
judged again from 4 random start times within 5 s, and on 50 seeds a strict judge tries a start every 5 frames
and every single start cell (the whole grass run before the group in the search); the constructive rules and
the train telegraph are checked. Result: no failure in 206 902 groups; crossing takes 30 frames on average,
234-285 at worst. At run time (seed 42, 300 lanes) 1 group in 100 is re-rolled, none replaced by grass.

## Scoring, medals, save
- **Score = lanes crossed (the furthest column) + banked points.** The HUD shows the score, the line length
  and the best.
- Egg medals in the house medal tiers: **bronze egg** at 50, **silver egg** at 100, **gold egg** at 200,
  **pearl egg** (speckled, shining) at 300.
- Save RAM: best score, best co-op score, the longest parade banked, runs, medals won.

## Flow
The title is the only menu. A or the D-pad starts immediately with the first hop. Pads 2-4 join with A or
Start and leave with B. After death, the game-over panel shows the scores and medal; A returns to the title
with the same lobby. Start pauses and resumes during play.

## Two to four parents (co-op)
Each parent has their own parade and pad. Ducklings join the parent that collects them; eliminated parents
scatter their ducklings while the remaining parents continue. The shared camera blends the parents' positions
with a pull toward the leader. Line caps shrink for larger families. Results show each parent's score and the
family total, without a competitive winner. Player colours distinguish all four parents.

## Audio
- Sound effects synthesised in code at start-up with the house synthesiser (house_audio.h: ha_tone, ha_hiss;
  src/sfx.c): a **quack** per hop (four voices, the pitch varied hop by hop, Father's lower), a **peep** when a
  duckling joins (higher as the line grows), a **car horn** (two detuned brassy tones) when a duckling is
  knocked off, a **splash** and a **plop**, the **train bell** (a struck bell, repeating while a light flashes
  on screen), the **banking fanfare** (a brass arpeggio D5 F#5 A5 D6) and the photo's **click**, the fox's
  **growl**, a thud, a bump; the UI sounds are the house kit's own (confirm, pause, medal, swish). The house
  loudness targets and echo (on the wet sounds only). Pan by screen x.
- Music: a jaunty **marching-band** loop in D major (tools/make_music.py with the house music kit: a 4-channel
  MOD, fife, tuba oom-pah, bass drum and snare with rolls, hi-hat; speed 8 at 150 BPM, so a row is exactly
  8 frames and a beat 32 frames: 112.5 beats a minute), and **layers the game adds as the parade grows**,
  played in step with the module on their own voices: a **trumpet** counter-melody from 3 ducklings in the
  line(s), a **glockenspiel** from 8 (the title plays the trumpet).
- Voices (the 8-voice guideline counts the module's 4 channels): the layers take voices 3 and 2 while they
  play, the effects share the rest of 0..3 (the oldest is stolen). Sample memory stays under the 64-KiB
  guideline (the SNES ARAM): the quacks and the layers' instruments are kept at 16 kHz, and only the house
  sounds the game uses are made. Strict mode reports nothing over a 6000-frame bot run.

## Art
All code-drawn (tools/make_art.py, the house helpers of games/common/tools/house_style.py): flat shades, 1-px
outlines outside the shape (never black: each material's darkest shade pushed towards violet), light from the
top-left, readable at 1x. The lanes are seen from above; the characters in the classic 3/4 view of top-down
games (from the side going right or left, from the front or the back going down or up).
- Mother Duck, 16x16: a white farm duck, an orange bill and feet, a blue speculum, a pink ribbon; four
  directions x rest, stretch (take-off), squash (landing), blink; hit (flattened, X eyes), swept (her head in
  the rapids). Father Duck: the same tiles, OBJ palette 1: a **mallard drake** (green head, white collar,
  grey-brown body, yellow bill, the ribbon becomes his curl).
- Ducklings, about 10x10 in 16x16 cells: fluffy yellow, orange beak, a tuft; four directions x rest and hop,
  peep (beak open), tumble (spinning), flutter (wings up), paddle (swimming, ripples).
- Traffic from above, both directions drawn (the light stays top-left): cute rounded cars in six colours
  (windscreen, roof, rear window, wheels, head and tail lights), a yellow bus, cyclists (red helmet), joggers
  (3/4 view), a ride-on lawnmower with its gardener; logs (2-4 cells, cut rings at the ends), lily pads (one
  with a flower), the **swan paddle boat**; the train (a green locomotive with its headlight, carriages), the
  crossing light (two red lamps flashing in turn) and the nest pond's sign in the sky band.
- The field: grass with tufts and flowers, round trees with their shadow, bushes with berries, rocks, the
  hedge; asphalt with kerbs and lane dashes, bike lanes with their painted bike; rapids with foam streaks (16
  shifts: the current moves with the logs) and banks; rails on sleepers; sandy park paths with grass edges;
  lily ponds; the nest pond (reeds, the grassy island with the nest and its eggs). Drifting **cloud shadows**
  (BG2, blended half and half) and a band of clouds in the sky.
- Effects: splashes, feathers, dust puffs, hearts, peep notes, the fox's "!" bubble, checker-dithered shadows
  under the hops. The **egg medals** use the kit's medal palette and tiers (bronze, silver, gold, pearl with
  speckles). The fox (watching, pouncing, trotting off with Mother).

## Balance and feel sources
| # | Source | Licence | What we took (numbers and facts only) |
|---|---|---|---|
| 1 | **Expo-Crossy-Road**, Evan Bacon, https://github.com/EvanBacon/Expo-Crossy-Road (src/GameSettings.ts, src/CrossyPlayer.ts, src/Row/Road.ts, Water.ts, RailRoad.ts, src/CrossyGame.ts, src/GameEngine.ts, master branch, read 2026-09-30) | **MIT** (LICENSE: "Copyright (c) 2016-present Evan Bacon") | hop = two 0.1-s tweens (0.2 s), in-air position at 75% of the move; stretch 1.2 / squash 0.8 / bounce back, 0.1 s each; idle squash to 0.8 over 0.3 s each way; cars 0.02-0.08 tiles/frame, 1-2 per lane, spaced 5-8 tiles, wrapping over 22 tiles; logs 0.02-0.07 tiles/frame, 2-3 per lane, spaced 5-8; lily pads static, 2-3 per row, 2-3 apart, one kept in reach of the row before; train 0.8 tiles/frame, 1-3 carriages, a 220-tile cycle (4.6 s), the light flashing 15 x 200 ms, the train showing 2.2 s after the light starts; collision box = half widths - 0.1 tile; the hero belongs to the lane its rounded position is in; camera easing 0.03 per frame; first 10 rows grass (5 blocked by trees, 5 open); row mix: grass, road-type, water 1/3 each, road-type is a railway 1/4 of the time |
| 2 | **Crossy Road Wiki, "Eagle"** (Fandom), https://crossyroad.fandom.com/wiki/Eagle, as summarised by a web search on 2026-09-30 (the page itself answered 402) | CC BY-SA (Fandom) - facts only | the eagle takes the player after about 5 s of idling (measured 4.9-5.19 s) or after going back 3 lanes |
| 3 | **Computer Archeology, Frogger**, https://computerarcheology.com/Arcade/Frogger/ (RAM map, disassembly notes) | facts cited, no code | the frog's position steps in multiples of 16 px ($10): a 16-px grid; objects step one pixel when a per-lane countdown expires (a per-lane speed) |
| 4 | **Frogger**, Wikipedia, https://en.wikipedia.org/wiki/Frogger, and the 20 Games Challenge page https://20_games_challenge.gitlab.io/games/frogger/ | CC BY-SA / facts only | five road lanes and five river lanes between safe strips, alternating directions and speeds, a 30-s timer per frog |

### From the sources to our world
- **Space**: 1 tile of the clone = 1 cell = 16 px (Frogger's step). Speeds in tiles/frame x 16 = px/frame.
- **Time**: kept (60 Hz). The hop is 12 frames; a car at the clone's top speed crosses a cell in 12.5 frames.
- **The loop** of 22 cells (352 px) holds the 14 visible rows plus 4 cells hidden above and 4 below.
- The clone has no difficulty curve; Frogger speeds up level by level: our speeds and densities start below
  the clone's middle and reach its full range at lane 300.
- The clone kills the hero one row behind the camera; the real game sends the eagle after ~5 s idle: our
  camera creep gives 8 s at the start and 5 s from lane 300, with a telegraphed warning.
- The clone's train shows 2.2 s after its light: our warning is 2 s (>= 1 s is tested).

## The bot (--opt bot=1, bot=2 for both parents)
It plays from the **screen only**: the sprites in OAM (its duck, the vehicles and their facing, the platforms,
the trains, the crossing lights, the waiting ducklings), the lane cells in the BG3 map and the BG3 scroll
register (which map column is on the left of the screen). Speeds are measured by following the sprites from
frame to frame (a lane is kept out of until 16 frames are measured; margins shrink as the measure lengthens);
what it cannot see is assumed dangerous (a vehicle may be just off screen; a lit crossing light is a train).
When its duck stands (it decides one frame after a landing, on the picture of the landing), it turns what it
sees into the rules' lanes and tries each move with the game's own search (lanes.c judge_run, the fairness
judge): the furthest place where a duck can rest (grass, a nest pond, a lily pad) reached soonest wins; a lost
duckling close by is worth a detour; a dead end (a pad with no way on) is remembered and the search looks
much further. Result over seeds 1-10 (36 000 frames each, make duckparade-check): **319 lanes on average**,
min 50, max 801 (364 180 206 50 280 538 801 247 313 219).

## Performance (make duckparade-bench)
A long bot run is recorded (--opt record=1) and replayed from its input script (the same game frame for frame,
without the bot's thinking); tests/bench_frames.c renders each frame three times and keeps the cheapest.
| Run (host: WSL2, x86-64) | average | median | 99% | 99.9% | worst |
|---|---|---|---|---|---|
| Mother Duck, seed 7 (748 lanes, 88 sprites at most) | 0.38 ms | 0.37 ms | 0.69 ms | 0.85 ms | 0.92 ms |
| Mother and Father, seed 6 (461 lanes, 103 sprites) | 0.37 ms | 0.36 ms | 0.68 ms | 0.75 ms | 0.94 ms |
The game's own update is about 5 us a frame (the judge runs one start per frame at most, 16 us). A20 estimate at
x15-x20: **5.7-7.6 ms on average, 10-14 ms for 99% of the frames**, the very worst frames 14-19 ms (a busy
screen: 20 columns of traffic, trains, the parades; the budget is 16.7 ms). The cloud shadows and the fox's red
edge cost about 6% of the render.

## Tests (make duckparade-check)
- `tests/test_rules.c`: the tuning table against its sources (hop timing, speeds, the loop, the fox's timing),
  the lanes (loops, trains telegraphed, platforms), the hop (12 frames, the buffer, blocking, the arc and the
  squash), roads, rivers and ponds (riding, being carried off, snapping), the parade (joining, following the
  exact path, the knock-off, the gap closing, banking, the scattering), the camera and the fox, the judge
  against the game (every straight crossing the game allows is seen by the judge: 6400 cases), the staged
  generator equal to the unstaged one, and the **fairness** over 2000 seeds (above).
- `tests/smoke_test.sh`: the title idles, strict mode clean, the start, the fox, a blind rhythm, retry, pause,
  save RAM, Father's join, co-op scoring, **the bot on 10 seeds (>= 150 lanes on average)**, **determinism**
  (a scripted run and a bot run, twice: same state and picture).
- `tests/test_ui.c` (tests/ui_test.sh): the whole game on the real picture: the title (logo, prompt and glyph,
  copyright, the marching family), get ready, the first hop, the HUD's score on every frame of a run, the
  duckling count, the sky band hiding the world's sprites (the band is the same with them hidden), the
  family photo after each banking, the pause (dimmed, PAUSED, frozen) and the resume, the game-over banner,
  panel, score, best, the egg medal and the retry line; co-op: two scores and the family panel. Screenshots.
- `tests/state_test.sh`: save states at the title, in a run, in co-op, paused, on the game-over panel and at 4
  pseudo-random frames of long runs: the state continues exactly like the run that was not interrupted, in the
  same process and in a fresh one; bad states refused; `tools/state_audit.py` checks every mutable static.
- `tools/screenshots.sh` (make duckparade-screenshots): docs/screenshots/.

## Completion checks
Title-only flow, A-start, four-parent co-op, UI, smoke and save-state checks pass. Save states preserve the
visible record for their timeline while battery records retain the greatest score; the title replay test
again includes a complete game over and return to title in the same process and a fresh process.
Screenshots and Windows executables are refreshed. Human playtesting of three/four-player balance remains.
