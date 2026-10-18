# Seal Shove: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/sealshove/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

## Pitch
Four round seals in woolly caps and scarves on an ice floe, and only one may stay. Waddle on the slippery ice,
belly-slide into the others and shove them into the sea, while the floe cracks and chunks of it break off and drift
away. The last seal on the floe wins the round; the first to 3 rounds wins the match. A party game for up to four
(bots fill the free places), and alone a ladder of matches against bots that get a little smarter each time.

## Legal and originality
- The mechanics are the classic arena knock-out ("sumo") of many party games: bodies pushed by momentum and mass, a
  dash, an arena that shrinks, the last one standing wins the round, rounds make a match. These mechanics are not
  protected, and we did not take them from any one game: no name, art, colour scheme, sound, music, text, level,
  number or code of any other game was looked at or used.
- Everything here is ours: the seals in their caps and scarves, the ice floe that cracks into chunks, the fish and
  their two powers, the penguins watching, the bots and their levels, the touch controls, the medals, the sounds and
  the music. The code is our own (C11 on the RetroStone SDK, integer fixed point), written from this design; every
  number below was tuned with our own bots for our 320 x 240 screen.

## Rules
Seen from above (the seals in the house's 3/4 view), on a round ice floe of radius 94 px in the middle of the sea.
**Always 4 seals**: the players first (1-4), bots in the free places. Positions and speeds are Q12 fixed point; the
floe is a grid of 4-px cells, its edge the grid's field smoothed between the cell centres (play.c floe_field): a seal
whose middle leaves the ice falls in.

- **Waddle (d-pad)**: 0.06 px a frame a frame of push; the ice keeps 1 - 11/256 of the speed each frame while the
  d-pad is held (top speed 1.4 px a frame, reached in about a second), 1 - 16/256 when let go: it slides on about
  22 px. Turning is an arc: the old speed fades slowly while the new push builds up. Heavy, funny, readable.
- **Belly slide (A)**: 4 px a frame in the d-pad's direction (or where the seal faces) for 14 frames, nearly
  frictionless (4/256: about 50 px), then the flippers brake (26/256 for 14 frames: about 25 px more). Then 45 frames
  (0.75 s) of cooldown; a sparkle when it is back. A slide that hits a seal ends there.
- **Bumps**: circles of radius 10 px (12 with the big belly). Two seals that meet bounce apart along the line between
  them: the impulse of their closing speed with 110% restitution (a little bouncier than elastic), split by their
  masses (a seal 256, a sliding one 1.5 x, the big belly 2.5 x, a mega slide 2.5 x); they always part at 0.8 px a
  frame at least. The pushed seal is "knocked" for 30 frames: it slides with 18/256 friction even if it pushes back
  (holding the d-pad against the slide still shortens it). A walking shove sends a seal about 15-30 px, a slide's hit
  50-70 px, a mega slide's well over 100.
- **Clash**: two slides head-on bounce both back at 2.6 px a frame (split by mass), and both are dizzy (no control)
  for 24 frames (stars turn over their heads).
- **Falling off**: the seal slides on a little, splashes (10 frames), its head pops up and it swims off (80 frames):
  out of the round. If another seal pushed it within the last 2.5 s, that seal gets the **knock-out**; two knock-outs
  within 1.5 s: **DOUBLE KNOCK-OUT!**
- **The floe**: a wobbly round outline (+-4 px, a new shape every round), three rings of chunks round a core: 12, 9
  and 6 chunks, each ring turned and twisted its own way. From 2.5 s after GO! a chunk on the edge **cracks** (a dark
  crack round it, it blinks faster and faster: CRACK_WARN = 100 frames), then **breaks off** (a crunch, ice chips) and
  drifts away, sinking. Only a chunk touching the water can crack (outer rings 6 times as likely as the inner one,
  the middle ring 3 times), every 130 frames at first, 5 fewer each time down to 60. A seal on a chunk that breaks
  falls with it. The core goes last, 4 s after the last chunk round it. When every player is out (1 player: you
  fell), the floe crumbles faster (a crack every 30 frames, 50 of warning) so the bots finish the round quickly.
- **Fish**: 7 s after GO!, then every 9-15 s, a fish flops onto solid ice (14 px of ice round it, 34 px from every
  seal) for 8 s. The seal that touches it gets its power: the **blue fish: BIG BELLY!** (2.5 x the mass and a bigger
  body for 7 s: hard to push, hits hard), the **golden fish: MEGA SLIDE!** (its next slide within 10 s goes 1.3 x as
  fast and hits as 2.5 x its mass). A badge over the seal shows it (an iron weight on blue, a bolt on gold; blinking
  before it ends).
- **Rounds and matches**: ROUND n (1.5 s, everyone in place on a circle of radius 46, facing the middle), GO!, the
  fight; the last seal on the floe wins the round (the last two falling in the same moment: NOBODY WINS THIS ROUND!).
  The first to 3 rounds wins the match.
- **1 player**: you against 3 bots. Win the match and the next one comes with smarter bots (level 0, 1, 2... 5);
  lose a match and it is game over. **Score**: 10 a knock-out, 25 a round, 100 a match, +50 a double knock-out,
  +50 a clean sweep (the round won with all three knock-outs yours). **Medals**: bronze 100, silver 400, gold 750,
  pearl 1200.
- **2-4 players**: one match (the bots at level 1 fill the floe), then the results rank the players by rounds won
  (ties broken by knock-outs, the match's winner first); when a bot won the match, the banner names it.

### The bots: smart but fair
play.c bot_think, the levels in BOT_LEVEL_TABLE (game.h). A bot decides every few frames (24-38 frames at level 0,
7-12 at level 5; sooner just after it was hit) and keeps its heading in between:
1. **Knocked and sliding**: it digs in against the slide (if it does not "forget": mistake %).
2. **The edge**: it looks where its own slide will take it (14 frames ahead) and walks back to the middle of the safe
   ice when that point is closer to the water than its margin (14-25 px; the margin shrinks on a small floe, so that
   nobody hides on the core for ever). It sees the cracking chunks as water only some of the time (30% at level 0,
   100% at level 5).
3. **Dodging**: a slide coming at it (on course, within 24 frames) is dodged sideways, to the side with more ice, 8%
   of the time at level 0, 60% at level 5; the better ones dodge with a slide of their own.
4. **A fish** within 90 px, on safe ice: it goes for it (30-90% of its decisions).
5. **A target**: the seal that is nearest and nearest the water (2 px of distance per px of ice between it and the
   water), not one that two others already go for. It walks round to its middle side (the attack point behind it,
   opposite to the water), then slides into it when lined up (cos >= 0.8) within 70 px (22% of the chances at level 0,
   72% at level 5), aiming where it will be, and only if it stops on the ice when it misses (a careful bot); lined up
   and close, it simply shoves it by walking.
- **Mistakes**: its aim wobbles (20/256 of a turn at level 0, 4 at level 5), it forgets the edge now and then (16% of
  its decisions at level 0, 2% at level 5) and so walks or slides off by itself, it reacts late.
- **Fair**: the bots target any seal, not the player; at most two go for the same one; the player bot (bot=1, the
  tests) uses the same brain with the quickest reactions.
- Rounds of 4 bots (test_rules --rounds, 40 rounds a level): 19-23 s each with the intro, 7-9 chunks broken, 3 falls,
  nearly all of them knock-outs, about one draw in 40.

### Tuning (every number in src/game.h)
| Rule | Value | Where |
|---|---|---|
| the floe | radius 94 (+-4), cells of 4 px; rings 94-72-50-30, 12 + 9 + 6 chunks + the core | FLOE_*, RING*, CELL |
| cracks | first 2.5 s after GO!, then every 130 frames (-5 each, 60 at least), warned 100 frames; core 4 s after; hurry 30/50 | FIRST_CRACK, CRACK_*, CORE_DELAY, HURRY_* |
| waddle | 0.06 px/frame^2, friction 11/256 held, 16/256 let go (top speed 1.4) | WADDLE_ACC, MOVE_FRICTION, IDLE_FRICTION |
| belly slide | 4 px a frame, 14 frames at 4/256, brake 14 frames at 26/256, cooldown 45 | DASH_*, BRAKE_*, DASH_COOLDOWN |
| bumps | radius 10 (12), mass 256, slide x1.5, restitution 110%, 0.8 px/frame at least; knocked 30 frames at 18/256 | SEAL_R, HEAVY_R, *_MASS*, BUMP_*, KNOCK_* |
| clash | 2.6 px a frame back, dizzy 24 frames | CLASH_V, STUN_FRAMES |
| falls | splash 10 frames, out 80; knock-out credit 2.5 s; double 1.5 s | FALL_*, KO_CREDIT, DOUBLE_WINDOW |
| fish | first 7 s, then 9-15 s, lasts 8 s; big belly 2.5 x for 7 s; mega slide 1.3 x speed, 2.5 x mass, within 10 s | FISH_*, HEAVY_*, MEGA_* |
| rounds | intro 1.5 s, GO! 40 frames, winner 130 frames, match 170 frames, first to 3 | INTRO_FRAMES, GO_SHOW, END_FRAMES, MATCH_FRAMES, WIN_ROUNDS |
| score | knock-out 10, round 25, match 100, double +50, sweep +50 | PTS_* |
| bots | levels 0-5 (1 player: one more each match won), level 1 with 2-4 players | BOT_LEVEL_TABLE, BOT_* |
| touch | the stick on the left 55% of the screen, full push at 26 px; DASH radius 22 at 36, 38 from the bottom-right corner | STICK_*, BTN_* |
| medals (1 player) | bronze 100, silver 400, gold 750, pearl 1200 | MEDAL_SCORES |

**The medals** were tuned with the player bot (`bot=1`: the bots' brain at its quickest, no mistakes to speak of)
and the human-like one (`botskill=E`: it reacts 2E frames later, aims 2E/256 of a turn worse, dodges 5E% less,
slides 4E% less, forgets the edge 2E% more and the cracks 6E% more), with `tests/test_rules.c --medals RUNS E`
(one-player runs to the game over; seeds 7000 + 31 k):

| Player | runs | mean score | matches won (mean) | none | bronze | silver | gold | pearl |
|---|---|---|---|---|---|---|---|---|
| a beginner (E = 10) | 20 | 155 | 0.2 | 7 | 13 | 0 | 0 | 0 |
| E = 8 | 20 | 279 | 0.6 | 5 | 9 | 6 | 0 | 0 |
| E = 6 | 12 | 364 | 1.0 | 1 | 5 | 6 | 0 | 0 |
| E = 3 | 12 | 764 | 2.4 | 0 | 2 | 4 | 5 | 1 |
| the player bot (E = 0) | 12 | 910 | 2.8 | 0 | 1 | 4 | 3 | 4 |

So bronze is a beginner's first round or two won (13 of 20 beginner runs), silver a match won and a good second
one, gold two or three matches won, pearl a long run into the level-3 and level-4 bots. A match takes 1-3 minutes.

## Screen and layers
320x240 at 60 Hz, fixed step, the floe in the middle of the stage (centre 160, 136).

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the sea: a raster gradient, lighter at the top, the house 2-line dither | - |
| BG3 | the sea panorama (1024 x 256): ripples (palette cycling: they shimmer), icebergs, ice bits, a fishing boat | fixed (+ shake) |
| BG2 | the floe: a canvas of its own tiles (the ones it may cover: 569), PAINTED pixel by pixel by draw.c from the rules' field: the lit rim (top-left), the shaded rim, drifts of snow (value noise), twinkles, the side below the edge and the submerged skirt round it, a dark crack round a cracking chunk and its blink (2 animated palette entries) | fixed (+ shake); window 1 keeps its 208 columns |
| BG1 | the house UI kit: the logo, ROUND n and GO! (2x, in the middle of the ring of seals), the round's and match's winner on a panel (under the winner when it stands in the floe's top half, over it otherwise), the messages, the hint, the panels, PAUSED | fixed |
| OBJ | the seals (OBJ 0, 1, 4, 5), a chunk drifting off (OBJ 6, painted into one of 2 sprite slots of 56 x 56), splashes, puffs, chips, stars, fish, icons, the HUD's pips, the touch controls (OBJ 2), the penguins (OBJ 7), the kit's digits, glyphs and medals (OBJ 3) | |

Only the tiles of a chunk that changed are painted again (its box; the whole floe at a new round: about 5 ms on the
A20, under the intro). **VRAM**: the floe's canvas shares its tiles with the title's logo (the logo shows only on the
title, the floe only in play: draw.c paints the floe again when a match starts), so the game stays well inside the
64 KiB guideline (no strict-mode warning in English); the glyph pool of the translated texts is at 896..1023.
A falling seal is drawn behind the floe (sprite priority 1): it sinks under the rim.

**Widescreen** (docs/spec.md "Widescreen"): the game opts in (`max_width`, `WIDESCREEN_sealshove = 1`). The floe and
the rules are the same at every width; a wider screen shows more sea: at 427 columns an iceberg with three penguins
on the left and a fishing boat on the right, at 520 more icebergs, penguins and ice bits. The HUD follows the screen's
top corners, the DASH button its bottom-right corner, the UI kit centres itself. A state saved at one width loads at
another. Checks at 320, 427 and 520; the touch test plays the same drag at 320, 427, 520 and 560 and compares.

**Tall screen** (docs/spec.md "Tall screen", the zoomed recipe): a phone held upright. `rs_screen_zoom(ZOOM_H)` (520 lines,
game.h) first in `game_init`: the app scales the 520 lines to the display's height and crops the sides, 222 columns on a
1080x2520 phone (the floe, 196 px with its wobble, and the splashes at its rim), 240 on a 1080x2340 one; the floe is
5x on a 1080x2520 phone. The stage sits `TALL_UP` (130) lines down. The rules are the same.
- **The HUD at the screen's top**: the score at its top, the seals' heads and pips in two rows under the frontend's corner
  buttons, at the edges of the columns shown (`vis_left()` / `vis_right()`, game.h). The DASH button in the bottom-right
  corner of the columns shown, the stick on their left 55% (main.c and draw.c read the same columns).
- **The sea past the stage** (the raster): its gradient carried on; BG3 shows other parts of the 1024-px panorama, moved
  into the columns shown: above, the boat on the left and the lone penguin's iceberg under the score; below (from line
  256), the three penguins' iceberg on the left, the DASH button on the right. Their penguins are drawn with them (and
  cheer with the others). The UI and the floe are hidden there; line 0 puts the stage's settings back.
- **One view**: the title, the panels and the pause stay in the zoomed view of the play (`rs_screen_fill(1)` on every
  screen, never the letterboxed full width), laid out in the columns shown with the kit's games/common/src/house_tall.h
  (`ht_narrow()`: a tall screen showing fewer than 320 columns), its standard calls and rows as in the other games:
  `ht_logo` (SEAL over SHOVE when it does not fit on one line), `ht_title_draw` (the slots on row 15), `ht_banner`,
  `ht_gameover_panel`, `ht_results_*`, `ht_retry_line_at`, `ht_pause`. The title's big seal (decor) bobs in the sea
  under the title's lines. In play, ROUND n, the winner's panel, the messages and the hint are wrapped in the columns
  shown (`ht_big`, `ht_text`). The 4 seals share one floe: no split screen. The taps (start, back to the title) count
  anywhere on the screen.
- Landscape and 4:3 are the same pixels as before (checked at 2340x1080 and 320x240).

## Screens (the house flow: no "get ready", the title is the only menu)
title (the logo, the sea and the penguins, a seal in its cap bobbing, the player slots, PRESS A TO SHOVE with the A
glyph, BEST, the join line, the copyright line) -> P1 presses A (or Start, the d-pad; a tap) -> ROUND 1 (A: BELLY
SLIDE under it in the first round of a 1-player run) -> GO! -> the fight (HUD: each seal's head and its 3 round pips in
the top corners, the head bobbing in the water once it is out, the players' tags with 2-4 players; 1 player: the score
in big digits at the top) -> the round's winner on a panel -> the next round, or the match's winner -> 1 player: MATCH 2
with smarter bots, or (a match lost) GAME OVER: the panel (SCORE, BEST or NEW BEST, the medal, the matches won);
2-4 players: the results -> one press (or a tap): back to the title.

## Controls
| Input | Action |
|---|---|
| d-pad | waddle (8 directions, momentum) |
| A | belly slide (then a 0.75 s cooldown) |
| A (or Start, the d-pad) on the title | start |
| Select (or Start in play) | pause / resume |
| pads 2-4: A or Start on the title | join (B leaves) |
| A, Start or Select on the game-over panel | back to the title |

### Touch (docs/spec.md "Touch")
The game reads the touches itself (`rs_game.touch_native = 1`): the phone apps pass it the fingers and draw no
buttons. Touch is player 1, alongside the pad (players 2-4 join with pads).

| Touch | Action |
|---|---|
| a finger down on the left 55% of the screen, dragged | a **virtual stick**: a ring where it went down, a knob under the finger; the seal waddles that way, from a soft push (a short drag) to the full push (26 px and more) |
| the round **DASH** button (bottom right) | belly slide (as A); it greys out during the cooldown |
| a tap (a finger lifted) on the title | start |
| a tap on the game-over panel | back to the title (after the same lock as A; a finger left down from the match does not count) |
| the app's pause button | pause / resume (it presses Start); the fingers do nothing while paused |

The controls are drawn in the house style of the apps' drawn buttons (a see-through blue disc, the house checker, a
cream ring, a navy outline; filled and lower while pressed, grey on cooldown), the button with a sliding seal, once a
finger has touched the screen. Two fingers work at once. What the fingers set is in the registered state (main.c TV,
touch_*): a load forgets the fingers, one still down starts again. `tests/touch_test.sh` (in the check) plays
scripted fingers.

## Up to four players
Pads 2-4 join with A on the title (B leaves). Always four seals: the players (P1 red cap, P2 blue, P3 green, P4
purple: tools/build_assets.py turns the cap and scarf with house_style.player_palettes) and bots in the free places,
named FLUFF, MOCHI, PUDDLE and BISCUIT by their place (never translated). A numbered tag in the player's colour floats
over each player's seal. One match; the results rank the players by rounds won, with medals.

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py; the floe painted by src/draw.c): no image
generation. The seals: cool grey bodies, pale muzzle and belly, ink-bead eyes with a glint, a knitted cap with a
ribbed brim and a pom-pom and a scarf with a flapping end, in the house's 3/4 view (front, back, side; a 2-frame
waddle; the belly slide flat and stretched); the title's big seal sits up and blinks. The floe: white ice lit from the
top-left with soft drifts of snow and twinkles, its side and a submerged skirt below, cracks in dark blue; chunks
drift off, then turn wet and sink (the house checker). The sea: a deep gradient with shimmering ripples, icebergs in
the floe's style, ice bits, a little red fishing boat. Splashes and droplets, snow puffs behind a slide, ice chips, the
chubby blue fish and the golden fish, penguins that cheer at every splash. The title logo: SEAL in the caps' red,
SHOVE in ice blue.

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects (main.c sound_init): a swish and a "zip" for a
slide, a rubbery "boing" for a bump, a thud and a big boing for a slide's hit, "bonk-bonk" and a wobble for a clash,
sharp crackles when the ice cracks, a crunch and a hiss when a chunk breaks off, a "sploosh" and a bubbly bloop for a
splash, two flops for a fish, a quick climb for a power, a whistle for GO!; the house ding for a round won and the
messages, the house sounds for the menus, the medals and the game over. 1.6 s of samples in all (with the house set:
inside the 64 KiB guideline).

