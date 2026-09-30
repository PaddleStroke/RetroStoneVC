# Duck Parade: design

(c) 2026 Pierre-Louis Boyer (8BCraft). Code MIT, assets CC BY-NC-SA 4.0 (games/duckparade/LICENSE). A game for the
RetroStone virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md, games/common).

## Pitch
Mother Duck has lost her ducklings all over town. Hop across endless roads, rivers, railways, park paths and
lily ponds, pick up the lost ducklings on the grass, and lead them home in a line behind you, hop for hop,
like the Snake game. A long parade is worth a lot at the next **nest pond**, but every duckling can be hit on
the way. One hop per press, instant retry, a best score to beat; a friend can join as Father Duck.

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
| backdrop | the house raster sky in the top band (one colour per line), then the grass colour | - |
| BG1 | UI (house_ui: font, panels, banners, the title logo) | fixed |
| BG2 | the lanes, top-down: grass with flowers and trees, asphalt with dashes and kerbs, rails on sleepers, gravel park paths, rapids with current streaks (palette-cycled per direction), lily ponds, nest ponds with reeds | camera |
| BG3 | drifting cloud shadows over the field (colour math: subtract, halved) | camera + wind |
| OBJ | ducks, ducklings, shadows, cars, bikes, buses, joggers, lawnmowers, logs, lily pads, the paddle boat, the train, the crossing lights, the fox, splashes, feathers, the HUD digits | |

## Controls
| Button | Action |
|---|---|
| Right, A | hop forward (to the next lane) |
| Up, Down | hop along the lane (sidestep) |
| Left | hop back |
| Start | pause / resume |
| A, Start on the game-over panel | retry at once |
| pad 2, A on the title or get-ready screen | Father Duck joins (co-op) |

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
| **lily pond** | the deep water (a pike lurks) | the static lily pads |
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
  is where Mother was k hops ago (Snake's rule). Each duckling hops 2 frames after the one in front of it
  (a ripple down the line). On a log, the path is kept relative to the log.
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
Lanes are made in **units**: a hazard group (1-5 lanes of one family, mixed families later on) followed by a
grass run (1-3 grass columns, sometimes a nest pond). The first 8 columns are a safe meadow (a hedge on the
left, trees on the top and bottom rows).

Family mix (after the meadow), from the clone's row mix (grass 1/3, road-type 1/3 of which 1/4 railway,
water 1/3), with park paths and ponds taken from the road and water shares: road 25%, railway 8%, park 9%,
river 25%, lily pond 8%, and the grass runs between groups.

