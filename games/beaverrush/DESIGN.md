# Beaver Rush: design

(c) 2026 Pierre-Louis Boyer (8BCraft). All rights reserved (games/beaverrush/LICENSE). A game for the
RetroStone virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md, games/common).

## Pitch
A young beaver has one job: gnaw the giant tree, log by log, before the river runs dry. Every press of Left
or Right hops the beaver to that side of the trunk and gnaws the bottom log away: the trunk drops by one,
chips fly, and the log tumbles into the river, floats off and becomes part of the family's dam in the
background. Branches come down with the trunk: never be under one. A timer bar drains faster and faster;
only gnawing fills it. One life, instant retry, a best score to beat.

The mechanics follow the genre's reference feel (Timberman, Digital Melody, 2014): a trunk of stacked
segments, a two-sided chop that also moves the player, a branch at head height after the drop kills, a
draining bar that each chop refills a little, the drain speeding up by level. Nothing else is taken from it:
no name, art, sound, layout or text. The twist (the dam, the scenes, the woodpecker, the versus rules), the
characters, the screens, the sounds and the music are ours.

## Legal and originality
- Timberman is proprietary. We use only its **mechanics** (not protected), as described by published reviews
  and guides and as measured in permissively licensed clones.
- Only MIT-licensed clones were read, only for their numbers and rules; the code of Beaver Rush is our own
  (C11 on the RetroStone SDK, integer fixed point). No clone's art or sound was looked at or used.
- All sources and their licences: "Feel sources" below.

## Rules
### The trunk
- A stack of **segments** 24 px tall. Segment 0 is the bottom one, level with the beaver's head and its
  teeth; segments 0..7 are on screen (a branch is visible 7 gnaws before it reaches the head: the reference
  shows it 4 or more gnaws ahead).
- Each segment has a branch on the **left**, on the **right** or **none**; some are **golden logs**.
- **Gnaw** (Left or Right): the beaver moves to that side and gnaws segment 0 away. The trunk drops by one.
  - **Hit A (the drop)**: if the new segment 0 has a branch on the beaver's side, the branch lands on its
    head: bonk, game over.
  - **Hit B (walking in)**: pressing the side where segment 0 holds a branch walks the beaver into it
    (the branch is at head height on that side): bonk, game over. The log is not gnawed.
  - The rules are instant: the drop is shown in 4 frames, but a press is resolved on the frame it happens,
    so fast players are never slowed by the animation.
- **Fairness**: with hit B, a branch on one side followed directly (the next segment up) by a branch on the
  other side cannot be survived: the beaver cannot move under the first and stays under the second. The
  generator therefore never puts two branches on **opposite sides in adjacent segments**; alternating
  branches always have an empty segment between them. Every other sequence is survivable (the proof is
  short: under a branch the beaver must stay, and the next segment is empty or on the same side; under an
  empty segment it can choose the side the next segment leaves free). The fairness test checks it over
  thousands of seeds by brute force (every reachable beaver position, every step).

### The timer
- A bar on top of the screen. It starts **full** and does not move until the first gnaw.
- It drains continuously; each gnaw refills a little; a golden log refills a quarter. Empty: the beaver runs
  out of breath (it slumps, "zzz"): game over.
- The drain goes up by **level**: one level per **20 logs** (as the reference). The curve is the reference
  clone's break-even chop rate (below): 2.9 gnaws per second to hold the bar at level 0, 4.6 at level 1, 5.3
  at level 2, 5.8 at level 4, 6.5 at level 15 (300 logs). From level 15 on we add 0.12 gnaws/s per level (an
  endgame ramp, ours), so every run ends: 7.7/s at 500 logs, 8.9/s at 700.

### Generation
Segments are made one at a time by a small generator with its own xorshift RNG (the seed is the run's; the
2-player game gives both trees the same seed, so the same sequence). Per **stage** (every 50 logs):

