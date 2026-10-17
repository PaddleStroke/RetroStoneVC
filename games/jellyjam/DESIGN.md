# Jelly Jam: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/jellyjam/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

## Pitch
A falling-pairs puzzle duel in a jam kitchen. Pairs of shiny jellies fall into your jar: put four of a colour together
and they melt, the ones above fall and melt in turn - a chain - and every melt rains grey jellies into your opponent's
jar. One player duels the kitchen's chefs, a mouse to a bear, each quicker and cleverer; two to four friends play
everyone against everyone, best of three. Just one more chef.

## Legal and originality
- The mechanics come from **Puyo Puyo** (Compile, 1991; Sega since), which is proprietary. We use only its
  **mechanics**, which are not protected: a 6 x 12 field with a hidden row above, pairs that fall, move and turn (wall
  kicks, the flip in a one-wide well), four or more of a colour touching orthogonally clear, gravity and chains,
  garbage sent to the opponent and cancelled by your own clears (offset), the game lost when the field fills up to the
  spawn point, a CPU ladder in one player.
- Nothing else is taken from it: no name, character, art, colour, sound, music, text, voice, score table or number
  measured from it; we did not look at its code or anyone's clone. The theme (a jam kitchen, jellies with faces, jam
  jars), the hero Jam and the eight animal chefs (Pip, Dot, Hopsy, Mochi, Truffle, Basil, Bao, Bruno) and their names,
  the scoring and rain numbers, the brains, the rain's targeting for 3-4 players, the touch controls, the medals, the
  sounds and the music are ours. The code is our own (C11 on the RetroStone SDK, integer only), written from this
  design.
- Every number below is ours, tuned with our own bots for our 320x240 screen.

## Rules
The field (play.c) is **6 columns x 12 rows** on screen, plus a **hidden 13th row** above (it holds jellies, they
never melt there; a jelly landing above it is lost). Four colours: strawberry, lime, blueberry, lemon; and the grey
**rain**. Every field gets **the same pairs in the same order** (a hash of the duel's or round's seed).

- **The pair** comes in on the 3rd column, its pivot in the 12th row (the child above it, in the hidden row). Left /
  right move it a column (held: again after 10 frames, then every 3); **A turns it clockwise, B counter-clockwise**:
  the child goes round the pivot. **Wall kicks**: a turn blocked on the child's side (a wall or a stack) pushes the
  pair a column away; a child turned into the floor or the stack lifts the pair a row; blocked on both sides (a well one
  column wide), a second press within 20 frames **flips** the pair. It falls at 10/256 of a cell a frame at the start
  (a cell in 26 frames), 3/256 faster per duel level and 5/256 faster every 20 s of a duel or a round (at most a cell in
  4 frames); down drops it at 112/256 (a point a row). Landed, it waits 28 frames (moves and turns on the ground, a floor kick or a flip lifting it too,
  restart the wait, 8 times at most; down lands it at once). Then each jelly falls on its own (a horizontal pair over
  uneven ground **splits**).
- **Melting**: **4 or more jellies of a colour touching** (up, down, left, right, on screen) flash for 26 frames and
  pop (42 frames in all); **rain next to a melting group melts with it**. The jellies above fall (accelerating) and may
  form new groups: a **chain**, step after step.
- **Points** for each step: 10 x jellies x (chain power + colour bonus + group bonus), at least 10 x jellies. Chain
  power 0, 6, 14, 28, 48, 80, then +32 a step; colour bonus 0, 3, 6, 12 (1-4 colours at once); group bonus 0, 2, 3, 4,
  5, 6, 7, 10 (groups of 4, 5, 6 ... 11+).
- **Rain**: every 60 points (soft drop included) is a grey jelly sent; what is left waits for the next melt. **Your own
  melts first cancel the rain coming to you (offset)**, the rest goes to the opponent. Rain falls on a field when its
  turn is over (its pair landed, its chain done): **5 rows (30) at most at once**, full rows first, the odd ones on random
  columns; the rest waits. The queue shows above the field: small (1), big (6), a pile (30), a jar (180), a crown (720).
- **Losing**: a field filled up to the spawn cell (3rd column, 12th row) when its next pair should come is out (its
  jellies spill out of the jar).