Music: "Floe Polka" (tools/make_music.py): G major, speed 5 (150 BPM), A A' B A' C (about 32 s): an oom-pah polka:
the round bass on the beats (root and fifth: a waddle), the water-drop pluck on the off-beats, a light kit, and the
soft square lead hopping up the chord and tumbling back down; A' sings it an octave up with the pluck doubling, B
slides off sideways (C D Bm Em), C waddles home. Two alternatives (tools/make_music_alt.py -> music-alt/alt1.mod,
alt2.mod; previews in docs/music-alternatives/sealshove/, not committed): "Snowdrift Lullaby" (a gentle floating tune
in C major, speed 7) and "Penguin Parade" (a jaunty march in F major with a snare roll, speed 6). To switch, call
make_music_alt.py OUTDIR N from tools/build_assets.py instead of make_music.py.

## Tests
`make sealshove-check`: the rules alone (tests/test_rules.c: the integer maths; momentum and friction (slow to get
going, top speed, sliding on and stopping, a turning arc); bumps (the pushed seal goes, the pusher nearly stops, a
heavy seal moves less, a slide's shove, resisting shortens it, a clash bounces both back dizzy); the slide and its
cooldown; falling off (the splash, out, the pusher's knock-out, none when walking off alone or pushed long ago, a
double knock-out); the floe (its size, every chunk fits a drift sprite, a new shape every round, the first crack on
the edge after GO!, the warning then the break, a seal on a breaking chunk falls, edge inwards only, the core last);
rounds and the match (the splash awaited, the winner, a draw, first to 3, the next match); the bots (back from the
edge, they push a still player off, bot rounds end with few self-falls and no seat always winning, the player bot
beats level 0, determinism); the fish (big belly, mega slide, fish land on the ice); the touch stick and button),
the libretro loader, the smoke tests (flow, start inputs, waddling and sliding, the game over and back to the title,
pause, joining and leaving, 2 and 4 players, no strict-mode warning, save RAM, the bot scoring, determinism with
scripted, bot and human-like runs), the touch tests, the UI screens (tests/ui_test.sh: the title alone and with 4
players, a round's start, play, the shrunk floe, a dash clash, 4 players, pause, a round won, the game over with a
medal, the 4-player results), the save states (the title's lobby, a match, a match played by touch, 4 players,
paused, the game over, the results) and their audit, at 320, 427 and 520 columns, states across widths, and the 22
languages (tests/i18n_runs.txt).