| Stage (logs) | Branch chance | Zig-zag runs | Notes |
|---|---|---|---|
| 0 (0-49) | 40% (the reference's 2 in 5) | none | the first 4 segments are empty |
| 1 (50-99) | 44% | none | |
| 2 (100-149) | 48% | 10% | the woodpecker arrives |
| 3.. | +4% per stage up to 60% | +4% per stage up to 26%, 3 to 6 branches | |

- A branch right above another one is always on the **same side** (a "stack"); otherwise the side is a coin
  toss, and a same-side stack of 2-3 is chosen 15% of the time.
- **Zig-zag runs**: branches alternating sides with exactly one empty segment between them (L . R . L), the
  fastest fair pattern: a side switch on every second gnaw.
- At most 3 empty segments in a row (the rhythm never goes slack), at most 4 same-side branches in a row.
- **Golden logs**: an empty segment turns golden with a 1-in-24 chance once at least 30 segments have passed
  since the last one. Gnawing it: +5 logs (the golden log counts 5) and a quarter of the bar.

### Stages and the dam (the twist)
- Every gnawed log splashes into the river and **floats to the dam** in the background; the dam visibly grows
  **log by log** (each log is drawn into the dam's tiles at its slot) and the pond behind it rises.
- The dam has **6 sections** of **50 logs** (a 5 x 10 brick pattern of small logs). **Every 50 logs** (a
  milestone) a section completes: a splash on the dam, the beaver family on the dam **cheers**, and the scene
  **advances**: dawn, day, sunset, night; then the next season (summer, autumn, winter, spring) with its
  palettes (foliage, snow, blossoms, falling leaves, snowflakes, fireflies at summer night), and so on.
  After 300 logs the dam is whole: the next milestones plaster the sections with mud and grass, log by log.
- The **woodpecker** (from stage 2): it clings to the trunk beside the next branch on the beaver's side (the
  next place where the beaver will have to switch) and pecks at it; it pecks twice as fast when a zig-zag run
  is on screen. A hint, not a rule: it never changes the game.

### Scoring, medals, save
- Score = logs gnawed (a golden log counts 5). Big digits at the top.
- Medals (the house medal tiers, docs/art-direction.md): bronze at 50, silver at 100, gold at 200, the top
  tier at 300.
- Save RAM: the best score, the number of runs, the medals won, the versus wins of each pad.

## Flow
1. **Title**: the logo "BEAVER RUSH", the beaver idle at the foot of the tree, the dam scene at dawn, the
   blinking prompt, the best score, "(C) 2026 8BCRAFT".
2. **Get ready**: the same scene, "GET READY". The first gnaw starts the run (and gnaws).
3. **Play**.
4. **Bonk** (a branch) or **out of breath** (the timer): the beaver's dizzy or sleepy face, a hit-stop, a
   small screen shake, the bonk sound; the tree stays.
5. **Game over** panel slides up (the house panel): SCORE, BEST (NEW BEST), MEDAL. After 0.6 s, any gnaw
   button retries at once: back to 2 with a fresh tree (the dam and the scene are those of the new run).

**Pause**: Start during a run (the house pause: dimmed picture, PAUSED); Start again resumes.

## Controls
| Button | Action |
|---|---|
| Left, B (also Y) | gnaw on the left |
| Right, A (also X) | gnaw on the right |
| Start | pause / resume (starts on the title) |
| Pad 2, a gnaw button on the title or get-ready screen | player 2 joins (versus) |

## 2-player versus
Split screen (SDK viewports): the left half is player 1, the right half player 2, each with its tree, its
beaver, its timer and score. Both trees have the **same seed** (the same branch and golden-log sequence) and
the same timer rules: the timers race. The **last beaver standing wins** (a bonk or an empty bar ends that
player; the round ends when one is out; both on the same frame: the higher score wins, else a draw).
**Stolen chips**: each time a player reaches a milestone (every 50 logs) they send **one branch** to the
rival: it appears at the top of the rival's visible trunk (segment 7, seven gnaws of warning, marked with a
red ribbon), on the side where the rival stands, in the first empty segment from 7 up where the fairness rule
still holds (it is never next to a branch of the other side, never on a golden log). Both players get the
same number of chances; a stolen branch is visible as long as any other one.