### One player: the chefs
Duels against the computer chefs, one after the other: **Pip** the mouse, **Dot** the duckling, **Hopsy** the
bunny, **Mochi** the cat, **Truffle** the piglet, **Basil** the fox, **Bao** the panda and **Bruno** the bear, the head
chef; then they come back with Bruno's brain and faster pairs (level 9, 10...). A duel won: **+1500 x its level**, the
next chef ("LEVEL 2 - VS DOT"); **a duel lost is the game over** (back to the title, the house flow). The score: the
points of every melt and chain, and the bonuses. The banner before the first duel shows the controls.

**The chefs' brains** (play.c ai_play; the table CPU_TABLE in game.h): smart but fair. They see the same pairs, the
current one and the next only, and press the same buttons as a player, at their own pace. Each pair is planned in
stages, one a frame while the chef "thinks" (so no frame costs much: under 0.4 ms on the build host): every spot the
pair can reach from the spawn column, judged cheaply (colours touching, heights, spots where one more jelly would
melt a group, the chain the spot fires); then, for its 4 best spots, the longest chain one or two more jellies could
fire (its chain-building sense); then, for those that plan ahead, the next pair after its 3 best spots; then its pick.
It fires a chain when it is at least its **ambition** long, or when its field is crowded (a column of 8, or 40 jellies),
or (aware chefs) when 6 or more rain are coming, or when the chain would bury the opponent.

| Chef | Thinks | Then an input every | Soft drops | Plans the next pair | Fires from | Mistakes | Judgement wobble | Aware |
|---|---|---|---|---|---|---|---|---|
| 1 Pip | 64 frames | 20 | no | no | 1 step | 45% | 100 | no |
| 2 Dot | 52 | 17 | no | no | 1 | 36% | 85 | no |
| 3 Hopsy | 36 | 12 | no | no | 2 | 20% | 55 | yes |
| 4 Mochi | 28 | 10 | yes | no | 2 | 14% | 40 | yes |
| 5 Truffle | 22 | 8 | yes | yes | 3 | 9% | 30 | yes |
| 6 Basil | 16 | 7 | yes | yes | 3 | 5% | 20 | yes |
| 7 Bao | 12 | 6 | yes | yes | 4 | 3% | 12 | yes |
| 8 Bruno | 9 | 5 | yes | yes | 5 | 1% | 6 | yes |

A mistake: the pair goes to one of its 5 best spots at random; the wobble is noise added to its judgement of each spot.

### Two to four players
Everyone against everyone in their own jar, side by side (pads 2-4 join on the title). **The rain goes to the leader**:
the field with the least on it (its jellies plus the rain already coming to it), the next one round the table on a
tie. Why: it hits whoever is safest right now, so nobody already buried is finished off by everyone (no ganging up on
the weakest), it keeps the rounds close and short, and a big chain naturally lands on the one to beat; round robin
was fairer on paper but kept hitting the struggling players in turn. **The last jar standing wins the round; the first
to win 2 rounds (best of 3) wins**; the results rank everyone by rounds won (then by how long they lasted the last
round), with medals for the places.

### Tuning (every number in src/game.h)
| Rule | Value | Where |
|---|---|---|
| the field | 6 x 12, a hidden row, spawn 3rd column 12th row | COLS, VROWS, ROWS, SPAWN_* |
| the fall | 10/256 a frame, +3 a level, +5 every 20 s, at most 64; soft drop 112; touch swipe 176 | FALL_*, SPEEDUP_*, SOFT_SPEED, FAST_SPEED |
| landing | waits 28 frames, 8 restarts | LOCK_* |
| moving | repeat after 10 frames, every 3; quick turn within 20 | DAS_*, QUICK_TURN |
| melting | 4 or more; 42 frames (26 flashing) | MELT_* |
| points | chain power, colour and group bonuses above | CHAIN_POWER, COLOUR_BONUS, GROUP_BONUS |
| rain | 60 points each; 30 at most at once | RAIN_POINTS, RAIN_MAX_DROP |
| duels | +1500 x level; 8 chefs | WIN_BONUS, CPU_TABLE |
| rounds | best of 3 | WINS_NEEDED |
| the brains' judgement | colours touching 10, potential 12 x (half chains)^2, heights, crowded 8 / 40 | AI_* |
| touch | a cell's width of drag a column; drag down 28 px: soft drop | play.c touch_column, TOUCH_SOFT_PX |
| medals (1 player) | bronze 3000, silver 12000, gold 35000, pearl 70000 | MEDAL_SCORES |