**Difficulty** d = lane / 300 (capped at 1; lanes past 300 add up to 20% more speed by lane 600):
| | lane 0 | lane 300 |
|---|---|---|
| cars, speed (px/frame) | 0.32-0.70 | 0.32-1.28 (the clone: 0.02-0.08 tiles/frame) |
| cars per loop | 1-2 | 2-3 (the clone: 1-2 per 22 tiles) |
| road group | 1-2 lanes | 1-4 lanes (Frogger: 5) |
| logs, speed | 0.32-0.60 | 0.32-1.12 (the clone: 0.02-0.07) |
| log length | 3-4 cells | 2-3 cells |
| river group | 1-2 lanes | 1-3, alternating directions (Frogger's river) |
| train period | 8-10 s | 5-8 s (the clone: 4.6 s) |
| camera creep | 0.25 px/frame | 0.40 px/frame |

**The judge** (lanes.c): every hazard group is checked before it is used, by an exact time-expanded
reachability search over the duck's own moves (hop 12 frames in 4 directions, waiting, riding, snapping),
on a pixel-accurate model of the lanes (bitsets of the 224 positions of each column, per frame):
- the start set is every cell of the previous grass run that the duck can reach;
- from **each of 4 start times** (spread over the traffic period) the group must be crossed into the next
  grass run within **4 s** (the fox's grace is 5-8 s);
- a group that fails is re-rolled (up to 4 tries, easier each time), then replaced by grass: the generator
  can never hand out an impossible pattern.
- Constructive rules on top: trains are telegraphed 2 s ahead (>= 1 s is checked); adjacent river lanes flow
  in opposite directions; a lily pond keeps a pad in reach of the cells before it (the clone's rule); every
  grass column keeps 4 free cells, and the free cells of a grass run are connected.

The fairness test replays the generator over thousands of seeds with a stricter judge (every 5 frames of
start time, from each single start cell, within 5 s) and checks the telegraph and the constructive rules.

## Scoring, medals, save
- **Score = lanes crossed (the furthest column) + banked points.** The HUD shows the score, the line length
  and the best.
- Egg medals in the house medal tiers: **bronze egg** at 50, **silver egg** at 100, **gold egg** at 200,
  **pearl egg** (speckled, shining) at 300.
- Save RAM: best score, best co-op score, the longest parade banked, runs, medals won.

## Flow
1. **Title**: the house logo "DUCK PARADE", Mother and three ducklings marching in place, "PRESS A TO HOP",
   the best score, the copyright line.
2. **Get ready**: the meadow, "GET READY"; the first hop starts the run.
3. **Play**.
4. **Death**: a thud (hit: squashed flat, feathers), a splash (swept), or the fox's pounce; a short freeze,
   the ducklings scatter peeping.
5. **Game over**: the house banner and panel (score, best, medal); after 0.6 s, A or Start retries at once.
**Pause**: Start during a run (house pause: dimmed, "PAUSED").

## Two players: Mother and Father Duck (co-op)
Father Duck joins from the title or get-ready screen (pad 2, A). He is palette-swapped as a **mallard drake**:
a green head, a white collar, a grey-brown body. Each parent has **their own parade** (a lost duckling joins
whichever parent walks over it). They share the camera, which follows the parent in front; a parent who is
hit is out (their ducklings scatter), and the run ends when both are out. The panel shows each parent's
score and the family total (a friendly score, no winner).

## Audio
- Sound effects synthesised in code at start-up (sfx.c): a **quack** per hop (four variants, pitch varied),
  a **peep** when a duckling joins (rising with the line length), a **car horn** (two detuned squares) when a
  duckling is knocked off, a **splash**, the **train bell** (a struck-metal ding, repeating), the **banking
  fanfare** (a brass arpeggio), a camera click for the photo, a thud, the fox's growl, a bump, the UI sounds.
- Music: a jaunty **marching-band** loop in D major, 112 BPM (tools/make_music.py, a 3-channel MOD: snare and
  bass drum, tuba, fife melody) and **layers the game adds as the parade grows**, sequenced in step with the
  module on their own voices (a row is exactly 8 frames): a trumpet counter-melody from 3 ducklings, a
  glockenspiel from 8. Music + layers + sound effects stay within the 8 voices.

## Art
All code-drawn (tools/make_art.py, the house helpers of games/common/tools/house_style.py): flat shades, 1-px
dark outlines, light from the top-left, readable at 1x.
- Mother Duck, 16x16 top-down: a white body, an orange beak and feet, a blue wing speculum, a bonnet ribbon;
  hop frames (stretch, squash), idle, bump, hit (flat), swept, caught. Father Duck: the mallard palette.
- Ducklings, 10x10 in 16x16 cells: fluffy yellow, orange beaks; hop, peep, tumble, flutter, paddle.
- Cars: cute rounded top-down cars (red, blue, yellow, green, pink, white), bikes, a bus; joggers, a
  lawnmower; logs (3 lengths), lily pads, the swan paddle boat; the train (locomotive, carriages).
- The fox (the warning pose and the pounce), splashes, feathers, the photo frame, egg medals.

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

## Tests (make duckparade-check)
- `tests/test_rules.c`: the tuning table (hop timing, speeds converted from the sources, the loop, the fox
  timing from creep), the hop and parade rules (following, the knock-off and the gap closing, banking, riding
  and snapping), and the **fairness** of the generator over thousands of seeds (the strict judge, the train
  telegraph, the constructive rules).
- `tests/smoke_test.sh`: scripted runs (no input: the fox; a blind rhythm: hit), pause, retry, save RAM, the
  2-player join, strict mode clean; **the bot** (`--opt bot=1`), which plays from the **screen only** (OAM and
  the BG2 map: lane tiles, trees, vehicles, platforms, lights, ducklings) and must cross **150 lanes or more on
  average over 10 seeds** (the distribution is printed); **determinism** (the same run twice: state and
  picture).
- `tests/test_ui.c` (via `tests/ui_test.sh`): the whole game with scripted input; the HUD, the title, the
  banners, the panel and the pause checked on the rendered picture and the layers, with screenshots.
- `tests/state_test.sh`: save states (sdk/tests/test_states.c) and tools/state_audit.py.
- `tools/screenshots.sh`: docs/screenshots/.