## Screen and layers
320x240 at 60 Hz. Ground (the near bank) at y = 200; the trunk is 48 px wide, centred (x 136-184).

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the sky gradient (raster, one colour per line, per time of day), the pond and the river (water colours per line, shimmer) | - |
| BG4 | clouds, far mountains, the far forest (the pond covers its foot as it rises: window per line) | per-line: clouds drift, mountains 1/8, forest 1/4 of the camera lean |
| BG3 | the dam (a canvas of unique tiles: each log is drawn into them), the river banks, the near bank with the stump, framing trees | per-line: dam 1/2, near bank 1 |
| BG2 | the trunk and its branches (redrawn on each gnaw; the drop is a vertical scroll) | the drop |
| BG1 | the house UI: text, panels, the logo, the timer bar | fixed |
| OBJ | the beaver(s), the tumbling log, chips, splashes, floating logs, the family, the woodpecker, sky objects (sun, moon, stars), weather particles, digits, medals | |

**Camera lean**: when the beaver changes side the camera eases 6 px toward it and the background layers follow
at their depth (1/8 .. 1): a small parallax that makes the valley deep without moving the trunk.

2 players: two viewports of 159 px (the SDK's standard left/right layout); the BG2 map holds the two trees side
by side (x 0-255 and 256-511), each viewport scrolls to its own; the background is the same valley.

## The beaver
About 24x24 on screen (32x32 cells): brown fur, a lighter belly and muzzle, **big orange teeth**, a flat
cross-hatched tail, small round ears. Frames: idle (2, a breath and a tail twitch), **gnaw** (3: wind-up,
the bite with a squash, 20% wider and 15% lower, and the recovery), hop (the side change, 1), bonk (dizzy, with
stars), out of breath (slumped, zzz), cheer (arms up). Player 2 is a darker, reddish beaver.

## Audio
- Sound effects synthesised at start-up (src/sfx.c): **chomp** (a crunchy bite: a noise burst through a
  resonant low-pass and a short wood knock; each gnaw plays it at a pitch varied +-6% by a tiny RNG), the
  **log splash**, the **golden chime**, the **branch bonk** (a hollow wood knock and a boing), the **milestone
  cheer** (a short rising arpeggio and a crowd-like noise swell), the breathless slump, the woodpecker's tok,
  a stolen-chip whoosh; the house UI sounds (panel, pause, join, medal) come from games/common.
- Music: a lively banjo/folk loop (tools/make_music.py: a 4-channel MOD, synthesised plucked banjo, a
  double-bass walk, a fiddle-like lead, a brushed snare; G major). It is rendered at four tempos (BPM 128,
  136, 144, 152); the game picks the next loop's tempo from the drain level, so the music speeds up subtly as
  the timer does (the switch happens at the loop's end, every 8 bars).

## Feel sources
The reference: Timberman (Digital Melody, 2014, proprietary). Its feel is described by reviews and guides,
and several permissively licensed clones were tuned against it. Only numbers and rules were taken.

| # | Source | Licence | What we took (numbers and rules only) |
|---|---|---|---|
| 1 | **Timber** (the "Timber!!!" game of *Beginning C++ Game Programming*, J. Horton), the clone at https://github.com/aholmes2534/Timberman (`main.cpp`) | **MIT** (LICENSE file of the repo) | a 6-s bar (drawn 400 px wide), drained 1 s per second; each chop adds (2 / score + 0.15) s; 6 branch positions; a new branch is left 1/5, right 1/5, none 3/5; the hit check is on the lowest position, after the trunk moved down |
| 2 | **Timberman-clone**, carlosebmachado-games, https://github.com/carlosebmachado-games/Timberman-clone (`scripts/game.gd`, `scripts/time.gd`) | **MIT** (LICENSE file) | never two branch segments in a row, the first 3 segments plain, 10 segments; the bar drains 10% per second, a chop adds 1.8% |
| 3 | **lumberjack**, jayhxmo, https://github.com/jayhxmo/lumberjack (`Source/Tree/Tree.cs`, `Source/Mechanics/Health.cs`) | **MIT** (LICENSE file) | 16 segments; branches on 4 in 5 segments, the side after a branch forced; the bar (100) drains 1 per 50 ms (5 s) |
| 4 | **GameGrin**, "Timberman Review", https://www.gamegrin.com/reviews/timberman-review/ | article (facts cited) | the timer "gets quicker with each level", a level "every 20 branches chopped" |
| 5 | **iMore**, "Timberman: Top 8 tips, hints, and cheats", https://www.imore.com/timberman-top-8-tips-hints-and-cheats-you-need-know | article | a branch appears at the top of the screen 4 chops before it reaches the head |
| 6 | **Pure Nintendo**, "Review: Timberman (Nintendo 3DS)", https://purenintendo.com/review-timberman-nintendo-3ds/ | article | the 3DS version levels up every 50 chops (we use 50 for our scene milestones) |

### From the sources to Beaver Rush
- **The drain curve**. In source 1 the chop rate needed to hold the bar at score s is
  R(s) = 1 s/s / (2/s + 0.15) s = s / (2 + 0.15 s) chops per second (2.9 at s = 10, 5 at 40, 6 at 120, a
  limit of 6.67). Timberman raises the drain by level (sources 4, 6) instead of lowering the refill, so we
  keep the refill **fixed** and make the drain of level L equal to the refill times R(20 L + 10) (the rate
  at the middle of the level): the same pressure curve, in steps, as the reference.
- **The bar at level 0** lasts 6 s from full (source 1; source 3: 5 s, source 2: 10 s). With R(10) = 2.857
  chops/s to hold it, one gnaw refills 1/6 / 2.857 = **5.83%** of the bar (0.35 s of level-0 time).
- **Branches**: 40% at the start (source 1: 2 in 5), up to 60% (source 3 goes to 80%, too dense with our
  fairness gaps); source 2's "never two in a row" is relaxed into our fairness rule (same-side stacks are
  allowed, opposite sides need a gap), which keeps the reference's "switch or die" decisions.
- **Look-ahead**: 8 segments on screen (source 1 shows 6, source 2 10, source 3 16; the guide: 4 chops of
  warning at least).

### The final table (games/beaverrush/src/tuning.h)
| Quantity | Reference | Beaver Rush |
|---|---|---|
| bar at level 0, from full | 6 s | 6 s (360 frames) |
| refill per gnaw | 0.15 s + 2 s / score | 5.83% of the bar (fixed) |
| break-even rate | s / (2 + 0.15 s) chops/s, rising with every chop | the same curve per level of 20 logs: 2.9, 4.6, 5.3, 5.6, 5.8 ... 6.5 at 300, then +0.12 per level |
| level length | 20 branches (review) | 20 logs |
| branch chance | 40% | 40% rising to 60% (+4% per 50 logs) |
| segments shown | 6 | 8 (0..7) |
| golden log | - | 1 in 24 after 30 plain segments; +5 logs, +25% bar |
| milestone | 50 chops (3DS) | 50 logs: a dam section, the next scene |

## Tests (make beaverrush-check)
- `tests/test_rules.c`: the tuning curves (the level-0 bar lasts 360 frames, the refill, the break-even rate
  of each level within 1% of R(20 L + 10), monotonic, the endgame ramp), the gnaw rules (hit A, hit B, the
  drop), the timer (no drain before the first gnaw, empty = out), golden logs, milestones and the dam slots,
  the generator's **fairness over 20000 seeds x 1000 segments** (the invariant, and a brute-force search over
  every reachable beaver position), its statistics per stage (branch share, zig-zags, golden spacing), the
  stolen-chip insertion (never unfair), and **determinism** (the same seed, the same trunk and run).
- `tests/smoke_test.sh`: scripted runs (the title waits, a wrong press bonks, doing nothing after the start
  runs out of breath), pause, retry, save RAM, player 2 joins, a versus round ends with a winner, strict
  mode clean; **the bot** (`--opt bot=1`) plays from the **screen state only** (the BG2 map: which tiles are
  branch tiles on each row; OAM: its beaver; BG1: the timer bar) at a human cadence (7 to 9 frames between
  gnaws, 3 more to switch sides): over **10 seeds** it must average **300 logs or more** (the distribution
  is printed); determinism (a scripted run and a bot run twice: the same state hash and picture).
- `tests/test_ui.c` (the UI screenshot test): the game runs headless to the title, a run, a milestone, a
  golden log, night, the game-over panel and a versus round; each picture is checked (the timer bar's
  filled width matches the timer, the digits, the beaver on its side, the logo, the panel, the two halves
  in versus) and written to build/beaverrush-ui/.
- `tests/state_test.sh`: save states in a run, a versus round, paused, on the panel (sdk/tests/test_states.c).
- `tools/screenshots.sh`: docs/screenshots/ (title, early play, a milestone, a golden log, night, game over,
  2 players).