**The medals** were tuned with the player bot (`bot=1`: the chefs' brain as a quick, mistake-free player that plans
ahead and fires chains of 5) and the human-like one (`botskill=E`: it thinks 10 + 5E frames, presses every 3 + E,
misplaces 4E% of its pairs at one of its 5 best spots, wobbles 12E in its judgement, plans ahead only below E = 3 and
fires shorter chains), 12 runs each of the one-player game: E = 10 (a beginner) beat Pip in 9 of 12 runs, mean 3550
points (9 bronze); E = 8: mean 10200, chefs 2-4; E = 6: 11700 (all bronze, 4 silver); E = 4: 30900 (all silver, 4
gold); E = 2: 39700 (6 gold, 1 pearl); the player bot: 55600, up to chef 9 (10 gold, 3 pearl). So bronze is beating
Pip, silver three or four chefs, gold six, pearl the whole kitchen.

## Screen and layers
320x240 at 60 Hz, fixed step. The house layout:

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | dark plum (the jar frames' rounded corners) | - |
| BG3 | the kitchen: a dusty-teal tiled wall, the fields' jar frames (4 px) and their dark plum insides, the wooden board between two fields, shelves of jam jars in the margins of a wide screen | shake |
| BG2 | the fields' jellies, **painted by software** (below) | shake |
| BG1 | the house UI kit: the logo, the title's lines, the banners (LEVEL 2, ROUND 1, GO!, YOU WIN!), the chain counts, the tags and scores of 3-4 players, the panels, PAUSED | fixed |
| OBJ | the next pairs, the rain queues, jam droplets, round stars (OBJ 6, the jelly palette), the hero Jam (OBJ 0, 1, 4, 5: the players' colours), the chef of the duel (OBJ 7, loaded with the chef), the kit's digits, glyphs and medals (OBJ 3) | |

**The fields, painted by software.** Two fields have 16-px cells (on the tile grid), three or four have 12-px cells (not
on it), so the jellies are not BG metatiles: each field is a picture of 8x8 tiles (its inside, the frame's width round
it, a 16-px band above for the hidden row and the pair coming in), painted every frame from the world - each jelly
at its pixel height as it falls, joined to its neighbours of the same colour (16 body shapes per colour and size),
squashing as it lands, flashing and shrinking as it melts, the child swinging round the pivot as it turns - and the
tiles that changed are uploaded (`rs_tiles_load8`). A tile with no jelly is the blank tile; the others come from a
pool of BG2 tiles handed out as needed, so the VRAM used follows the jellies on screen. One BG palette for all the
jellies (an outline, white, four colours of 3 shades, the rain's grey). Measured: no strict-mode warning with 1-4
fields (VRAM, sprites, OAM).

**The layout** (play.c field_layout): 2 fields (1 player and the chef, or 2 players) of 96 x 192 at x 20 and 204,
the wooden board between them with the next pairs, the chef (its name and level) and the hero; 3 fields of 72 x 144
112 px apart; 4 fields side by side, 80 px apart (their frames touching), the tags, scores, stars and next pairs above
them. **Widescreen** (docs/spec.md "Widescreen"): the game opts in; the fields keep their places on the centred 320-px
stage and the rules are unchanged (play.c never asks the width); a wider screen shows more kitchen (shelves of jam
jars in the margins) and, from 427 columns, the hero and the chef step out beside the fields onto their own shelves.
A state saved at one width loads at another.

## Screens (the house flow: no "get ready", the title is the only menu)
title (the logo, Jam the jelly chef bobbing among four jellies, the player slots, PRESS A TO COOK with the A glyph,
BEST, the join line, the copyright line; a kitchen counter with jars) -> P1 presses A (or B, Start; a tap) -> one
player: the banner LEVEL 1 - VS PIP (and the controls, the first time), GO!, the duel; YOU WIN! and the next chef ...
until a duel is lost: the jar spills, the game-over panel (the banner, SCORE, BEST or NEW BEST, the medal, the level
reached) -> one press (or a tap): back to the title. Two to four players: ROUND 1, GO!, the round, P2 WINS! ...
the results ranking (rounds won, medals for the places). Select (or Start) pauses.

## Controls
| Input | Action |
|---|---|
| d-pad left / right | move the pair (held: it slides on) |
| d-pad down | drop it faster |
| A (or X) | turn it clockwise |
| B (or Y) | turn it counter-clockwise |
| A (or B, Start) on the title | start |
| Select (or Start in a duel) | pause / resume |
| pads 2-4: A or Start on the title | join (B leaves) |
| A, Start or Select on the game-over panel | back to the title |

### Touch (docs/spec.md "Touch")
The game reads the touches itself (`rs_game.touch_native = 1`): the phone apps pass it the fingers and draw no
buttons. Touch is player 1, alongside the pad (players 2-4 join with pads).

| Touch | Action |
|---|---|
| a finger dragged left or right (anywhere) | the pair follows it column by column: a cell's width of drag a column (16 px, 12 with 3-4 fields), counted from where the finger took the pair |
| a tap | turn the pair clockwise |
| a swipe down | drop the pair at once (it falls fast until it lands) |
| a finger dragged down 28 px (more down than sideways) | drop faster while it stays there |
| a tap on the title / on the game-over panel | start / back to the title (after the panel's lock; a finger left down from the run does not count) |
| the app's pause button | pause / resume (it presses Start); the fingers do nothing while paused |

A new pair under a finger still down is steered from where the finger is. What the fingers set (the finger's slot,
where it took the pair) is in the registered state (main.c touch_*). `tests/touch_test.sh` plays scripted fingers
at 320, 427, 520 and 560 columns.

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation. The jellies: round
shiny blobs (3 shades lit from the top-left, a white glint, a dark violet outline open where they join), 16 and 12 px,
each colour with its own face (strawberry: round eyes and a smile; lime: happy arcs; blueberry: sleepy lids; lemon:
wide eyes and an "o"; the rain dozes), blinking now and then, surprised as they melt; jam droplets fly when they
pop. The hero **Jam**: a strawberry-jam jelly in a chef's toque (idle, happy, sad; players 2-4 blue, green, purple).
The chefs: 32x32 busts of our own animal cooks in toques and neckerchiefs (idle, happy, dizzy). The kitchen: a dusty
teal tiled wall, jar frames with a lilac glass rim round dark plum insides, wood, jam jars with gingham lids. The
title logo: JELLY in strawberry, JAM in apricot.

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects (main.c sound_init): a tiny tick for a step, a
wobbly "bloop" for a turn, a soft "plop" for a landing, a juicy "pop" for a melt (pitched up a step each chain step)
with a bright bell from the 2nd step, a whoosh when rain is sent, a heavy "thud" (and a small shake) when it falls on
you, a long sad slide when a jar is lost, a four-note fanfare for a duel or a round won; the house sounds for the menus,
the medals and the game over.

Music: "Jam Session" (tools/make_music.py): G major, speed 6 (125 BPM), A A' B A' C (about 38 s): the water-drop
pluck bouncing on the off-beats like jelly, a hopping bass, a soft kick and hat (the busy cook's knife), the breathy
lead with a cheeky phrase that wobbles up and plops down. Two alternatives (tools/make_music_alt.py ->
music-alt/alt1.mod, alt2.mod; previews in docs/music-alternatives/jellyjam/): "Slow Simmer" (a cosy sway in F major)
and "Sugar Rush" (a quick sparkly chase in A minor with drums). To switch, call make_music_alt.py OUTDIR N from
tools/build_assets.py instead of make_music.py.

## Tests
`make jellyjam-check`: the rules alone (tests/test_rules.c: turns, moves to the walls, wall kicks, the floor kick, the
quick turn; the landing wait, the split pair, the hidden row; groups of 4 (not 3, not diagonal, not grey alone, not in
the hidden row), rain melting next to a group; gravity and 2- and 3-step chains, the points and bonuses; the rain
(amounts, the offset, 5 rows at most, the odd ones, the leader as the target); losing, rounds and best of 3, places,
duels and the next chef; the brains (no suicide, they fire when threatened, every chef alone keeps going 90 s, chef 8
beats chef 1, determinism); the layouts; touch columns), the libretro loader, the smoke tests (flow, start inputs,
moves, turns, soft drop, the game over and back to the title, pause, joining and leaving, 2-4 players, no strict-mode
warning, save RAM, the bot beating chef 1 with chains, determinism), the touch tests, the UI screens
(tests/ui_test.sh), the save states (the title's lobby, a duel, a banner, the game over, a run played by touch, 4
players, paused, the results) and their audit, at 320, 427 and 520 columns, states across widths, and the 22 languages
(tests/i18n_runs.txt).
